#include "missionpage.h"
#include "title/teamnamedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QDialog>
#include <QApplication>
#include <QScreen>

#ifdef Q_OS_LINUX
#include "dd_api/led_api.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════
MissionPage::MissionPage(int missionNumber, QWidget *parent)
    : QWidget(parent)
    , m_missionNumber(missionNumber)
    , m_contentLayout(nullptr)
    , m_missionStack(nullptr)
    , m_terminalOutput(nullptr)
    , m_nextButton(nullptr)
    , m_typeTimer(nullptr)
    , m_currentLineIndex(0)
{
    setObjectName(QStringLiteral("missionPage_%1").arg(missionNumber));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_contentLayout = rootLayout;

    setupMission();
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 1 — Terminal story + dummy problem
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission1()
{
    m_correctAnswer = QStringLiteral("1947");

    m_missionStack = new QStackedWidget(this);
    m_contentLayout->addWidget(m_missionStack);

    // ── Page 0: Terminal story ─────────────────────────────────────────
    auto *storyPage = new QWidget(m_missionStack);
    storyPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *storyLayout = new QVBoxLayout(storyPage);
    storyLayout->setContentsMargins(6, 6, 6, 6);
    storyLayout->setSpacing(0);

    // Terminal window frame
    auto *terminalFrame = new QFrame(storyPage);
    terminalFrame->setObjectName(QStringLiteral("terminalFrame"));
    terminalFrame->setStyleSheet(QStringLiteral(
        "QFrame#terminalFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 6px; }"
        "QFrame#terminalFrame QPushButton { background-color: transparent; border: none; padding: 0; }"));
    auto *frameLayout = new QVBoxLayout(terminalFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    // Title bar (cmd style)
    auto *titleBar = new QWidget(terminalFrame);
    titleBar->setFixedHeight(32);
    titleBar->setStyleSheet(QStringLiteral(
        "background-color: #1a1a2e; border: none; border-bottom: 1px solid #444;"
        "border-top-left-radius: 6px; border-top-right-radius: 6px;"));
    auto *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(12, 0, 12, 0);
    auto *titleLabel = new QLabel(QStringLiteral("SECURITY_TERMINAL.exe"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 13px; font-family: 'Consolas', 'Courier New', monospace; "
        "background: transparent; border: none;"));
    titleBarLayout->addWidget(titleLabel);
    titleBarLayout->addStretch();
    frameLayout->addWidget(titleBar);

    // Terminal body
    auto *terminalArea = new QScrollArea(storyPage);
    terminalArea->setWidgetResizable(true);
    terminalArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    terminalArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    terminalArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background-color: #0c0c0c; border: none; }"
        "QScrollBar:vertical { background: #0c0c0c; width: 6px; }"
        "QScrollBar::handle:vertical { background: #333; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

    auto *terminalWidget = new QWidget(terminalArea);
    terminalWidget->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *termLayout = new QVBoxLayout(terminalWidget);
    termLayout->setContentsMargins(10, 6, 10, 6);
    termLayout->setSpacing(0);

    m_terminalOutput = new QLabel(terminalWidget);
    m_terminalOutput->setTextFormat(Qt::RichText);
    m_terminalOutput->setWordWrap(true);
    m_terminalOutput->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_terminalOutput->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 16px; font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: 500; background: transparent; border: none; "
        "letter-spacing: -1px; line-height: 110%;"));

    termLayout->addWidget(m_terminalOutput);
    termLayout->addStretch();
    terminalArea->setWidget(terminalWidget);
    frameLayout->addWidget(terminalArea, 1);

    // Next button area
    auto *btnArea = new QWidget(storyPage);
    btnArea->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: none;"));
    auto *btnLayout = new QHBoxLayout(btnArea);
    btnLayout->setContentsMargins(16, 0, 16, 2);

    m_nextButton = new QPushButton(QStringLiteral("▶ PROCEED"), btnArea);
    m_nextButton->setFixedSize(200, 48);
    m_nextButton->setCursor(Qt::PointingHandCursor);
    m_nextButton->setVisible(false);
    m_nextButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #0a1628;"
        "  color: #00ff41;"
        "  border: 1px solid #00ff41;"
        "  border-radius: 6px;"
        "  font-size: 16px;"
        "  font-weight: 800;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  letter-spacing: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #00ff41;"
        "  color: #0c0c0c;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #00cc33;"
        "  color: #0c0c0c;"
        "}"));

    auto *btnGlow = new QGraphicsDropShadowEffect(m_nextButton);
    btnGlow->setBlurRadius(18);
    btnGlow->setColor(QColor(0, 255, 65, 140));
    btnGlow->setOffset(0, 0);
    m_nextButton->setGraphicsEffect(btnGlow);

    btnLayout->addStretch();
    btnLayout->addWidget(m_nextButton);
    btnLayout->addStretch();

    storyLayout->addWidget(terminalFrame, 1);
    storyLayout->addWidget(btnArea);

    m_missionStack->addWidget(storyPage); // index 0

    // ── Page 1: Mission problem page (image left, input+keypad right) ──
    auto *problemPage = new QWidget(m_missionStack);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(12, 6, 12, 6);
    problemLayout->setSpacing(6);

    // Header
    auto *problemHeader = new QLabel(QStringLiteral("[MISSION] 1단계 인증"), problemPage);
    problemHeader->setAlignment(Qt::AlignCenter);
    problemHeader->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 22px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    problemLayout->addWidget(problemHeader);

    auto *divider = new QFrame(problemPage);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("background-color: #00ff41; max-height: 1px; border: none;"));
    problemLayout->addWidget(divider);
    problemLayout->addSpacing(4);

    // ── Body: left image + right input/keypad ──────────────────────────
    auto *bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(12);

    // Left: mission image inside bordered panel (same style as right panel)
    auto *leftPanel = new QWidget(problemPage);
    leftPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 14, 16, 14);
    leftLayout->setSpacing(0);
    leftLayout->setAlignment(Qt::AlignCenter);

    auto *imageLabel = new QLabel(leftPanel);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    QPixmap missionPixmap(QStringLiteral(":/images/misson_1.png"));
    if (!missionPixmap.isNull()) {
        imageLabel->setPixmap(missionPixmap.scaled(360, 340, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imageLabel->setText(QStringLiteral("[ IMAGE ]"));
        imageLabel->setStyleSheet(QStringLiteral(
            "color: #666; font-size: 18px; background: transparent; border: none; "
            "min-width: 200px; min-height: 200px;"));
    }
    leftLayout->addWidget(imageLabel);

    bodyRow->addWidget(leftPanel, 1);

    // Right: answer display + input button (bordered, centered)
    auto *rightPanel = new QWidget(problemPage);
    rightPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 14, 16, 14);
    rightLayout->setSpacing(12);
    rightLayout->setAlignment(Qt::AlignHCenter);

    rightLayout->addStretch();

    // Current answer label
    auto *answerTitle = new QLabel(QStringLiteral("현재 입력된 정답"), rightPanel);
    answerTitle->setAlignment(Qt::AlignCenter);
    answerTitle->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 14px; font-family: 'Consolas', monospace; "
        "background: transparent; border: none;"));
    rightLayout->addWidget(answerTitle);

    auto *answerDisplay = new QLabel(QStringLiteral("-"), rightPanel);
    answerDisplay->setObjectName(QStringLiteral("answerDisplay"));
    answerDisplay->setAlignment(Qt::AlignCenter);
    answerDisplay->setMinimumHeight(48);
    answerDisplay->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 28px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    rightLayout->addWidget(answerDisplay);

    // "입력하기" button — opens the team-name style popup
    auto *btnInput = new QPushButton(QStringLiteral("▶ 입력하기"), rightPanel);
    btnInput->setCursor(Qt::PointingHandCursor);
    btnInput->setFocusPolicy(Qt::NoFocus);
    btnInput->setAutoRepeat(false);
    btnInput->setFixedSize(200, 48);
    btnInput->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1628; color: #00ff41; border: 1px solid #00ff41; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: #00ff41; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #00cc33; color: #0c0c0c; }"));

    auto *inputBtnGlow = new QGraphicsDropShadowEffect(btnInput);
    inputBtnGlow->setBlurRadius(16);
    inputBtnGlow->setColor(QColor(0, 255, 65, 140));
    inputBtnGlow->setOffset(0, 0);
    btnInput->setGraphicsEffect(inputBtnGlow);

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay]() {
        bool ok = false;
        const QString answer = TeamNameDialog::getInput(
            this,
            QStringLiteral("ENTER ANSWER"),
            QStringLiteral("정답을 입력해주세요."),
            20,
            &ok);
        if (ok && !answer.isEmpty()) {
            m_answerInputRaw = answer;
            answerDisplay->setText(answer);
        }
    });

    auto *inputBtnRow = new QHBoxLayout();
    inputBtnRow->addStretch();
    inputBtnRow->addWidget(btnInput);
    inputBtnRow->addStretch();
    rightLayout->addLayout(inputBtnRow);

    // "제출" button
    auto *btnSubmit = new QPushButton(QStringLiteral("▶ 제출"), rightPanel);
    btnSubmit->setCursor(Qt::PointingHandCursor);
    btnSubmit->setFocusPolicy(Qt::NoFocus);
    btnSubmit->setAutoRepeat(false);
    btnSubmit->setFixedSize(200, 48);
    btnSubmit->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1a0a28; color: #ff4444; border: 1px solid #ff4444; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: #ff4444; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #cc3333; color: #0c0c0c; }"));

    auto *submitGlow = new QGraphicsDropShadowEffect(btnSubmit);
    submitGlow->setBlurRadius(14);
    submitGlow->setColor(QColor(255, 68, 68, 140));
    submitGlow->setOffset(0, 0);
    btnSubmit->setGraphicsEffect(submitGlow);

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay]() {
        if (m_answerInputRaw.isEmpty()) return;
        const bool correct = (m_answerInputRaw.trimmed().compare(
            m_correctAnswer, Qt::CaseInsensitive) == 0);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
        }
    });

    auto *submitRow = new QHBoxLayout();
    submitRow->addStretch();
    submitRow->addWidget(btnSubmit);
    submitRow->addStretch();
    rightLayout->addLayout(submitRow);

    rightLayout->addStretch();

    bodyRow->addWidget(rightPanel, 1);

    problemLayout->addLayout(bodyRow, 1);

    m_missionStack->addWidget(problemPage); // index 1
    m_missionStack->setCurrentIndex(0);

    // ── Story lines ────────────────────────────────────────────────────
    // 각 줄을 테이블 row로 구성 → 타임스탬프 열 고정폭으로 정렬 보장
    // s = 공통 스타일 (모든 셀 동일 font-size, font-family, font-weight)
    const QString s = QStringLiteral("font-size:16px; font-family:Consolas,Courier New,monospace; font-weight:500; letter-spacing:-1px;");

    m_storyLines.clear();
    m_storyLines << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:12]</td>"
                                   "<td style='color:#00bfff; %1'>SECURITY SYSTEM v2.1</td>"
                                   "</tr>").arg(s)
                 << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:14]</td>"
                                   "<td style='color:#ff4444; %1'>[1단계 인증]</td>"
                                   "</tr>").arg(s)
                 << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:16]</td>"
                                   "<td style='color:#00ff41; %1'>비콘 장애 원인을 조사하던 중</td>"
                                   "</tr>").arg(s)
                 << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:18]</td>"
                                   "<td style='color:#00ff41; %1'>손상된 장치에서 마지막 통신 로그가 복구되었습니다.</td>"
                                   "</tr>").arg(s)
                 << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:21]</td>"
                                   "<td style='color:#00ff41; %1'>장애 직전 비콘이 수신한 신호 데이터가 남아있습니다.</td>"
                                   "</tr>").arg(s)
                 << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:24]</td>"
                                   "<td style='color:#eab308; %1'>LED 점멸 신호를 해독하여</td>"
                                   "</tr>").arg(s)
                 << QStringLiteral("<tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap; width:90px;'>[21:04:26]</td>"
                                   "<td style='color:#eab308; %1'>1단계 인증 코드를 입력하십시오.</td>"
                                   "</tr>").arg(s);

    // ── Timer for typing effect ────────────────────────────────────────
    m_typeTimer = new QTimer(this);
    m_typeTimer->setSingleShot(true);
    QObject::connect(m_typeTimer, &QTimer::timeout, this, &MissionPage::typeNextLine);

    // ── Next button → go to problem page ───────────────────────────────
    QObject::connect(m_nextButton, &QPushButton::clicked, this, [this]() {
#ifdef Q_OS_LINUX
        led_show_problem();
#endif
        m_missionStack->setCurrentIndex(1);
    });
}

