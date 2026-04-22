#include "endingsequence.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDialog>
#include <QApplication>
#include <QScreen>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QPushButton>
#include <QLineEdit>
#include <QGridLayout>
#include <QFrame>
#include <QFont>
#include <QRegularExpression>
#include <QDebug>

// ════════════════════════════════════════════════════════════════════════════
// Static entry-point  (signature kept identical to old API)
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::show(QWidget *parent,
                          const QString & /*teamName*/,
                          int /*remainingSeconds*/,
                          int /*totalSeconds*/)
{
    qDebug() << "[ENDING] System Override & Glitch sequence started";

    QWidget *topLevel = parent->window();
    const QSize screenSize = topLevel->size();

    // Full-screen modal dialog hosts the EndingSequence widget
    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedSize(screenSize);
    dlg->move(0, 0);
    dlg->setStyleSheet(QStringLiteral("background: #000;"));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *seq = new EndingSequence(dlg);
    layout->addWidget(seq);

    // Close dialog when user clicks "입력하기"
    QObject::connect(seq, &EndingSequence::enterRequested, dlg, &QDialog::accept);

    // Start animation after the dialog's event-loop begins
    QTimer::singleShot(200, seq, &EndingSequence::startEndingSequence);

    dlg->exec();
    qDebug() << "[ENDING] System Override & Glitch sequence completed";

    // After ending animation, show the code input dialog
    // Keep showing until the user enters the correct code
    while (!showCodeInputDialog(topLevel, QStringLiteral("1212"))) {
        // Loop until correct
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════════════
EndingSequence::EndingSequence(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(this);
    root->addWidget(m_stackedWidget);

    buildErrorPage();
    buildSuccessPage();
    buildFlashOverlay();

    m_stackedWidget->setCurrentIndex(0);   // start on error page
}

// ════════════════════════════════════════════════════════════════════════════
// Error page  (index 0)  —  dark-red "ACCESS DENIED"
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::buildErrorPage()
{
    m_errorPage = new QWidget(m_stackedWidget);
    m_errorPage->setStyleSheet(QStringLiteral(
        "background: qlineargradient(y1:0, y2:1, stop:0 #1a0000, stop:1 #330000);"));

    auto *outer = new QVBoxLayout(m_errorPage);
    outer->setContentsMargins(0, 0, 0, 0);

    // Content container (this widget gets shaken during glitch)
    auto *content = new QWidget(m_errorPage);
    content->setObjectName(QStringLiteral("errorContent"));
    content->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *vbox = new QVBoxLayout(content);
    vbox->setContentsMargins(32, 24, 32, 24);
    vbox->setSpacing(16);

    vbox->addStretch(3);

    // ── Scanline decoration ─────────────────────────────────────────
    auto *scanline1 = new QLabel(
        QStringLiteral("========================================"), content);
    scanline1->setAlignment(Qt::AlignCenter);
    scanline1->setStyleSheet(QStringLiteral(
        "color: rgba(255,40,40,80); font-size: 10px; font-family: Consolas; "
        "background: transparent; border: none;"));
    vbox->addWidget(scanline1);

    vbox->addSpacing(8);

    // ── Title ───────────────────────────────────────────────────────
    m_errorTitle = new QLabel(
        QStringLiteral("  SYSTEM ERROR: ACCESS DENIED  "), content);
    m_errorTitle->setAlignment(Qt::AlignCenter);
    m_errorTitle->setStyleSheet(QStringLiteral(
        "color: #ff2222; font-size: 36px; font-weight: 900; "
        "font-family: 'Consolas', 'Courier New', monospace; "
        "background: transparent; border: none;"));
    auto *titleGlow = new QGraphicsDropShadowEffect(m_errorTitle);
    titleGlow->setBlurRadius(50);
    titleGlow->setColor(QColor(255, 0, 0, 200));
    titleGlow->setOffset(0, 0);
    m_errorTitle->setGraphicsEffect(titleGlow);
    vbox->addWidget(m_errorTitle);

    vbox->addSpacing(12);

    // ── Sub text ────────────────────────────────────────────────────
    m_errorSub = new QLabel(QStringLiteral("비콘 제어권 상실"), content);
    m_errorSub->setAlignment(Qt::AlignCenter);
    m_errorSub->setStyleSheet(QStringLiteral(
        "color: #ff6666; font-size: 22px; font-weight: 700; "
        "font-family: 'Consolas', monospace; "
        "background: transparent; border: none;"));
    vbox->addWidget(m_errorSub);

    vbox->addSpacing(20);

    // ── Detail text ─────────────────────────────────────────────────
    m_errorDetail = new QLabel(content);
    m_errorDetail->setAlignment(Qt::AlignCenter);
    m_errorDetail->setTextFormat(Qt::RichText);
    m_errorDetail->setWordWrap(true);
    m_errorDetail->setText(QStringLiteral(
        "<span style='color:#cc4444; font-size:14px; font-family:Consolas;'>"
        "[ERR] beacon_auth.service — CRITICAL FAILURE<br>"
        "[ERR] gyro_nav.service — CONNECTION LOST<br>"
        "[ERR] thermal_ctrl.service — OVERHEATED<br>"
        "[ERR] qr_decoder.service — CORRUPTED<br>"
        "[ERR] buzzer_morse.service — HIJACKED<br>"
        "<br>"
        "<span style='color:#ff4444;'>ATTEMPTING SYSTEM OVERRIDE...</span>"
        "</span>"));
    m_errorDetail->setStyleSheet(QStringLiteral(
        "background: transparent; border: none; line-height: 170%;"));
    vbox->addWidget(m_errorDetail);

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

    m_stackedWidget->addWidget(m_errorPage);   // index 0
}

// ════════════════════════════════════════════════════════════════════════════
// Success page  (index 1)  —  clean blue "ACCESS GRANTED" + master code
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::buildSuccessPage()
{
    m_successPage = new QWidget(m_stackedWidget);
    m_successPage->setStyleSheet(QStringLiteral(
        "background: qlineargradient(y1:0, y2:1, stop:0 #0a0e1a, stop:1 #0d1b2a);"));

    auto *vbox = new QVBoxLayout(m_successPage);
    vbox->setContentsMargins(32, 24, 32, 24);
    vbox->setSpacing(12);

    vbox->addStretch(3);

    // ── Decorative top line ─────────────────────────────────────────
    auto *topLine = new QLabel(
        QStringLiteral("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"), m_successPage);
    topLine->setAlignment(Qt::AlignCenter);
    topLine->setStyleSheet(QStringLiteral(
        "color: rgba(0,191,255,60); font-size: 12px; font-family: Consolas; "
        "background: transparent; border: none;"));
    vbox->addWidget(topLine);

    vbox->addSpacing(4);

    // ── Status tag ──────────────────────────────────────────────────
    auto *statusTag = new QLabel(
        QStringLiteral("[ STATUS: SYSTEM OVERRIDE COMPLETE ]"), m_successPage);
    statusTag->setAlignment(Qt::AlignCenter);
    statusTag->setStyleSheet(QStringLiteral(
        "color: #00bfff; font-size: 13px; font-weight: 600; "
        "font-family: Consolas; background: transparent; border: none;"));
    vbox->addWidget(statusTag);

    vbox->addSpacing(12);

    // ── Title ───────────────────────────────────────────────────────
    m_successTitle = new QLabel(
        QStringLiteral("접근 승인 (ACCESS GRANTED)"), m_successPage);
    m_successTitle->setAlignment(Qt::AlignCenter);
    m_successTitle->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 34px; font-weight: 900; "
        "font-family: 'Consolas', 'Courier New', monospace; "
        "background: transparent; border: none;"));
    auto *successGlow = new QGraphicsDropShadowEffect(m_successTitle);
    successGlow->setBlurRadius(60);
    successGlow->setColor(QColor(0, 255, 65, 180));
    successGlow->setOffset(0, 0);
    m_successTitle->setGraphicsEffect(successGlow);
    vbox->addWidget(m_successTitle);

    vbox->addSpacing(28);

    // ── Code header ─────────────────────────────────────────────────
    auto *codeHeader = new QLabel(
        QStringLiteral("마스터 퇴근 코드"), m_successPage);
    codeHeader->setAlignment(Qt::AlignCenter);
    codeHeader->setStyleSheet(QStringLiteral(
        "color: #8899aa; font-size: 15px; font-weight: 600; "
        "font-family: Consolas; background: transparent; border: none;"));
    vbox->addWidget(codeHeader);

    vbox->addSpacing(8);

    // ── Master code ─────────────────────────────────────────────────
    m_masterCodeLabel = new QLabel(
        QStringLiteral("LG-SW-BOOT-12TH-FREE"), m_successPage);
    m_masterCodeLabel->setAlignment(Qt::AlignCenter);
    m_masterCodeLabel->setStyleSheet(QStringLiteral(
        "color: #ffffff; font-size: 46px; font-weight: 900; "
        "font-family: 'Consolas', 'Courier New', monospace; "
        "background: transparent; border: none; "
        "letter-spacing: 4px;"));
    auto *codeGlow = new QGraphicsDropShadowEffect(m_masterCodeLabel);
    codeGlow->setBlurRadius(40);
    codeGlow->setColor(QColor(0, 191, 255, 160));
    codeGlow->setOffset(0, 0);
    m_masterCodeLabel->setGraphicsEffect(codeGlow);
    vbox->addWidget(m_masterCodeLabel);

    vbox->addSpacing(28);

    // ── Sub text ────────────────────────────────────────────────────
    m_successSub = new QLabel(m_successPage);
    m_successSub->setAlignment(Qt::AlignCenter);
    m_successSub->setTextFormat(Qt::RichText);
    m_successSub->setWordWrap(true);
    m_successSub->setText(QStringLiteral(
        "<span style='color:#4a90d9; font-size:15px; font-family:Consolas; line-height:170%;'>"
        "패스트파이브 전 층 비콘이 복구되었습니다.<br><br>"
        "<span style='color:#00ff41; font-weight:bold;'>"
        "이 코드를 입력하면 퇴근이 승인됩니다.</span>"
        "</span>"));
    m_successSub->setStyleSheet(QStringLiteral(
        "background: transparent; border: none;"));
    vbox->addWidget(m_successSub);

    vbox->addSpacing(4);

    auto *bottomLine = new QLabel(
        QStringLiteral("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"), m_successPage);
    bottomLine->setAlignment(Qt::AlignCenter);
    bottomLine->setStyleSheet(QStringLiteral(
        "color: rgba(0,191,255,60); font-size: 12px; font-family: Consolas; "
        "background: transparent; border: none;"));
    vbox->addWidget(bottomLine);

    vbox->addSpacing(20);

    // ── "입력하기" button ────────────────────────────────────────
    auto *enterBtn = new QPushButton(QStringLiteral("▶  입력하기"), m_successPage);
    enterBtn->setFixedSize(220, 52);
    enterBtn->setCursor(Qt::PointingHandCursor);
    enterBtn->setFocusPolicy(Qt::NoFocus);
    enterBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0d1b2a; color: #00ff41; "
        "border: 2px solid #00ff41; border-radius: 8px; "
        "font-size: 18px; font-weight: 900; font-family: Consolas; }"
        "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"
        "QPushButton:pressed { background: #00cc33; color: #0c0c0c; }" ));
    auto *btnGlow = new QGraphicsDropShadowEffect(enterBtn);
    btnGlow->setBlurRadius(30);
    btnGlow->setColor(QColor(0, 255, 65, 120));
    btnGlow->setOffset(0, 0);
    enterBtn->setGraphicsEffect(btnGlow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(enterBtn);
    btnRow->addStretch();
    vbox->addLayout(btnRow);

    connect(enterBtn, &QPushButton::clicked, this, &EndingSequence::enterRequested);

    vbox->addStretch(3);

    m_stackedWidget->addWidget(m_successPage);   // index 1
}

