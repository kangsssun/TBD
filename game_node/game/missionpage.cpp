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

    // ── Problem page (image left, input+keypad right) ──────────────────
    auto *problemPage = new QWidget(this);
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
        imageLabel->setPixmap(missionPixmap.scaled(380, 350, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imageLabel->setText(QStringLiteral("[ IMAGE ]"));
        imageLabel->setStyleSheet(QStringLiteral(
            "color: #666; font-size: 18px; background: transparent; border: none; "
            "min-width: 200px; min-height: 200px;"));
    }
    leftLayout->addWidget(imageLabel);

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

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
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
    });

    auto *inputBtnRow = new QHBoxLayout();
    inputBtnRow->addStretch();
    inputBtnRow->addWidget(btnInput);
    inputBtnRow->addStretch();
    rightLayout->addLayout(inputBtnRow);

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
        if (m_answerInputRaw.isEmpty()) return;
        const bool correct = (m_answerInputRaw.trimmed().compare(
            m_correctAnswer, Qt::CaseInsensitive) == 0);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
            btnSubmit->setEnabled(false);
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
        "QFrame#popupFrame { background-color: #0c0c0c; border: 1px solid #444; border-radius: 6px; }"
        "QFrame#popupFrame QPushButton { background-color: transparent; border: none; padding: 0; }"));
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
}

// ═══════════════════════════════════════════════════════════════════════════
// Story popup — shown on mission entry
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showStoryPopup()
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;

    if (m_missionNumber == 1) {
        storyLines
            << QStringLiteral("<span style='color:#666; %1'>[17:20:44]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>[1단계 인증]</span>").arg(sf)
            << QString()
            << QStringLiteral("<span style='color:#666; %1'>[17:20:45]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>비콘 장애 원인을 조사합니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:46]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>손상된 장치에서 마지막 통신 로그가 복구되었습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:47]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>장애 직전 비콘이 수신한 신호 데이터가 남아있습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:48]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>LED 점멸 신호를 해독하여 1단계 인증 코드를 입력하십시오.</span>").arg(sf)
            << QString()
            << QStringLiteral("<span style='color:#666; %1'>[17:20:48]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>LED 점멸 신호를 해독하여 1단계 인증 코드를 입력하십시오.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:20:50]</span>&nbsp;&nbsp;"
                              "<span style='color:#00bfff; %1'>LED 점멸 신호는 PLAY 버튼 클릭 시 LED 2 부분에 나타납니다.</span>").arg(sf)

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
            << QString()
            << QStringLiteral("<span style='color:#666; %1'>[17:22:12]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>2단계 인증은 청각 방식입니다.</span>").arg(sf)
            << QString()
            << QStringLiteral("<span style='color:#666; %1'>[17:22:13]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>암호를 설정한 존재는 여러분 곁에서</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:14]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>LG인으로 첫 발을 내딱는 모습을 지켜봐 왓습니다.</span>").arg(sf)
            << QString()
            << QStringLiteral("<span style='color:#666; %1'>[17:22:16]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>멜로디를 듣고 알파벳으로 변환하여</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:22:17]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>입력하십시오.</span>").arg(sf);

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
                << QString()
                << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>신호를 해독했습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:17]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>그런데... 비콘이 단순 장애라기엔</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:18]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>갉힌 흔적이 있습니다.</span>").arg(sf)
                << QString()
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
                << QString()
                << QStringLiteral("<span style='color:#666; %1'>[17:21:16]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>입력한 코드가 등록된 신호와</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:21:17]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>일치하지 않습니다.</span>").arg(sf)
                << QString()
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
                << QString()
                << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>멜로디를 해독했습니다.</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:33]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>단말기 주변에서 정체불명의 털 뭉치가</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:34]</span>&nbsp;&nbsp;"
                                  "<span style='color:#00ff41; %1'>발견되었습니다.</span>").arg(sf)
                << QString()
                << QStringLiteral("<span style='color:#666; %1'>[17:23:35]</span>&nbsp;&nbsp;"
                                  "<span style='color:#eab308; %1'>교육 담당자 분들이 당황해 하고 있습니다.</span>").arg(sf);

            showTerminalPopup(
                QStringLiteral("SYSTEM_VERIFY.exe"),
                resultLines,
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
                << QString()
                << QStringLiteral("<span style='color:#666; %1'>[17:23:32]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>입력한 알파벳 시퀀스가 등록된</span>").arg(sf)
                << QStringLiteral("<span style='color:#666; %1'>[17:23:33]</span>&nbsp;&nbsp;"
                                  "<span style='color:#ff4444; %1'>키워드와 일치하지 않습니다.</span>").arg(sf)
                << QString()
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
}

// ═══════════════════════════════════════════════════════════════════════════
// Image popup — show a centered image with confirm button
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showImagePopup(const QString &imagePath,
                                  const QString &btnText,
                                  const QString &btnColor,
                                  const QColor &glowColor)
{
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
}

void MissionPage::resetToStory()
{
    if (m_missionNumber == 1) {
        showStoryPopup();
    }
}

void MissionPage::startMission()
{
    if (m_missionNumber == 1 || m_missionNumber == 2) {
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
    m_correctAnswer = QStringLiteral(""); // TODO: set correct answer

    auto *problemPage = new QWidget(this);
    problemPage->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *problemLayout = new QVBoxLayout(problemPage);
    problemLayout->setContentsMargins(12, 6, 12, 6);
    problemLayout->setSpacing(6);

    // Header
    auto *problemHeader = new QLabel(QStringLiteral("[MISSION] 2단계 인증"), problemPage);
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

    // Left: bordered empty panel (placeholder for mission 2 content)
    auto *leftPanel = new QWidget(problemPage);
    leftPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 14, 16, 14);
    leftLayout->setSpacing(0);
    leftLayout->setAlignment(Qt::AlignCenter);

    auto *placeholderLabel = new QLabel(QStringLiteral("[ MISSION 2 ]"), leftPanel);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet(QStringLiteral(
        "color: #444; font-size: 20px; font-weight: 600; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    leftLayout->addWidget(placeholderLabel);

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

    QObject::connect(btnInput, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
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
    });

    auto *inputBtnRow = new QHBoxLayout();
    inputBtnRow->addStretch();
    inputBtnRow->addWidget(btnInput);
    inputBtnRow->addStretch();
    rightLayout->addLayout(inputBtnRow);

    QObject::connect(btnSubmit, &QPushButton::clicked, this, [this, answerDisplay, btnSubmit]() {
        if (m_answerInputRaw.isEmpty()) return;
        const bool correct = (m_answerInputRaw.trimmed().compare(
            m_correctAnswer, Qt::CaseInsensitive) == 0);
        showResultPopup(correct);
        if (!correct) {
            m_answerInputRaw.clear();
            answerDisplay->setText(QStringLiteral("-"));
            btnSubmit->setEnabled(false);
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

    // Default placeholder
    auto *placeholder = new QLabel(
        QStringLiteral("MISSION %1 — 준비 중").arg(m_missionNumber), this);
    placeholder->setObjectName(QStringLiteral("eventDescriptionLabel"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    m_contentLayout->addWidget(placeholder, 1);
}
