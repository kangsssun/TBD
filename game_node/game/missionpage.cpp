#include "missionpage.h"
#include "title/teamnamedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QDialog>
#include <QApplication>
#include <QScreen>
#include <QThread>
#include <QEvent>
#include <QGridLayout>
#include <QStyle>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QShowEvent>
#include <QHideEvent>

#ifdef Q_OS_LINUX
#include "dd_api/led_api.h"
#include "dd_api/buzzer_api.h"
#include "dd_api/camera_api.h"
#endif

namespace {
void resetButtonHoverState(QPushButton *button)
{
    if (!button) return;

    button->setDown(false);
    button->clearFocus();

    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(button, &leaveEvent);

    if (QStyle *style = button->style()) {
        style->unpolish(button);
        style->polish(button);
    }

    button->update();
}
}

bool MissionPage::s_operatorMode = false;

void MissionPage::setOperatorMode(bool enabled)
{
    s_operatorMode = enabled;
}

bool MissionPage::isOperatorMode()
{
    return s_operatorMode;
}

void MissionPage::syncOperatorModeUi()
{
    if (!s_operatorMode) {
        return;
    }

    const QList<QPushButton *> buttons = findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button && button->text().contains(QStringLiteral("SUBMIT"), Qt::CaseInsensitive)) {
            button->setEnabled(true);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════
MissionPage::MissionPage(int missionNumber, QWidget *parent)
    : QWidget(parent)
    , m_missionNumber(missionNumber)
    , m_contentLayout(nullptr)
    , m_mission3PreviewArea(nullptr)
    , m_mission3CaptureStatus(nullptr)
    , m_mission3PopupWatchTimer(nullptr)
    , m_mission3CameraActive(false)
    , m_mission3CameraStarting(false)
    , m_mission3StoryPopupDismissed(false)
    , m_mission3CaptureInProgress(false)
    , m_mission3PendingStop(false)
    , m_mission3CapturedImagePath()
    , m_mission3LastCameraAttemptMs(0)
{
    setObjectName(QStringLiteral("missionPage_%1").arg(missionNumber));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_contentLayout = rootLayout;

    if (m_missionNumber == 3) {
        qApp->installEventFilter(this);
        m_mission3PopupWatchTimer = new QTimer(this);
        m_mission3PopupWatchTimer->setInterval(150);
        QObject::connect(m_mission3PopupWatchTimer, &QTimer::timeout, this, [this]() {
            evaluateMission3CameraPreview();
        });
    }

    setupMission();
}

MissionPage::~MissionPage()
{
    if (m_missionNumber == 3) {
        qApp->removeEventFilter(this);
    }
    stopMission3CameraPreview();
}

bool MissionPage::eventFilter(QObject *watched, QEvent *event)
{
    if (m_missionNumber == 3 && isVisible() && watched && event) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget != this && widget != window() && widget->isWindow()) {
            const bool isBlockingDialog = qobject_cast<QDialog *>(widget)
                || widget->windowModality() != Qt::NonModal;

            if (isBlockingDialog) {
                const QEvent::Type type = event->type();

                if (type == QEvent::Show || type == QEvent::ShowToParent || type == QEvent::WindowActivate) {
                    if (m_mission3CameraActive) {
                        if (m_mission3CaptureInProgress) {
                            m_mission3PendingStop = true;
                        } else {
                            stopMission3CameraPreview();
                        }
                    }
                }

                if (type == QEvent::Hide || type == QEvent::Close || type == QEvent::WindowDeactivate) {
                    QTimer::singleShot(0, this, [this]() {
                        evaluateMission3CameraPreview();
                    });
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void MissionPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    syncOperatorModeUi();
    if (m_missionNumber == 3) {
        if (m_mission3PopupWatchTimer && !m_mission3PopupWatchTimer->isActive()) {
            m_mission3PopupWatchTimer->start();
        }
        QTimer::singleShot(0, this, [this]() {
            evaluateMission3CameraPreview();
        });
    }
}

void MissionPage::hideEvent(QHideEvent *event)
{
    if (m_missionNumber == 3) {
        if (m_mission3PopupWatchTimer && m_mission3PopupWatchTimer->isActive()) {
            m_mission3PopupWatchTimer->stop();
        }
        stopMission3CameraPreview();
    }
    QWidget::hideEvent(event);
}

QString MissionPage::buildMission3CapturePath() const
{
    QDir captureDir(QCoreApplication::applicationDirPath() + QStringLiteral("/captures"));
    if (!captureDir.exists()) {
        captureDir.mkpath(QStringLiteral("."));
    }

    const QString fileName = QStringLiteral("mission3_%1.jpg")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    return captureDir.filePath(fileName);
}

void MissionPage::startMission3CameraPreview()
{
    if (m_missionNumber != 3 || m_mission3CameraActive || m_mission3CameraStarting || !m_mission3PreviewArea) {
        return;
    }

    // 카메라 초기화 실패 시 10초 쿨다운
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_mission3LastCameraAttemptMs > 0 && (now - m_mission3LastCameraAttemptMs) < 3000) {
        return;
    }

#ifdef Q_OS_LINUX
    if (!isVisible() || !m_mission3PreviewArea->isVisible()) {
        return;
    }

    const QRect rawPreviewRect(
        m_mission3PreviewArea->mapToGlobal(QPoint(0, 0)),
        m_mission3PreviewArea->size());
    if (!rawPreviewRect.isValid()) {
        return;
    }

    QScreen *targetScreen = m_mission3PreviewArea->screen();
    if (!targetScreen) {
        targetScreen = QApplication::screenAt(rawPreviewRect.center());
    }
    if (!targetScreen) {
        targetScreen = QApplication::primaryScreen();
    }

    const QRect screenRect = targetScreen
        ? targetScreen->geometry()
        : QRect(rawPreviewRect.topLeft(), rawPreviewRect.size());
    QRect boundedPreviewRect = rawPreviewRect.intersected(screenRect);
    if (boundedPreviewRect.width() < 40 || boundedPreviewRect.height() < 40) {
        return;
    }

    int width = qMax(40, boundedPreviewRect.width());
    int height = qMax(40, boundedPreviewRect.height());

    if (width % 2 != 0) {
        --width;
    }
    if (height % 2 != 0) {
        --height;
    }
    if (width < 2 || height < 2) {
        return;
    }

    const int drawX = boundedPreviewRect.left();
    const int drawY = boundedPreviewRect.top();

    const QPoint topLeft(drawX, drawY);

    m_mission3CameraStarting = true;
    const int result = camera_init(topLeft.x(), topLeft.y(), width, height);
    m_mission3CameraStarting = false;
    m_mission3CameraActive = (result == 0);
    if (!m_mission3CameraActive) {
        m_mission3LastCameraAttemptMs = QDateTime::currentMSecsSinceEpoch();
    } else {
        m_mission3LastCameraAttemptMs = 0;
    }
#endif
}

void MissionPage::stopMission3CameraPreview()
{
    if (m_missionNumber != 3) {
        return;
    }

#ifdef Q_OS_LINUX
    if (!m_mission3CameraActive) {
        m_mission3CameraStarting = false;
        return;
    }

    m_mission3CameraActive = false;
    m_mission3CameraStarting = false;
    camera_close();
    refreshMission3PreviewPlaceholder();
#endif
}

void MissionPage::refreshMission3PreviewPlaceholder()
{
    if (m_missionNumber != 3 || !m_mission3PreviewArea) {
        return;
    }

    if (auto *previewLabel = qobject_cast<QLabel *>(m_mission3PreviewArea)) {
        previewLabel->clear();
        previewLabel->setText(QStringLiteral("[ LIVE CAMERA ]"));
    }

    m_mission3PreviewArea->update();
    m_mission3PreviewArea->repaint();

    if (QWidget *root = window()) {
        const QPoint topLeft = m_mission3PreviewArea->mapTo(root, QPoint(0, 0));
        root->update(QRect(topLeft, m_mission3PreviewArea->size()).adjusted(-2, -2, 2, 2));
        root->repaint(QRect(topLeft, m_mission3PreviewArea->size()).adjusted(-2, -2, 2, 2));
    }
}

bool MissionPage::hasBlockingPopupOpen() const
{
    if (QWidget *modal = QApplication::activeModalWidget()) {
        if (modal->isVisible()) {
            return true;
        }
    }

    if (QWidget *popup = QApplication::activePopupWidget()) {
        if (popup->isVisible()) {
            return true;
        }
    }

    const QWidget *rootWindow = window();
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *top : topLevels) {
        if (!top || !top->isVisible()) {
            continue;
        }
        if (top == rootWindow) {
            continue;
        }
        if (qobject_cast<QDialog *>(top)) {
            return true;
        }
        if (top->windowModality() != Qt::NonModal) {
            return true;
        }
    }

    return false;
}

void MissionPage::evaluateMission3CameraPreview()
{
    if (m_missionNumber != 3) {
        return;
    }

    const bool canRenderArea = (m_mission3PreviewArea && m_mission3PreviewArea->isVisible());
    const bool popupOpen = hasBlockingPopupOpen();
    const bool shouldRun = isVisible() && canRenderArea && m_mission3StoryPopupDismissed && !popupOpen;

    if (m_mission3CaptureStatus) {
        if (!m_mission3StoryPopupDismissed) {
            m_mission3CaptureStatus->setText(QStringLiteral("스토리 팝업 종료 후 LIVE 시작"));
        } else if (popupOpen) {
            m_mission3CaptureStatus->setText(QStringLiteral("팝업 표시 중 - LIVE 일시중단"));
        } else if (m_mission3CameraActive) {
            m_mission3CaptureStatus->clear();
        } else {
            m_mission3CaptureStatus->setText(QStringLiteral("LIVE 준비 중..."));
        }
    }

    if (!shouldRun) {
        if (m_mission3CameraActive) {
            if (m_mission3CaptureInProgress) {
                m_mission3PendingStop = true;
            } else {
                stopMission3CameraPreview();
            }
        }
        return;
    }

    m_mission3PendingStop = false;
    if (!m_mission3CameraActive && !m_mission3CameraStarting) {
        startMission3CameraPreview();
        if (m_mission3CaptureStatus && m_mission3CameraActive) {
            m_mission3CaptureStatus->clear();
        }
    }
}

bool MissionPage::isAnswerAccepted(const QString &answer) const
{
    if (s_operatorMode) {
        return true;
    }

    const QString normalized = answer.trimmed();
    if (normalized.compare(QStringLiteral("9999"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return normalized.compare(m_correctAnswer, Qt::CaseInsensitive) == 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 1 — Terminal story + dummy problem
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission1()
{
    m_correctAnswer = QStringLiteral("1947");

    // ── Problem page (image left, input+keypad right) ──────────────────
    auto *problemPage = new QWidget(this);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(12, 6, 12, 6);
    problemLayout->setSpacing(6);

    // Header
    auto *problemHeader = new QLabel(QStringLiteral("[MISSION 1] - LED 모스부호"), problemPage);
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

    auto *imageArea = new QWidget(leftPanel);
    imageArea->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(0, 0, 0, 0);
    imageAreaLayout->setSpacing(0);

    auto *imageLabel = new QLabel(imageArea);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    QPixmap missionPixmap(QStringLiteral(":/images/misson_1.png"));
    if (!missionPixmap.isNull()) {
        imageLabel->setPixmap(missionPixmap.scaled(380, 350, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imageLabel->setText(QStringLiteral("[ IMAGE ]"));
        imageLabel->setStyleSheet(QStringLiteral(
            "color: #666; font-size: 18px; background: transparent; border: none; "
            "min-width: 200px; min-height: 200px;"));
    }
            imageAreaLayout->addStretch();
            imageAreaLayout->addWidget(imageLabel, 0, Qt::AlignCenter);
            imageAreaLayout->addStretch();
            leftLayout->addWidget(imageArea, 1);

    // "PLAY" button — trigger LED signal (below image)
    auto *btnReplay = new QPushButton(QStringLiteral("\u25b6 PLAY"), leftPanel);
    btnReplay->setCursor(Qt::PointingHandCursor);
    btnReplay->setFocusPolicy(Qt::NoFocus);
    btnReplay->setAutoRepeat(false);
    btnReplay->setFixedSize(200, 48);
    btnReplay->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1a28; color: #00bfff; border: 1px solid #00bfff; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:pressed { background-color: #0099cc; color: #0c0c0c; }"
        "QPushButton:disabled { background-color: #111; color: #555; border: 1px solid #333; }")
    );

    auto *replayGlow = new QGraphicsDropShadowEffect(btnReplay);
    replayGlow->setBlurRadius(16);
    replayGlow->setColor(QColor(0, 191, 255, 140));
    replayGlow->setOffset(0, 0);
    btnReplay->setGraphicsEffect(replayGlow);

    QObject::connect(btnReplay, &QPushButton::clicked, this, [btnReplay]() {
        btnReplay->setEnabled(false);
#ifdef Q_OS_LINUX
        auto *thread = QThread::create([]() {
            led_show_problem();
        });
        QObject::connect(thread, &QThread::finished, btnReplay, [btnReplay]() {
            btnReplay->setEnabled(true);
        });
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
#else
        QTimer::singleShot(3000, btnReplay, [btnReplay]() {
            btnReplay->setEnabled(true);
        });
#endif
    });

    leftLayout->addSpacing(20);
    leftLayout->addWidget(btnReplay, 0, Qt::AlignCenter);
    leftLayout->addSpacing(14);

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
    rightLayout->addSpacing(16);

    // "INPUT" button — opens the team-name style popup
    auto *btnInput = new QPushButton(QStringLiteral("\u25b6 INPUT"), rightPanel);
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

    // "SUBMIT" button (created early so btnInput's lambda can reference it)
    auto *btnSubmit = new QPushButton(QStringLiteral("\u25b6 SUBMIT"), rightPanel);
    btnSubmit->setCursor(Qt::PointingHandCursor);
    btnSubmit->setFocusPolicy(Qt::NoFocus);
    btnSubmit->setAutoRepeat(false);
    btnSubmit->setFixedSize(200, 48);
    btnSubmit->setEnabled(s_operatorMode);
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

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit, btnInput]() {
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
            btnSubmit->setEnabled(true);
        }

        resetButtonHoverState(btnInput);
    });

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
        if (m_answerInputRaw.isEmpty() && !s_operatorMode) return;
        const bool correct = isAnswerAccepted(m_answerInputRaw);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
            btnSubmit->setEnabled(false);
        }
    });

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    actionRow->addStretch();
    actionRow->addWidget(btnInput);
    actionRow->addWidget(btnSubmit);
    actionRow->addStretch();
    rightLayout->addLayout(actionRow);

    rightLayout->addStretch();

    bodyRow->addWidget(rightPanel, 1);

    problemLayout->addLayout(bodyRow, 1);

    m_contentLayout->addWidget(problemPage);
}

