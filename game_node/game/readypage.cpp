#include "readypage.h"
#include "missionpage.h"
#include "chat/contactgmdialog.h"
#include "notice/noticedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QFont>

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
}

void ReadyPage::setMissionProgress(int percent)
{
    percent = qBound(0, percent, 100);
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

        // When mission completes, advance to the next mission
        QObject::connect(mission, &MissionPage::missionCompleted, this, [this](int completedNumber) {
            auto *nextMission = new MissionPage(completedNumber + 1, this);
            setMissionWidget(nextMission);
            nextMission->startMission();
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
    readyPageLayout->setContentsMargins(8, 8, 8, 8);
    readyPageLayout->setSpacing(0);

    auto *mainPanel = new QWidget(this);
    mainPanel->setObjectName(QStringLiteral("hackerMainPanel"));
    auto *mainPanelLayout = new QVBoxLayout(mainPanel);
    mainPanelLayout->setContentsMargins(12, 10, 12, 10);
    mainPanelLayout->setSpacing(8);

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
        m_unreadNotices = 0;
        m_systemNoticesButton->setText(QStringLiteral("SYSTEM NOTICES"));
        m_systemNoticesButton->setStyleSheet(QString());
        NoticeDialog::show(this, m_notices);
    });

    auto *contactGmButton = new QPushButton(QStringLiteral("CONTACT GM"), headerButtonArea);
    contactGmButton->setObjectName(QStringLiteral("contactGmButton"));
    contactGmButton->setCursor(Qt::PointingHandCursor);
    contactGmButton->setFixedHeight(headerControlHeight);
    m_contactGmButton = contactGmButton;
    QObject::connect(contactGmButton, &QPushButton::clicked, this, [this]() {
        m_unreadMessages = 0;
        m_contactGmButton->setText(QStringLiteral("CONTACT GM"));
        m_contactGmButton->setStyleSheet(QString());
        ContactGmDialog::show(this, m_teamName, m_directMessages, m_sendMessageCb);
    });

    headerButtonLayout->addWidget(systemNoticesButton);
    headerButtonLayout->addWidget(contactGmButton);

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
    m_eventLayout->setContentsMargins(8, 8, 8, 8);
    m_eventLayout->setSpacing(8);

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

MissionPage *ReadyPage::currentMission() const
{
    return qobject_cast<MissionPage *>(m_currentMissionWidget);
}