// ════════════════════════════════════════════════════════════════════════════
// Flash overlay  —  white sheet covering everything
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::buildFlashOverlay()
{
    m_flashOverlay = new QWidget(this);
    m_flashOverlay->setStyleSheet(QStringLiteral("background-color: #ffffff;"));
    m_flashOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_flashOverlay->setGeometry(rect());
    m_flashOverlay->raise();

    m_flashEffect = new QGraphicsOpacityEffect(m_flashOverlay);
    m_flashEffect->setOpacity(0.0);
    m_flashOverlay->setGraphicsEffect(m_flashEffect);
}

// ════════════════════════════════════════════════════════════════════════════
// Sequence kick-off
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::startEndingSequence()
{
    // Ensure overlay covers full widget after layout settles
    m_flashOverlay->setGeometry(rect());
    m_flashOverlay->raise();

    startGlitch();

    // After GLITCH_DURATION_MS → white-out flash
    QTimer::singleShot(GLITCH_DURATION_MS, this, &EndingSequence::startWhiteout);
}

// ════════════════════════════════════════════════════════════════════════════
// Stage 1 — Glitch  (50 ms timer shakes the error content)
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::startGlitch()
{
    // Save original position of the error content widget
    auto *content = m_errorPage->findChild<QWidget *>(QStringLiteral("errorContent"));
    if (content) {
        m_errorOrigPos = content->pos();
    }

    m_glitchElapsed = 0;

    m_glitchTimer = new QTimer(this);
    m_glitchTimer->setInterval(50);
    connect(m_glitchTimer, &QTimer::timeout, this, [this, content]() {
        if (!content) return;

        m_glitchElapsed += 50;

        // Random offset [-5, 5] px in both axes
        auto *rng = QRandomGenerator::global();
        const int dx = rng->bounded(11) - 5;   // -5..+5
        const int dy = rng->bounded(11) - 5;
        content->move(m_errorOrigPos.x() + dx,
                      m_errorOrigPos.y() + dy);

        // Randomly flicker visibility of sub-elements for extra glitch feel
        const bool showSub = (rng->bounded(100) > 20);   // 80 % visible
        m_errorSub->setVisible(showSub);
        m_errorDetail->setVisible(rng->bounded(100) > 10);
    });
    m_glitchTimer->start();
}