// ═══════════════════════════════════════════════════════════════════════════
// Generic terminal popup with typing animation
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showTerminalPopup(const QString &title,
                                     const QStringList &lines,
                                     const QString &btnText,
                                     const QString &btnColor,
                                     const QColor &glowColor)
{
    if (m_missionNumber == 3) {
        stopMission3CameraPreview();
        refreshMission3PreviewPlaceholder();
    }

    auto *dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedSize(540, 360);
    dialog->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));

    auto *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Terminal frame ─────────────────────────────────────────────────
    auto *frame = new QFrame(dialog);
    frame->setObjectName(QStringLiteral("popupFrame"));
    frame->setStyleSheet(QStringLiteral(
        "QFrame#popupFrame { background-color: #0c0c0c; border: 1px solid #10b981; border-radius: 6px; }"
        "QFrame#popupFrame QPushButton { background-color: transparent; border: none; padding: 0; }"));
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(frame);
    titleBar->setFixedHeight(32);
    titleBar->setStyleSheet(QStringLiteral(
        "background-color: #1a1a2e; border: none; border-bottom: 1px solid #10b981;"
        "border-top-left-radius: 6px; border-top-right-radius: 6px;"));
    auto *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(12, 0, 12, 0);
    auto *titleLabel = new QLabel(title, titleBar);
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
    auto *closeBtnLayout = new QHBoxLayout(btnArea);
    closeBtnLayout->setContentsMargins(16, 0, 16, 16);

    auto *closeBtn = new QPushButton(btnText, btnArea);
    closeBtn->setFixedSize(200, 44);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setVisible(false);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1628; color: %1; border: 1px solid %1; "
        "border-radius: 6px; font-size: 17px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: %1; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: %1; color: #0c0c0c; }").arg(btnColor));

    auto *glow = new QGraphicsDropShadowEffect(closeBtn);
    glow->setBlurRadius(16);
    glow->setColor(glowColor);
    glow->setOffset(0, 0);
    closeBtn->setGraphicsEffect(glow);

    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    closeBtnLayout->addStretch();
    closeBtnLayout->addWidget(closeBtn);
    closeBtnLayout->addStretch();
    frameLayout->addWidget(btnArea);

    mainLayout->addWidget(frame);

    // ── Typing animation ───────────────────────────────────────────────
    auto *popupTimer = new QTimer(dialog);
    popupTimer->setSingleShot(true);
    auto lineIdx = std::make_shared<int>(0);

    QObject::connect(popupTimer, &QTimer::timeout, dialog,
        [textLabel, closeBtn, popupTimer, lines, lineIdx]() {
            const int idx = *lineIdx;
            if (idx >= lines.size()) {
                QString html = textLabel->text();
                html.replace(QStringLiteral("<span style='color:#00ff41;'>▌</span>"), QString());
                textLabel->setText(html);
                closeBtn->setVisible(true);
                return;
            }
            QString html;
            for (int i = 0; i <= idx; ++i) {
                const QString &line = lines.at(i);
                html += line.isEmpty() ? QStringLiteral("<br>")
                                       : (line + QStringLiteral("<br>"));
            }
            html += QStringLiteral("<span style='color:#00ff41;'>▌</span>");
            textLabel->setText(html);
            (*lineIdx)++;
            const int delays[] = { 500, 400, 300, 400, 400, 400, 300, 500 };
            popupTimer->start(delays[qMin(idx, 7)]);
        });

    // Center on screen
    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dialog->move(geo.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
    }

    // ── Dark overlay behind popup ──────────────────────────────────
    QWidget *topLevel = window();
    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    popupTimer->start(600);
    dialog->exec();

    overlay->hide();
    overlay->deleteLater();

    if (m_missionNumber == 3) {
        m_mission3StoryPopupDismissed = true;
        evaluateMission3CameraPreview();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Story popup — shown on mission entry
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showStoryPopup()
{
    if (s_operatorMode) {
        if (m_missionNumber == 3) {
            m_mission3StoryPopupDismissed = true;
            evaluateMission3CameraPreview();
        }
        return;
    }

    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;

    if (m_missionNumber == 1) {
        storyLines
            << QStringLiteral("<span style='color:#666; %1'>[17:20:44]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>[1단계 인증]</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:45]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:45]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>비콘 장애 원인을 조사합니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:46]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>손상된 장치에서 마지막 통신 로그가 복구되었습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:47]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>장애 직전 비콘이 수신한 신호 데이터가 남아있습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:49]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:49]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>LED 점멸 신호를 해독하여 1단계 인증 코드를 입력하십시오.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:50]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>신호는 PLAY 버튼 클릭 시 LED 2에 나타납니다.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SECURITY_TERMINAL.exe"),
            storyLines,
            QStringLiteral("\u25b6 PROCEED"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));

    } else if (m_missionNumber == 2) {
        storyLines
            << QStringLiteral("<span style='color:#666; %1'>[17:22:10]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>[2단계 인증]</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:12]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:12]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>2단계 인증은 청각 방식입니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:13]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:13]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>암호를 설정한 존재는 항상 여러분 곁에 있었습니다. </span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:14]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>그 존재는 LG인으로 첫 발을 내딛는 모습을 지켜봐 왔습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>멜로디를 듣고 알파벳으로 변환하여 입력하십시오.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SECURITY_TERMINAL.exe"),
            storyLines,
            QStringLiteral("\u25b6 PROCEED"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));
    } else if (m_missionNumber == 3) {
        storyLines
            << QStringLiteral("<span style='color:#666; %1'>[17:25:10]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>[3단계 인증]</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:11]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>비콘 부품 일부가 사라진 위치를 확인했습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:12]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>주변에 해바라기씨 껍질 다수 발견.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:13]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>용의자가 특정되고 있습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:14]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:15]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>3단계 인증 코드는 뒤섞인 부품에 담겨있습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>이를 다시 조립하여 스캔하십시오.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:25:17]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>행운을 기원합니다.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SECURITY_TERMINAL.exe"),
            storyLines,
            QStringLiteral("\u25b6 PROCEED"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));
    } else if (m_missionNumber == 4) {
        storyLines
            << QStringLiteral("<span style='color:#666; %1'>[17:27:10]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>[4단계 인증]</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:11]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>환풍구 이물질 확인됨.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:12]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>성분 분석 결과 : 해바라기씨 껍질.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:13]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>갓찌가 서버 환풍구를 막아놨습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:14]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>지금 이 순간에도 서버가 과열되고 있습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:15]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>4단계 인증은 온도 기반입니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:27:17]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>목표 범위 : 25.0°C ~ 25.5°C · 3초 유지</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SECURITY_TERMINAL.exe"),
            storyLines,
            QStringLiteral("\u25b6 PROCEED"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));
    } else if (m_missionNumber == 5) {
        storyLines
            << QStringLiteral("<span style='color:#666; %1'>[17:29:10]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>[5단계 인증] 최종 관문</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:29:11]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>갓찌가 마지막으로 설치한 물리 잠금장치입니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:29:12]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>단말기를 정확한 각도로 기울여 5초간 유지하면</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:29:13]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>시스템 복구 코드가 발급됩니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:29:14]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:29:15]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>각도가 흔들리면 카운트가 초기화됩니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:29:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>손이 떨리면 갓찌가 웃습니다.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SECURITY_TERMINAL.exe"),
            storyLines,
            QStringLiteral("\u25b6 PROCEED"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Result popup — correct / incorrect
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showResultPopup(bool correct)
{
    if (s_operatorMode) {
        emit missionCompleted(m_missionNumber);
        return;
    }

    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (m_missionNumber == 1) {
        if (correct) {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:21:14]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>입력값 대조 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:15]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>1단계 인증 성공</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>신호를 해독했습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:17]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>그런데... 비콘이 단순 장애라기엔 갉힌 흔적이 있습니다.</span>").arg(sf)

                << QStringLiteral("<span style='color:#666; %1'>[17:21:19]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:19]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>누군가 의도적으로 망가뜨린 것 같습니다.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 확인"),
                QStringLiteral("#00ff41"),
                QColor(0, 255, 65, 140));

            showImagePopup(
                QStringLiteral(":/images/mission1_complete.png"),
                QStringLiteral("\u25b6 확인"),
                QStringLiteral("#00ff41"),
                QColor(0, 255, 65, 140));

            emit missionCompleted(m_missionNumber);
        } else {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:21:14]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>입력값 대조 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:15]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>불일치 감지</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:15]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>1단계 인증 실패</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>입력한 코드가 등록된 신호와 일치하지 않습니다.</span>").arg(sf)

                << QStringLiteral("<span style='color:#666; %1'>[17:21:18]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:18]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>신호를 처음부터 다시 확인하십시오.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 다시 시도"),
                QStringLiteral("#ff4444"),
                QColor(255, 68, 68, 140));
        }
    } else if (m_missionNumber == 2) {
        if (correct) {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:23:30]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>키워드 일치 확인됨</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:31]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>2단계 인증 성공</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>멜로디를 해독했습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:33]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>단말기 주변에서 정체불명의 털 뭉치가 발견되었습니다.</span>").arg(sf)

                << QStringLiteral("<span style='color:#666; %1'>[17:23:35]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:35]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>교육 담당자 분들이 당황해 하고 있습니다.</span>").arg(sf);

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
                                  "<span style='color:#ff4444; %1'>음계 시퀀스 대조 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:31]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>키워드 불일치 감지</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:31]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>2단계 청각 인증 실패</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>입력한 알파벳 시퀀스가 등록된 키워드와 일치하지 않습니다.</span>").arg(sf)
    
                << QStringLiteral("<span style='color:#666; %1'>[17:23:34]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:34]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>멜로디를 다시 재생해보세요.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:35]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>음계와 알파벳 대응표를 재확인하십시오.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 다시 시도"),
                QStringLiteral("#ff4444"),
                QColor(255, 68, 68, 140));
        }
    } else if (m_missionNumber == 3) {
        if (correct) {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:26:20]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>QR 스캔 감지됨</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:21]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>물리 보안 토큰 일치</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:22]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>3단계 인증 성공</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:23]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:24]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>뒤섞인 부품을 잘 조립했습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:25]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>숨긴 장소가 너무 허술했습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:26]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>용의자가 자아비판 중인 것 같습니다.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 확인"),
                QStringLiteral("#00ff41"),
                QColor(0, 255, 65, 140));

            showImagePopup(
                QStringLiteral(":/images/mission3_complete.png"),
                QStringLiteral("\u25b6 확인"),
                QStringLiteral("#00ff41"),
                QColor(0, 255, 65, 140));

            emit missionCompleted(m_missionNumber);
        } else {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:26:20]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>QR 코드 스캔 감지</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:21]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>코드 유효성 검증 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:22]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>미등록 코드 확인됨</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:23]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>3단계 물리 보안 인증 실패</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:24]</span>&nbsp;&nbsp;"
                                  "<span style='color:#888; %1'>...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:25]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>스캔된 코드가 인증 코드와 일치하지 않습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:26:26]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>올바른 QR 코드로 다시 조합하십시오.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 다시 시도"),
                QStringLiteral("#ff4444"),
                QColor(255, 68, 68, 140));
        }
    } else if (m_missionNumber == 4) {
        if (correct) {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:28:20]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>서버 온도 모니터링 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:21]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>현재 온도 : 25.2°C</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:22]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>정상 범위 진입 확인</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:23]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>정상 범위 3초 유지 완료</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:24]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>냉각 시스템 재가동</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:25]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>서버 온도 정상화 완료</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:26]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>갓찌의 표정이 점점 어두워지고 있습니다.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 확인"),
                QStringLiteral("#00ff41"),
                QColor(0, 255, 65, 140));

            emit missionCompleted(m_missionNumber);
        } else {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:28:20]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>서버 온도 모니터링 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:21]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>현재 온도 : 23.1°C</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:22]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>정상 범위 미달</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:23]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>서버 온도가 허용 범위에 미치지 못하고 있습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:24]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>가온이 필요합니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:28:25]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>온도를 25.0°C ~ 25.5°C 범위로 높이십시오.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 다시 시도"),
                QStringLiteral("#ff4444"),
                QColor(255, 68, 68, 140));
        }
    } else if (m_missionNumber == 5) {
        if (correct) {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:30:20]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>물리 잠금장치 감지 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:21]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>기울기 각도 측정 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:22]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>목표 각도 진입 확인</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:23]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>5초 유지 완료</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:24]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>금고 잠금 해제됨</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:25]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>퇴근 코드 생성 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:26]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>전 단계 인증 완료</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 확인"),
                QStringLiteral("#00ff41"),
                QColor(0, 255, 65, 140));

            emit missionCompleted(m_missionNumber);
        } else {
            resultLines
                << QStringLiteral("<span style='color:#666; %1'>[17:30:20]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>기울기 각도 측정 중...</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:21]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>목표 각도 미진입</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:22]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>현재 각도가 잠금 해제 허용 범위에 도달하지 않았습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:30:23]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>화면의 게이지를 확인하며 단말기 각도를 조정하십시오.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
                QStringLiteral("\u25b6 다시 시도"),
                QStringLiteral("#ff4444"),
                QColor(255, 68, 68, 140));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Image popup — show a centered image with confirm button
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showImagePopup(const QString &imagePath,
                                  const QString &btnText,
                                  const QString &btnColor,
                                  const QColor &glowColor)
{
    if (m_missionNumber == 3) {
        stopMission3CameraPreview();
        refreshMission3PreviewPlaceholder();
    }

    auto *dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedSize(540, 360);
    dialog->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));

    auto *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *frame = new QFrame(dialog);
    frame->setObjectName(QStringLiteral("imgPopupFrame"));
    frame->setStyleSheet(QStringLiteral(
        "QFrame#imgPopupFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 6px; }"
        "QFrame#imgPopupFrame QPushButton { background-color: transparent; border: none; padding: 0; }"));
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(10, 10, 10, 10);
    frameLayout->setSpacing(8);

    auto *imageLabel = new QLabel(frame);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    QPixmap pix(imagePath);
    if (!pix.isNull()) {
        imageLabel->setPixmap(pix.scaled(500, 280, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imageLabel->setText(QStringLiteral("[ IMAGE ]"));
        imageLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 18px; background: transparent; border: none;"));
    }
    frameLayout->addWidget(imageLabel, 1);

    auto *btnArea = new QWidget(frame);
    btnArea->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: none;"));
    auto *btnLayout = new QHBoxLayout(btnArea);
    btnLayout->setContentsMargins(16, 0, 16, 8);

    auto *closeBtn = new QPushButton(btnText, btnArea);
    closeBtn->setFixedSize(200, 44);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setEnabled(false);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1628; color: %1; border: 1px solid %1; "
        "border-radius: 6px; font-size: 17px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: %1; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: %1; color: #0c0c0c; }").arg(btnColor));

    auto *glow = new QGraphicsDropShadowEffect(closeBtn);
    glow->setBlurRadius(16);
    glow->setColor(glowColor);
    glow->setOffset(0, 0);
    closeBtn->setGraphicsEffect(glow);

    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    // Delay-enable button to prevent accidental dismiss
    QTimer::singleShot(1000, closeBtn, [closeBtn]() {
        closeBtn->setEnabled(true);
    });

    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    frameLayout->addWidget(btnArea);

    mainLayout->addWidget(frame);

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dialog->move(geo.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
    }

    QWidget *topLevel = window();
    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    dialog->exec();

    overlay->hide();
    overlay->deleteLater();

    if (m_missionNumber == 3) {
        QTimer::singleShot(0, this, [this]() {
            evaluateMission3CameraPreview();
        });
    }
}

