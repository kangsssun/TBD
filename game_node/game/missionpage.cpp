#include "missionpage.h"

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
    btnLayout->setContentsMargins(16, 4, 16, 8);

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
    frameLayout->addWidget(btnArea);

    storyLayout->addWidget(terminalFrame, 1);

    m_missionStack->addWidget(storyPage); // index 0

    // ── Page 1: Dummy problem page ─────────────────────────────────────
    auto *problemPage = new QWidget(m_missionStack);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(24, 24, 24, 24);
    problemLayout->setSpacing(16);

    auto *problemHeader = new QLabel(QStringLiteral("[MISSION 1] 1단계 인증"), problemPage);
    problemHeader->setAlignment(Qt::AlignCenter);
    problemHeader->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 24px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));

    auto *divider = new QFrame(problemPage);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("background-color: #00ff41; max-height: 1px; border: none;"));

    auto *problemBody = new QLabel(
        QStringLiteral("LED 점멸 패턴을 분석하여 인증 코드를 입력하십시오.\n\n"
                       "패턴: ● ● ○ ● ○ ○ ● ●\n\n"
                       "힌트: 이진수 → 십진수 변환\n\n"
                       "[정답을 입력하세요]"),
        problemPage);
    problemBody->setAlignment(Qt::AlignCenter);
    problemBody->setWordWrap(true);
    problemBody->setStyleSheet(QStringLiteral(
        "color: #c0c0c0; font-size: 18px; "
        "font-family: 'Consolas', monospace; line-height: 170%; "
        "background: transparent; border: none;"));

    problemLayout->addStretch();
    problemLayout->addWidget(problemHeader);
    problemLayout->addWidget(divider);
    problemLayout->addSpacing(16);
    problemLayout->addWidget(problemBody);
    problemLayout->addStretch();

    m_missionStack->addWidget(problemPage); // index 1
    m_missionStack->setCurrentIndex(0);

    // ── Story lines ────────────────────────────────────────────────────
    // 각 줄을 테이블 row로 구성 → 타임스탬프 열 고정폭으로 정렬 보장
    // s = 공통 스타일 (모든 셀 동일 font-size, font-family, font-weight)
    const QString s = QStringLiteral("font-size:16px; font-family:Consolas,Courier New,monospace; font-weight:500; letter-spacing:-1px;");

    m_storyLines.clear();
    m_storyLines << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:12]</td>"
                                   "<td style='color:#00bfff; %1'>SECURITY SYSTEM v2.1</td>"
                                   "</tr></table>").arg(s)
                 << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:14]</td>"
                                   "<td style='color:#ff4444; %1'>[1단계 인증]</td>"
                                   "</tr></table>").arg(s)
                 << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:16]</td>"
                                   "<td style='color:#00ff41; %1'>비콘 장애 원인을 조사하던 중</td>"
                                   "</tr></table>").arg(s)
                 << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:18]</td>"
                                   "<td style='color:#00ff41; %1'>손상된 장치에서 마지막 통신 로그가 복구되었습니다.</td>"
                                   "</tr></table>").arg(s)
                 << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:21]</td>"
                                   "<td style='color:#00ff41; %1'>장애 직전 비콘이 수신한 신호 데이터가 남아있습니다.</td>"
                                   "</tr></table>").arg(s)
                 << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:24]</td>"
                                   "<td style='color:#eab308; %1'>LED 점멸 신호를 해독하여</td>"
                                   "</tr></table>").arg(s)
                 << QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-bottom:0px;'><tr>"
                                   "<td style='color:#666; %1 padding-right:10px; white-space:nowrap;'>[21:04:26]</td>"
                                   "<td style='color:#eab308; %1'>1단계 인증 코드를 입력하십시오.</td>"
                                   "</tr></table>").arg(s);

    // ── Timer for typing effect ────────────────────────────────────────
    m_typeTimer = new QTimer(this);
    m_typeTimer->setSingleShot(true);
    QObject::connect(m_typeTimer, &QTimer::timeout, this, &MissionPage::typeNextLine);

    // ── Next button → go to problem page ───────────────────────────────
    QObject::connect(m_nextButton, &QPushButton::clicked, this, [this]() {
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
        current += QStringLiteral("<br><br><span style='color:#00ff41;'>▌</span>");
        m_terminalOutput->setText(current);
        if (m_nextButton) m_nextButton->setVisible(true);
        return;
    }

    // Build text: all lines so far + new line + cursor
    QString html;
    for (int i = 0; i <= m_currentLineIndex; ++i) {
        html += m_storyLines.at(i);
    }
    html += QStringLiteral("<span style='color:#00ff41;'>▌</span>");
    m_terminalOutput->setText(html);

    m_currentLineIndex++;

    // Vary delay per line for realism
    const int delays[] = { 400, 300, 500, 500, 600, 500, 400 };
    int idx = qMin(m_currentLineIndex - 1, 6);
    m_typeTimer->start(delays[idx]);
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
