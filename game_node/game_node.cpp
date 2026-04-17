#include "game_node.h"
#include "ui_game_node.h"
#include "intro/emergencypage.h"
#include "game/readypage.h"
#include "title/teamnamedialog.h"

#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QProcess>
#include <QTimer>
#include <QMouseEvent>
#include <QEvent>
#include <QStackedWidget>

// ---------------------------------------------------------------------------
// Constructor – Wire up pages and flow
// ---------------------------------------------------------------------------
GameNode::GameNode(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GameNode)
    , m_introPageIndex(-1)
    , m_readyPageIndex(-1)
    , m_emergencyPage(nullptr)
    , m_readyPage(nullptr)
    , m_blinkTimer(new QTimer(this))
    , m_teamDialogOpen(false)
    , m_ignoreTitleTap(false)
    , m_titleAudioProcess(new QProcess(this))
    , m_titleMusicStarted(false)
{
    ui->setupUi(this);
    applyStyles();
    setupAlsaEnvironment();

    // 재생 완료 시 자동 루프 (aplay는 루프 옵션이 없으므로 finished 시그널로 반복)
    QObject::connect(m_titleAudioProcess,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     this, [this](int /*exitCode*/, QProcess::ExitStatus /*status*/) {
        if (m_titleMusicStarted) {
            // 아직 타이틀 화면이면 다시 재생
            m_titleMusicStarted = false;
            playTitleMusicIfNeeded();
        }
    });
    ui->titlePage->installEventFilter(this);
    ui->labelStartGame->installEventFilter(this);

    QObject::connect(ui->btnBackFromAnswer, &QPushButton::clicked, this, &GameNode::showTitleScreen);
    QObject::connect(ui->btnBackFromSetting, &QPushButton::clicked, this, &GameNode::showTitleScreen);

    // ── Emergency intro page ───────────────────────────────────────────
    m_emergencyPage = new EmergencyPage();
    m_introPageIndex = ui->stackedWidget->addWidget(m_emergencyPage);

    // ── Ready (game) page ──────────────────────────────────────────────
    m_readyPage = new ReadyPage();
    m_readyPageIndex = ui->stackedWidget->addWidget(m_readyPage);

    // ── Connect emergency page confirm → go to ready page ──────────────
    QObject::connect(m_emergencyPage, &EmergencyPage::confirmed, this, [this]() {
        m_readyPage->resetCountdown(43 * 60 + 18);
        if (m_readyPageIndex >= 0 && m_readyPageIndex < ui->stackedWidget->count()) {
            ui->stackedWidget->setCurrentIndex(m_readyPageIndex);
        }
    });

    // ── When intro page becomes current, reset its animation ───────────
    QObject::connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
        if (index == m_introPageIndex && m_emergencyPage) {
            m_emergencyPage->reset();
        }
    });

    // ── "START GAME" blinking label ────────────────────────────────────
    ui->labelStartGame->setStyleSheet(
        QStringLiteral("font-size: 28px; font-weight: 800; color: #00ffff; "
                       "letter-spacing: 4px;"));
    ui->labelStartGame->setCursor(Qt::PointingHandCursor);

    auto *labelGlow = new QGraphicsDropShadowEffect(ui->labelStartGame);
    labelGlow->setBlurRadius(22);
    labelGlow->setColor(QColor(0, 255, 255, 200));
    labelGlow->setOffset(0, 0);
    ui->labelStartGame->setGraphicsEffect(labelGlow);

    QObject::connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        ui->labelStartGame->setVisible(!ui->labelStartGame->isVisible());
    });
    m_blinkTimer->start(600);

    // Start on the title page
    ui->stackedWidget->setCurrentIndex(0);

    QObject::connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
        if (index == 0) {
            playTitleMusicIfNeeded();
        } else {
            stopTitleMusic();
        }

        // 긴급재난 화면을 벗어나면 음악 정지
        if (index != m_introPageIndex && m_emergencyPage) {
            m_emergencyPage->stopMusic();
        }
    });

    playTitleMusicIfNeeded();
}