void MissionPage::showMission2HintPopup()
{
    if (m_missionNumber == 3) {
        stopMission3CameraPreview();
        refreshMission3PreviewPlaceholder();
    }

    auto *dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedSize(560, 420);
    dialog->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));

    auto *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *frame = new QFrame(dialog);
    frame->setObjectName(QStringLiteral("hintPopupFrame"));
    frame->setStyleSheet(QStringLiteral(
        "QFrame#hintPopupFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 6px; }"
        "QFrame#hintPopupFrame QLabel { background: transparent; border: none; }"
        "QFrame#hintPopupFrame QPushButton { padding: 0; }"));
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(16, 14, 16, 14);
    frameLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("AUDIO SCALE HINT"), frame);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "color: #eab308; font-size: 24px; font-weight: 900; "
        "font-family: 'Consolas', monospace;"));
    frameLayout->addWidget(titleLabel);

    auto *descLabel = new QLabel(QStringLiteral("멜로디를 음계로 나눠 듣고 아래 버튼 순서를 참고해 알파벳으로 변환하세요."), frame);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet(QStringLiteral(
        "color: #9ca3af; font-size: 15px; font-family: 'Consolas', monospace; line-height: 130%;"));
    frameLayout->addWidget(descLabel);

    auto *buttonPanel = new QWidget(frame);
    buttonPanel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *buttonGrid = new QGridLayout(buttonPanel);
    buttonGrid->setContentsMargins(8, 12, 8, 12);
    buttonGrid->setHorizontalSpacing(12);
    buttonGrid->setVerticalSpacing(12);

    const QStringList scaleLabels = {
        QStringLiteral("도"), QStringLiteral("레"), QStringLiteral("미"), QStringLiteral("파"),
        QStringLiteral("솔"), QStringLiteral("라"), QStringLiteral("시"), QStringLiteral("도")
    };

    for (int i = 0; i < scaleLabels.size(); ++i) {
        auto *noteButton = new QPushButton(scaleLabels.at(i), buttonPanel);
        noteButton->setFixedSize(110, 56);
        noteButton->setCursor(Qt::PointingHandCursor);
        noteButton->setFocusPolicy(Qt::NoFocus);
        noteButton->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #111827; color: #00ff41; border: 1px solid #00ff41; "
            "border-radius: 8px; font-size: 22px; font-weight: 800; "
            "font-family: 'Noto Sans KR', 'Consolas', monospace; }"
            "QPushButton:hover { background-color: #00ff41; color: #0c0c0c; }"
            "QPushButton:pressed { background-color: #00cc33; color: #0c0c0c; }")
        );

        auto *noteGlow = new QGraphicsDropShadowEffect(noteButton);
        noteGlow->setBlurRadius(14);
        noteGlow->setColor(QColor(0, 255, 65, 120));
        noteGlow->setOffset(0, 0);
        noteButton->setGraphicsEffect(noteGlow);

        QObject::connect(noteButton, &QPushButton::clicked, noteButton, [noteButton, i]() {
#ifdef Q_OS_LINUX
            noteButton->setEnabled(false);

            auto *thread = QThread::create([i]() {
                switch (i) {
                case 0: play_do(); break;
                case 1: play_re(); break;
                case 2: play_mi(); break;
                case 3: play_fa(); break;
                case 4: play_sol(); break;
                case 5: play_la(); break;
                case 6: play_si(); break;
                case 7: play_high_do(); break;
                default: break;
                }
            });

            QObject::connect(thread, &QThread::finished, noteButton, [noteButton]() {
                noteButton->setEnabled(true);
                resetButtonHoverState(noteButton);
            });
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            thread->start();
#else
            resetButtonHoverState(noteButton);
#endif
        });

        buttonGrid->addWidget(noteButton, i / 4, i % 4);
    }

    frameLayout->addWidget(buttonPanel, 1);

    auto *closeBtn = new QPushButton(QStringLiteral("▶ 닫기"), frame);
    closeBtn->setFixedSize(200, 44);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2a200a; color: #eab308; border: 1px solid #eab308; "
        "border-radius: 6px; font-size: 17px; font-weight: 800; font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: #eab308; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #ca8a04; color: #0c0c0c; }")
    );

    auto *closeGlow = new QGraphicsDropShadowEffect(closeBtn);
    closeGlow->setBlurRadius(16);
    closeGlow->setColor(QColor(234, 179, 8, 140));
    closeGlow->setOffset(0, 0);
    closeBtn->setGraphicsEffect(closeGlow);

    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    auto *closeRow = new QHBoxLayout();
    closeRow->setContentsMargins(0, 0, 0, 0);
    closeRow->addStretch();
    closeRow->addWidget(closeBtn);
    closeRow->addStretch();
    frameLayout->addLayout(closeRow);

    mainLayout->addWidget(frame);

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dialog->move(geo.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
    }

    QWidget *topLevel = window();
    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 180);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    dialog->exec();

    overlay->hide();
    overlay->deleteLater();

    if (m_missionNumber == 3) {
        QTimer::singleShot(0, this, [this]() {
            evaluateMission3CameraPreview();
        });
    }
}

