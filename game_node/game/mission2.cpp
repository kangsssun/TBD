#include "missionpage.h"
#include "missionpage_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QProgressBar>
#include <QRandomGenerator>

// ═══════════════════════════════════════════════════════════════════════════
// Mission 2 — 갓찌의 쳇바퀴 발전기 (Dynamic-Target Timing Game)
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission2()
{
    m_correctAnswer = QStringLiteral("WHEEL_CLEAR");

    auto *page = new QWidget(this);
    page->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(12, 6, 12, 6);
    pageLayout->setSpacing(6);

    // Header
    auto *header = new QLabel(
        QStringLiteral("[MISSION 2] - 갓찌의 쳇바퀴 발전기"), page);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 22px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    pageLayout->addWidget(header);

    auto *divider = new QFrame(page);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral(
        "background-color: #00ff41; max-height: 1px; border: none;"));
    pageLayout->addWidget(divider);
    pageLayout->addSpacing(4);

    // Main panel
    auto *mainPanel = new QWidget(page);
    mainPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *panelLayout = new QVBoxLayout(mainPanel);
    panelLayout->setContentsMargins(24, 20, 24, 20);
    panelLayout->setSpacing(14);

    // Instruction
    auto *instrLabel = new QLabel(QStringLiteral(
        "게이지가 안전 구간에 도달했을 때 정확히 [충전] 버튼을 누르세요.\n"
        "5회 연속 성공하면 미션 클리어! 실패하면 카운트가 줄어듭니다."),
        mainPanel);
    instrLabel->setAlignment(Qt::AlignCenter);
    instrLabel->setWordWrap(true);
    instrLabel->setStyleSheet(QStringLiteral(
        "color: #9ca3af; font-size: 13px; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    panelLayout->addWidget(instrLabel);

    panelLayout->addSpacing(8);

    // Target label
    m_wheelTargetLabel = new QLabel(mainPanel);
    m_wheelTargetLabel->setAlignment(Qt::AlignCenter);
    m_wheelTargetLabel->setStyleSheet(QStringLiteral(
        "color: #eab308; font-size: 18px; font-weight: 700; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    panelLayout->addWidget(m_wheelTargetLabel);

    panelLayout->addSpacing(4);

    // Gauge container (progress bar + markers)
    m_gaugeContainer = new QWidget(mainPanel);
    m_gaugeContainer->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *gaugeContainerLayout = new QVBoxLayout(m_gaugeContainer);
    gaugeContainerLayout->setContentsMargins(0, 0, 0, 0);
    gaugeContainerLayout->setSpacing(0);

    // Top arrow markers row
    m_topArrowRow = new QWidget(m_gaugeContainer);
    m_topArrowRow->setFixedHeight(18);
    m_topArrowRow->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    const QString arrowStyle = QStringLiteral(
        "color: #eab308; font-size: 14px; font-weight: 900; "
        "font-family: Consolas; background: transparent; border: none;");

    m_arrowTopLeft = new QLabel(QStringLiteral("\u25BC"), m_topArrowRow);
    m_arrowTopLeft->setStyleSheet(arrowStyle);
    m_arrowTopLeft->setFixedSize(16, 16);
    m_arrowTopLeft->setAlignment(Qt::AlignCenter);

    m_arrowTopRight = new QLabel(QStringLiteral("\u25BC"), m_topArrowRow);
    m_arrowTopRight->setStyleSheet(arrowStyle);
    m_arrowTopRight->setFixedSize(16, 16);
    m_arrowTopRight->setAlignment(Qt::AlignCenter);

    gaugeContainerLayout->addWidget(m_topArrowRow);

    // Progress bar (gauge) — safe zone shown via background gradient
    m_wheelProgressBar = new QProgressBar(m_gaugeContainer);
    m_wheelProgressBar->setRange(0, 100);
    m_wheelProgressBar->setValue(0);
    m_wheelProgressBar->setTextVisible(true);
    m_wheelProgressBar->setFixedHeight(40);
    m_wheelProgressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #1a1a2e; border: 1px solid #333; border-radius: 6px; "
        "text-align: center; color: #fff; font-size: 16px; font-weight: 700; "
        "font-family: Consolas; }"
        "QProgressBar::chunk { background: qlineargradient("
        "x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #00bfff, stop:0.5 #00ff41, stop:1 #00bfff); "
        "border-radius: 5px; }"));
    gaugeContainerLayout->addWidget(m_wheelProgressBar);

    // Semi-transparent safe zone overlay (sits on top of progress bar)
    m_safeZoneOverlay = new QLabel(m_wheelProgressBar);
    m_safeZoneOverlay->setStyleSheet(QStringLiteral(
        "background: rgba(234, 179, 8, 50); "
        "border: 2px solid rgba(234, 179, 8, 180); "
        "border-radius: 4px;"));
    m_safeZoneOverlay->setFixedHeight(38);
    m_safeZoneOverlay->raise();

    // Bottom arrow markers row
    m_bottomArrowRow = new QWidget(m_gaugeContainer);
    m_bottomArrowRow->setFixedHeight(18);
    m_bottomArrowRow->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    m_arrowBottomLeft = new QLabel(QStringLiteral("\u25B2"), m_bottomArrowRow);
    m_arrowBottomLeft->setStyleSheet(arrowStyle);
    m_arrowBottomLeft->setFixedSize(16, 16);
    m_arrowBottomLeft->setAlignment(Qt::AlignCenter);

    m_arrowBottomRight = new QLabel(QStringLiteral("\u25B2"), m_bottomArrowRow);
    m_arrowBottomRight->setStyleSheet(arrowStyle);
    m_arrowBottomRight->setFixedSize(16, 16);
    m_arrowBottomRight->setAlignment(Qt::AlignCenter);

    gaugeContainerLayout->addWidget(m_bottomArrowRow);

    // Safe zone marker label (thin bar below, kept for legacy but hidden)
    m_safeZoneMarker = nullptr;

    panelLayout->addWidget(m_gaugeContainer);

    panelLayout->addSpacing(6);

    // Status label
    m_wheelStatusLabel = new QLabel(mainPanel);
    m_wheelStatusLabel->setAlignment(Qt::AlignCenter);
    m_wheelStatusLabel->setStyleSheet(QStringLiteral(
        "color: #9ca3af; font-size: 16px; font-weight: 600; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    m_wheelStatusLabel->setText(QStringLiteral("성공: 0 / %1").arg(MAX_SUCCESS));
    panelLayout->addWidget(m_wheelStatusLabel);

    panelLayout->addSpacing(10);

    // Charge button
    m_chargeButton = new QPushButton(QStringLiteral("충전"), mainPanel);
    m_chargeButton->setCursor(Qt::PointingHandCursor);
    m_chargeButton->setFocusPolicy(Qt::NoFocus);
    m_chargeButton->setFixedSize(240, 56);
    m_chargeButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1628; color: #00ff41; border: 2px solid #00ff41; "
        "border-radius: 8px; font-size: 22px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover  { background-color: #00ff41; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #00cc33; color: #0c0c0c; }"
        "QPushButton:disabled { background-color: #111; color: #555; border: 1px solid #333; }"));

    auto *chargeGlow = new QGraphicsDropShadowEffect(m_chargeButton);
    chargeGlow->setBlurRadius(18);
    chargeGlow->setColor(QColor(0, 255, 65, 160));
    chargeGlow->setOffset(0, 0);
    m_chargeButton->setGraphicsEffect(chargeGlow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(m_chargeButton);
    btnRow->addStretch();
    panelLayout->addLayout(btnRow);

    panelLayout->addStretch();

    pageLayout->addWidget(mainPanel, 1);
    m_contentLayout->addWidget(page);

    // ── Connections ─────────────────────────────────────────────────
    m_wheelTimer = new QTimer(this);
    m_wheelTimer->setInterval(15);
    QObject::connect(m_wheelTimer, &QTimer::timeout,
                     this, &MissionPage::updateWheelGauge);
    QObject::connect(m_chargeButton, &QPushButton::clicked,
                     this, [this]() {
        // Debounce: disable button briefly to prevent double-fire
        m_chargeButton->setEnabled(false);
        onChargeButtonClicked();
        QTimer::singleShot(300, this, [this]() {
            if (m_chargeButton) m_chargeButton->setEnabled(true);
        });
    });

    // ── Init state & start ──────────────────────────────────────────
    m_gaugeValue   = 0;
    m_movingUp     = true;
    m_successCount = 0;
    // Delay first target generation so layout is fully settled
    QTimer::singleShot(200, this, [this]() {
        generateNewTarget();
    });
    // Timer will be started after story popup is dismissed
    // (see showMission2Story)
}

// ═══════════════════════════════════════════════════════════════════════════
// Gauge update (ping-pong 0 ↔ 100)
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::updateWheelGauge()
{
    constexpr int STEP = 4;

    if (m_movingUp) {
        m_gaugeValue += STEP;
        if (m_gaugeValue >= 100) {
            m_gaugeValue = 100;
            m_movingUp = false;
        }
    } else {
        m_gaugeValue -= STEP;
        if (m_gaugeValue <= 0) {
            m_gaugeValue = 0;
            m_movingUp = true;
        }
    }

    m_wheelProgressBar->setValue(m_gaugeValue);

    // Build background gradient with safe zone highlight
    const double sMin = m_targetMin / 100.0;
    const double sMax = m_targetMax / 100.0;
    const double sMinPre = qMax(0.0, sMin - 0.001);
    const double sMaxPost = qMin(1.0, sMax + 0.001);
    const QString bgGrad = QStringLiteral(
        "qlineargradient(x1:0,y1:0,x2:1,y2:0, "
        "stop:0 #1a1a2e, stop:%1 #1a1a2e, "
        "stop:%2 rgba(234,179,8,40), stop:%3 rgba(234,179,8,40), "
        "stop:%4 #1a1a2e, stop:1 #1a1a2e)")
        .arg(sMinPre, 0, 'f', 4)
        .arg(sMin, 0, 'f', 4)
        .arg(sMax, 0, 'f', 4)
        .arg(sMaxPost, 0, 'f', 4);

    // Highlight chunk when inside safe zone
    if (m_gaugeValue >= m_targetMin && m_gaugeValue <= m_targetMax) {
        m_wheelProgressBar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: %1; border: 1px solid #00ff41; border-radius: 6px; "
            "text-align: center; color: #fff; font-size: 16px; font-weight: 700; "
            "font-family: Consolas; }"
            "QProgressBar::chunk { background: qlineargradient("
            "x1:0, y1:0, x2:1, y2:0, "
            "stop:0 #00ff41, stop:0.5 #7dff7d, stop:1 #00ff41); "
            "border-radius: 5px; }").arg(bgGrad));
    } else {
        m_wheelProgressBar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: %1; border: 1px solid #333; border-radius: 6px; "
            "text-align: center; color: #fff; font-size: 16px; font-weight: 700; "
            "font-family: Consolas; }"
            "QProgressBar::chunk { background: qlineargradient("
            "x1:0, y1:0, x2:1, y2:0, "
            "stop:0 #00bfff, stop:0.5 #00ff41, stop:1 #00bfff); "
            "border-radius: 5px; }").arg(bgGrad));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Random target generation (15% window)
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::generateNewTarget()
{
    m_targetMin = QRandomGenerator::global()->bounded(5, 81);
    m_targetMax = m_targetMin + 15;

    if (m_wheelTargetLabel) {
        m_wheelTargetLabel->setText(
            QStringLiteral("안전 구간: %1% ~ %2%").arg(m_targetMin).arg(m_targetMax));
    }
    // Update safe zone overlay & arrow positions
    if (m_safeZoneOverlay && m_wheelProgressBar) {
        const int totalW = m_wheelProgressBar->width();
        if (totalW > 0) {
            const int xStart = static_cast<int>(totalW * m_targetMin / 100.0);
            const int xEnd   = static_cast<int>(totalW * m_targetMax / 100.0);
            const int w = xEnd - xStart;
            m_safeZoneOverlay->setGeometry(xStart, 1, w, 38);

            // Position individual arrow labels at exact start/end
            if (m_arrowTopLeft)  m_arrowTopLeft->move(xStart - 8, 0);
            if (m_arrowTopRight) m_arrowTopRight->move(xEnd - 8, 0);
            if (m_arrowBottomLeft)  m_arrowBottomLeft->move(xStart - 8, 0);
            if (m_arrowBottomRight) m_arrowBottomRight->move(xEnd - 8, 0);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Charge button handler
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::onChargeButtonClicked()
{
    // Read the latest gauge value (avoid stale value from event queue delay)
    const int currentGauge = m_wheelProgressBar->value();
    const bool hit = (currentGauge >= m_targetMin && currentGauge <= m_targetMax);

    if (hit) {
        m_successCount++;
        m_wheelStatusLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 16px; font-weight: 600; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        m_wheelStatusLabel->setText(
            QStringLiteral("충전 성공!  ( %1 / %2 )").arg(m_successCount).arg(MAX_SUCCESS));
    } else {
        if (m_successCount > 0) {
            m_successCount--;
        }
        m_wheelStatusLabel->setStyleSheet(QStringLiteral(
            "color: #ff4444; font-size: 16px; font-weight: 600; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        m_wheelStatusLabel->setText(
            QStringLiteral("실패!  ( %1 / %2 )").arg(m_successCount).arg(MAX_SUCCESS));
    }

    checkGameClear();
}

// ═══════════════════════════════════════════════════════════════════════════
// Clear check
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::checkGameClear()
{
    if (m_successCount >= MAX_SUCCESS) {
        m_wheelTimer->stop();
        m_chargeButton->setEnabled(false);

        m_wheelStatusLabel->setStyleSheet(QStringLiteral(
            "color: #00ff41; font-size: 20px; font-weight: 800; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        m_wheelStatusLabel->setText(QStringLiteral("발전 완료! 미션 클리어!"));

        m_wheelProgressBar->setValue(100);
        m_wheelProgressBar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #1a1a2e; border: 2px solid #00ff41; border-radius: 6px; "
            "text-align: center; color: #fff; font-size: 16px; font-weight: 700; "
            "font-family: Consolas; }"
            "QProgressBar::chunk { background: #00ff41; border-radius: 5px; }"));

        // Delay, then show result popup
        QTimer::singleShot(1200, this, [this]() {
            showResultPopup(true);
        });
    } else {
        generateNewTarget();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 2 — Hint popup (not used in wheel game, kept as stub)
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission2HintPopup()
{
    // No hint popup for the wheel generator game.
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 2 — Story popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission2Story()
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;
    storyLines
        << QStringLiteral("<span style='color:#666; %1'>[17:22:10]</span>&nbsp;&nbsp;"
                          "<span style='color:#ff4444; %1'>[2단계 인증] 갓찌의 쳇바퀴 발전기</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:12]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>갓찌가 설치한 보안 발전기를 가동시켜야 합니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:13]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>전력 게이지가 좌우로 빠르게 움직입니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:14]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>노란색으로 표시된 안전 구간에 도달했을 때</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:15]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>정확히 [충전] 버튼을 눌러주세요.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:16]</span>&nbsp;&nbsp;"
                          "<span style='color:#888; %1'>...</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:17]</span>&nbsp;&nbsp;"
                          "<span style='color:#eab308; %1'>5회 연속 성공하면 발전기가 가동됩니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:22:18]</span>&nbsp;&nbsp;"
                          "<span style='color:#eab308; %1'>실패하면 충전 횟수가 초기화됩니다. 집중하세요!</span>").arg(sf);

    showTerminalPopup(
        QStringLiteral("SECURITY_TERMINAL.exe"),
        storyLines,
        QStringLiteral("\u25b6 PROCEED"),
        QStringLiteral("#00ff41"),
        QColor(0, 255, 65, 140));

    // Start gauge movement after story popup is dismissed
    if (m_wheelTimer && !m_wheelTimer->isActive()) {
        m_wheelTimer->start();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 2 — Result popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission2Result(bool correct)
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (correct) {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:23:30]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>발전기 출력 감지 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:31]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>전력 충전 5회 성공 확인</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>쳇바퀴 발전기 가동 완료</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:33]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:34]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>2단계 인증 성공</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:35]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>갓찌가 쳇바퀴에서 내려왔습니다.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 확인"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));

        showImagePopup(
            QStringLiteral(":/images/mission2_complete.png"),
            QStringLiteral("\u25b6 확인"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));

        emit missionCompleted(m_missionNumber);
    } else {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:23:30]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>발전기 출력 불안정...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:31]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>충전 실패 — 전력 부족</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:23:33]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>다시 시도하세요.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 다시 시도"),
            QStringLiteral("#ff4444"),
            QColor(255, 68, 68, 140));
    }
}
