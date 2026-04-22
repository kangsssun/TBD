#include "missionpage.h"
#include "missionpage_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QThread>
#include <QMetaObject>
#include <QFile>

#ifdef Q_OS_LINUX
#include "dd_api/camera_api.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Mission 3 — Camera capture + QR code scan
// ═══════════════════════════════════════════════════════════════════════════
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
    leftLayout->setSpacing(12);
    leftLayout->setAlignment(Qt::AlignHCenter);

    leftLayout->addStretch();

    auto *liveTitle = new QLabel(QStringLiteral("LIVE"), leftPanel);
    liveTitle->setAlignment(Qt::AlignCenter);
    liveTitle->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 14px; font-family: 'Consolas', monospace; "
        "background: transparent; border: none;"));
    leftLayout->addWidget(liveTitle);

    auto *imageLabel = new QLabel(QStringLiteral("[ LIVE CAMERA ]"), leftPanel);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedSize(320, 240);
    imageLabel->setStyleSheet(QStringLiteral(
        "color: #00bfff; font-size: 18px; font-family: 'Consolas', monospace; "
        "background: #05070b; border: 1px dashed #00bfff; border-radius: 6px;"));

    m_mission3PreviewArea = imageLabel;

    leftLayout->addWidget(imageLabel, 0, Qt::AlignCenter);
    leftLayout->addSpacing(16);

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

    auto *resetRow = new QHBoxLayout();
    resetRow->setContentsMargins(0, 0, 0, 0);
    resetRow->setSpacing(12);
    resetRow->addStretch();
    resetRow->addWidget(btnReset);
    resetRow->addStretch();
    leftLayout->addLayout(resetRow);

    leftLayout->addStretch();

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
    btnSubmit->setEnabled(false);
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
        btnSubmit->setEnabled(false);
        captureStatus->setText(QStringLiteral("캡처 결과를 초기화했습니다."));
        resetButtonHoverState(btnReset);
    });

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, btnSubmit, captureStatus]() {
        if (m_mission3CapturedImagePath.isEmpty()) {
            captureStatus->setText(QStringLiteral("제출할 캡처 이미지를 먼저 생성하세요."));
            return;
        }
        // 이미지 파일을 읽어서 서버로 QR 디코드 요청 전송
        QFile imgFile(m_mission3CapturedImagePath);
        if (!imgFile.open(QIODevice::ReadOnly)) {
            captureStatus->setText(QStringLiteral("이미지 파일을 열 수 없습니다."));
            return;
        }
        QByteArray imageData = imgFile.readAll();
        imgFile.close();

        if (!m_qrSubmitCb) {
            captureStatus->setText(QStringLiteral("서버 연결 콜백이 설정되지 않았습니다."));
            return;
        }

        btnSubmit->setEnabled(false);
        captureStatus->setText(QStringLiteral("QR 코드 분석 중..."));
        captureStatus->setVisible(true);

        m_qrSubmitCb(imageData, [this, btnSubmit, captureStatus](const QString &status, const QString &result) {
            Q_UNUSED(result);
            QMetaObject::invokeMethod(this, [this, btnSubmit, captureStatus, status]() {
                btnSubmit->setEnabled(true);
                captureStatus->setVisible(true);
                if (status == QStringLiteral("correct")) {
                    captureStatus->setText(QStringLiteral("✅ 정답! QR 인증 성공"));
                    showResultPopup(true);
                } else {
                    captureStatus->setText(QStringLiteral("❌ QR 인증 실패. 다시 시도하세요."));
                    showResultPopup(false);
                }
            }, Qt::QueuedConnection);
        });
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

// ═══════════════════════════════════════════════════════════════════════════
// Mission 3 — Story popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission3Story()
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;
    storyLines
        << QStringLiteral("<span style='color:#666; %1'>[17:25:10]</span>&nbsp;&nbsp;"
                          "<span style='color:#ff4444; %1'>[4단계 인증] 관리자 권한 토큰 복원</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:25:11]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>퇴실 권한을 얻기 위한 물리 보안 토큰(QR)이 파손되어 있습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:25:12]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>조각난 부품 주변에 아주 작은 '발자국'들이 찍혀있습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:25:13]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>용의자가 자신의 정체를 숨기기 위해 부품을 섞어버렸습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:25:14]</span>&nbsp;&nbsp;"
                          "<span style='color:#888; %1'>...</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:25:15]</span>&nbsp;&nbsp;"
                          "<span style='color:#00bfff; %1'>뒤섞인 QR 코드를 올바르게 조립하고 카메라 모듈로 스캔하십시오.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:25:16]</span>&nbsp;&nbsp;"
                          "<span style='color:#00bfff; %1'>권한을 얻으면 서버실 내부 CCTV를 확인할 수 있습니다.</span>").arg(sf);

    showTerminalPopup(
        QStringLiteral("SECURITY_TERMINAL.exe"),
        storyLines,
        QStringLiteral("\u25b6 확인"),
        QStringLiteral("#00ff41"),
        QColor(0, 255, 65, 140));
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 3 — Result popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission3Result(bool correct)
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (correct) {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:26:20]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>관리자 물리 보안 토큰 일치</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:21]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>4단계 인증 성공 / 관리자 권한 획득</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:22]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:23]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>내부 CCTV 영상이 복구되었습니다. 영상을 확인합니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:24]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>전선을 갉아먹고, 해바라기씨를 숨기고, 부품을 부수던 범인은...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:25]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>부트캠프 마스코트, 햄스터 '갓찌'입니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:26]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>수료식 날, 떠나는 교육생들과 헤어지기 싫었던 갓찌의 아쉬움이 빚어낸 소동이었습니다.</span>").arg(sf);

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
                              "<span style='color:#ff4444; %1'>카메라 모듈 QR 스캔 감지</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:21]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>코드 유효성 검증 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:22]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>미등록 코드 확인됨</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:23]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>4단계 인증 실패</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:24]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:26:25]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>올바른 배열로 다시 조합하여 스캔하십시오.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 다시 시도"),
            QStringLiteral("#ff4444"),
            QColor(255, 68, 68, 140));
    }
}