void MissionPage::resetToStory()
{
    if (m_missionNumber >= 1 && m_missionNumber <= 5) {
        syncOperatorModeUi();
        if (m_missionNumber == 3) {
            m_mission3StoryPopupDismissed = false;
            m_mission3CapturedImagePath.clear();
            evaluateMission3CameraPreview();
        }
        if (s_operatorMode) {
            if (m_missionNumber == 3) {
                m_mission3StoryPopupDismissed = true;
                evaluateMission3CameraPreview();
            }
            return;
        }
        showStoryPopup();
    }
}

void MissionPage::setQrSubmitCallback(const std::function<void(const QByteArray &, const std::function<void(const QString &, const QString &)> &)> &cb)
{
    m_qrSubmitCb = cb;
}

void MissionPage::startMission()
{
    if (m_missionNumber >= 1 && m_missionNumber <= 5) {
        syncOperatorModeUi();
        if (m_missionNumber == 3) {
            m_mission3StoryPopupDismissed = false;
            m_mission3CapturedImagePath.clear();
            evaluateMission3CameraPreview();
        }
        if (s_operatorMode) {
            if (m_missionNumber == 3) {
                m_mission3StoryPopupDismissed = true;
                evaluateMission3CameraPreview();
            }
            return;
        }
        QTimer::singleShot(500, this, [this]() {
            showStoryPopup();
        });
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 2 — left border only, right answer input
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission2()
{
    m_correctAnswer = QStringLiteral("LUCKY");

    auto *problemPage = new QWidget(this);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(12, 6, 12, 6);
    problemLayout->setSpacing(6);

    // Header
    auto *problemHeader = new QLabel(QStringLiteral("[MISSION 2] - 보안 멜로디"), problemPage);
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

    // ── Body: left empty panel + right input ───────────────────────────
    auto *bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(12);

    // Left: mission image panel
    auto *leftPanel = new QWidget(problemPage);
    leftPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 14, 16, 14);
    leftLayout->setSpacing(0);

    auto *imageArea = new QWidget(leftPanel);
    imageArea->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(0, 0, 0, 0);
    imageAreaLayout->setSpacing(0);

    auto *imageLabel = new QLabel(imageArea);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    QPixmap missionPixmap(QStringLiteral(":/images/mission_2.png"));
    if (!missionPixmap.isNull()) {
        imageLabel->setPixmap(missionPixmap.scaled(430, 390, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imageLabel->setText(QStringLiteral("[ IMAGE ]"));
        imageLabel->setStyleSheet(QStringLiteral(
            "color: #666; font-size: 18px; background: transparent; border: none; "
            "min-width: 200px; min-height: 200px;"));
    }
            imageAreaLayout->addStretch();
            imageAreaLayout->addWidget(imageLabel, 0, Qt::AlignCenter);
            imageAreaLayout->addStretch();
            leftLayout->addWidget(imageArea, 1);

    // "PLAY" button — trigger buzzer melody (below placeholder)
    auto *btnPlay = new QPushButton(QStringLiteral("\u25b6 PLAY"), leftPanel);
    btnPlay->setCursor(Qt::PointingHandCursor);
    btnPlay->setFocusPolicy(Qt::NoFocus);
    btnPlay->setAutoRepeat(false);
    btnPlay->setFixedSize(160, 48);
    btnPlay->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1a28; color: #00bfff; border: 1px solid #00bfff; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:pressed { background-color: #0099cc; color: #0c0c0c; }"
        "QPushButton:disabled { background-color: #111; color: #555; border: 1px solid #333; }")
    );

    auto *playGlow = new QGraphicsDropShadowEffect(btnPlay);
    playGlow->setBlurRadius(16);
    playGlow->setColor(QColor(0, 191, 255, 140));
    playGlow->setOffset(0, 0);
    btnPlay->setGraphicsEffect(playGlow);

    auto *btnHint = new QPushButton(QStringLiteral("\u25b6 HINT"), leftPanel);
    btnHint->setCursor(Qt::PointingHandCursor);
    btnHint->setFocusPolicy(Qt::NoFocus);
    btnHint->setAutoRepeat(false);
    btnHint->setFixedSize(160, 48);
    btnHint->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2a200a; color: #eab308; border: 1px solid #eab308; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: #eab308; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #ca8a04; color: #0c0c0c; }")
    );

    auto *hintGlow = new QGraphicsDropShadowEffect(btnHint);
    hintGlow->setBlurRadius(16);
    hintGlow->setColor(QColor(234, 179, 8, 140));
    hintGlow->setOffset(0, 0);
    btnHint->setGraphicsEffect(hintGlow);

    QObject::connect(btnPlay, &QPushButton::clicked, this, [btnPlay]() {
        btnPlay->setEnabled(false);
#ifdef Q_OS_LINUX
        auto *thread = QThread::create([]() {
            buzzer_show_problem();
        });
        QObject::connect(thread, &QThread::finished, btnPlay, [btnPlay]() {
            btnPlay->setEnabled(true);
        });
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
#else
        QTimer::singleShot(3000, btnPlay, [btnPlay]() {
            btnPlay->setEnabled(true);
        });
#endif
    });

    QObject::connect(btnHint, &QPushButton::clicked, this, [this]() {
        showMission2HintPopup();
    });

    leftLayout->addSpacing(20);
    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(12);
    buttonRow->addStretch();
    buttonRow->addWidget(btnPlay);
    buttonRow->addWidget(btnHint);
    buttonRow->addStretch();
    leftLayout->addLayout(buttonRow);
    leftLayout->addSpacing(14);

    bodyRow->addWidget(leftPanel, 1);

    // Right: answer display + input button (same as mission 1)
    auto *rightPanel = new QWidget(problemPage);
    rightPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 14, 16, 14);
    rightLayout->setSpacing(12);
    rightLayout->setAlignment(Qt::AlignHCenter);

    rightLayout->addStretch();

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
    rightLayout->addSpacing(16);

    // INPUT button
    auto *btnInput = new QPushButton(QStringLiteral("\u25b6 INPUT"), rightPanel);
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

    // SUBMIT button
    auto *btnSubmit = new QPushButton(QStringLiteral("\u25b6 SUBMIT"), rightPanel);
    btnSubmit->setCursor(Qt::PointingHandCursor);
    btnSubmit->setFocusPolicy(Qt::NoFocus);
    btnSubmit->setAutoRepeat(false);
    btnSubmit->setFixedSize(200, 48);
    btnSubmit->setEnabled(s_operatorMode);
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

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit, btnInput]() {
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
            btnSubmit->setEnabled(true);
        }

        resetButtonHoverState(btnInput);
    });

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
        if (m_answerInputRaw.isEmpty() && !s_operatorMode) return;
        const bool correct = isAnswerAccepted(m_answerInputRaw);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
            btnSubmit->setEnabled(false);
        }
    });

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    actionRow->addStretch();
    actionRow->addWidget(btnInput);
    actionRow->addWidget(btnSubmit);
    actionRow->addStretch();
    rightLayout->addLayout(actionRow);

    rightLayout->addStretch();

    bodyRow->addWidget(rightPanel, 1);

    problemLayout->addLayout(bodyRow, 1);

    m_contentLayout->addWidget(problemPage);
}

