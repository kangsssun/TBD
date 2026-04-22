#include "readypage.h"
#include "missionpage.h"
#include "chat/contactgmdialog.h"
#include "notice/noticedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QFont>
#include <QDialog>
#include <QScrollArea>
#include <QApplication>
#include <QScreen>
#include <QJsonObject>
#include <QDebug>
#include <QPixmap>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QGraphicsOpacityEffect>

ReadyPage::ReadyPage(QWidget *parent)
    : QWidget(parent)
    , m_systemUserLabel(nullptr)
    , m_timeRemainingLabel(nullptr)
    , m_missionProgressBar(nullptr)
    , m_globalProgressBar(nullptr)
    , m_readyTimer(new QTimer(this))
    , m_remainingSeconds(0)
    , m_eventLayout(nullptr)
    , m_currentMissionWidget(nullptr)
    , m_eventTitleLabel(nullptr)
    , m_systemNoticesButton(nullptr)
    , m_contactGmButton(nullptr)
    , m_missionPercentLabel(nullptr)
    , m_globalPercentLabel(nullptr)
    , m_unreadNotices(0)
    , m_unreadMessages(0)
    , m_currentProgress(0)
{
    setupUi();
    setMissionWidget(new MissionPage(1, this));

    m_readyTimer->setInterval(1000);
    QObject::connect(m_readyTimer, &QTimer::timeout, this, [this]() {
        if (m_remainingSeconds > 0) {
            --m_remainingSeconds;
            updateHeader();
        }
    });
}

// ────────────────────────────────────────────────────────────────────────────
// Public API
// ────────────────────────────────────────────────────────────────────────────
void ReadyPage::setTeamName(const QString &name)
{
    m_teamName = name;
    updateHeader();
}

void ReadyPage::resetCountdown(int seconds)
{
    m_remainingSeconds = qMax(0, seconds);
    m_readyTimer->stop();
    updateHeader();
}

void ReadyPage::pauseCountdown()
{
    m_readyTimer->stop();
}

void ReadyPage::resumeCountdown()
{
    if (m_remainingSeconds > 0)
        m_readyTimer->start();
}

void ReadyPage::syncTimer(int seconds, bool running)
{
    m_remainingSeconds = qMax(0, seconds);
    updateHeader();
    if (running && m_remainingSeconds > 0) {
        m_readyTimer->start();
    } else {
        m_readyTimer->stop();
    }
}

void ReadyPage::addSystemNotice(const QString &text)
{
    m_notices.append(text);
    m_unreadNotices++;
    if (m_systemNoticesButton) {
        m_systemNoticesButton->setText(QStringLiteral("SYSTEM NOTICES [%1]").arg(m_unreadNotices));
        m_systemNoticesButton->setStyleSheet(QStringLiteral(
            "QPushButton#systemNoticesButton { border: 2px solid #ef4444; color: #ef4444; }"));
    }
}

void ReadyPage::addGmDirectMessage(const QString &text)
{
    m_directMessages.append(text);
    m_unreadMessages++;
    if (m_contactGmButton) {
        m_contactGmButton->setText(QStringLiteral("CONTACT GM [%1]").arg(m_unreadMessages));
        m_contactGmButton->setStyleSheet(QStringLiteral(
            "QPushButton#contactGmButton { border: 2px solid #ef4444; color: #ef4444; }"));
    }
    emit gmMessageReceived(text);
}

void ReadyPage::setMissionProgress(int percent)
{
    percent = qBound(0, percent, 100);
    m_currentProgress = percent;
    if (m_missionProgressBar) m_missionProgressBar->setValue(percent);
    if (m_missionPercentLabel) m_missionPercentLabel->setText(QStringLiteral("%1%").arg(percent));
}

void ReadyPage::setGlobalProgress(int percent)
{
    percent = qBound(0, percent, 100);
    if (m_globalProgressBar) m_globalProgressBar->setValue(percent);
    if (m_globalPercentLabel) m_globalPercentLabel->setText(QStringLiteral("%1%").arg(percent));
}

void ReadyPage::setSendMessageCallback(const std::function<void(const QString &)> &cb)
{
    m_sendMessageCb = cb;
}

void ReadyPage::setProgressUpdateCallback(const std::function<void(int missionNumber, int progress)> &cb)
{
    m_progressUpdateCb = cb;
}

void ReadyPage::setQrSubmitCallback(const std::function<void(const QByteArray &, const std::function<void(const QString &, const QString &)> &)> &cb)
{
    m_qrSubmitCb = cb;
}

