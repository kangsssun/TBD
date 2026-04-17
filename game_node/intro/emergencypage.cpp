#include "emergencypage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QPauseAnimation>
#include <QEasingCurve>
#include <QTimer>

EmergencyPage::EmergencyPage(QWidget *parent)
    : QWidget(parent)
    , m_stack(nullptr)
    , m_storyLayer(nullptr)
    , m_dimLayer(nullptr)
    , m_flashLayer(nullptr)
    , m_storyFlashEffect(nullptr)
    , m_storyFlashAnim(nullptr)
    , m_flashEffect(nullptr)
    , m_pulseAnim(nullptr)
    , m_storyTimer(nullptr)
    , m_confirmButton(nullptr)
{
    setupUi();
}

void EmergencyPage::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *stackContainer = new QWidget(this);
    m_stack = new QStackedLayout(stackContainer);
    m_stack->setContentsMargins(0, 0, 0, 0);
    m_stack->setStackingMode(QStackedLayout::StackAll);
    rootLayout->addWidget(stackContainer);

    // ── Story layer (black background with flashing text) ──────────────
    m_storyLayer = new QWidget(this);
    m_storyLayer->setStyleSheet(QStringLiteral("background-color:#000000;"));
    auto *storyLayout = new QVBoxLayout(m_storyLayer);
    storyLayout->setContentsMargins(36, 36, 36, 36);
    storyLayout->setSpacing(0);

    auto *storyLabel = new QLabel(
        QStringLiteral("때는 3025년, SW 교육 과정 마지막 날.\n\n"
                       "긴급 점검이라는 안내와 함께\n"
                       "건물의 모든 출입문이 봉쇄되었다.\n\n"
                       "남은 시간 안에 시스템 복구 코드를 찾아 탈출하라.\n\n"
                       "실패하면, 영원히 이곳에 갇힌다."),
        m_storyLayer);
    storyLabel->setAlignment(Qt::AlignCenter);
    storyLabel->setWordWrap(true);
    storyLabel->setStyleSheet(
        QStringLiteral("color:#ffffff; font-size:22px; font-weight:700; "
                       "line-height:160%; background:transparent; border:none;"));

    m_storyFlashEffect = new QGraphicsOpacityEffect(storyLabel);
    m_storyFlashEffect->setOpacity(0.32);
    storyLabel->setGraphicsEffect(m_storyFlashEffect);

    m_storyFlashAnim = new QSequentialAnimationGroup(this);
    m_storyFlashAnim->setLoopCount(-1);

    auto *storyFlash1 = new QPropertyAnimation(m_storyFlashEffect, "opacity", m_storyFlashAnim);
    storyFlash1->setDuration(70);
    storyFlash1->setStartValue(0.32);
    storyFlash1->setEndValue(1.0);
    storyFlash1->setEasingCurve(QEasingCurve::OutCubic);
    m_storyFlashAnim->addAnimation(storyFlash1);

    auto *storyFlashBack1 = new QPropertyAnimation(m_storyFlashEffect, "opacity", m_storyFlashAnim);
    storyFlashBack1->setDuration(130);
    storyFlashBack1->setStartValue(1.0);
    storyFlashBack1->setEndValue(0.28);
    storyFlashBack1->setEasingCurve(QEasingCurve::InOutSine);
    m_storyFlashAnim->addAnimation(storyFlashBack1);

    auto *storyFlash2 = new QPropertyAnimation(m_storyFlashEffect, "opacity", m_storyFlashAnim);
    storyFlash2->setDuration(55);
    storyFlash2->setStartValue(0.28);
    storyFlash2->setEndValue(0.92);
    storyFlash2->setEasingCurve(QEasingCurve::OutCubic);
    m_storyFlashAnim->addAnimation(storyFlash2);

    auto *storyFlashBack2 = new QPropertyAnimation(m_storyFlashEffect, "opacity", m_storyFlashAnim);
    storyFlashBack2->setDuration(180);
    storyFlashBack2->setStartValue(0.92);
    storyFlashBack2->setEndValue(0.30);
    storyFlashBack2->setEasingCurve(QEasingCurve::InOutSine);
    m_storyFlashAnim->addAnimation(storyFlashBack2);

    auto *storyHold = new QPauseAnimation(520, m_storyFlashAnim);
    m_storyFlashAnim->addAnimation(storyHold);

    storyLayout->addStretch();
    storyLayout->addWidget(storyLabel);
    storyLayout->addStretch();

    // ── Dim layer (emergency alert card) ───────────────────────────────
    m_dimLayer = new QWidget(this);
    m_dimLayer->setStyleSheet(QStringLiteral("background-color:#000000;"));
    auto *dimLayout = new QVBoxLayout(m_dimLayer);
    dimLayout->setContentsMargins(0, 0, 0, 0);
    dimLayout->setSpacing(0);

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addStretch();

    auto *alertCard = new QWidget(m_dimLayer);
    alertCard->setObjectName(QStringLiteral("emergencyAlertCard"));
    alertCard->setMinimumSize(760, 480);
    alertCard->setMaximumSize(980, 660);
    alertCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    alertCard->setStyleSheet(QStringLiteral("background-color:#1e1e1e; border:4px solid #ff1744; border-radius:10px;"));

    auto *alertGlow = new QGraphicsDropShadowEffect(alertCard);
    alertGlow->setBlurRadius(36);
    alertGlow->setColor(QColor(255, 23, 68, 190));
    alertGlow->setOffset(0, 0);
    alertCard->setGraphicsEffect(alertGlow);

    auto *alertLayout = new QVBoxLayout(alertCard);
    alertLayout->setContentsMargins(16, 16, 16, 16);
    alertLayout->setSpacing(12);

    auto *headerStrip = new QLabel(QStringLiteral("[ 긴급 공지 ]"), alertCard);
    headerStrip->setAlignment(Qt::AlignCenter);
    headerStrip->setFixedHeight(54);
    headerStrip->setStyleSheet(QStringLiteral("background-color:#ff1744; color:#ffffff; border:1px solid #ff8ea7; border-radius:6px; font-size:22px; font-weight:800;"));

    auto *headerGlow = new QGraphicsDropShadowEffect(headerStrip);
    headerGlow->setBlurRadius(24);
    headerGlow->setColor(QColor(255, 23, 68, 170));
    headerGlow->setOffset(0, 0);
    headerStrip->setGraphicsEffect(headerGlow);

    auto *textContainer = new QWidget(alertCard);
    textContainer->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *textLayout = new QVBoxLayout(textContainer);
    textLayout->setContentsMargins(2, 2, 2, 2);
    textLayout->setSpacing(14);

    const QString noticeTextStyle = QStringLiteral("color:#ffffff; font-size:18px; font-weight:700; line-height:148%; border:none; background:transparent;");

    auto *noticeBody = new QLabel(
        QStringLiteral("비콘 및 패스트 파이브 출입 시스템 전면에 장애가 발생했습니다.\n\n"
                       "원인은 아직 밝혀지지 않았으며 복구 시까지 모든 교육생은 퇴실 불가입니다.\n\n"
                       "5단계 인증을 통해 시스템 복구 코드를 획득한 팀부터 순서대로 퇴실이 승인됩니다.\n\n"
                       "- LG SW Bootcamp 12기 교육 담당자 드림"),
        textContainer);
    noticeBody->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    noticeBody->setWordWrap(true);
    noticeBody->setStyleSheet(noticeTextStyle);
    noticeBody->setAttribute(Qt::WA_TranslucentBackground, true);

    textLayout->addStretch();
    textLayout->addWidget(noticeBody);
    textLayout->addStretch();

    m_confirmStyle = QStringLiteral("background-color:#ff1744; color:#ffffff; border:1px solid #ff8ea7; border-radius:8px; font-size:20px; font-weight:800; padding:10px 18px;");

    m_confirmButton = new QPushButton(QStringLiteral("확인 (게임 시작)"), alertCard);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setMinimumHeight(52);
    m_confirmButton->setFixedWidth(292);
    m_confirmButton->setEnabled(true);
    m_confirmButton->setStyleSheet(m_confirmStyle);

    auto *buttonGlow = new QGraphicsDropShadowEffect(m_confirmButton);
    buttonGlow->setBlurRadius(24);
    buttonGlow->setColor(QColor(255, 23, 68, 180));
    buttonGlow->setOffset(0, 0);
    m_confirmButton->setGraphicsEffect(buttonGlow);

    alertLayout->addWidget(headerStrip);
    alertLayout->addWidget(textContainer, 1);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 14);
    bottomRow->setSpacing(0);
    bottomRow->addStretch();
    bottomRow->addWidget(m_confirmButton, 0, Qt::AlignVCenter);
    bottomRow->addStretch();
    alertLayout->addLayout(bottomRow);
    alertLayout->addSpacing(8);

    row->addWidget(alertCard);
    row->addStretch();
    dimLayout->addStretch();
    dimLayout->addLayout(row);
    dimLayout->addStretch();

    // ── Flash layer (red pulse overlay) ────────────────────────────────
    m_flashLayer = new QWidget(this);
    m_flashLayer->setStyleSheet(QStringLiteral("background-color:#ff1744;"));
    m_flashLayer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_flashEffect = new QGraphicsOpacityEffect(m_flashLayer);
    m_flashEffect->setOpacity(0.0);
    m_flashLayer->setGraphicsEffect(m_flashEffect);

    // ── Stack ordering ─────────────────────────────────────────────────
    m_stack->addWidget(m_dimLayer);
    m_stack->addWidget(m_flashLayer);
    m_stack->addWidget(m_storyLayer);
    m_stack->setCurrentWidget(m_storyLayer);

    // StackAll shows all — hide the layers not yet needed
    m_dimLayer->hide();
    m_flashLayer->hide();

    // ── Story timer (5 seconds on black screen) ────────────────────────
    m_storyTimer = new QTimer(this);
    m_storyTimer->setSingleShot(true);

    // ── Pulse animation ────────────────────────────────────────────────
    m_pulseAnim = new QSequentialAnimationGroup(this);
    for (int i = 0; i < 30; ++i) {
        auto *fadeIn = new QPropertyAnimation(m_flashEffect, "opacity", m_pulseAnim);
        fadeIn->setDuration(95);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(0.5);
        fadeIn->setEasingCurve(QEasingCurve::InOutSine);
        m_pulseAnim->addAnimation(fadeIn);

        auto *fadeOut = new QPropertyAnimation(m_flashEffect, "opacity", m_pulseAnim);
        fadeOut->setDuration(105);
        fadeOut->setStartValue(0.5);
        fadeOut->setEndValue(0.0);
        fadeOut->setEasingCurve(QEasingCurve::InOutSine);
        m_pulseAnim->addAnimation(fadeOut);
    }

    // ── Connections ────────────────────────────────────────────────────
    QObject::connect(m_confirmButton, &QPushButton::clicked, this, &EmergencyPage::confirmed);

    QObject::connect(m_storyTimer, &QTimer::timeout, this, [this]() {
        m_storyFlashAnim->stop();
        m_storyFlashEffect->setOpacity(1.0);
        m_storyLayer->hide();
        m_dimLayer->show();
        m_flashLayer->show();
        m_stack->setCurrentWidget(m_flashLayer);
        m_pulseAnim->start();
    });

    QObject::connect(m_pulseAnim, &QSequentialAnimationGroup::finished, this, [this]() {
        m_confirmButton->setStyleSheet(m_confirmStyle);
        m_confirmButton->setEnabled(true);
        m_confirmButton->setCursor(Qt::PointingHandCursor);
        m_confirmButton->update();
    });
}

void EmergencyPage::reset()
{
    m_pulseAnim->stop();
    m_flashEffect->setOpacity(0.0);
    m_storyFlashAnim->stop();
    m_storyFlashEffect->setOpacity(0.32);

    // Show story layer first, hide others
    m_dimLayer->hide();
    m_flashLayer->hide();
    m_storyLayer->show();
    m_storyLayer->raise();
    m_stack->setCurrentWidget(m_storyLayer);

    m_storyFlashAnim->start();
    m_storyTimer->start(5000);

    m_confirmButton->setStyleSheet(m_confirmStyle);
    m_confirmButton->setEnabled(true);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
}