void MissionPage::setupMission3()
{
    m_correctAnswer = QStringLiteral("18");

    auto *problemPage = new QWidget(this);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(12, 6, 12, 6);
    problemLayout->setSpacing(6);

    auto *problemHeader = new QLabel(QStringLiteral("[MISSION 3] - 뒤섞인 인증 코드 스캔"), problemPage);
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

    auto *bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(12);

    auto *leftPanel = new QWidget(problemPage);
    leftPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 14, 16, 14);
    leftLayout->setSpacing(0);

    auto *imageArea = new QWidget(leftPanel);
    imageArea->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(0, 0, 0, 0);
    imageAreaLayout->setSpacing(0);

    auto *imageLabel = new QLabel(QStringLiteral("[ LIVE CAMERA ]"), imageArea);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedSize(320, 240);
    imageLabel->setStyleSheet(QStringLiteral(
        "color: #00bfff; font-size: 18px; font-family: 'Consolas', monospace; "
        "background: #05070b; border: 1px dashed #00bfff; border-radius: 6px;"));

    m_mission3PreviewArea = imageLabel;

    imageAreaLayout->addStretch();
    imageAreaLayout->addWidget(imageLabel, 0, Qt::AlignCenter);
    imageAreaLayout->addStretch();
    leftLayout->addWidget(imageArea, 1);

    auto *btnCapture = new QPushButton(QStringLiteral("\u25b6 CAPTURE"), leftPanel);
    btnCapture->setCursor(Qt::PointingHandCursor);
    btnCapture->setFocusPolicy(Qt::NoFocus);
    btnCapture->setAutoRepeat(false);
    btnCapture->setFixedSize(200, 48);
    btnCapture->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1628; color: #00ff41; border: 1px solid #00ff41; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: #00ff41; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #00cc33; color: #0c0c0c; }"));

    auto *captureBtnGlow = new QGraphicsDropShadowEffect(btnCapture);
    captureBtnGlow->setBlurRadius(16);
    captureBtnGlow->setColor(QColor(0, 255, 65, 140));
    captureBtnGlow->setOffset(0, 0);
    btnCapture->setGraphicsEffect(captureBtnGlow);

    auto *captureStatus = new QLabel(QStringLiteral("스토리 팝업 종료 후 LIVE 시작"), leftPanel);
    captureStatus->setAlignment(Qt::AlignCenter);
    captureStatus->setWordWrap(true);
    captureStatus->setStyleSheet(QStringLiteral(
        "color: #7dd3fc; font-size: 13px; font-family: 'Consolas', monospace; "
        "background: transparent; border: none;"));
    captureStatus->setVisible(false);
    m_mission3CaptureStatus = captureStatus;

    auto *btnReset = new QPushButton(QStringLiteral("\u25b6 RESET"), leftPanel);
    btnReset->setCursor(Qt::PointingHandCursor);
    btnReset->setFocusPolicy(Qt::NoFocus);
    btnReset->setAutoRepeat(false);
    btnReset->setFixedSize(200, 48);
    btnReset->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1628; color: #00ff41; border: 1px solid #00ff41; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:hover { background-color: #00ff41; color: #0c0c0c; }"
        "QPushButton:pressed { background-color: #00cc33; color: #0c0c0c; }"));

    auto *resetBtnGlow = new QGraphicsDropShadowEffect(btnReset);
    resetBtnGlow->setBlurRadius(16);
    resetBtnGlow->setColor(QColor(0, 255, 65, 140));
    resetBtnGlow->setOffset(0, 0);
    btnReset->setGraphicsEffect(resetBtnGlow);

    leftLayout->addSpacing(8);
    leftLayout->addWidget(btnReset, 0, Qt::AlignHCenter);
    leftLayout->addSpacing(14);

    bodyRow->addWidget(leftPanel, 1);

    auto *rightPanel = new QWidget(problemPage);
    rightPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 14, 16, 14);
    rightLayout->setSpacing(12);
    rightLayout->setAlignment(Qt::AlignHCenter);

    rightLayout->addStretch();

    auto *answerTitle = new QLabel(QStringLiteral("캡처된 이미지는 하단에 표시됩니다."), rightPanel);
    answerTitle->setAlignment(Qt::AlignCenter);
    answerTitle->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 14px; font-family: 'Consolas', monospace; "
        "background: transparent; border: none;"));
    rightLayout->addWidget(answerTitle);

    auto *capturedImageLabel = new QLabel(QStringLiteral("[ CAPTURED IMAGE ]"), rightPanel);
    capturedImageLabel->setAlignment(Qt::AlignCenter);
    capturedImageLabel->setFixedSize(320, 240);
    capturedImageLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 18px; font-family: 'Consolas', monospace; "
        "background: #05070b; border: 1px dashed #00ff41; border-radius: 6px;"));
    rightLayout->addWidget(capturedImageLabel, 0, Qt::AlignCenter);
    rightLayout->addSpacing(16);

    auto *btnSubmit = new QPushButton(QStringLiteral("\u25b6 SUBMIT"), rightPanel);
    btnSubmit->setCursor(Qt::PointingHandCursor);
    btnSubmit->setFocusPolicy(Qt::NoFocus);
    btnSubmit->setAutoRepeat(false);
    btnSubmit->setFixedSize(200, 48);
    btnSubmit->setEnabled(true); // 카메라 없어도 QR fallback 가능
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

    QObject::connect(btnCapture, &QPushButton::clicked, this,
                     [this, btnCapture, btnSubmit, captureStatus, capturedImageLabel]() {
#ifdef Q_OS_LINUX
        evaluateMission3CameraPreview();
        if (!m_mission3CameraActive) {
            captureStatus->setText(QStringLiteral("카메라 시작 실패: 장치 상태를 확인하세요."));
            resetButtonHoverState(btnCapture);
            return;
        }

        btnCapture->setEnabled(false);
        captureStatus->setText(QStringLiteral("캡처 중..."));
        m_mission3CaptureInProgress = true;

        const QString capturePath = buildMission3CapturePath();
        auto *thread = QThread::create([this, btnCapture, btnSubmit, captureStatus, capturedImageLabel, capturePath]() {
            const QByteArray pathBytes = capturePath.toLocal8Bit();
            const int result = camera_capture(pathBytes.constData());

            QMetaObject::invokeMethod(this, [this, btnCapture, btnSubmit, captureStatus, capturedImageLabel, result, capturePath]() {
                btnCapture->setEnabled(true);
                resetButtonHoverState(btnCapture);
                m_mission3CaptureInProgress = false;

                if (result == 0) {
                    m_mission3CapturedImagePath = capturePath;
                    QPixmap capturedPixmap(capturePath);
                    if (!capturedPixmap.isNull()) {
                        capturedImageLabel->setPixmap(capturedPixmap.scaled(
                            capturedImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        capturedImageLabel->setText(QString());
                    }
                    captureStatus->setText(QStringLiteral("저장 완료"));
                    btnSubmit->setEnabled(true);
                } else {
                    captureStatus->setText(QStringLiteral("캡처 실패: 카메라 프레임 수신 오류"));
                    btnSubmit->setEnabled(false);
                }

                if (m_mission3PendingStop) {
                    stopMission3CameraPreview();
                    m_mission3PendingStop = false;
                } else {
                    evaluateMission3CameraPreview();
                }
            }, Qt::QueuedConnection);
        });

        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
#else
        captureStatus->setText(QStringLiteral("카메라 기능은 Linux 장치에서만 지원됩니다."));
        resetButtonHoverState(btnCapture);
#endif
    });

    QObject::connect(btnReset, &QPushButton::clicked, this,
                     [this, btnReset, capturedImageLabel, captureStatus, btnSubmit]() {
        if (m_mission3CaptureInProgress) {
            captureStatus->setText(QStringLiteral("캡처 중에는 초기화할 수 없습니다."));
            resetButtonHoverState(btnReset);
            return;
        }

        m_mission3CapturedImagePath.clear();
        capturedImageLabel->clear();
        capturedImageLabel->setText(QStringLiteral("[ CAPTURED IMAGE ]"));
        btnSubmit->setEnabled(s_operatorMode);
        captureStatus->setText(QStringLiteral("캡처 결과를 초기화했습니다."));
        resetButtonHoverState(btnReset);
    });

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, captureStatus]() {
        // 촬영된 이미지가 없으면 제출 불가
        if (m_mission3CapturedImagePath.isEmpty()) {
            captureStatus->setText(QStringLiteral("제출할 캡처 이미지를 먼저 촬영하세요."));
            return;
        }

        // 이미지 파일 읽기
        QFile imgFile(m_mission3CapturedImagePath);
        if (!imgFile.open(QIODevice::ReadOnly)) {
            captureStatus->setText(QStringLiteral("이미지 파일을 읽을 수 없습니다."));
            return;
        }
        QByteArray imageData = imgFile.readAll();
        imgFile.close();

        if (imageData.isEmpty()) {
            captureStatus->setText(QStringLiteral("이미지 파일이 비어있습니다."));
            return;
        }

        captureStatus->setText(QStringLiteral("QR 이미지를 서버로 전송 중..."));

        if (m_qrSubmitCb) {
            m_qrSubmitCb(imageData, [this, captureStatus](const QString &status, const QString &result) {
                QMetaObject::invokeMethod(this, [this, captureStatus, status, result]() {
                    if (status == QStringLiteral("correct")) {
                        captureStatus->setText(QStringLiteral("✅ 정답! QR 인증 성공"));
                        showResultPopup(true);
                    } else if (status == QStringLiteral("wrong")) {
                        captureStatus->setText(QStringLiteral("❌ 오답: ") + result);
                        showResultPopup(false);
                    } else {
                        // invalid
                        captureStatus->setText(QStringLiteral("⚠ 유효하지 않은 QR입니다. 다시 촬영해주세요."));
                        showResultPopup(false);
                    }
                }, Qt::QueuedConnection);
            });
        } else {
            captureStatus->setText(QStringLiteral("서버 연결이 필요합니다."));
        }
    });

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    actionRow->addStretch();
    actionRow->addWidget(btnCapture);
    actionRow->addWidget(btnSubmit);
    actionRow->addStretch();
    rightLayout->addLayout(actionRow);

    rightLayout->addStretch();

    bodyRow->addWidget(rightPanel, 1);
    problemLayout->addLayout(bodyRow, 1);

    m_contentLayout->addWidget(problemPage);
}