void EndingSequence::stopGlitch()
{
    if (m_glitchTimer) {
        m_glitchTimer->stop();
        m_glitchTimer->deleteLater();
        m_glitchTimer = nullptr;
    }

    // Restore original position and visibility
    auto *content = m_errorPage->findChild<QWidget *>(QStringLiteral("errorContent"));
    if (content) {
        content->move(m_errorOrigPos);
    }
    m_errorSub->setVisible(true);
    m_errorDetail->setVisible(true);
}

// ════════════════════════════════════════════════════════════════════════════
// Stage 2 — White-out flash  (opacity 0 → 1 in 300 ms)
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::startWhiteout()
{
    m_flashOverlay->setGeometry(rect());
    m_flashOverlay->raise();

    auto *fadeIn = new QPropertyAnimation(m_flashEffect, "opacity", this);
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    connect(fadeIn, &QPropertyAnimation::finished,
            this, &EndingSequence::revealSuccess);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

// ════════════════════════════════════════════════════════════════════════════
// Stage 3 — Switch to success page, fade overlay out  (1 → 0 in 800 ms)
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::revealSuccess()
{
    // Stop glitch, swap page while screen is pure white
    stopGlitch();
    m_stackedWidget->setCurrentIndex(1);

    // Fade the white overlay out to reveal the clean success screen
    auto *fadeOut = new QPropertyAnimation(m_flashEffect, "opacity", this);
    fadeOut->setDuration(800);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

// ════════════════════════════════════════════════════════════════════════════
// Code input dialog — 4-digit keypad (same style as recovery code)
// Returns true if the user entered the correct code.
// ════════════════════════════════════════════════════════════════════════════
bool EndingSequence::showCodeInputDialog(QWidget *topLevel, const QString &correctCode)
{
    bool accepted = false;

    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 220);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose, false);
    dlg->setFixedSize(540, 520);
    dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));

    auto *mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *frame = new QFrame(dlg);
    frame->setObjectName(QStringLiteral("codeFrame"));
    frame->setStyleSheet(QStringLiteral(
        "QFrame#codeFrame { background-color: #0c0c0c; border: 2px solid #00ff41; border-radius: 8px; }"));
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(24, 16, 24, 16);
    frameLayout->setSpacing(10);

    // ── Title ───────────────────────────────────────────────────────
    auto *titleLabel = new QLabel(QStringLiteral("🔑 마스터 퇴근 코드 입력"), frame);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 22px; font-weight: 900; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    frameLayout->addWidget(titleLabel);

    // ── Description ─────────────────────────────────────────────────
    auto *descLabel = new QLabel(QStringLiteral("화면에 표시된 코드를 입력하세요."), frame);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral(
        "color: #cccccc; font-size: 13px; font-family: 'Consolas', monospace; "
        "background: transparent; border: none;"));
    frameLayout->addWidget(descLabel);

    // ── Input field (read-only — keypad only) ───────────────────────
    auto *inputLine = new QLineEdit(frame);
    inputLine->setAlignment(Qt::AlignCenter);
    inputLine->setPlaceholderText(QStringLiteral("4자리 코드 입력"));
    inputLine->setMaxLength(4);
    inputLine->setReadOnly(true);
    inputLine->setFocusPolicy(Qt::NoFocus);
    inputLine->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1a1a2e; color: #00ff41; border: 2px solid #00ff41; "
        "border-radius: 4px; font-size: 28px; font-weight: bold; "
        "font-family: 'Consolas', monospace; padding: 6px; }"));
    frameLayout->addWidget(inputLine);

    // ── Error message ───────────────────────────────────────────────
    auto *errLabel = new QLabel(frame);
    errLabel->setAlignment(Qt::AlignCenter);
    errLabel->setFixedHeight(20);
    errLabel->setStyleSheet(QStringLiteral(
        "color: #ff4444; font-size: 13px; font-family: Consolas; "
        "background: transparent; border: none;"));
    errLabel->hide();
    frameLayout->addWidget(errLabel);

    // ── Numeric keypad ──────────────────────────────────────────────
    const QString keyBtnStyle = QStringLiteral(
        "QPushButton { background: #1a1a2e; color: #00ff41; border: 1px solid #00ff41; "
        "border-radius: 6px; font-size: 22px; font-weight: 900; font-family: Consolas; }"
        "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"
        "QPushButton:pressed { background: #00cc33; color: #0c0c0c; }");
    const QString delBtnStyle = QStringLiteral(
        "QPushButton { background: #1a1a2e; color: #ff4444; border: 1px solid #ff4444; "
        "border-radius: 6px; font-size: 18px; font-weight: 900; font-family: Consolas; }"
        "QPushButton:hover { background: #ff4444; color: #0c0c0c; }");

    auto *keypadWidget = new QWidget(frame);
    auto *keypadGrid = new QGridLayout(keypadWidget);
    keypadGrid->setContentsMargins(20, 0, 20, 0);
    keypadGrid->setSpacing(8);

    // 1-9
    for (int i = 1; i <= 9; ++i) {
        auto *btn = new QPushButton(QString::number(i), keypadWidget);
        btn->setFixedSize(80, 50);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(keyBtnStyle);
        keypadGrid->addWidget(btn, (i - 1) / 3, (i - 1) % 3);
        QObject::connect(btn, &QPushButton::clicked, dlg, [inputLine, btn, i]() {
            if (inputLine->text().length() < 4) {
                inputLine->setText(inputLine->text() + QString::number(i));
                btn->setEnabled(false);
                QTimer::singleShot(250, btn, [btn]() { btn->setEnabled(true); });
            }
        });
    }

    // Bottom row: [DEL] [0] [확인]
    auto *delBtn = new QPushButton(QStringLiteral("⌫"), keypadWidget);
    delBtn->setFixedSize(80, 50);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setFocusPolicy(Qt::NoFocus);
    delBtn->setStyleSheet(delBtnStyle);
    keypadGrid->addWidget(delBtn, 3, 0);
    QObject::connect(delBtn, &QPushButton::clicked, dlg, [inputLine, delBtn]() {
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
    QObject::connect(zeroBtn, &QPushButton::clicked, dlg, [inputLine, zeroBtn]() {
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
        "QPushButton { background: #0a1628; color: #00ff41; border: 2px solid #00ff41; "
        "border-radius: 6px; font-size: 18px; font-weight: 900; font-family: Consolas; }"
        "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
    auto *glow = new QGraphicsDropShadowEffect(submitBtn);
    glow->setBlurRadius(16);
    glow->setColor(QColor(0, 255, 157, 120));
    glow->setOffset(0, 0);
    submitBtn->setGraphicsEffect(glow);
    keypadGrid->addWidget(submitBtn, 3, 2);

    frameLayout->addWidget(keypadWidget);
    mainLayout->addWidget(frame);

    // ── Submit logic ────────────────────────────────────────────────
    QObject::connect(submitBtn, &QPushButton::clicked, dlg,
        [dlg, inputLine, errLabel, correctCode, &accepted]() {
            const QString input = inputLine->text().trimmed();
            if (input.isEmpty()) {
                errLabel->setText(QStringLiteral("코드를 입력해주세요."));
                errLabel->show();
                return;
            }
            if (input == correctCode) {
                accepted = true;
                dlg->accept();
            } else {
                errLabel->setText(QStringLiteral("❌ 잘못된 코드입니다. 다시 입력해주세요."));
                errLabel->show();
                inputLine->clear();
            }
        });

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
    }

    dlg->exec();
    dlg->deleteLater();
    overlay->hide();
    overlay->deleteLater();

    return accepted;
}