void ReadyPage::setMissionWidget(MissionPage *mission)
{
    if (!m_eventLayout) return;

    // Remove the old mission widget if present
    if (m_currentMissionWidget) {
        m_eventLayout->removeWidget(m_currentMissionWidget);
        m_currentMissionWidget->setParent(nullptr);
        m_currentMissionWidget->deleteLater();
    }

    m_currentMissionWidget = mission;
    if (mission) {
        m_eventLayout->addWidget(mission, 1);
        if (m_eventTitleLabel) m_eventTitleLabel->hide();

        // Forward QR submit callback to mission page
        if (m_qrSubmitCb) {
            mission->setQrSubmitCallback(m_qrSubmitCb);
        }

        // When mission completes, advance to the next mission
        QObject::connect(mission, &MissionPage::missionCompleted, this, [this](int completedNumber) {
            setMissionProgress(m_currentProgress + 20);

            if (m_progressUpdateCb) {
                m_progressUpdateCb(completedNumber + 1, m_currentProgress);
            }

            // 미션 5 클리어 → 게임 클리어 처리
            if (completedNumber == 5) {
                // 서버에 game_clear 전송 → 서버가 recovery_code를 보내면 game_node.cpp에서 처리
                if (m_serverMessageCb) {
                    QJsonObject msg;
                    msg["type"] = "game_clear";
                    m_serverMessageCb(msg);
                }
                return;
            }

            auto *nextMission = new MissionPage(completedNumber + 1, this);
            setMissionWidget(nextMission);
            nextMission->startMission();
        });

        // 미션3 문제 화면 표시 5초 후 이벤트 1 트리거
        QObject::connect(mission, &MissionPage::missionProblemShown, this, [this](int mNum) {
            if (mNum == 3) {
                QTimer::singleShot(5000, this, [this]() {
                    showEvent1();
                });
            }
        });
    } else {
        if (m_eventTitleLabel) m_eventTitleLabel->show();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// UI Construction
// ────────────────────────────────────────────────────────────────────────────
void ReadyPage::setupUi()
{
    setObjectName(QStringLiteral("readyPage"));
    auto *readyPageLayout = new QVBoxLayout(this);
    readyPageLayout->setContentsMargins(4, 4, 4, 4);
    readyPageLayout->setSpacing(0);

    auto *mainPanel = new QWidget(this);
    mainPanel->setObjectName(QStringLiteral("hackerMainPanel"));
    auto *mainPanelLayout = new QVBoxLayout(mainPanel);
    mainPanelLayout->setContentsMargins(8, 6, 8, 4);
    mainPanelLayout->setSpacing(4);

    // ── Top bar ────────────────────────────────────────────────────────
    auto *topBar = new QWidget(mainPanel);
    topBar->setObjectName(QStringLiteral("topBarPanel"));
    auto *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->setSpacing(12);

    m_systemUserLabel = new QLabel(QStringLiteral("TEAM: TEAM 1"), topBar);
    m_systemUserLabel->setObjectName(QStringLiteral("teamTerminalLabel"));
    m_systemUserLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto *timerBox = new QWidget(topBar);
    timerBox->setObjectName(QStringLiteral("timerBox"));
    const int headerControlHeight = 54;
    timerBox->setFixedHeight(headerControlHeight);
    timerBox->setMinimumWidth(150);
    auto *timerBoxLayout = new QHBoxLayout(timerBox);
    timerBoxLayout->setContentsMargins(0, 0, 0, 0);
    timerBoxLayout->setSpacing(0);
    timerBoxLayout->setAlignment(Qt::AlignCenter);

    m_timeRemainingLabel = new QLabel(QStringLiteral("45:00"), timerBox);
    m_timeRemainingLabel->setObjectName(QStringLiteral("timeRemainingLabel"));
    m_timeRemainingLabel->setAlignment(Qt::AlignCenter);
    m_timeRemainingLabel->setContentsMargins(0, 0, 0, 0);
    QFont timerFont = m_timeRemainingLabel->font();
    timerFont.setPixelSize(24);
    timerFont.setWeight(QFont::DemiBold);
    m_timeRemainingLabel->setFont(timerFont);
    timerBoxLayout->addWidget(m_timeRemainingLabel);

    auto *headerButtonArea = new QWidget(topBar);
    headerButtonArea->setObjectName(QStringLiteral("headerButtonArea"));
    auto *headerButtonLayout = new QHBoxLayout(headerButtonArea);
    headerButtonLayout->setContentsMargins(0, 0, 0, 0);
    headerButtonLayout->setSpacing(8);

    auto *systemNoticesButton = new QPushButton(QStringLiteral("SYSTEM NOTICES"), headerButtonArea);
    systemNoticesButton->setObjectName(QStringLiteral("systemNoticesButton"));
    systemNoticesButton->setCursor(Qt::PointingHandCursor);
    systemNoticesButton->setFixedHeight(headerControlHeight);
    m_systemNoticesButton = systemNoticesButton;
    QObject::connect(systemNoticesButton, &QPushButton::clicked, this, [this]() {
        m_systemNoticesButton->setEnabled(false);
        m_unreadNotices = 0;
        m_systemNoticesButton->setText(QStringLiteral("SYSTEM NOTICES"));
        m_systemNoticesButton->setStyleSheet(QString());
        NoticeDialog::show(this, m_notices);
        QTimer::singleShot(300, m_systemNoticesButton, [this]() {
            m_systemNoticesButton->setEnabled(true);
        });
    });

    auto *contactGmButton = new QPushButton(QStringLiteral("CONTACT GM"), headerButtonArea);
    contactGmButton->setObjectName(QStringLiteral("contactGmButton"));
    contactGmButton->setCursor(Qt::PointingHandCursor);
    contactGmButton->setFixedHeight(headerControlHeight);
    m_contactGmButton = contactGmButton;
    QObject::connect(contactGmButton, &QPushButton::clicked, this, [this]() {
        m_contactGmButton->setEnabled(false);
        m_unreadMessages = 0;
        m_contactGmButton->setText(QStringLiteral("CONTACT GM"));
        m_contactGmButton->setStyleSheet(QString());
        ContactGmDialog::show(this, m_teamName, m_directMessages, [this](const QString &text) {
            // 보낸 메시지도 히스토리에 저장
            m_directMessages.append(QStringLiteral("[ME] ") + text);
            if (m_sendMessageCb) m_sendMessageCb(text);
        });
        QTimer::singleShot(300, m_contactGmButton, [this]() {
            m_contactGmButton->setEnabled(true);
        });
    });

    auto *helpButton = new QPushButton(
        MissionPage::isOperatorMode() ? QStringLiteral("SKIP") : QStringLiteral("GUIDE"),
        headerButtonArea);
    helpButton->setObjectName(QStringLiteral("helpButton"));
    helpButton->setCursor(Qt::PointingHandCursor);
    helpButton->setFixedHeight(headerControlHeight);
    helpButton->setFixedWidth(contactGmButton->sizeHint().width());
    QObject::connect(helpButton, &QPushButton::clicked, this, [this, helpButton]() {
        // ── Operator mode: force-skip to next mission ──────────────
        if (MissionPage::isOperatorMode()) {
            auto *cur = currentMission();
            if (!cur) return;
            const int curNum = cur->missionNumber();
            if (curNum >= 5) {
                // Last mission: go to ending via server (recovery code flow)
                helpButton->setEnabled(false);
                if (m_serverMessageCb) {
                    QJsonObject msg;
                    msg["type"] = "game_clear";
                    m_serverMessageCb(msg);
                }
                return;
            }

            // Prevent rapid repeated clicks from skipping multiple missions
            helpButton->setEnabled(false);
            QTimer::singleShot(800, helpButton, [helpButton]() {
                helpButton->setEnabled(true);
            });

            setMissionProgress(m_currentProgress + 20);
            if (m_progressUpdateCb) {
                m_progressUpdateCb(curNum + 1, m_currentProgress);
            }
            auto *nextMission = new MissionPage(curNum + 1, this);
            setMissionWidget(nextMission);
            nextMission->startMission();
            return;
        }

        // ── Normal mode: show game guide dialog ────────────────────
        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("gameHelpDialog"));
        dialog.setWindowTitle(QStringLiteral("GAME GUIDE"));
        dialog.setModal(true);
        dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dialog.setAttribute(Qt::WA_StyledBackground, true);
        dialog.setFixedSize(760, 520);
        dialog.setStyleSheet(QStringLiteral(R"(
            QDialog#gameHelpDialog {
                background-color: #0b1220;
                border: 1px solid #10b981;
                border-radius: 12px;
            }
            QLabel#helpTitle {
                background: transparent;
                color: #ffffff;
                font-size: 28px;
                font-weight: 800;
                letter-spacing: 2px;
            }
            QLabel#helpBody {
                background-color: #0b1220;
                color: #e5e7eb;
                font-size: 15px;
                line-height: 1.45em;
            }
            QWidget#helpBodyContainer {
                background-color: #0b1220;
            }
            QPushButton#helpCloseButton {
                background-color: #142338;
                color: #bfdbfe;
                border: 1px solid #2563eb;
                border-radius: 8px;
                padding: 10px 24px;
                font-size: 15px;
                font-weight: 700;
            }
            QPushButton#helpCloseButton:hover {
                background-color: #1a3150;
                color: #ffffff;
            }
            QPushButton#helpCloseButton:pressed {
                background-color: #2563eb;
                color: #ffffff;
            }
            QScrollArea#helpScroll {
                background: transparent;
                border: none;
            }
            QScrollArea#helpScroll > QWidget > QWidget {
                background-color: #0b1220;
            }
        )"));

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(24, 24, 24, 20);
        layout->setSpacing(16);

        auto *titleLabel = new QLabel(QStringLiteral("GAME GUIDE"), &dialog);
        titleLabel->setObjectName(QStringLiteral("helpTitle"));
        titleLabel->setAlignment(Qt::AlignCenter);

        auto *scroll = new QScrollArea(&dialog);
        scroll->setObjectName(QStringLiteral("helpScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        auto *bodyContainer = new QWidget();
        bodyContainer->setObjectName(QStringLiteral("helpBodyContainer"));
        auto *bodyLayout = new QVBoxLayout(bodyContainer);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(10);

        auto *bodyLabel = new QLabel(bodyContainer);
        bodyLabel->setObjectName(QStringLiteral("helpBody"));
        bodyLabel->setWordWrap(true);
        bodyLabel->setTextFormat(Qt::RichText);
        bodyLabel->setText(QStringLiteral(
            "<b>게임 진행</b><br>"
            "- 총 5개 미션을 순서대로 해결합니다.<br>"
            "- 미션 제출 성공 시 자동으로 다음 미션으로 이동합니다.<br><br>"
            "<b>상단 버튼 설명</b><br>"
            "- SYSTEM NOTICES: 운영자 공지 확인<br>"
            "- CONTACT GM: GM에게 문의 메시지 전송<br>"
            "- GUIDE: 현재 도움말 창 열기<br><br>"
            "<b>미션 화면 버튼 설명</b><br>"
            "- PLAY: 미션별 힌트/신호 재생<br>"
            "- INPUT: 정답 직접 입력 팝업 열기<br>"
            "- CAPTURE: 카메라 화면 캡처 (미션3)<br>"
            "- RESET: 캡처 결과 초기화 (미션3)<br>"
            "- SUBMIT: 현재 입력/캡처 결과 제출"));

        bodyLayout->addWidget(bodyLabel);
        bodyLayout->addStretch();
        scroll->setWidget(bodyContainer);

        auto *closeButton = new QPushButton(QStringLiteral("닫기"), &dialog);
        closeButton->setObjectName(QStringLiteral("helpCloseButton"));
        closeButton->setCursor(Qt::PointingHandCursor);
        closeButton->setFixedHeight(46);

        layout->addWidget(titleLabel);
        layout->addWidget(scroll, 1);
        layout->addWidget(closeButton, 0, Qt::AlignRight);

        QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

        const QPoint centerGlobal = this->mapToGlobal(this->rect().center());
        dialog.move(centerGlobal - QPoint(dialog.width() / 2, dialog.height() / 2));
        dialog.exec();
    });

    headerButtonLayout->addWidget(systemNoticesButton);
    headerButtonLayout->addWidget(contactGmButton);
    headerButtonLayout->addWidget(helpButton);

    topBarLayout->addWidget(m_systemUserLabel, 3);
    topBarLayout->addWidget(timerBox, 1, Qt::AlignLeft);
    topBarLayout->addStretch();
    topBarLayout->addWidget(headerButtonArea, 0, Qt::AlignRight);

    // ── Divider ────────────────────────────────────────────────────────
    auto *dividerTop = new QWidget(mainPanel);
    dividerTop->setObjectName(QStringLiteral("thinDivider"));
    dividerTop->setFixedHeight(1);

    // ── Progress area ──────────────────────────────────────────────────
    auto *progressArea = new QWidget(mainPanel);
    progressArea->setObjectName(QStringLiteral("progressArea"));
    auto *progressAreaLayout = new QVBoxLayout(progressArea);
    progressAreaLayout->setContentsMargins(0, 0, 0, 0);
    progressAreaLayout->setSpacing(8);

    auto *missionProgressRow = new QWidget(progressArea);
    missionProgressRow->setObjectName(QStringLiteral("missionProgressRow"));
    auto *missionProgressLayout = new QHBoxLayout(missionProgressRow);
    missionProgressLayout->setContentsMargins(0, 0, 0, 0);
    missionProgressLayout->setSpacing(10);

    auto *missionLabel = new QLabel(QStringLiteral("MISSION PROGRESS"), missionProgressRow);
    missionLabel->setObjectName(QStringLiteral("missionProgressLabel"));
    missionLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    missionLabel->setMinimumWidth(170);

    m_missionProgressBar = new QProgressBar(missionProgressRow);
    m_missionProgressBar->setObjectName(QStringLiteral("missionProgressBar"));
    m_missionProgressBar->setRange(0, 100);
    m_missionProgressBar->setValue(0);
    m_missionProgressBar->setTextVisible(false);

    m_missionPercentLabel = new QLabel(QStringLiteral("0%"), missionProgressRow);
    m_missionPercentLabel->setObjectName(QStringLiteral("missionPercentLabel"));
    m_missionPercentLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    missionProgressLayout->addWidget(missionLabel, 1);
    missionProgressLayout->addWidget(m_missionProgressBar, 8);
    missionProgressLayout->addWidget(m_missionPercentLabel, 1);

    auto *globalProgressRow = new QWidget(progressArea);
    globalProgressRow->setObjectName(QStringLiteral("globalProgressRow"));
    auto *globalProgressLayout = new QHBoxLayout(globalProgressRow);
    globalProgressLayout->setContentsMargins(0, 0, 0, 0);
    globalProgressLayout->setSpacing(10);

    auto *globalLabel = new QLabel(QStringLiteral("AVERAGE PROGRESS"), globalProgressRow);
    globalLabel->setObjectName(QStringLiteral("globalProgressLabel"));
    globalLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    globalLabel->setMinimumWidth(170);

    m_globalProgressBar = new QProgressBar(globalProgressRow);
    m_globalProgressBar->setObjectName(QStringLiteral("globalProgressBar"));
    m_globalProgressBar->setRange(0, 100);
    m_globalProgressBar->setValue(0);
    m_globalProgressBar->setTextVisible(false);

    m_globalPercentLabel = new QLabel(QStringLiteral("0%"), globalProgressRow);
    m_globalPercentLabel->setObjectName(QStringLiteral("globalPercentLabel"));
    m_globalPercentLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    globalProgressLayout->addWidget(globalLabel, 1);
    globalProgressLayout->addWidget(m_globalProgressBar, 8);
    globalProgressLayout->addWidget(m_globalPercentLabel, 1);

    progressAreaLayout->addWidget(missionProgressRow);
    progressAreaLayout->addWidget(globalProgressRow);

    // ── Divider ────────────────────────────────────────────────────────
    auto *dividerBottom = new QWidget(mainPanel);
    dividerBottom->setObjectName(QStringLiteral("thinDivider"));
    dividerBottom->setFixedHeight(1);

    // ── Event container (mission area) ─────────────────────────────────
    auto *eventContainer = new QWidget(mainPanel);
    eventContainer->setObjectName(QStringLiteral("eventContainer"));
    m_eventLayout = new QVBoxLayout(eventContainer);
    m_eventLayout->setContentsMargins(4, 2, 4, 2);
    m_eventLayout->setSpacing(4);

    auto *eventTitle = new QLabel(QStringLiteral("MISSION 1 - SYSTEM RECOVERY"), eventContainer);
    eventTitle->setObjectName(QStringLiteral("eventTitleLabel"));
    eventTitle->setAlignment(Qt::AlignCenter);
    m_eventTitleLabel = eventTitle;
    m_eventLayout->addWidget(eventTitle);

    // ── Assemble main panel ────────────────────────────────────────────
    mainPanelLayout->addWidget(topBar);
    mainPanelLayout->addWidget(dividerTop);
    mainPanelLayout->addWidget(progressArea);
    mainPanelLayout->addWidget(dividerBottom);
    mainPanelLayout->addWidget(eventContainer, 1);

    readyPageLayout->addWidget(mainPanel);
}

