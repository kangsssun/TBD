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
    updateHeader();
    if (m_remainingSeconds > 0) {
        m_readyTimer->start();
    } else {
        m_readyTimer->stop();
    }

    // 미션 페이지를 스토리(터미널) 화면으로 초기화
    auto *mp = qobject_cast<MissionPage *>(m_currentMissionWidget);
    if (mp) {
        mp->resetToStory();
    }
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

    m_timeRemainingLabel = new QLabel(QStringLiteral("43:18"), timerBox);
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
    QObject::connect(systemNoticesButton, &QPushButton::clicked, this, [this]() {
        NoticeDialog::show(this);
    });

    auto *contactGmButton = new QPushButton(QStringLiteral("CONTACT GM"), headerButtonArea);
    contactGmButton->setObjectName(QStringLiteral("contactGmButton"));
    contactGmButton->setCursor(Qt::PointingHandCursor);
    contactGmButton->setFixedHeight(headerControlHeight);
    QObject::connect(contactGmButton, &QPushButton::clicked, this, [this]() {
        ContactGmDialog::show(this, m_teamName);
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
    m_missionProgressBar->setValue(80);
    m_missionProgressBar->setTextVisible(false);

    auto *missionPercent = new QLabel(QStringLiteral("80%"), missionProgressRow);
    missionPercent->setObjectName(QStringLiteral("missionPercentLabel"));
    missionPercent->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    missionProgressLayout->addWidget(missionLabel, 1);
    missionProgressLayout->addWidget(m_missionProgressBar, 8);
    missionProgressLayout->addWidget(missionPercent, 1);

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
    m_globalProgressBar->setValue(45);
    m_globalProgressBar->setTextVisible(false);

    auto *globalPercent = new QLabel(QStringLiteral("45%"), globalProgressRow);
    globalPercent->setObjectName(QStringLiteral("globalPercentLabel"));
    globalPercent->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    globalProgressLayout->addWidget(globalLabel, 1);
    globalProgressLayout->addWidget(m_globalProgressBar, 8);
    globalProgressLayout->addWidget(globalPercent, 1);

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