GameNode::~GameNode()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// QSS – Escape-room dark theme
// ---------------------------------------------------------------------------
void GameNode::applyStyles()
{
    const QString qss = QStringLiteral(R"(
        /* ── Global ────────────────────────────────────────────── */
        QWidget {
            background-color: #0D0D12;
            color: #d6d7e0;
            font-size: 16px;
            font-family: "Consolas", "Courier New", monospace;
        }

        /* ── Title Page Background ─────────────────────────────── */
        QWidget#titlePage {
            background-image: url(:/images/title.png);
            background-repeat: no-repeat;
            background-position: center;
        }

        QDialog#teamDialog {
            background-color: #0b1220;
            border: 1px solid #10b981;
            border-radius: 12px;
        }

        QLabel#teamDialogTitle {
            background: transparent;
            color: #ffffff;
            font-size: 28px;
            font-weight: 800;
            letter-spacing: 2px;
        }

        QLabel#teamDialogSubtitle {
            background: transparent;
            color: #94a3b8;
            font-size: 15px;
        }

        QLineEdit#teamNameLineEdit {
            background-color: #111827;
            color: #f8fafc;
            border: 1px solid #1f2937;
            border-radius: 10px;
            padding: 12px 18px;
            font-size: 24px;
            selection-background-color: #2563eb;
        }

        QLineEdit#teamNameLineEdit:focus {
            border: 1px solid #2563eb;
        }

        QScrollArea#teamKeypadScroll {
            background: transparent;
            border: none;
        }

        QScrollArea#teamKeypadScroll > QWidget > QWidget {
            background: transparent;
        }

        QWidget#teamKeypadContainer,
        QStackedWidget#keyboardPages,
        QWidget#keyboardPage,
        QWidget#teamKeypadSpecialBar {
            background: transparent;
            border: none;
        }

        /* ── Title Label ──────────────────────────────────────── */
        QLabel#titleLabel {
            font-size: 36px;
            font-weight: bold;
            color: #ff4444;
            padding: 20px;
        }

        /* ── Section Labels ───────────────────────────────────── */
        QLabel#answerStageLabel,
        QLabel#settingLabel {
            font-size: 26px;
            font-weight: bold;
            color: #ffffff;
        }

        QWidget#readyPage {
            background-color: #0f172a;
        }

        QWidget#hackerMainPanel {
            background-color: #0f172a;
            border: 1px solid #10b981;
            border-radius: 10px;
        }

        QWidget#topBarPanel {
            background: transparent;
            border: none;
        }

        QWidget#headerButtonArea {
            background: transparent;
            border: none;
        }

        QLabel#teamTerminalLabel {
            background: transparent;
            color: #ffffff;
            font-size: 30px;
            font-weight: 800;
            letter-spacing: 2px;
        }

        QWidget#timerBox {
            background: #201217;
            background-color: #201217;
            border: 1px solid #7f1d1d;
            border-radius: 4px;
        }

        QLabel#timeRemainingLabel {
            background: transparent;
            background-color: transparent;
            color: #ef4444;
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1px;
        }

        QPushButton#systemNoticesButton,
        QPushButton#contactGmButton {
            background-color: transparent;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 14px;
            font-weight: 700;
            letter-spacing: 1px;
        }

        QPushButton#systemNoticesButton {
            color: #eab308;
            background-color: #2a240f;
            border: 1px solid #ca8a04;
        }

        QPushButton#systemNoticesButton:hover {
            background-color: #3a3112;
            color: #fef08a;
        }

        QPushButton#contactGmButton {
            color: #60a5fa;
            background-color: #142338;
            border: 1px solid #2563eb;
        }

        QPushButton#contactGmButton:hover {
            background-color: #1a3150;
            color: #bfdbfe;
        }

        QWidget#thinDivider {
            background-color: #10b981;
            border: none;
        }

        QLabel#missionProgressLabel,
        QLabel#missionPercentLabel {
            background-color: #0f172a;
            color: #10b981;
            font-size: 15px;
            font-weight: 700;
        }

        QLabel#globalProgressLabel,
        QLabel#globalPercentLabel {
            background-color: #0f172a;
            color: #3b82f6;
            font-size: 15px;
            font-weight: 700;
        }

        QWidget#progressArea,
        QWidget#missionProgressRow,
        QWidget#globalProgressRow {
            background-color: #0f172a;
            border: none;
        }

        QWidget#hackerMainPanel QProgressBar,
        QWidget#hackerMainPanel QProgressBar::chunk {
            border: none;
        }

        QProgressBar#missionProgressBar,
        QProgressBar#globalProgressBar {
            background: #0f172a;
            background-color: #0f172a;
            border: none;
            border-radius: 5px;
            min-height: 10px;
            max-height: 10px;
        }

        QProgressBar#missionProgressBar::chunk {
            background-color: #10b981;
            border-radius: 5px;
        }

        QProgressBar#globalProgressBar::chunk {
            background-color: #3b82f6;
            border-radius: 5px;
        }

        QWidget#eventContainer {
            background-color: #000000;
            border: 1px solid #14532d;
            border-radius: 8px;
        }

        QLabel#eventTitleLabel {
            color: #ffffff;
            font-size: 30px;
            font-weight: 800;
        }

        QLabel#eventDescriptionLabel {
            color: #9ca3af;
            font-size: 17px;
        }

        /* ── Buttons (common) ─────────────────────────────────── */
        QPushButton {
            background-color: #2e2e2e;
            color: #f0f0f0;
            border: 2px solid #ff4444;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 18px;
            font-weight: bold;
            outline: none;
        }
        QPushButton:focus {
            outline: none;
        }
        QPushButton:hover {
            background-color: #3c3c3c;
            border-color: #ff6666;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #ff4444;
            color: #1a1a1a;
        }

        /* ── START GAME indicator ───────────────────────────── */
        QLabel#labelStartGame {
            background: transparent;
            color: #00ffff;
            font-size: 28px;
            font-weight: 800;
            letter-spacing: 4px;
            padding: 8px 24px;
        }

        /* ── Line Edit ────────────────────────────────────────── */
        QLineEdit#answerLineEdit {
            background-color: #2a2a2a;
            color: #ffffff;
            border: 2px solid #555555;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 18px;
            selection-background-color: #ff4444;
        }
        QLineEdit#answerLineEdit:focus {
            border-color: #ff4444;
        }

        /* ── Back buttons – subdued ───────────────────────────── */
        QPushButton#btnBackFromAnswer,
        QPushButton#btnBackFromSetting {
            background-color: transparent;
            border: 1px solid #666666;
            color: #999999;
            font-size: 14px;
        }
        QPushButton#btnBackFromAnswer:hover,
        QPushButton#btnBackFromSetting:hover {
            color: #ffffff;
            border-color: #aaaaaa;
        }

        /* ── On-screen keyboard keys ──────────────────────────── */
        QPushButton#kbKey {
            background-color: #1a1a2e;
            color: #00ffff;
            border: 1px solid #00cccc;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
            font-family: "Consolas", "Courier New", monospace;
            padding: 2px;
            outline: none;
            min-width: 40px;
            min-height: 32px;
        }
        QPushButton#kbKey:hover {
            background-color: rgba(0, 255, 255, 40);
            color: #ffffff;
        }
        QPushButton#kbKey:pressed {
            background-color: rgba(0, 255, 255, 120);
            color: #0a0a0f;
            border-color: #00ffff;
        }

        /* ── Special keyboard keys (wider) ────────────────────── */
        QPushButton#kbSpecial {
            background-color: #1a1a2e;
            color: #ff00ff;
            border: 1px solid #cc00cc;
            border-radius: 4px;
            font-size: 13px;
            font-weight: bold;
            font-family: "Consolas", "Courier New", monospace;
            padding: 1px;
            outline: none;
            min-height: 28px;
        }
        QPushButton#kbSpecial:hover {
            background-color: rgba(255, 0, 255, 40);
            color: #ffffff;
        }
        QPushButton#kbSpecial:pressed {
            background-color: rgba(255, 0, 255, 120);
            color: #0a0a0f;
            border-color: #ff00ff;
        }

        QPushButton#btnConfirm {
            border: 2px solid #00d5ff;
            color: #00ffff;
        }
        QPushButton#btnConfirm:hover {
            background-color: rgba(0, 255, 255, 35);
            border-color: #7df9ff;
            color: #ffffff;
        }
        QPushButton#btnConfirm:pressed {
            background-color: #00ffff;
            border-color: #7df9ff;
            color: #0a0a0f;
        }

        QPushButton#btnCancel {
            border: 2px solid #ff4dff;
            color: #ff7cff;
        }
        QPushButton#btnCancel:hover {
            background-color: rgba(255, 0, 255, 35);
            border-color: #ff9cff;
            color: #ffffff;
        }
        QPushButton#btnCancel:pressed {
            background-color: #ff4dff;
            border-color: #ff9cff;
            color: #0a0a0f;
        }
    )");

    setStyleSheet(qss);
}

