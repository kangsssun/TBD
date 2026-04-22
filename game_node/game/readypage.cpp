#include "readypage.h"
#include "missionpage.h"
#include "chat/contactgmdialog.h"
#include "notice/noticedialog.h"
#include "ending/endingsequence.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
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
                // 서버에 game_clear 전송
                if (m_serverMessageCb) {
                    QJsonObject msg;
                    msg["type"] = "game_clear";
                    m_serverMessageCb(msg);
                }
                // 엔딩 시퀀스 시작
                QTimer::singleShot(500, this, [this]() {
                    showEndingSequence();
                });
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
        MissionPage::isOperatorMode() ? QStringLiteral("SKIP ▶") : QStringLiteral("GUIDE"),
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
                // Last mission: go to ending
                helpButton->setEnabled(false);
                showEndingSequence();
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
        auto *titleLabel = new QLabel(QStringLiteral("⚠️ EVENT 1: 바이러스 제거 ⚠️"), outerFrame);
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
                        statusLabel->setText(QStringLiteral("❌ 오답! 타이머가 1분 감소합니다."));
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
// ENDING SEQUENCE — delegated to ending/endingsequence.h
// ════════════════════════════════════════════════════════════════════════════
void ReadyPage::showEndingSequence()
{
    EndingSequence::show(this, m_teamName, m_remainingSeconds);
}