// ────────────────────────────────────────────────────────────────────────────
// Private helpers
// ────────────────────────────────────────────────────────────────────────────
void ReadyPage::updateHeader()
{
    if (m_systemUserLabel) {
        m_systemUserLabel->setText(QStringLiteral("TEAM: %1").arg(m_teamName.toUpper()));
    }
    if (m_timeRemainingLabel) {
        m_timeRemainingLabel->setText(formatRemainingTime());
    }
}

QString ReadyPage::formatRemainingTime() const
{
    const int min = m_remainingSeconds / 60;
    const int sec = m_remainingSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'));
}

void ReadyPage::setServerMessageCallback(const std::function<void(const QJsonObject &)> &cb)
{
    m_serverMessageCb = cb;
}

// ════════════════════════════════════════════════════════════════════════════
// EVENT 1: 바이러스 제거 긴급 이벤트
// ════════════════════════════════════════════════════════════════════════════
void ReadyPage::showEvent1()
{
    qDebug() << "[EVENT] Event 1 triggered";

    // ── GM에 이벤트 시작 알림 ──
    if (m_serverMessageCb) {
        QJsonObject msg;
        msg["type"] = "event_status";
        msg["event"] = "event1";
        msg["status"] = "started";
        m_serverMessageCb(msg);
    }

    QWidget *topLevel = window();

    // ═══ 1단계: 이벤트 시작 알림 다이얼로그 ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(500, 200);
        dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #ff2222;"));

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(24, 20, 24, 16);
        layout->setSpacing(12);

        auto *msgLabel = new QLabel(dlg);
        msgLabel->setWordWrap(true);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setText(QStringLiteral(
            "[EMERGENCY ALERT]\n\n"
            "외부 바이러스가 보안 시스템 커널에 침투했습니다.\n"
            "긴급 이벤트 1이 시작됩니다."));
        msgLabel->setStyleSheet(QStringLiteral(
            "color: #ff4444; font-size: 16px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(msgLabel, 1);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnOk = new QPushButton(QStringLiteral("확인"), dlg);
        btnOk->setFixedSize(100, 36);
        btnOk->setCursor(Qt::PointingHandCursor);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a1a2e; color: #ff4444; border: 1px solid #ff4444; "
            "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
            "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"));
        btnRow->addWidget(btnOk);
        layout->addLayout(btnRow);

        QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

        // 오버레이
        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
    }

    // ═══ 2단계: 긴급 이벤트 메인 패널 ═══
    bool solved = false;
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);  // 수동 삭제
        dlg->setFixedSize(620, 420);

        auto *mainLayout = new QVBoxLayout(dlg);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 외부 경고 박스
        auto *outerFrame = new QFrame(dlg);
        outerFrame->setStyleSheet(QStringLiteral(
            "background-color: #1a0000; border: 3px solid #ff2222; border-radius: 8px;"));
        auto *outerGlow = new QGraphicsDropShadowEffect(outerFrame);
        outerGlow->setBlurRadius(40);
        outerGlow->setColor(QColor(255, 34, 34, 160));
        outerGlow->setOffset(0, 0);
        outerFrame->setGraphicsEffect(outerGlow);

        auto *outerLayout = new QVBoxLayout(outerFrame);
        outerLayout->setContentsMargins(20, 16, 20, 16);
        outerLayout->setSpacing(12);

        // 제목
        auto *titleLabel = new QLabel(QStringLiteral("EVENT 1: 바이러스 제거"), outerFrame);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #ff2222; font-size: 24px; font-weight: 900; "
            "font-family: 'Consolas', monospace; background: transparent; border: none; "
            "text-shadow: 0 0 10px rgba(255,34,34,0.5);"));
        outerLayout->addWidget(titleLabel);

        // 내부 박스
        auto *innerFrame = new QFrame(outerFrame);
        innerFrame->setStyleSheet(QStringLiteral(
            "background-color: #0d0000; border: 1px solid #ff4444; border-radius: 4px;"));
        auto *innerLayout = new QVBoxLayout(innerFrame);
        innerLayout->setContentsMargins(16, 14, 16, 14);
        innerLayout->setSpacing(10);

        auto *questionLabel = new QLabel(innerFrame);
        questionLabel->setWordWrap(true);
        questionLabel->setAlignment(Qt::AlignCenter);
        questionLabel->setText(QStringLiteral(
            "외부 바이러스가 보안 시스템 커널에 침투했습니다.\n"
            "바이러스 모듈명 'virus.ko'를 제거하는 올바른 명령어를 선택하십시오.\n"));
        questionLabel->setStyleSheet(QStringLiteral(
            "color: #ffffff; font-size: 15px; font-family: 'Consolas', monospace; "
            "background: transparent; border: none; line-height: 140%;"));
        innerLayout->addWidget(questionLabel);

        // 선택지
        auto *choicesLabel = new QLabel(innerFrame);
        choicesLabel->setWordWrap(true);
        choicesLabel->setAlignment(Qt::AlignCenter);
        choicesLabel->setText(QStringLiteral(
            "A) insmod virus.ko\n"
            "B) rmmod virus\n"
            "C) modprobe virus\n"
            "D) depmod virus"));
        choicesLabel->setStyleSheet(QStringLiteral(
            "color: #ffcc00; font-size: 16px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none; "
            "line-height: 160%;"));
        innerLayout->addWidget(choicesLabel);

        // 경고
        auto *warningLabel = new QLabel(QStringLiteral(
            "(오답 시 바이러스로 인해 타이머가 1분 감소합니다.)"), innerFrame);
        warningLabel->setAlignment(Qt::AlignCenter);
        warningLabel->setStyleSheet(QStringLiteral(
            "color: #ff6666; font-size: 13px; font-family: 'Consolas', monospace; "
            "background: transparent; border: none;"));
        innerLayout->addWidget(warningLabel);

        outerLayout->addWidget(innerFrame, 1);

        // A/B/C/D 버튼
        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(12);
        btnRow->addStretch();

        const QStringList choices = { QStringLiteral("A"), QStringLiteral("B"),
                                      QStringLiteral("C"), QStringLiteral("D") };
        auto *statusLabel = new QLabel(outerFrame);
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setStyleSheet(QStringLiteral(
            "color: #ff4444; font-size: 14px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        statusLabel->setVisible(false);

        for (const QString &ch : choices) {
            auto *btn = new QPushButton(ch, outerFrame);
            btn->setFixedSize(80, 44);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a0000; color: #ffffff; border: 2px solid #ff4444; "
                "border-radius: 6px; font-size: 20px; font-weight: 900; font-family: Consolas; }"
                "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"
                "QPushButton:pressed { background: #cc0000; }"));
            btnRow->addWidget(btn);

            QObject::connect(btn, &QPushButton::clicked, dlg,
                [this, dlg, ch, statusLabel, &solved]() {
                    if (ch == QStringLiteral("B")) {
                        // 정답
                        solved = true;
                        dlg->accept();
                    } else {
                        // 오답 — 타이머 1분 감소 요청
                        statusLabel->setText(QStringLiteral("오답! 타이머가 1분 감소합니다."));
                        statusLabel->setVisible(true);
                        if (m_serverMessageCb) {
                            QJsonObject msg;
                            msg["type"] = "timer_penalty";
                            msg["seconds"] = 60;
                            m_serverMessageCb(msg);
                        }
                        // 타이머 로컬도 감소
                        m_remainingSeconds = qMax(0, m_remainingSeconds - 60);
                        updateHeader();
                        QTimer::singleShot(1500, statusLabel, [statusLabel]() {
                            statusLabel->setVisible(false);
                        });
                    }
                });
        }
        btnRow->addStretch();
        outerLayout->addLayout(btnRow);
        outerLayout->addWidget(statusLabel);

        mainLayout->addWidget(outerFrame);

        // 오버레이
        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
        dlg->deleteLater();
    }

    // ═══ 3단계: 이벤트 종료 알림 다이얼로그 ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(500, 160);
        dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #00ff41;"));

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(24, 20, 24, 16);
        layout->setSpacing(12);

        auto *msgLabel = new QLabel(dlg);
        msgLabel->setWordWrap(true);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setText(QStringLiteral("시스템 정상화 완료. 본 미션으로 복귀합니다."));
        msgLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 16px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(msgLabel, 1);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnOk = new QPushButton(QStringLiteral("확인"), dlg);
        btnOk->setFixedSize(100, 36);
        btnOk->setCursor(Qt::PointingHandCursor);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a1a2e; color: #00ff41; border: 1px solid #00ff41; "
            "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
            "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
        btnRow->addWidget(btnOk);
        layout->addLayout(btnRow);

        QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
    }

    // ── GM에 이벤트 종료 알림 ──
    if (m_serverMessageCb) {
        QJsonObject msg;
        msg["type"] = "event_status";
        msg["event"] = "event1";
        msg["status"] = "completed";
        m_serverMessageCb(msg);
    }

    qDebug() << "[EVENT] Event 1 completed";
}

MissionPage *ReadyPage::currentMission() const
{
    return qobject_cast<MissionPage *>(m_currentMissionWidget);
}

