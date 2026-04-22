#include "endingsequence.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QDialog>
#include <QApplication>
#include <QScreen>
#include <QPixmap>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

#include <memory>

// ════════════════════════════════════════════════════════════════════════════
// Public entry-point
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::show(QWidget *parent,
                          const QString &teamName,
                          int remainingSeconds,
                          int totalSeconds)
{
    qDebug() << "[ENDING] Ending sequence started";

    QWidget *topLevel = parent->window();
    const QSize screenSize = topLevel->size();

    showBootLog(topLevel, screenSize);
    showImageSequence(topLevel, screenSize);
    showExitConfirmation(topLevel);
    showRetrospective(topLevel);
    showRetrospectiveConfirm(topLevel);
    showMissionClear(topLevel, screenSize, teamName, remainingSeconds, totalSeconds);

    qDebug() << "[ENDING] Ending sequence completed";
}

// ════════════════════════════════════════════════════════════════════════════
// 1단계: 부팅 로그 화면
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::showBootLog(QWidget *topLevel, const QSize &screenSize)
{
    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose, false);
    dlg->setFixedSize(screenSize);
    dlg->move(0, 0);
    dlg->setStyleSheet(QStringLiteral("background-color: #000000;"));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(30, 20, 30, 20);
    layout->setSpacing(0);

    auto *textLabel = new QLabel(dlg);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setWordWrap(true);
    textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    textLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 15px; font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: 500; background: transparent; border: none; line-height: 150%;"));
    textLabel->setText(QString());
    layout->addWidget(textLabel, 1);

    QStringList bootLines;
    bootLines << QStringLiteral("<span style='color:#888'>[17:45:00]</span> Booting ESCAPE-OS v12.0...")
              << QStringLiteral("<span style='color:#888'>[17:45:00]</span> Beacon Repair....")
              << QStringLiteral("<span style='color:#888'>[17:45:01]</span> <span style='color:#00ff41'>[  OK  ]</span> Started LED Beacon Service")
              << QStringLiteral("<span style='color:#888'>[17:45:02]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Sound Authentication Module")
              << QStringLiteral("<span style='color:#888'>[17:45:03]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Camera QR Decoder")
              << QStringLiteral("<span style='color:#888'>[17:45:04]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Thermal Control Unit")
              << QStringLiteral("<span style='color:#888'>[17:45:05]</span> <span style='color:#00ff41'>[  OK  ]</span> Started Gyro Navigation System")
              << QStringLiteral("<span style='color:#888'>[17:45:06]</span> <span style='color:#ffcc00'>[ INFO ]</span> rmmod virus.ko ... <span style='color:#00ff41'>SUCCESS</span>")
              << QStringLiteral("<span style='color:#888'>[17:45:07]</span> <span style='color:#00ff41'>[  OK  ]</span> All security layers restored")
              << QStringLiteral("<span style='color:#888'>[17:45:08]</span> ...");

    auto *bootTimer = new QTimer(dlg);
    bootTimer->setSingleShot(true);
    auto lineIdx = std::make_shared<int>(0);

    QObject::connect(bootTimer, &QTimer::timeout, dlg,
        [textLabel, bootTimer, bootLines, lineIdx, dlg]() {
            const int idx = *lineIdx;
            if (idx >= bootLines.size()) {
                QTimer::singleShot(1500, dlg, &QDialog::accept);
                return;
            }
            QString html = textLabel->text();
            if (!html.isEmpty()) html += QStringLiteral("<br>");
            html += bootLines.at(idx);
            textLabel->setText(html);
            (*lineIdx)++;
            bootTimer->start(600);
        });

    bootTimer->start(800);
    dlg->exec();
    dlg->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════
