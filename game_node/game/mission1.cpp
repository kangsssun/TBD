#include "missionpage.h"
#include "missionpage_utils.h"
#include "title/teamnamedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QThread>

#ifdef Q_OS_LINUX
#include "dd_api/led_api.h"
#endif

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
    QPixmap missionPixmap(QStringLiteral(":/images/mission_1.png"));
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
        if (m_answerInputRaw.isEmpty()) return;
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
// Mission 1 — Story popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission1Story()
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;
    storyLines
        << QStringLiteral("<span style='color:#666; %1'>[17:20:44]</span>&nbsp;&nbsp;"
                          "<span style='color:#ff4444; %1'>[1단계 인증] 시스템 로그 분석</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:45]</span>&nbsp;&nbsp;"
                          "<span style='color:#888; %1'>...</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:45]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>먼저 비콘 통신 장애의 원인을 조사해야 합니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:46]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>메인 서버에서 마지막 통신 로그를 확보했습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:47]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>장애 직전 비콘이 수신한 암호화된 신호가 남아있습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:49]</span>&nbsp;&nbsp;"
                          "<span style='color:#888; %1'>...</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:49]</span>&nbsp;&nbsp;"
                          "<span style='color:#00bfff; %1'>PLAY 버튼을 눌러 LED 점멸 신호(모스부호)를 확인하고</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:20:50]</span>&nbsp;&nbsp;"
                          "<span style='color:#00bfff; %1'>해독된 1단계 인증 코드를 입력하십시오.</span>").arg(sf);

    showTerminalPopup(
        QStringLiteral("SECURITY_TERMINAL.exe"),
        storyLines,
        QStringLiteral("\u25b6 확인"),
        QStringLiteral("#00ff41"),
        QColor(0, 255, 65, 140));
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 1 — Result popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission1Result(bool correct)
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (correct) {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:21:14]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>입력값 대조 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:15]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>1단계 인증 성공</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>통신 로그 복호화가 완료되었습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:17]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>단순한 소프트웨어 오류가 아닙니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:18]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>비콘 메인보드 전선이 예리하게 절단되어 있습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:19]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>누군가... 아니, '무언가'가 이빨로 갉아먹은 흔적입니다.</span>").arg(sf);

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
                              "<span style='color:#ff4444; %1'>해독된 코드가 시스템 로그와 일치하지 않습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:18]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:21:18]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>신호를 처음부터 다시 분석하십시오.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 다시 시도"),
            QStringLiteral("#ff4444"),
            QColor(255, 68, 68, 140));
    }
}