void MissionPage::setupMission4()
{
    m_correctAnswer = QStringLiteral("25.2");

    auto *problemPage = new QWidget(this);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(10, 4, 10, 4);
    problemLayout->setSpacing(4);

    auto *problemHeader = new QLabel(QStringLiteral("[MISSION 4] - 서버 온도 안정화"), problemPage);
    problemHeader->setAlignment(Qt::AlignCenter);
    problemHeader->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 22px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    problemLayout->addWidget(problemHeader);

    auto *divider = new QFrame(problemPage);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("background-color: #00ff41; max-height: 1px; border: none;"));
    problemLayout->addWidget(divider);
    problemLayout->addSpacing(2);

    auto *bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(10);

    auto *leftPanel = new QWidget(problemPage);
    leftPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 10, 12, 10);
    leftLayout->setSpacing(8);

    leftLayout->addStretch();

    auto *btnPlay = new QPushButton(QStringLiteral("\u25b6 PLAY"), leftPanel);
    btnPlay->setCursor(Qt::PointingHandCursor);
    btnPlay->setFocusPolicy(Qt::NoFocus);
    btnPlay->setAutoRepeat(false);
    btnPlay->setFixedSize(200, 48);
    btnPlay->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1a28; color: #00bfff; border: 1px solid #00bfff; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:pressed { background-color: #0099cc; color: #0c0c0c; }"
        "QPushButton:disabled { background-color: #111; color: #555; border: 1px solid #333; }")
    );

    auto *playGlow = new QGraphicsDropShadowEffect(btnPlay);
    playGlow->setBlurRadius(16);
    playGlow->setColor(QColor(0, 191, 255, 140));
    playGlow->setOffset(0, 0);
    btnPlay->setGraphicsEffect(playGlow);

    QObject::connect(btnPlay, &QPushButton::clicked, this, [btnPlay]() {
        btnPlay->setEnabled(false);
        QTimer::singleShot(3000, btnPlay, [btnPlay]() {
            btnPlay->setEnabled(true);
        });
    });

    leftLayout->addWidget(btnPlay, 0, Qt::AlignCenter);
    leftLayout->addStretch();

    bodyRow->addWidget(leftPanel, 1);

    auto *rightPanel = new QWidget(problemPage);
    rightPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(12, 10, 12, 10);
    rightLayout->setSpacing(8);
    rightLayout->setAlignment(Qt::AlignHCenter);

    rightLayout->addStretch();

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
    rightLayout->addSpacing(10);

    auto *btnInput = new QPushButton(QStringLiteral("\u25b6 INPUT"), rightPanel);
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

    auto *btnSubmit = new QPushButton(QStringLiteral("\u25b6 SUBMIT"), rightPanel);
    btnSubmit->setCursor(Qt::PointingHandCursor);
    btnSubmit->setFocusPolicy(Qt::NoFocus);
    btnSubmit->setAutoRepeat(false);
    btnSubmit->setFixedSize(200, 48);
    btnSubmit->setEnabled(s_operatorMode);
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

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit, btnInput]() {
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
            btnSubmit->setEnabled(true);
        }

        resetButtonHoverState(btnInput);
    });

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
        if (m_answerInputRaw.isEmpty() && !s_operatorMode) return;
        const bool correct = isAnswerAccepted(m_answerInputRaw);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
            btnSubmit->setEnabled(false);
        }
    });

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    actionRow->addStretch();
    actionRow->addWidget(btnInput);
    actionRow->addWidget(btnSubmit);
    actionRow->addStretch();
    rightLayout->addLayout(actionRow);

    rightLayout->addStretch();

    bodyRow->addWidget(rightPanel, 1);
    problemLayout->addLayout(bodyRow, 1);

    m_contentLayout->addWidget(problemPage);
}