// ---------------------------------------------------------------------------
// Slots – Screen navigation
// ---------------------------------------------------------------------------
void GameNode::showTitleScreen()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void GameNode::showAnswerInputScreen()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void GameNode::showSettingScreen()
{
    ui->stackedWidget->setCurrentIndex(2);
}

void GameNode::onStartClicked()
{
    if (ui->stackedWidget->currentIndex() != 0)
        return;

    m_teamDialogOpen = true;
    bool accepted = false;
    QString teamName = TeamNameDialog::getTeamName(this, &accepted);
    m_teamDialogOpen = false;
    m_ignoreTitleTap = true;
    QTimer::singleShot(200, this, [this]() { m_ignoreTitleTap = false; });

    if (accepted) {
        stopTitleMusic();
        m_teamName = teamName.isEmpty() ? QStringLiteral("TEAM 1") : teamName;
        m_readyPage->setTeamName(m_teamName);

        if (m_introPageIndex >= 0 && m_introPageIndex < ui->stackedWidget->count()) {
            ui->stackedWidget->setCurrentIndex(m_introPageIndex);
        } else if (m_readyPageIndex >= 0 && m_readyPageIndex < ui->stackedWidget->count()) {
            m_readyPage->resetCountdown(43 * 60 + 18);
            ui->stackedWidget->setCurrentIndex(m_readyPageIndex);
        } else {
            showAnswerInputScreen();
        }
    }
}

