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
#include <QDialog>
#include <QGridLayout>
#include <QApplication>
#include <QScreen>

#ifdef Q_OS_LINUX
#include "dd_api/buzzer_api.h"
#endif

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
// Mission 2 — Hint popup (audio scale reference)
// ═══════════════════════════════════════════════════════════════════════════
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
        "QFrame#hintPopupFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 0px; }"
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
}