// 2단계: 이미지 시퀀스
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::showImageSequence(QWidget *topLevel, const QSize &screenSize)
{
    // ending 이미지 경로 탐색
    QStringList searchDirs;
    searchDirs << QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/ending"))
               << QStringLiteral("/mnt/nfs/ending")
               << QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../ending"));
    QString endingDir;
    for (const QString &d : searchDirs) {
        if (QFileInfo::exists(d + QStringLiteral("/1.jpg"))) { endingDir = d; break; }
    }
    if (endingDir.isEmpty()) {
        qDebug() << "[ENDING] ending 이미지 디렉토리를 찾을 수 없습니다.";
        return;
    }
    qDebug() << "[ENDING] Using ending dir:" << endingDir;

    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose, false);
    dlg->setFixedSize(screenSize);
    dlg->move(0, 0);
    dlg->setStyleSheet(QStringLiteral("background-color: #000000;"));

    auto *mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *imageLabel = new QLabel(dlg);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet(QStringLiteral("background: black; border: none;"));
    mainLayout->addWidget(imageLabel, 1);

    // 현재 이미지 인덱스 (1-based)
    auto currentImg = std::make_shared<int>(1);

    auto loadImage = [imageLabel, endingDir, screenSize](int num) {
        QString imgPath = endingDir + QStringLiteral("/") + QString::number(num) + QStringLiteral(".jpg");
        QPixmap pix(imgPath);
        if (!pix.isNull()) {
            imageLabel->setPixmap(pix.scaled(screenSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            imageLabel->setText(QStringLiteral("[ Image %1 not found ]").arg(num));
            imageLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 24px; background: black; border: none;"));
        }
    };

    // 1번 이미지 표시
    loadImage(1);

    // 1번 이미지: 하단 1/4 클릭 시 다음으로
    auto *clickArea = new QPushButton(dlg);
    clickArea->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    clickArea->setCursor(Qt::PointingHandCursor);
    clickArea->setFocusPolicy(Qt::NoFocus);
    int clickH = screenSize.height() / 4;
    clickArea->setGeometry(0, screenSize.height() - clickH, screenSize.width(), clickH);
    clickArea->raise();

    // 자동 진행 타이머
    auto *autoTimer = new QTimer(dlg);
    autoTimer->setSingleShot(true);

    // 퇴실 버튼 (7번 이미지용) — 하단 중앙
    auto *exitBtn = new QPushButton(dlg);
    exitBtn->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    exitBtn->setCursor(Qt::PointingHandCursor);
    exitBtn->setFocusPolicy(Qt::NoFocus);
    exitBtn->setGeometry(0, screenSize.height() * 2 / 3,
                        screenSize.width(), screenSize.height() / 3);
    exitBtn->hide();

    // 확인 버튼 (8번 이미지용) — 우측
    auto *confirmBtn = new QPushButton(dlg);
    confirmBtn->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setFocusPolicy(Qt::NoFocus);
    confirmBtn->setGeometry(screenSize.width() / 2, screenSize.height() / 3,
                           screenSize.width() / 2, screenSize.height() / 3);
    confirmBtn->hide();

    // 이미지 전환 함수
    auto advanceImage = [=]() {
        int next = *currentImg + 1;
        if (next > 8) return;
        *currentImg = next;
        loadImage(next);

        // 버튼 제어
        clickArea->hide();
        exitBtn->hide();
        confirmBtn->hide();

        if (next >= 2 && next <= 6) {
            // 2~6: 1초마다 자동 진행
            autoTimer->start(1000);
        } else if (next == 7) {
            // 7번: 퇴실 버튼 표시
            exitBtn->show();
            exitBtn->raise();
        } else if (next == 8) {
            // 8번: 확인 버튼 표시
            confirmBtn->show();
            confirmBtn->raise();
        }
    };

    // 1번 이미지 클릭 → 2번으로
    QObject::connect(clickArea, &QPushButton::clicked, dlg, [=]() {
        if (*currentImg == 1) advanceImage();
    });

    // 자동 타이머 → 다음 이미지
    QObject::connect(autoTimer, &QTimer::timeout, dlg, [=]() {
        if (*currentImg >= 2 && *currentImg <= 6) advanceImage();
    });

    // 7번의 퇴실 버튼 → 8번으로
    QObject::connect(exitBtn, &QPushButton::clicked, dlg, [=]() {
        if (*currentImg == 7) advanceImage();
    });

    // 8번의 확인 버튼 → 퇴실 완료 팝업
    QObject::connect(confirmBtn, &QPushButton::clicked, dlg, [=]() {
        dlg->accept();
    });

    dlg->exec();
    dlg->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════
// 3단계: 퇴실 처리 팝업
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::showExitConfirmation(QWidget *topLevel)
{
    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedSize(420, 180);
    dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #00ff41;"));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto *msgLabel = new QLabel(QStringLiteral("퇴실 처리 되었습니다"), dlg);
    msgLabel->setAlignment(Qt::AlignCenter);
    msgLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 18px; font-weight: bold; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    layout->addWidget(msgLabel, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *btnOk = new QPushButton(QStringLiteral("확인"), dlg);
    btnOk->setFixedSize(100, 36);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setFocusPolicy(Qt::NoFocus);
    btnOk->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a1a2e; color: #00ff41; border: 1px solid #00ff41; "
        "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
        "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
    btnRow->addWidget(btnOk);
    layout->addLayout(btnRow);

    QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
    }

    dlg->exec();
    overlay->hide();
    overlay->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════
// 4단계: 회고 안내 (이벤트 스타일 팝업)
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::showRetrospective(QWidget *topLevel)
{
    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedSize(580, 440);

    auto *mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *frame = new QFrame(dlg);
    frame->setStyleSheet(QStringLiteral(
        "background-color: #1a0000; border: 3px solid #ff2222; border-radius: 8px;"));
    auto *glow = new QGraphicsDropShadowEffect(frame);
    glow->setBlurRadius(40);
    glow->setColor(QColor(255, 34, 34, 160));
    glow->setOffset(0, 0);
    frame->setGraphicsEffect(glow);

    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(24, 20, 24, 20);
    frameLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("⚠️ [긴급 공지] ⚠️"), frame);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "color: #ff2222; font-size: 22px; font-weight: 900; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    frameLayout->addWidget(titleLabel);

    auto *innerFrame = new QFrame(frame);
    innerFrame->setStyleSheet(QStringLiteral(
        "background-color: #0d0000; border: 1px solid #ff4444; border-radius: 4px;"));
    auto *innerLayout = new QVBoxLayout(innerFrame);
    innerLayout->setContentsMargins(20, 16, 20, 16);
    innerLayout->setSpacing(8);

    auto *contentLabel = new QLabel(innerFrame);
    contentLabel->setWordWrap(true);
    contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    contentLabel->setTextFormat(Qt::RichText);
    contentLabel->setText(QStringLiteral(
        "<p style='color:#ffffff; font-size:14px; font-family:Consolas; line-height:160%;'>"
        "퇴실처리 잊지 말고 꼭 해주세요! (17:20~ )<br><br>"
        "일일회고는 퇴근 여부와 상관 없이 모든 교육생 필수 참여입니다.<br><br>"
        "아래의 링크로 접속하셔서 참여해주시면 됩니다.<br>"
        "퇴실 전까지 작성 완료해주세요 :)<br><br>"
        "<span style='color:#ffcc00;'>[QR 코드 이미지]</span><br><br>"
        "성의 있는 답변 부탁드립니다.😊<br>"
        "감사합니다."
        "</p>"));
    contentLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    innerLayout->addWidget(contentLabel);

    frameLayout->addWidget(innerFrame, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *btnOk = new QPushButton(QStringLiteral("확인"), frame);
    btnOk->setFixedSize(120, 44);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setFocusPolicy(Qt::NoFocus);
    btnOk->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a0000; color: #ff4444; border: 2px solid #ff4444; "
        "border-radius: 6px; font-size: 16px; font-weight: 900; font-family: Consolas; }"
        "QPushButton:hover { background: #ff4444; color: #0c0c0c; }"));
    btnRow->addWidget(btnOk);
    frameLayout->addLayout(btnRow);

    mainLayout->addWidget(frame);

    QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
    }

    dlg->exec();
    overlay->hide();
    overlay->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════
// 5단계: "회고 작성하신거 맞죠?" 확인
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::showRetrospectiveConfirm(QWidget *topLevel)
{
    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedSize(420, 200);
    dlg->setStyleSheet(QStringLiteral("background-color: #0c0c0c; border: 2px solid #00ff41;"));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(16);

    auto *msgLabel = new QLabel(QStringLiteral("회고 작성하신거 맞죠? 그쵸? 😊"), dlg);
    msgLabel->setAlignment(Qt::AlignCenter);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 20px; font-weight: bold; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    layout->addWidget(msgLabel, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *btnOk = new QPushButton(QStringLiteral("네, 작성했습니다!"), dlg);
    btnOk->setFixedSize(180, 40);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setFocusPolicy(Qt::NoFocus);
    btnOk->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a1a2e; color: #00ff41; border: 1px solid #00ff41; "
        "border-radius: 4px; font-size: 14px; font-weight: bold; font-family: Consolas; }"
        "QPushButton:hover { background: #00ff41; color: #0c0c0c; }"));
    btnRow->addWidget(btnOk);
    layout->addLayout(btnRow);

    QObject::connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

    auto *overlay = new QWidget(topLevel);
    overlay->setStyleSheet(QStringLiteral("background-color: rgba(0, 0, 0, 200);"));
    overlay->setGeometry(topLevel->rect());
    overlay->show();
    overlay->raise();

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        dlg->move(geo.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
    }

    dlg->exec();
    overlay->hide();
    overlay->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════
// 6단계: Mission Clear 화면
// ════════════════════════════════════════════════════════════════════════════
void EndingSequence::showMissionClear(QWidget *topLevel, const QSize &screenSize,
                                      const QString &teamName, int remainingSeconds,
                                      int totalSeconds)
{
    int elapsed = totalSeconds - remainingSeconds;
    if (elapsed < 0) elapsed = 0;
    int clearMin = elapsed / 60;
    int clearSec = elapsed % 60;
    QString clearTime = QString::asprintf("%02d:%02d", clearMin, clearSec);

    auto *dlg = new QDialog(topLevel);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFixedSize(screenSize);
    dlg->move(0, 0);
    dlg->setStyleSheet(QStringLiteral("background-color: #000000;"));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addStretch(2);

    auto *titleLabel = new QLabel(QStringLiteral("🎉 Mission Clear! 🎉"), dlg);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 48px; font-weight: 900; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    layout->addWidget(titleLabel);

    layout->addSpacing(40);

    auto *teamLabel = new QLabel(QStringLiteral("팀명: ") + teamName, dlg);
    teamLabel->setAlignment(Qt::AlignCenter);
    teamLabel->setStyleSheet(QStringLiteral(
        "color: #ffffff; font-size: 32px; font-weight: bold; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    layout->addWidget(teamLabel);

    layout->addSpacing(20);

    auto *timeLabel = new QLabel(QStringLiteral("Clear Time: ") + clearTime, dlg);
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setStyleSheet(QStringLiteral(
        "color: #ffcc00; font-size: 36px; font-weight: bold; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    layout->addWidget(timeLabel);

    layout->addStretch(3);

    dlg->exec();
}