void MissionPage::startTypingAnimation()
{
    m_currentLineIndex = 0;
    m_terminalOutput->setText(QStringLiteral("<span style='color:#00ff41;'>▌</span>"));
    if (m_nextButton) m_nextButton->setVisible(false);
    m_typeTimer->start(600); // initial delay
}

void MissionPage::typeNextLine()
{
    if (m_currentLineIndex >= m_storyLines.size()) {
        // All lines typed — show cursor + Next button
        QString current = m_terminalOutput->text();
        // Remove old cursor, add final cursor
        current.replace(QStringLiteral("<span style='color:#00ff41;'>▌</span>"), QString());
        current += QStringLiteral("<br><span style='color:#00ff41;'>▌</span>");
        m_terminalOutput->setText(current);
        if (m_nextButton) m_nextButton->setVisible(true);
        return;
    }

    // Build text: all lines so far in a single table + cursor
    QString html = QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    for (int i = 0; i <= m_currentLineIndex; ++i) {
        html += m_storyLines.at(i);
    }
    html += QStringLiteral("</table>");
    html += QStringLiteral("<span style='color:#00ff41;'>▌</span>");
    m_terminalOutput->setText(html);

    m_currentLineIndex++;

    // Vary delay per line for realism
    const int delays[] = { 400, 300, 500, 500, 600, 500, 400 };
    int idx = qMin(m_currentLineIndex - 1, 6);
    m_typeTimer->start(delays[idx]);
}