void MissionPage::setupMission5()
{
    m_correctAnswer = QStringLiteral("5");

    auto *problemPage = new QWidget(this);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(12, 6, 12, 6);
    problemLayout->setSpacing(6);

    auto *problemHeader = new QLabel(QStringLiteral("[MISSION 5] - 최종 잠금 해제"), problemPage);
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

    auto *bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(12);

    auto *leftPanel = new QWidget(problemPage);
    leftPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 14, 16, 14);
    leftLayout->setSpacing(0);

    leftLayout->addStretch();

    auto *btnPlay = new QPushButton(QStringLiteral("\u25b6 PLAY"), leftPanel);
    btnPlay->setCursor(Qt::PointingHandCursor);
    btnPlay->setFocusPolicy(Qt::NoFocus);
    btnPlay->setAutoRepeat(false);
    btnPlay->setFixedSize(200, 48);
    btnPlay->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0a1a28; color: #00bfff; border: 1px solid #00bfff; "
        "border-radius: 6px; font-size: 18px; font-weight: 800; "
        "font-family: 'Consolas', monospace; }"
        "QPushButton:pressed { background-color: #0099cc; color: #0c0c0c; }"
        "QPushButton:disabled { background-color: #111; color: #555; border: 1px solid #333; }")
    );

    auto *playGlow = new QGraphicsDropShadowEffect(btnPlay);
    playGlow->setBlurRadius(16);
    playGlow->setColor(QColor(0, 191, 255, 140));
    playGlow->setOffset(0, 0);
    btnPlay->setGraphicsEffect(playGlow);

    QObject::connect(btnPlay, &QPushButton::clicked, this, [btnPlay]() {
        btnPlay->setEnabled(false);
        QTimer::singleShot(3000, btnPlay, [btnPlay]() {
            btnPlay->setEnabled(true);
        });
    });

    leftLayout->addSpacing(20);
    leftLayout->addWidget(btnPlay, 0, Qt::AlignCenter);
    leftLayout->addSpacing(14);

    bodyRow->addWidget(leftPanel, 1);

    auto *rightPanel = new QWidget(problemPage);
    rightPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 14, 16, 14);
    rightLayout->setSpacing(12);
    rightLayout->setAlignment(Qt::AlignHCenter);

    rightLayout->addStretch();

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
    rightLayout->addSpacing(16);

    auto *btnInput = new QPushButton(QStringLiteral("\u25b6 INPUT"), rightPanel);
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

    auto *btnSubmit = new QPushButton(QStringLiteral("\u25b6 SUBMIT"), rightPanel);
    btnSubmit->setCursor(Qt::PointingHandCursor);
    btnSubmit->setFocusPolicy(Qt::NoFocus);
    btnSubmit->setAutoRepeat(false);
    btnSubmit->setFixedSize(200, 48);
    btnSubmit->setEnabled(s_operatorMode);
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

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit, btnInput]() {
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
            btnSubmit->setEnabled(true);
        }

        resetButtonHoverState(btnInput);
    });

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
        if (m_answerInputRaw.isEmpty() && !s_operatorMode) return;
        const bool correct = isAnswerAccepted(m_answerInputRaw);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
            btnSubmit->setEnabled(false);
        }
    });

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    actionRow->addStretch();
    actionRow->addWidget(btnInput);
    actionRow->addWidget(btnSubmit);
    actionRow->addStretch();
    rightLayout->addLayout(actionRow);

    rightLayout->addStretch();

    bodyRow->addWidget(rightPanel, 1);
    problemLayout->addLayout(bodyRow, 1);

    m_contentLayout->addWidget(problemPage);
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
    if (m_missionNumber == 2) {
        setupMission2();
        return;
    }
    if (m_missionNumber == 3) {
        setupMission3();
        return;
    }
    if (m_missionNumber == 4) {
        setupMission4();
        return;
    }
    if (m_missionNumber == 5) {
        setupMission5();
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