QString GameNode::findFirstSongFile() const
{
    const QStringList baseDirs = {
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/songs")),
        QStringLiteral("/mnt/nfs/songs")
    };

    for (const QString &dirPath : baseDirs) {
        const QString filePath = QDir::cleanPath(dirPath + QStringLiteral("/title.wav"));
        if (QFileInfo::exists(filePath)) {
            return filePath;
        }
    }

    return QString();
}

QString GameNode::findAplayProgram() const
{
    const QStringList candidates = {
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/aplay")),
        QStringLiteral("/mnt/nfs/aplay"),
        QStringLiteral("aplay")
    };

    for (const QString &candidate : candidates) {
        if (candidate == QStringLiteral("aplay") || QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return QStringLiteral("aplay");
}

void GameNode::setupAlsaEnvironment()
{
    // aplay 실행 시 필요한 ALSA 환경변수를 QProcess에 설정
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    env.insert(QStringLiteral("ALSA_CONFIG_PATH"),
               QStringLiteral("/mnt/nfs/alsa-lib/share/alsa/alsa.conf"));

    QString ldPath = env.value(QStringLiteral("LD_LIBRARY_PATH"));
    const QStringList alsaLibPaths = {
        QStringLiteral("/mnt/nfs/alsa-lib/lib"),
        QStringLiteral("/mnt/nfs/alsa-lib/lib/alsa-lib"),
        QStringLiteral("/mnt/nfs/alsa-lib/lib/alsa-lib/smixer")
    };
    for (const QString &p : alsaLibPaths) {
        if (!ldPath.contains(p)) {
            if (!ldPath.isEmpty()) ldPath += QLatin1Char(':');
            ldPath += p;
        }
    }
    env.insert(QStringLiteral("LD_LIBRARY_PATH"), ldPath);

    m_titleAudioProcess->setProcessEnvironment(env);
}

void GameNode::playTitleMusicIfNeeded()
{
    if (m_titleMusicStarted) {
        return;
    }

    const QString songFile = findFirstSongFile();
    if (songFile.isEmpty()) {
        return;
    }

    if (m_titleAudioProcess->state() != QProcess::NotRunning) {
        m_titleAudioProcess->kill();
        m_titleAudioProcess->waitForFinished(300);
    }

    const QString aplay = findAplayProgram();
    const QStringList args = { QStringLiteral("-Dhw:0,0"), songFile };

    m_titleAudioProcess->start(aplay, args);
    m_titleMusicStarted = m_titleAudioProcess->waitForStarted(800);
}

void GameNode::stopTitleMusic()
{
    m_titleMusicStarted = false;
    if (m_titleAudioProcess->state() != QProcess::NotRunning) {
        m_titleAudioProcess->terminate();
        if (!m_titleAudioProcess->waitForFinished(300)) {
            m_titleAudioProcess->kill();
            m_titleAudioProcess->waitForFinished(300);
        }
    }
}

// ---------------------------------------------------------------------------
// Touch anywhere on title screen → team name dialog
// ---------------------------------------------------------------------------
bool GameNode::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == ui->titlePage || watched == ui->labelStartGame)
        && ui->stackedWidget->currentIndex() == 0) {
        if (m_teamDialogOpen) {
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            if (!m_ignoreTitleTap) {
                onStartClicked();
            }
            return true;
        }

        if (event->type() == QEvent::MouseButtonPress) {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