// ═══════════════════════════════════════════════════════════════════════════
// Result popup — terminal-style dialog with typing animation
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showResultPopup(bool correct)
{
    auto *dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedSize(540, 320);
    dialog->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));

    auto *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Terminal frame ─────────────────────────────────────────────────
    auto *frame = new QFrame(dialog);
    frame->setObjectName(QStringLiteral("resultFrame"));
    frame->setStyleSheet(QStringLiteral(
        "QFrame#resultFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 6px; }"
        "QFrame#resultFrame QPushButton { background-color: transparent; border: none; padding: 0; }"));
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(frame);
    titleBar->setFixedHeight(32);
    titleBar->setStyleSheet(QStringLiteral(
        "background-color: #1a1a2e; border: none; border-bottom: 1px solid #444;"
        "border-top-left-radius: 6px; border-top-right-radius: 6px;"));
    auto *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(12, 0, 12, 0);
    auto *titleLabel = new QLabel(QStringLiteral("SYSTEM_VERIFY.exe"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 13px; font-family: 'Consolas', 'Courier New', monospace; "
        "background: transparent; border: none;"));
    titleBarLayout->addWidget(titleLabel);
    titleBarLayout->addStretch();
    frameLayout->addWidget(titleBar);

    // Terminal body
    auto *body = new QWidget(frame);
    body->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: none;"));
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(14, 10, 14, 10);
    bodyLayout->setSpacing(0);

    auto *textLabel = new QLabel(body);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setWordWrap(true);
    textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    textLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 16px; font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: 500; background: transparent; border: none; "
        "letter-spacing: -1px; line-height: 130%;"));

    textLabel->setText(QStringLiteral("<span style='color:#00ff41;'>▌</span>"));
    bodyLayout->addWidget(textLabel);
    bodyLayout->addStretch();
    frameLayout->addWidget(body, 1);

    // ── Close button (hidden until typing done) ────────────────────────
    auto *btnArea = new QWidget(frame);
    btnArea->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: none;"));
    auto *btnLayout = new QHBoxLayout(btnArea);
    btnLayout->setContentsMargins(16, 0, 16, 16);

    auto *closeBtn = new QPushButton(
        correct ? QStringLiteral("\u25b6 확인") : QStringLiteral("\u25b6 다시 시도"),
        btnArea);
    closeBtn->setFixedSize(200, 44);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setVisible(false);

    if (correct) {
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #0a1628; color: #00ff41; border: 1px solid #00ff41; "
            "border-radius: 6px; font-size: 17px; font-weight: 800; "
            "font-family: 'Consolas', monospace; }"
            "QPushButton:hover { background-color: #00ff41; color: #0c0c0c; }"
            "QPushButton:pressed { background-color: #00cc33; color: #0c0c0c; }"));
        auto *glow = new QGraphicsDropShadowEffect(closeBtn);
        glow->setBlurRadius(16);
        glow->setColor(QColor(0, 255, 65, 140));
        glow->setOffset(0, 0);
        closeBtn->setGraphicsEffect(glow);
    } else {
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #1a0a28; color: #ff4444; border: 1px solid #ff4444; "
            "border-radius: 6px; font-size: 17px; font-weight: 800; "
            "font-family: 'Consolas', monospace; }"
            "QPushButton:hover { background-color: #ff4444; color: #0c0c0c; }"
            "QPushButton:pressed { background-color: #cc3333; color: #0c0c0c; }"));
        auto *glow = new QGraphicsDropShadowEffect(closeBtn);
        glow->setBlurRadius(14);
        glow->setColor(QColor(255, 68, 68, 140));
        glow->setOffset(0, 0);
        closeBtn->setGraphicsEffect(glow);
    }

    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    frameLayout->addWidget(btnArea);

    mainLayout->addWidget(frame);

    // ── Build result lines (inline spans, no table) ────────────────────
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (correct) {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[18:04:34]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>입력값 대조 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[18:04:35]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>1단계 인증 성공</span>").arg(sf)
            << QString()  // empty line
            << QStringLiteral("<span style='color:#00ff41; %1'>신호를 해독했습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#00ff41; %1'>그런데... 비콘이 단순 장애라기엔</span>").arg(sf)
            << QStringLiteral("<span style='color:#00ff41; %1'>갉힌 흔적이 있습니다.</span>").arg(sf)
            << QString()  // empty line
            << QStringLiteral("<span style='color:#eab308; %1'>누군가 의도적으로 망가뜨린 것 같습니다.</span>").arg(sf);
    } else {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[18:04:33]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>입력값 대조 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[18:04:34]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>불일치 감지</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[18:04:34]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>1단계 인증 실패</span>").arg(sf)
            << QString()  // empty line
            << QStringLiteral("<span style='color:#ff4444; %1'>입력한 코드가 등록된 신호와</span>").arg(sf)
            << QStringLiteral("<span style='color:#ff4444; %1'>일치하지 않습니다.</span>").arg(sf)
            << QString()  // empty line
            << QStringLiteral("<span style='color:#eab308; %1'>신호를 처음부터 다시 확인하십시오.</span>").arg(sf);
    }

    // ── Typing animation timer ─────────────────────────────────────────
    auto *popupTimer = new QTimer(dialog);
    popupTimer->setSingleShot(true);

    auto lineIdx = std::make_shared<int>(0);

    QObject::connect(popupTimer, &QTimer::timeout, dialog,
        [textLabel, closeBtn, popupTimer, resultLines, lineIdx, correct, this]() {
            const int idx = *lineIdx;
            if (idx >= resultLines.size()) {
                // Done — remove cursor, show button
                QString html = textLabel->text();
                html.replace(QStringLiteral("<span style='color:#00ff41;'>▌</span>"), QString());
                textLabel->setText(html);
                closeBtn->setVisible(true);
                return;
            }

            // Build HTML from all lines typed so far
            QString html;
            for (int i = 0; i <= idx; ++i) {
                const QString &line = resultLines.at(i);
                if (line.isEmpty()) {
                    html += QStringLiteral("<br>");
                } else {
                    html += line + QStringLiteral("<br>");
                }
            }
            html += QStringLiteral("<span style='color:#00ff41;'>▌</span>");
            textLabel->setText(html);

            (*lineIdx)++;

            // Varying delays for realism
            const int delays[] = { 500, 400, 300, 400, 400, 400, 300, 500 };
            int d = delays[qMin(idx, 7)];
            popupTimer->start(d);
        });

    // Center on screen
    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dialog->move(geo.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
    }

    // Start typing after a short delay
    popupTimer->start(600);

    dialog->exec();

    if (correct) {
        emit missionCompleted(m_missionNumber);
    }
}

void MissionPage::resetToStory()
{
    if (m_missionStack) {
        m_missionStack->setCurrentIndex(0);
    }
    if (m_missionNumber == 1) {
        startTypingAnimation();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// setupMission — dispatch by number
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission()
{
    if (m_missionNumber == 1) {
        setupMission1();
        return;
    }

    // Default placeholder
    auto *placeholder = new QLabel(
        QStringLiteral("MISSION %1 — 준비 중").arg(m_missionNumber), this);
    placeholder->setObjectName(QStringLiteral("eventDescriptionLabel"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    m_contentLayout->addWidget(placeholder, 1);
}