// ════════════════════════════════════════════════════════════════════════════
// ENDING SEQUENCE — 부팅 로그 → 글리치 → 복구코드 → 퇴실 처리
// ════════════════════════════════════════════════════════════════════════════
void ReadyPage::showEndingSequence(const QString &recoveryCode,
                                   const QString &serverClearTime,
                                   int rank, int totalTeams, bool isLast)
{
    qDebug() << "[ENDING] Ending sequence started";

    QWidget *topLevel = window();
    const QSize screenSize = topLevel->size();

    // ═══ 1단계: 부팅 로그 화면 (갓찌 등장으로 중단) ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);
        dlg->setFixedSize(screenSize);
        dlg->move(0, 0);
        dlg->setStyleSheet(QStringLiteral("background-color: #000000;"));

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(30, 20, 30, 20);
        layout->setSpacing(0);

        auto *textLabel = new QLabel(dlg);
        textLabel->setTextFormat(Qt::RichText);
        textLabel->setWordWrap(true);
        textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        textLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 15px; font-family: 'Consolas', 'Courier New', monospace; "
            "font-weight: 500; background: transparent; border: none; line-height: 150%;"));
        textLabel->setText(QString());
        layout->addWidget(textLabel, 1);

        QStringList bootLines;
        bootLines << QStringLiteral("<span style='color:#888'>[17:45:00]</span> Booting ESCAPE-OS v12.0...")
                  << QStringLiteral("<span style='color:#888'>[17:45:00]</span> Beacon Repair....")
                  << QStringLiteral("<span style='color:#888'>[17:45:01]</span> <span style='color:#00ff41'>[  OK  ]</span> Started LED Beacon Service")
                  << QStringLiteral("<span style='color:#888'>[17:45:02]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Sound Authentication Module")
                  << QStringLiteral("<span style='color:#888'>[17:45:03]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Camera QR Decoder")
                  << QStringLiteral("<span style='color:#888'>[17:45:04]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Thermal Control Unit")
                  << QStringLiteral("<span style='color:#888'>[17:45:05]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Gyro Navigation System")
                  << QStringLiteral("<span style='color:#888'>[17:45:06]</span> <span style='color:#ffcc00'>[ INFO ]</span> rmmod virus.ko ... <span style='color:#00ff41'>SUCCESS</span>")
                  << QStringLiteral("<span style='color:#888'>[17:45:07]</span> <span style='color:#ff4444'>[ WARN ]</span> <span style='color:#ffcc00'>Unauthorized entity detected - scanning kernel...</span>")
                  << QStringLiteral("<span style='color:#888'>[17:45:07]</span> <span style='color:#ff2222'>[ALERT ]</span> <span style='color:#ff2222'>HAMSTER HIJACKED BOOT SEQUENCE</span>");

        auto *bootTimer = new QTimer(dlg);
        bootTimer->setSingleShot(true);
        auto lineIdx = std::make_shared<int>(0);

        QObject::connect(bootTimer, &QTimer::timeout, dlg,
            [textLabel, bootTimer, bootLines, lineIdx, dlg]() {
                const int idx = *lineIdx;
                if (idx >= bootLines.size()) {
                    QTimer::singleShot(1500, dlg, &QDialog::accept);
                    return;
                }
                QString html = textLabel->text();
                if (!html.isEmpty()) html += QStringLiteral("<br>");
                html += bootLines.at(idx);
                textLabel->setText(html);
                (*lineIdx)++;
                bootTimer->start(900);
            });

        bootTimer->start(1200);
        dlg->exec();
        dlg->deleteLater();
    }

    // ═══ 1-B: 갓찌 돌발 퀴즈 이벤트 ═══
    {
        qDebug() << "[ENDING] Ending quiz event started";

        if (m_serverMessageCb) {
            QJsonObject msg;
            msg["type"] = "event_status";
            msg["event"] = "ending_quiz";
            msg["status"] = "started";
            m_serverMessageCb(msg);
        }

        // --- 긴급 알림 다이얼로그 ---
        {
            auto *dlg = new QDialog(topLevel);
            dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setFixedSize(500, 220);
            dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #ff2222;"));

            auto *eLayout = new QVBoxLayout(dlg);
            eLayout->setContentsMargins(24, 20, 24, 16);
            eLayout->setSpacing(12);

            auto *msgLabel = new QLabel(dlg);
            msgLabel->setWordWrap(true);
            msgLabel->setAlignment(Qt::AlignCenter);
            msgLabel->setText(QStringLiteral(
                "[EMERGENCY ALERT]\n\n"
                "갓찌가 부팅 시퀀스를 탈취했습니다!\n"
                "돌발 퀴즈 이벤트가 시작됩니다."));
            msgLabel->setStyleSheet(QStringLiteral(
                "color: #ff4444; font-size: 16px; font-weight: bold; "
                "font-family: 'Consolas', monospace; background: transparent; border: none;"));
            eLayout->addWidget(msgLabel, 1);

            auto *eBtnRow = new QHBoxLayout();
            eBtnRow->addStretch();
            auto *btnOk = new QPushButton(QStringLiteral("확인"), dlg);
            btnOk->setFixedSize(100, 36);
            btnOk->setCursor(Qt::PointingHandCursor);
            btnOk->setFocusPolicy(Qt::NoFocus);
            btnOk->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a1a2e; color: #ff4444; border: 1px solid #ff4444; "
                "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
                "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"));
            eBtnRow->addWidget(btnOk);
            eLayout->addLayout(eBtnRow);

            QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

            auto *overlay = new QWidget(topLevel);
            overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
            overlay->setGeometry(topLevel->rect());
            overlay->show();
            overlay->raise();

            if (auto *screen = QApplication::primaryScreen()) {
                const QRect geo = screen->availableGeometry();
                dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
            }

            dlg->exec();
            overlay->hide();
            overlay->deleteLater();
        }

        // 실패 팝업 헬퍼
        auto showFailPopup = [topLevel]() {
            auto *failDlg = new QDialog(topLevel);
            failDlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            failDlg->setAttribute(Qt::WA_DeleteOnClose);
            failDlg->setFixedSize(520, 380);
            failDlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #ff2222;"));

            auto *fLayout = new QVBoxLayout(failDlg);
            fLayout->setContentsMargins(24, 20, 24, 16);
            fLayout->setSpacing(12);

            auto *fMsg = new QLabel(failDlg);
            fMsg->setWordWrap(true);
            fMsg->setAlignment(Qt::AlignCenter);
            fMsg->setTextFormat(Qt::RichText);
            fMsg->setText(QStringLiteral(
                "<div style='font-family: Consolas, monospace; color: #666; font-size: 11px;'>"
                "--------------------------------</div>"
                "<div style='font-family: Consolas, monospace; color: #ff4444; font-size: 20px; "
                "font-weight: bold; margin-top: 12px;'>오답입니다.</div>"
                "<div style='font-family: Consolas, monospace; color: #ffcc00; font-size: 14px; "
                "margin-top: 16px; font-style: italic;'>"
                "\"찌이익!!! (내 마음도 모르는 바보들이다 찍!)\"</div>"
                "<div style='font-family: Consolas, monospace; color: #cccccc; font-size: 14px; "
                "margin-top: 16px; line-height: 170%;'>"
                "갓찌가 분노하여 전원 코드를 강하게 흔들고 있습니다!<br>"
                "화면이 깜빡거립니다.<br>"
                "서둘러 다른 정답을 골라 갓찌를 달래주십시오!</div>"
                "<div style='font-family: Consolas, monospace; color: #666; font-size: 11px; "
                "margin-top: 12px;'>"
                "--------------------------------</div>"));
            fMsg->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
            fLayout->addWidget(fMsg, 1);

            auto *fBtnRow = new QHBoxLayout();
            fBtnRow->addStretch();
            auto *btnRetry = new QPushButton(QStringLiteral("다시 갓찌 달래기"), failDlg);
            btnRetry->setFixedSize(220, 44);
            btnRetry->setCursor(Qt::PointingHandCursor);
            btnRetry->setFocusPolicy(Qt::NoFocus);
            btnRetry->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a0000; color: #ff4444; border: 2px solid #ff4444; "
                "border-radius: 6px; font-size: 15px; font-weight: bold; font-family: Consolas; }"
                "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"));
            fBtnRow->addWidget(btnRetry);
            fBtnRow->addStretch();
            fLayout->addLayout(fBtnRow);

            QObject::connect(btnRetry, &QPushButton::clicked, failDlg, &QDialog::accept);

            auto *fOverlay = new QWidget(topLevel);
            fOverlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
            fOverlay->setGeometry(topLevel->rect());
            fOverlay->show();
            fOverlay->raise();

            if (auto *screen = QApplication::primaryScreen()) {
                const QRect geo = screen->availableGeometry();
                failDlg->move(geo.center() - QPoint(failDlg->width() / 2, failDlg->height() / 2));
            }

            failDlg->exec();
            fOverlay->hide();
            fOverlay->deleteLater();
        };

        // --- 문제 1: 마우스 ---
        {
            auto *dlg = new QDialog(topLevel);
            dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            dlg->setAttribute(Qt::WA_DeleteOnClose, false);
            dlg->setFixedSize(620, 440);

            auto *mainLayout = new QVBoxLayout(dlg);
            mainLayout->setContentsMargins(0, 0, 0, 0);
            mainLayout->setSpacing(0);

            auto *outerFrame = new QFrame(dlg);
            outerFrame->setStyleSheet(QStringLiteral(
                "background-color: #1a0000; border: 3px solid #ff2222; border-radius: 8px;"));
            auto *outerGlow = new QGraphicsDropShadowEffect(outerFrame);
            outerGlow->setBlurRadius(40);
            outerGlow->setColor(QColor(255, 34, 34, 160));
            outerGlow->setOffset(0, 0);
            outerFrame->setGraphicsEffect(outerGlow);

            auto *outerLayout = new QVBoxLayout(outerFrame);
            outerLayout->setContentsMargins(20, 16, 20, 16);
            outerLayout->setSpacing(12);

            auto *titleLabel = new QLabel(QStringLiteral("갓찌 돌발 퀴즈 — 문제 1"), outerFrame);
            titleLabel->setAlignment(Qt::AlignCenter);
            titleLabel->setStyleSheet(QStringLiteral(
                "color: #ff2222; font-size: 22px; font-weight: 900; "
                "font-family: 'Consolas', monospace; background: transparent; border: none; "
                "text-shadow: 0 0 10px rgba(255,34,34,0.5);"));
            outerLayout->addWidget(titleLabel);

            auto *innerFrame = new QFrame(outerFrame);
            innerFrame->setStyleSheet(QStringLiteral(
                "background-color: #0d0000; border: 1px solid #ff4444; border-radius: 4px;"));
            auto *innerLayout = new QVBoxLayout(innerFrame);
            innerLayout->setContentsMargins(16, 14, 16, 14);
            innerLayout->setSpacing(10);

            auto *questionLabel = new QLabel(innerFrame);
            questionLabel->setWordWrap(true);
            questionLabel->setAlignment(Qt::AlignCenter);
            questionLabel->setText(QStringLiteral(
                "갓찌가 보안 서버실에 처음 침투했을 때,\n"
                "불쌍하다며 가장 먼저 바깥으로 구출해 준\n"
                "컴퓨터 부품은?\n"));
            questionLabel->setStyleSheet(QStringLiteral(
                "color: #ffffff; font-size: 15px; font-family: 'Consolas', monospace; "
                "background: transparent; border: none; line-height: 150%;"));
            innerLayout->addWidget(questionLabel);

            auto *choicesLabel = new QLabel(innerFrame);
            choicesLabel->setWordWrap(true);
            choicesLabel->setAlignment(Qt::AlignCenter);
            choicesLabel->setText(QStringLiteral(
                "A) 키보드\n"
                "B) 마우스\n"
                "C) 모니터\n"
                "D) 그래픽카드"));
            choicesLabel->setStyleSheet(QStringLiteral(
                "color: #ffcc00; font-size: 16px; font-weight: bold; "
                "font-family: 'Consolas', monospace; background: transparent; border: none; "
                "line-height: 160%;"));
            innerLayout->addWidget(choicesLabel);

            outerLayout->addWidget(innerFrame, 1);

            auto *btnRow = new QHBoxLayout();
            btnRow->setSpacing(12);
            btnRow->addStretch();

            const QStringList q1Choices = { QStringLiteral("A"), QStringLiteral("B"),
                                             QStringLiteral("C"), QStringLiteral("D") };
            for (const QString &ch : q1Choices) {
                auto *btn = new QPushButton(ch, outerFrame);
                btn->setFixedSize(80, 44);
                btn->setCursor(Qt::PointingHandCursor);
                btn->setFocusPolicy(Qt::NoFocus);
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: #1a0000; color: #ffffff; border: 2px solid #ff4444; "
                    "border-radius: 6px; font-size: 20px; font-weight: 900; font-family: Consolas; }"
                    "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"
                    "QPushButton:pressed { background: #cc0000; }"));
                btnRow->addWidget(btn);

                QObject::connect(btn, &QPushButton::clicked, dlg,
                    [dlg, ch, &showFailPopup]() {
                        if (ch == QStringLiteral("B")) {
                            dlg->accept();
                        } else {
                            showFailPopup();
                        }
                    });
            }
            btnRow->addStretch();
            outerLayout->addLayout(btnRow);

            mainLayout->addWidget(outerFrame);

            auto *overlay = new QWidget(topLevel);
            overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
            overlay->setGeometry(topLevel->rect());
            overlay->show();
            overlay->raise();

            if (auto *screen = QApplication::primaryScreen()) {
                const QRect geo = screen->availableGeometry();
                dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
            }

            dlg->exec();
            overlay->hide();
            overlay->deleteLater();
            dlg->deleteLater();
        }

        // --- 문제 2: C 언어 ---
        {
            auto *dlg = new QDialog(topLevel);
            dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            dlg->setAttribute(Qt::WA_DeleteOnClose, false);
            dlg->setFixedSize(620, 440);

            auto *mainLayout = new QVBoxLayout(dlg);
            mainLayout->setContentsMargins(0, 0, 0, 0);
            mainLayout->setSpacing(0);

            auto *outerFrame = new QFrame(dlg);
            outerFrame->setStyleSheet(QStringLiteral(
                "background-color: #1a0000; border: 3px solid #ff2222; border-radius: 8px;"));
            auto *outerGlow = new QGraphicsDropShadowEffect(outerFrame);
            outerGlow->setBlurRadius(40);
            outerGlow->setColor(QColor(255, 34, 34, 160));
            outerGlow->setOffset(0, 0);
            outerFrame->setGraphicsEffect(outerGlow);

            auto *outerLayout = new QVBoxLayout(outerFrame);
            outerLayout->setContentsMargins(20, 16, 20, 16);
            outerLayout->setSpacing(12);

            auto *titleLabel = new QLabel(QStringLiteral("갓찌 돌발 퀴즈 — 문제 2"), outerFrame);
            titleLabel->setAlignment(Qt::AlignCenter);
            titleLabel->setStyleSheet(QStringLiteral(
                "color: #ff2222; font-size: 22px; font-weight: 900; "
                "font-family: 'Consolas', monospace; background: transparent; border: none; "
                "text-shadow: 0 0 10px rgba(255,34,34,0.5);"));
            outerLayout->addWidget(titleLabel);

            auto *innerFrame = new QFrame(outerFrame);
            innerFrame->setStyleSheet(QStringLiteral(
                "background-color: #0d0000; border: 1px solid #ff4444; border-radius: 4px;"));
            auto *innerLayout = new QVBoxLayout(innerFrame);
            innerLayout->setContentsMargins(16, 14, 16, 14);
            innerLayout->setSpacing(10);

            auto *questionLabel = new QLabel(innerFrame);
            questionLabel->setWordWrap(true);
            questionLabel->setAlignment(Qt::AlignCenter);
            questionLabel->setText(QStringLiteral(
                "갓찌가 12기 교육생들 어깨너머로 배워서,\n"
                "가장 완벽하게 마스터한 프로그래밍 언어는\n"
                "무엇일까?\n"));
            questionLabel->setStyleSheet(QStringLiteral(
                "color: #ffffff; font-size: 15px; font-family: 'Consolas', monospace; "
                "background: transparent; border: none; line-height: 150%;"));
            innerLayout->addWidget(questionLabel);

            auto *choicesLabel = new QLabel(innerFrame);
            choicesLabel->setWordWrap(true);
            choicesLabel->setAlignment(Qt::AlignCenter);
            choicesLabel->setText(QStringLiteral(
                "A) Java\n"
                "B) Python\n"
                "C) C 언어\n"
                "D) JavaScript"));
            choicesLabel->setStyleSheet(QStringLiteral(
                "color: #ffcc00; font-size: 16px; font-weight: bold; "
                "font-family: 'Consolas', monospace; background: transparent; border: none; "
                "line-height: 160%;"));
            innerLayout->addWidget(choicesLabel);

            outerLayout->addWidget(innerFrame, 1);

            auto *btnRow = new QHBoxLayout();
            btnRow->setSpacing(12);
            btnRow->addStretch();

            const QStringList q2Choices = { QStringLiteral("A"), QStringLiteral("B"),
                                             QStringLiteral("C"), QStringLiteral("D") };
            for (const QString &ch : q2Choices) {
                auto *btn = new QPushButton(ch, outerFrame);
                btn->setFixedSize(80, 44);
                btn->setCursor(Qt::PointingHandCursor);
                btn->setFocusPolicy(Qt::NoFocus);
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: #1a0000; color: #ffffff; border: 2px solid #ff4444; "
                    "border-radius: 6px; font-size: 20px; font-weight: 900; font-family: Consolas; }"
                    "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"
                    "QPushButton:pressed { background: #cc0000; }"));
                btnRow->addWidget(btn);

                QObject::connect(btn, &QPushButton::clicked, dlg,
                    [dlg, ch, &showFailPopup]() {
                        if (ch == QStringLiteral("C")) {
                            dlg->accept();
                        } else {
                            showFailPopup();
                        }
                    });
            }
            btnRow->addStretch();
            outerLayout->addLayout(btnRow);

            mainLayout->addWidget(outerFrame);

            auto *overlay = new QWidget(topLevel);
            overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
            overlay->setGeometry(topLevel->rect());
            overlay->show();
            overlay->raise();

            if (auto *screen = QApplication::primaryScreen()) {
                const QRect geo = screen->availableGeometry();
                dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
            }

            dlg->exec();
            overlay->hide();
            overlay->deleteLater();
            dlg->deleteLater();
        }

        // --- 성공 팝업 ---
        {
            auto *dlg = new QDialog(topLevel);
            dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setFixedSize(520, 440);
            dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #00ff41;"));

            auto *sLayout = new QVBoxLayout(dlg);
            sLayout->setContentsMargins(24, 20, 24, 16);
            sLayout->setSpacing(12);

            auto *sMsg = new QLabel(dlg);
            sMsg->setWordWrap(true);
            sMsg->setAlignment(Qt::AlignCenter);
            sMsg->setTextFormat(Qt::RichText);
            sMsg->setText(QStringLiteral(
                "<div style='font-family: Consolas, monospace; color: #666; font-size: 11px;'>"
                "--------------------------------</div>"
                "<div style='font-family: Consolas, monospace; color: #00ff41; font-size: 16px; "
                "font-weight: bold; margin-top: 10px; line-height: 180%;'>"
                "정답 입력 완료<br>"
                "갓찌의 만족도 100% 달성<br>"
                "전원 코드 안전 확보</div>"
                "<div style='font-family: Consolas, monospace; color: #ffcc00; font-size: 14px; "
                "margin-top: 14px; font-style: italic;'>"
                "\"찌익... (분하지만 정답이다 찍. 훌륭한 교육생들이었다 찍...)\"</div>"
                "<div style='font-family: Consolas, monospace; color: #cccccc; font-size: 14px; "
                "margin-top: 14px; line-height: 170%;'>"
                "갓찌가 해바라기씨를 오물거리며 서버실 구석으로 물러납니다.<br>"
                "드디어 마스터 코드가 활성화되었습니다.<br><br>"
                "<span style='color: #00ff41; font-weight: bold;'>탈출을 축하합니다!</span></div>"
                "<div style='font-family: Consolas, monospace; color: #666; font-size: 11px; "
                "margin-top: 10px;'>"
                "--------------------------------</div>"));
            sMsg->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
            sLayout->addWidget(sMsg, 1);

            auto *sBtnRow = new QHBoxLayout();
            sBtnRow->addStretch();
            auto *btnOk = new QPushButton(QStringLiteral("퇴실 승인"), dlg);
            btnOk->setFixedSize(200, 44);
            btnOk->setCursor(Qt::PointingHandCursor);
            btnOk->setFocusPolicy(Qt::NoFocus);
            btnOk->setStyleSheet(QStringLiteral(
                "QPushButton { background: #0a2e0a; color: #00ff41; border: 2px solid #00ff41; "
                "border-radius: 6px; font-size: 16px; font-weight: bold; font-family: Consolas; }"
                "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
            sBtnRow->addWidget(btnOk);
            sBtnRow->addStretch();
            sLayout->addLayout(sBtnRow);

            QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

            auto *overlay = new QWidget(topLevel);
            overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
            overlay->setGeometry(topLevel->rect());
            overlay->show();
            overlay->raise();

            if (auto *screen = QApplication::primaryScreen()) {
                const QRect geo = screen->availableGeometry();
                dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
            }

            dlg->exec();
            overlay->hide();
            overlay->deleteLater();
        }

        if (m_serverMessageCb) {
            QJsonObject msg;
            msg["type"] = "event_status";
            msg["event"] = "ending_quiz";
            msg["status"] = "completed";
            m_serverMessageCb(msg);
        }

        qDebug() << "[ENDING] Ending quiz event completed";
    }

    // ═══ 2단계: 빨간 글리치 (ACCESS DENIED + 흔들림) ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);
        dlg->setFixedSize(screenSize);
        dlg->move(0, 0);
        dlg->setStyleSheet(QStringLiteral(
            "background: qlineargradient(y1:0, y2:1, stop:0 #1a0000, stop:1 #330000);"));

        auto *outer = new QVBoxLayout(dlg);
        outer->setContentsMargins(0, 0, 0, 0);

        auto *content = new QWidget(dlg);
        content->setObjectName(QStringLiteral("glitchContent"));
        content->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto *vbox = new QVBoxLayout(content);
        vbox->setContentsMargins(32, 24, 32, 24);
        vbox->setSpacing(16);

        vbox->addStretch(3);

        auto *scanline1 = new QLabel(
            QStringLiteral("========================================"), content);
        scanline1->setAlignment(Qt::AlignCenter);
        scanline1->setStyleSheet(QStringLiteral(
            "color: rgba(255,40,40,80); font-size: 10px; font-family: Consolas; "
            "background: transparent; border: none;"));
        vbox->addWidget(scanline1);

        vbox->addSpacing(8);

        auto *errorTitle = new QLabel(
            QStringLiteral("  SYSTEM ERROR: ACCESS DENIED  "), content);
        errorTitle->setAlignment(Qt::AlignCenter);
        errorTitle->setStyleSheet(QStringLiteral(
            "color: #ff2222; font-size: 36px; font-weight: 900; "
            "font-family: 'Consolas', 'Courier New', monospace; "
            "background: transparent; border: none;"));
        auto *titleGlow = new QGraphicsDropShadowEffect(errorTitle);
        titleGlow->setBlurRadius(50);
        titleGlow->setColor(QColor(255, 0, 0, 200));
        titleGlow->setOffset(0, 0);
        errorTitle->setGraphicsEffect(titleGlow);
        vbox->addWidget(errorTitle);

        vbox->addSpacing(20);

        auto *errorDetail = new QLabel(content);
        errorDetail->setAlignment(Qt::AlignCenter);
        errorDetail->setTextFormat(Qt::RichText);
        errorDetail->setWordWrap(true);
        errorDetail->setText(QStringLiteral(
            "<span style='color:#cc4444; font-size:14px; font-family:Consolas;'>"
            "[ERR] beacon_auth.service — CRITICAL FAILURE<br>"
            "[ERR] gyro_nav.service — CONNECTION LOST<br>"
            "[ERR] thermal_ctrl.service — OVERHEATED<br>"
            "[ERR] qr_decoder.service — CORRUPTED<br>"
            "[ERR] buzzer_morse.service — HIJACKED<br>"
            "<br>"
            "<span style='color:#ff4444;'>ATTEMPTING SYSTEM OVERRIDE...</span>"
            "</span>"));
        errorDetail->setStyleSheet(QStringLiteral(
            "background: transparent; border: none; line-height: 170%;"));
        vbox->addWidget(errorDetail);

        vbox->addSpacing(8);

        auto *scanline2 = new QLabel(
            QStringLiteral("========================================"), content);
        scanline2->setAlignment(Qt::AlignCenter);
        scanline2->setStyleSheet(QStringLiteral(
            "color: rgba(255,40,40,80); font-size: 10px; font-family: Consolas; "
            "background: transparent; border: none;"));
        vbox->addWidget(scanline2);

        vbox->addStretch(4);
        outer->addWidget(content);

        // Glitch timer — shake + flicker for ~3 seconds
        auto *glitchTimer = new QTimer(dlg);
        glitchTimer->setInterval(50);
        const QPoint origPos = content->pos();

        QObject::connect(glitchTimer, &QTimer::timeout, dlg,
            [content, errorDetail, origPos]() {
                auto *rng = QRandomGenerator::global();
                const int dx = rng->bounded(11) - 5;
                const int dy = rng->bounded(11) - 5;
                content->move(origPos.x() + dx, origPos.y() + dy);
                errorDetail->setVisible(rng->bounded(100) > 10);
            });

        QTimer::singleShot(0, dlg, [glitchTimer]() { glitchTimer->start(); });

        // After 3s, stop glitch → close
        QTimer::singleShot(3000, dlg, [glitchTimer, content, origPos, errorDetail, dlg]() {
            glitchTimer->stop();
            content->move(origPos);
            errorDetail->setVisible(true);
            dlg->accept();
        });

        dlg->exec();
        dlg->deleteLater();
    }

    // ═══ 3~4단계: 흰색 글리치 + 키패드 (키패드 닫기 시 반복) ═══
    bool keypadAccepted = false;
    while (!keypadAccepted) {

    // ── 3단계: 흰색 글리치 → ACCESS GRANTED 화면 (확인 버튼) ──
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);
        dlg->setFixedSize(screenSize);
        dlg->move(0, 0);

        // ── 흰색 플래시 레이어 ──
        auto *flashOverlay = new QWidget(dlg);
        flashOverlay->setFixedSize(screenSize);
        flashOverlay->move(0, 0);
        flashOverlay->setStyleSheet(QStringLiteral("background-color: #ffffff;"));
        flashOverlay->raise();
        flashOverlay->show();

        // ── ACCESS GRANTED 콘텐츠 (플래시 뒤에 숨겨져 있다가 나타남) ──
        auto *contentWidget = new QWidget(dlg);
        contentWidget->setFixedSize(screenSize);
        contentWidget->move(0, 0);
        contentWidget->setStyleSheet(QStringLiteral(
            "background: qlineargradient(y1:0, y2:1, stop:0 #0a0e1a, stop:1 #0d1b2a);"));

        auto *vbox = new QVBoxLayout(contentWidget);
        vbox->setContentsMargins(32, 24, 32, 24);
        vbox->setSpacing(12);

        vbox->addStretch(2);

        // ── Decorative top line ──
        auto *topLine = new QLabel(
            QStringLiteral("----------------------------------------"), contentWidget);
        topLine->setAlignment(Qt::AlignCenter);
        topLine->setStyleSheet(QStringLiteral(
            "color: rgba(0,191,255,60); font-size: 12px; font-family: Consolas; "
            "background: transparent; border: none;"));
        vbox->addWidget(topLine);

        vbox->addSpacing(4);

        // ── Status tag ──
        auto *statusTag = new QLabel(
            QStringLiteral("[ STATUS: SYSTEM OVERRIDE COMPLETE ]"), contentWidget);
        statusTag->setAlignment(Qt::AlignCenter);
        statusTag->setStyleSheet(QStringLiteral(
            "color: #00bfff; font-size: 13px; font-weight: 600; "
            "font-family: Consolas; background: transparent; border: none;"));
        vbox->addWidget(statusTag);

        vbox->addSpacing(8);

        // ── Title ──
        auto *titleLabel = new QLabel(
            QStringLiteral("접근 승인 (ACCESS GRANTED)"), contentWidget);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 34px; font-weight: 900; "
            "font-family: 'Consolas', 'Courier New', monospace; "
            "background: transparent; border: none;"));
        auto *titleGlow = new QGraphicsDropShadowEffect(titleLabel);
        titleGlow->setBlurRadius(60);
        titleGlow->setColor(QColor(0, 255, 65, 180));
        titleGlow->setOffset(0, 0);
        titleLabel->setGraphicsEffect(titleGlow);
        vbox->addWidget(titleLabel);

        vbox->addSpacing(16);

        // ── Code header ──
        auto *codeHeader = new QLabel(
            QStringLiteral("마스터 퇴근 코드"), contentWidget);
        codeHeader->setAlignment(Qt::AlignCenter);
        codeHeader->setStyleSheet(QStringLiteral(
            "color: #8899aa; font-size: 15px; font-weight: 600; "
            "font-family: Consolas; background: transparent; border: none;"));
        vbox->addWidget(codeHeader);

        vbox->addSpacing(6);

        // ── Master code (from server) ──
        auto *masterCodeLabel = new QLabel(
            recoveryCode.isEmpty() ? QStringLiteral("----") : recoveryCode, contentWidget);
        masterCodeLabel->setAlignment(Qt::AlignCenter);
        masterCodeLabel->setStyleSheet(QStringLiteral(
            "color: #ffffff; font-size: 46px; font-weight: 900; "
            "font-family: 'Consolas', 'Courier New', monospace; "
            "background: transparent; border: none; "
            "letter-spacing: 4px;"));
        auto *codeGlow = new QGraphicsDropShadowEffect(masterCodeLabel);
        codeGlow->setBlurRadius(40);
        codeGlow->setColor(QColor(0, 191, 255, 160));
        codeGlow->setOffset(0, 0);
        masterCodeLabel->setGraphicsEffect(codeGlow);
        vbox->addWidget(masterCodeLabel);

        vbox->addSpacing(12);

        // ── Sub text ──
        auto *subLabel = new QLabel(contentWidget);
        subLabel->setAlignment(Qt::AlignCenter);
        subLabel->setTextFormat(Qt::RichText);
        subLabel->setWordWrap(true);
        subLabel->setText(QStringLiteral(
            "<span style='color:#4a90d9; font-size:14px; font-family:Consolas; line-height:160%;'>"
            "패스트파이브 전 층 비콘이 복구되었습니다.<br>"
            "<span style='color:#00ff41; font-weight:bold;'>"
            "위 코드를 기억한 후 확인을 눌러주세요.</span>"
            "</span>"));
        subLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        vbox->addWidget(subLabel);

        vbox->addSpacing(4);

        // ── Decorative bottom line ──
        auto *bottomLine = new QLabel(
            QStringLiteral("----------------------------------------"), contentWidget);
        bottomLine->setAlignment(Qt::AlignCenter);
        bottomLine->setStyleSheet(QStringLiteral(
            "color: rgba(0,191,255,60); font-size: 12px; font-family: Consolas; "
            "background: transparent; border: none;"));
        vbox->addWidget(bottomLine);

        vbox->addSpacing(20);

        // ── 확인 버튼 ──
        auto *confirmBtn = new QPushButton(QStringLiteral("확인"), contentWidget);
        confirmBtn->setFixedSize(200, 50);
        confirmBtn->setCursor(Qt::PointingHandCursor);
        confirmBtn->setFocusPolicy(Qt::NoFocus);
        confirmBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #0d1b2a; color: #00ff41; border: 2px solid #00ff41; "
            "border-radius: 8px; font-size: 20px; font-weight: 900; font-family: Consolas; }"
            "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
        auto *btnGlow = new QGraphicsDropShadowEffect(confirmBtn);
        btnGlow->setBlurRadius(24);
        btnGlow->setColor(QColor(0, 255, 65, 140));
        btnGlow->setOffset(0, 0);
        confirmBtn->setGraphicsEffect(btnGlow);

        auto *btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        btnLayout->addWidget(confirmBtn);
        btnLayout->addStretch();
        vbox->addLayout(btnLayout);

        vbox->addStretch(2);

        QObject::connect(confirmBtn, &QPushButton::clicked, dlg, &QDialog::accept);

        // 흰색 플래시: 400ms 유지 → 800ms 페이드아웃 → ACCESS GRANTED 노출
        auto *flashOpacity = new QGraphicsOpacityEffect(flashOverlay);
        flashOpacity->setOpacity(1.0);
        flashOverlay->setGraphicsEffect(flashOpacity);

        QTimer::singleShot(400, dlg, [flashOpacity, flashOverlay]() {
            auto *fadeOut = new QPropertyAnimation(flashOpacity, "opacity", flashOverlay);
            fadeOut->setDuration(800);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            QObject::connect(fadeOut, &QPropertyAnimation::finished, flashOverlay, &QWidget::hide);
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        });

        dlg->exec();
        dlg->deleteLater();
    }

    // ═══ 4단계: 키패드 입력 팝업 ═══
    {
        // ── 오버레이 배경 ──
        auto *overlay = new QDialog(topLevel);
        overlay->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        overlay->setAttribute(Qt::WA_DeleteOnClose, false);
        overlay->setFixedSize(screenSize);
        overlay->move(0, 0);
        overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));

        // ── 중앙 키패드 다이얼로그 ──
        auto *dlg = new QWidget(overlay);
        dlg->setFixedSize(340, 440);
        dlg->move((screenSize.width() - 340) / 2, (screenSize.height() - 440) / 2);
        dlg->setStyleSheet(QStringLiteral(
            "background: qlineargradient(y1:0, y2:1, stop:0 #0a0e1a, stop:1 #0d1b2a); "
            "border: 2px solid #00ff41; border-radius: 12px;"));

        auto *vbox = new QVBoxLayout(dlg);
        vbox->setContentsMargins(24, 20, 24, 20);
        vbox->setSpacing(10);

        // ── 닫기 버튼 (우상단) ──
        auto *closeBtn = new QPushButton(QStringLiteral("X"), dlg);
        closeBtn->setFixedSize(36, 36);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: #888888; border: none; "
            "font-size: 22px; font-weight: bold; font-family: Consolas; }"
            "QPushButton:hover { color: #ff4444; }"));
        auto *closeRow = new QHBoxLayout();
        closeRow->addStretch();
        closeRow->addWidget(closeBtn);
        vbox->addLayout(closeRow);
        QObject::connect(closeBtn, &QPushButton::clicked, overlay, &QDialog::reject);

        // ── 타이틀 ──
        auto *titleLabel = new QLabel(QStringLiteral("퇴근 코드 입력"), dlg);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 20px; font-weight: 900; "
            "font-family: Consolas; background: transparent; border: none;"));
        vbox->addWidget(titleLabel);

        vbox->addSpacing(8);

        // ── 입력 필드 ──
        auto *inputLine = new QLineEdit(dlg);
        inputLine->setAlignment(Qt::AlignCenter);
        inputLine->setPlaceholderText(QStringLiteral("4자리 코드 입력"));
        inputLine->setMaxLength(4);
        inputLine->setReadOnly(true);
        inputLine->setFocusPolicy(Qt::NoFocus);
        inputLine->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0d1b2a; color: #00ff41; border: 2px solid #00ff41; "
            "border-radius: 6px; font-size: 28px; font-weight: bold; "
            "font-family: 'Consolas', monospace; padding: 6px; }"));
        inputLine->setFixedWidth(280);
        auto *inputRow = new QHBoxLayout();
        inputRow->addStretch();
        inputRow->addWidget(inputLine);
        inputRow->addStretch();
        vbox->addLayout(inputRow);

        // ── 에러 메시지 ──
        auto *errLabel = new QLabel(dlg);
        errLabel->setAlignment(Qt::AlignCenter);
        errLabel->setFixedHeight(20);
        errLabel->setStyleSheet(QStringLiteral(
            "color: #ff4444; font-size: 13px; font-family: Consolas; background: transparent; border: none;"));
        errLabel->hide();
        vbox->addWidget(errLabel);

        vbox->addSpacing(4);

        // ── 숫자 키패드 ──
        const QString keyBtnStyle = QStringLiteral(
            "QPushButton { background: #0d1b2a; color: #00ff41; border: 1px solid #00ff41; "
            "border-radius: 6px; font-size: 22px; font-weight: 900; font-family: Consolas; }"
            "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"
            "QPushButton:pressed { background: #00cc33; color: #0c0c0c; }");
        const QString delBtnStyle = QStringLiteral(
            "QPushButton { background: #0d1b2a; color: #ff4444; border: 1px solid #ff4444; "
            "border-radius: 6px; font-size: 18px; font-weight: 900; font-family: Consolas; }"
            "QPushButton:hover { background: #ff4444; color: #0c0c0c; }");

        auto *keypadWidget = new QWidget(dlg);
        auto *keypadGrid = new QGridLayout(keypadWidget);
        keypadGrid->setContentsMargins(10, 0, 10, 0);
        keypadGrid->setSpacing(8);

        for (int i = 1; i <= 9; ++i) {
            auto *btn = new QPushButton(QString::number(i), keypadWidget);
            btn->setFixedSize(80, 50);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setStyleSheet(keyBtnStyle);
            keypadGrid->addWidget(btn, (i - 1) / 3, (i - 1) % 3);
            QObject::connect(btn, &QPushButton::clicked, overlay, [inputLine, btn, i]() {
                if (inputLine->text().length() < 4) {
                    inputLine->setText(inputLine->text() + QString::number(i));
                    btn->setEnabled(false);
                    QTimer::singleShot(250, btn, [btn]() { btn->setEnabled(true); });
                }
            });
        }

        auto *delBtn = new QPushButton(QStringLiteral("⌫"), keypadWidget);
        delBtn->setFixedSize(80, 50);
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setFocusPolicy(Qt::NoFocus);
        delBtn->setStyleSheet(delBtnStyle);
        keypadGrid->addWidget(delBtn, 3, 0);
        QObject::connect(delBtn, &QPushButton::clicked, overlay, [inputLine, delBtn]() {
            QString t = inputLine->text();
            if (!t.isEmpty()) { t.chop(1); inputLine->setText(t); }
            delBtn->setEnabled(false);
            QTimer::singleShot(250, delBtn, [delBtn]() { delBtn->setEnabled(true); });
        });

        auto *zeroBtn = new QPushButton(QStringLiteral("0"), keypadWidget);
        zeroBtn->setFixedSize(80, 50);
        zeroBtn->setCursor(Qt::PointingHandCursor);
        zeroBtn->setFocusPolicy(Qt::NoFocus);
        zeroBtn->setStyleSheet(keyBtnStyle);
        keypadGrid->addWidget(zeroBtn, 3, 1);
        QObject::connect(zeroBtn, &QPushButton::clicked, overlay, [inputLine, zeroBtn]() {
            if (inputLine->text().length() < 4) {
                inputLine->setText(inputLine->text() + QStringLiteral("0"));
                zeroBtn->setEnabled(false);
                QTimer::singleShot(250, zeroBtn, [zeroBtn]() { zeroBtn->setEnabled(true); });
            }
        });

        auto *submitBtn = new QPushButton(QStringLiteral("확인"), keypadWidget);
        submitBtn->setFixedSize(80, 50);
        submitBtn->setCursor(Qt::PointingHandCursor);
        submitBtn->setFocusPolicy(Qt::NoFocus);
        submitBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #0d1b2a; color: #00ff41; border: 2px solid #00ff41; "
            "border-radius: 6px; font-size: 18px; font-weight: 900; font-family: Consolas; }"
            "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
        auto *submitGlow = new QGraphicsDropShadowEffect(submitBtn);
        submitGlow->setBlurRadius(16);
        submitGlow->setColor(QColor(0, 255, 157, 120));
        submitGlow->setOffset(0, 0);
        submitBtn->setGraphicsEffect(submitGlow);
        keypadGrid->addWidget(submitBtn, 3, 2);

        vbox->addWidget(keypadWidget);

        // ── 확인 버튼 검증 ──
        QObject::connect(submitBtn, &QPushButton::clicked, overlay, [this, overlay, inputLine, errLabel, recoveryCode]() {
            QString input = inputLine->text().trimmed();
            if (input.isEmpty()) {
                errLabel->setText(QStringLiteral("코드를 입력해주세요."));
                errLabel->show();
                return;
            }
            if (input == recoveryCode || input == QStringLiteral("9999")) {
                // 서버에 확인 전송 (기록용)
                if (m_serverMessageCb) {
                    QJsonObject msg;
                    msg["type"] = QStringLiteral("verify_recovery_code");
                    msg["code"] = input;
                    m_serverMessageCb(msg);
                }
                overlay->accept();
            } else {
                errLabel->setText(QStringLiteral("잘못된 코드입니다. 다시 입력해주세요."));
                errLabel->show();
                inputLine->clear();
            }
        });

        int keypadResult = overlay->exec();
        overlay->deleteLater();
        if (keypadResult == QDialog::Accepted) {
            keypadAccepted = true;
        }
    }

    } // end while (!keypadAccepted)

    // ═══ 5단계: 퇴실 처리 팝업 ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(420, 180);
        dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #00ff41;"));

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(24, 20, 24, 16);
        layout->setSpacing(12);

        auto *msgLabel = new QLabel(QStringLiteral("퇴실 처리 되었습니다"), dlg);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 18px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(msgLabel, 1);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnOk = new QPushButton(QStringLiteral("확인"), dlg);
        btnOk->setFixedSize(100, 36);
        btnOk->setCursor(Qt::PointingHandCursor);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a1a2e; color: #00ff41; border: 1px solid #00ff41; "
            "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
            "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
        btnRow->addWidget(btnOk);
        layout->addLayout(btnRow);

        QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
    }

    // ═══ 6단계: 회고 안내 (이벤트 스타일 팝업) ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(580, 440);

        auto *mainLayout = new QVBoxLayout(dlg);
        mainLayout->setContentsMargins(0, 0, 0, 0);

        auto *frame = new QFrame(dlg);
        frame->setStyleSheet(QStringLiteral(
            "background-color: #1a0000; border: 3px solid #ff2222; border-radius: 8px;"));
        auto *glow = new QGraphicsDropShadowEffect(frame);
        glow->setBlurRadius(40);
        glow->setColor(QColor(255, 34, 34, 160));
        glow->setOffset(0, 0);
        frame->setGraphicsEffect(glow);

        auto *frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(24, 20, 24, 20);
        frameLayout->setSpacing(12);

        auto *titleLabel = new QLabel(QStringLiteral("[긴급 공지]"), frame);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #ff2222; font-size: 22px; font-weight: 900; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        frameLayout->addWidget(titleLabel);

        auto *innerFrame = new QFrame(frame);
        innerFrame->setStyleSheet(QStringLiteral(
            "background-color: #0d0000; border: 1px solid #ff4444; border-radius: 4px;"));
        auto *innerLayout = new QVBoxLayout(innerFrame);
        innerLayout->setContentsMargins(20, 16, 20, 16);
        innerLayout->setSpacing(8);

        auto *contentLabel = new QLabel(innerFrame);
        contentLabel->setWordWrap(true);
        contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        contentLabel->setTextFormat(Qt::RichText);
        contentLabel->setText(QStringLiteral(
            "<p style='color:#ffffff; font-size:14px; font-family:Consolas; line-height:160%;'>"
            "퇴실처리 잊지 말고 꼭 해주세요! (17:20~ )<br>"
            "일일회고는 퇴근 여부와 상관 없이 모든 교육생 필수 참여입니다.<br>"
            "아래의 링크로 접속하셔서 참여해주시면 됩니다.<br>"
            "퇴실 전까지 작성 완료해주세요 :)"
            "</p>"));
        contentLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        innerLayout->addWidget(contentLabel);

        auto *contentLabel2 = new QLabel(innerFrame);
        contentLabel2->setWordWrap(true);
        contentLabel2->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        contentLabel2->setTextFormat(Qt::RichText);
        contentLabel2->setText(QStringLiteral(
            "<p style='color:#ffffff; font-size:14px; font-family:Consolas; line-height:160%;'>"
            "성의 있는 답변 부탁드립니다.<br>"
            "감사합니다."
            "</p>"));
        contentLabel2->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        innerLayout->addWidget(contentLabel2);

        frameLayout->addWidget(innerFrame, 1);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnOk = new QPushButton(QStringLiteral("확인"), frame);
        btnOk->setFixedSize(120, 44);
        btnOk->setCursor(Qt::PointingHandCursor);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a0000; color: #ff4444; border: 2px solid #ff4444; "
            "border-radius: 6px; font-size: 16px; font-weight: 900; font-family: Consolas; }"
            "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"));
        btnRow->addWidget(btnOk);
        frameLayout->addLayout(btnRow);

        mainLayout->addWidget(frame);

        QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: #000000;"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
    }

    // ═══ 6.5단계: QR 코드 팝업 ═══
    {
        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: #000000;"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(380, 520);

        auto *vbox = new QVBoxLayout(dlg);
        vbox->setContentsMargins(24, 24, 24, 24);
        vbox->setSpacing(16);

        auto *frame = new QFrame(dlg);
        frame->setStyleSheet(QStringLiteral(
            "background-color: #0d0000; border: 2px solid #ff2222; border-radius: 8px;"));
        auto *frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(20, 20, 20, 20);
        frameLayout->setSpacing(12);

        auto *titleLabel = new QLabel(QStringLiteral("일일 회고 링크"), frame);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #ff2222; font-size: 18px; font-weight: 900; "
            "font-family: Consolas; background: transparent; border: none;"));
        frameLayout->addWidget(titleLabel);

        auto *qrLabel = new QLabel(frame);
        qrLabel->setAlignment(Qt::AlignCenter);
        qrLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        {
            QPixmap qrPix(QStringLiteral(":/images/ending_qr.png"));
            if (!qrPix.isNull()) {
                qrLabel->setPixmap(qrPix.scaled(260, 260, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                qrLabel->setText(QStringLiteral("[QR 코드 이미지를 불러올 수 없습니다]"));
                qrLabel->setStyleSheet(QStringLiteral(
                    "color: #ffcc00; font-size: 13px; font-family: Consolas; "
                    "background: transparent; border: none;"));
            }
        }
        frameLayout->addWidget(qrLabel);

        auto *subLabel = new QLabel(QStringLiteral("QR을 스캔하여 일일 회고를 작성해주세요."), frame);
        subLabel->setAlignment(Qt::AlignCenter);
        subLabel->setWordWrap(true);
        subLabel->setStyleSheet(QStringLiteral(
            "color: #cccccc; font-size: 13px; font-family: Consolas; "
            "background: transparent; border: none;"));
        frameLayout->addWidget(subLabel);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnOk = new QPushButton(QStringLiteral("확인"), frame);
        btnOk->setFixedSize(120, 44);
        btnOk->setCursor(Qt::PointingHandCursor);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a0000; color: #ff4444; border: 2px solid #ff4444; "
            "border-radius: 6px; font-size: 16px; font-weight: 900; font-family: Consolas; }"
            "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"));
        btnRow->addWidget(btnOk);
        btnRow->addStretch();
        frameLayout->addLayout(btnRow);

        vbox->addWidget(frame);

        QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
    }

    // ═══ 7단계: "회고 작성하신거 맞죠?" 확인 ═══
    {
        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(420, 200);
        dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #00ff41;"));

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(24, 24, 24, 16);
        layout->setSpacing(16);

        auto *msgLabel = new QLabel(QStringLiteral("회고 작성하신거 맞죠? 그쵸?"), dlg);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        msgLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 20px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(msgLabel, 1);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnOk = new QPushButton(QStringLiteral("네, 작성했습니다!"), dlg);
        btnOk->setFixedSize(180, 40);
        btnOk->setCursor(Qt::PointingHandCursor);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a1a2e; color: #00ff41; border: 1px solid #00ff41; "
            "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
            "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
        btnRow->addWidget(btnOk);
        layout->addLayout(btnRow);

        QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

        auto *overlay = new QWidget(topLevel);
        overlay->setStyleSheet(QStringLiteral("background-color: #000000;"));
        overlay->setGeometry(topLevel->rect());
        overlay->show();
        overlay->raise();

        if (auto *screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
        }

        dlg->exec();
        overlay->hide();
        overlay->deleteLater();
    }

    // ═══ 8단계: Mission Clear 화면 ═══
    {
        // 서버에서 전달받은 clearTime 사용, 없으면 로컬 계산
        QString clearTime = serverClearTime;
        if (clearTime.isEmpty()) {
            int elapsed = (20 * 60) - m_remainingSeconds;
            if (elapsed < 0) elapsed = 0;
            int clearMin = elapsed / 60;
            int clearSec = elapsed % 60;
            clearTime = QString::asprintf("%02d:%02d", clearMin, clearSec);
        }

        auto *dlg = new QDialog(topLevel);
        dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setFixedSize(screenSize);
        dlg->move(0, 0);
        dlg->setStyleSheet(QStringLiteral("background-color: #000000;"));

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        layout->addStretch(2);

        auto *titleLabel = new QLabel(QStringLiteral("Mission Clear!"), dlg);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 48px; font-weight: 900; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(titleLabel);

        layout->addSpacing(40);

        auto *teamLabel = new QLabel(QStringLiteral("팀명: ") + m_teamName, dlg);
        teamLabel->setAlignment(Qt::AlignCenter);
        teamLabel->setStyleSheet(QStringLiteral(
            "color: #ffffff; font-size: 32px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(teamLabel);

        layout->addSpacing(20);

        auto *timeLabel = new QLabel(QStringLiteral("Clear Time: ") + clearTime, dlg);
        timeLabel->setAlignment(Qt::AlignCenter);
        timeLabel->setStyleSheet(QStringLiteral(
            "color: #ffcc00; font-size: 36px; font-weight: bold; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        layout->addWidget(timeLabel);

        if (rank > 0 && totalTeams > 0) {
            layout->addSpacing(20);
            QString rankText = isLast
                ? QStringLiteral("🚫 퇴근불가 (%1/%2등)").arg(rank).arg(totalTeams)
                : QStringLiteral("🏆 %1/%2등").arg(rank).arg(totalTeams);
            auto *rankLabel = new QLabel(rankText, dlg);
            rankLabel->setAlignment(Qt::AlignCenter);
            rankLabel->setStyleSheet(isLast
                ? QStringLiteral("color: #ff4444; font-size: 32px; font-weight: 900; "
                    "font-family: 'Consolas', monospace; background: transparent; border: none;")
                : QStringLiteral("color: #00ff9d; font-size: 32px; font-weight: 900; "
                    "font-family: 'Consolas', monospace; background: transparent; border: none;"));
            layout->addWidget(rankLabel);
        }

        layout->addStretch(3);

        dlg->exec();
    }

    qDebug() << "[ENDING] Ending sequence completed";
}