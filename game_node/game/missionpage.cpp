#include "missionpage.h"

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
#include <QEvent>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QShowEvent>
#include <QHideEvent>

#ifdef Q_OS_LINUX
#include "dd_api/camera_api.h"
#include "dd_api/led_api.h"
#endif

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
    // Reserved for future operator-mode UI tweaks.
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

    // 카메라 초기화 실패 시 3초 쿨다운
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
            m_mission3CaptureStatus->clear();
        } else if (popupOpen) {
            m_mission3CaptureStatus->clear();
        } else if (m_mission3CameraActive) {
            m_mission3CaptureStatus->clear();
        } else {
            m_mission3CaptureStatus->clear();
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
    const QString normalized = answer.trimmed();
    return normalized.compare(m_correctAnswer, Qt::CaseInsensitive) == 0;
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
        "QFrame#popupFrame { background-color: #0c0c0c; border: 1px solid #10b981; border-radius: 0px; }"
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

    QObject::connect(closeBtn, &QPushButton::clicked, dialog, [dialog]() {
        QTimer::singleShot(100, dialog, &QDialog::accept);
    });

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

    // Flush pending mouse/touch events to prevent click-through
    // to widgets behind the popup (e.g. INPUT button on mission 1)
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QApplication::processEvents(QEventLoop::AllEvents, 50);

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
    if (m_missionNumber == 3) {
        m_mission3StoryPopupDismissed = true;
        evaluateMission3CameraPreview();
    }

    // 운영자 모드에서도 스토리 팝업은 표시 (스킵 제거)

    switch (m_missionNumber) {
    case 1: showMission1Story(); break;
    case 2: showMission2Story(); break;
    case 3: showMission3Story(); break;
    case 4: showMission4Story(); break;
    case 5: showMission5Story(); break;
    default: break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Result popup — correct / incorrect (dispatches to per-mission implementation)
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showResultPopup(bool correct)
{
    if (correct) {
        led_correct();
    }
    switch (m_missionNumber) {
    case 1: showMission1Result(correct); break;
    case 2: showMission2Result(correct); break;
    case 3: showMission3Result(correct); break;
    case 4: showMission4Result(correct); break;
    case 5: showMission5Result(correct); break;
    default: break;
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
        "QFrame#imgPopupFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 0px; }"
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

    QObject::connect(closeBtn, &QPushButton::clicked, dialog, [dialog]() {
        QTimer::singleShot(100, dialog, &QDialog::accept);
    });

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

void MissionPage::resetToStory()
{
    if (m_missionNumber >= 1 && m_missionNumber <= 5) {
        syncOperatorModeUi();
        if (m_missionNumber == 3) {
            m_mission3StoryPopupDismissed = false;
            m_mission3CapturedImagePath.clear();
            evaluateMission3CameraPreview();
        }
        showStoryPopup();
    }
}

void MissionPage::setQrSubmitCallback(const std::function<void(const QByteArray &, const std::function<void(const QString &, const QString &)> &)> &cb)
{
    m_qrSubmitCb = cb;
}

void MissionPage::onFrequencyChanged(int value)
{
    Q_UNUSED(value);
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
        QTimer::singleShot(500, this, [this]() {
            showStoryPopup();
            emit missionProblemShown(m_missionNumber);
        });
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
}

