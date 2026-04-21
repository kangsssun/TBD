#include "missionpage.h"

#include <functional>
#include <cmath>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSlider>
#include <QBrush>
#include <QPen>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QFont>
#include <QtMath>

#ifdef Q_OS_LINUX
extern "C" {
#include "dd_api/zyro_api.h"
}
#endif

// ════════════════════════════════════════════════════════════════════════════
// Mission 5 — Gyro Laser-Maze Escape
// ════════════════════════════════════════════════════════════════════════════

namespace {

// ── Scene dimensions ────────────────────────────────────────────────────
constexpr double SCENE_W  = 900.0;
constexpr double SCENE_H  = 400.0;
constexpr double WALL_T   = 6.0;      // wall / laser thickness
constexpr double PLAYER_R = 8.0;      // player radius
constexpr double GOAL_SIZE = 30.0;

// ── Physics constants ───────────────────────────────────────────────────
constexpr double ACCEL_SCALE  = 0.005;
constexpr double FRICTION     = 0.88;
constexpr double MAX_SPEED    = 2.5;
constexpr int    TICK_MS      = 16;    // ~60 FPS

// ── Start / Goal positions ──────────────────────────────────────────────
constexpr double START_X = 30.0;
constexpr double START_Y = 30.0;
constexpr double GOAL_X  = SCENE_W - GOAL_SIZE - 15.0;
constexpr double GOAL_Y  = SCENE_H - GOAL_SIZE - 15.0;

// ─────────────────────────────────────────────────────────────────────────
// Player (data-core ball)
// ─────────────────────────────────────────────────────────────────────────
class Player : public QGraphicsEllipseItem
{
public:
    double vx = 0.0;
    double vy = 0.0;

    explicit Player(QGraphicsItem *parent = nullptr)
        : QGraphicsEllipseItem(-PLAYER_R, -PLAYER_R,
                               PLAYER_R * 2, PLAYER_R * 2, parent)
    {
        QRadialGradient grad(0, 0, PLAYER_R);
        grad.setColorAt(0.0, QColor(120, 220, 255, 255));
        grad.setColorAt(0.6, QColor(0, 140, 255, 200));
        grad.setColorAt(1.0, QColor(0, 60, 180, 120));
        setBrush(QBrush(grad));
        setPen(QPen(QColor(0, 200, 255, 180), 1.2));
        setZValue(10);
    }

    void resetTo(double x, double y)
    {
        vx = 0.0;
        vy = 0.0;
        setPos(x, y);
    }
};

// ─────────────────────────────────────────────────────────────────────────
// Goal zone
// ─────────────────────────────────────────────────────────────────────────
class Goal : public QGraphicsRectItem
{
public:
    Goal(double x, double y, double size,
         QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(x, y, size, size, parent)
    {
        QRadialGradient grad(x + size / 2, y + size / 2, size / 2);
        grad.setColorAt(0.0, QColor(0, 255, 65, 200));
        grad.setColorAt(0.7, QColor(0, 200, 40, 140));
        grad.setColorAt(1.0, QColor(0, 120, 20, 80));
        setBrush(QBrush(grad));
        setPen(QPen(QColor(0, 255, 65, 160), 1));
        setZValue(3);
    }
};

// ─────────────────────────────────────────────────────────────────────────
// MazeGameWidget – the full game panel
// ─────────────────────────────────────────────────────────────────────────
class MazeGameWidget : public QWidget
{
public:
    explicit MazeGameWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_scene(new QGraphicsScene(0, 0, SCENE_W, SCENE_H, this))
        , m_view(new QGraphicsView(m_scene, this))
        , m_player(new Player)
        , m_goal(new Goal(GOAL_X, GOAL_Y, GOAL_SIZE))
        , m_gameTimer(new QTimer(this))
        , m_zyroTimer(nullptr)
        , m_zyroReady(false)
        , m_completed(false)
        , m_pitchInput(0.0)
        , m_rollInput(0.0)
        , m_zyroLabel(nullptr)
        , m_sliderX(nullptr)
        , m_sliderY(nullptr)
    {
        buildUi();
        buildMaze();

        m_player->resetTo(START_X, START_Y);
        m_scene->addItem(m_player);
        m_scene->addItem(m_goal);

        QObject::connect(m_gameTimer, &QTimer::timeout, this, [this]() {
            updateGame();
        });

        // Ensure fitInView fires after layout is settled
        QTimer::singleShot(0, this, [this]() { fitViewToScene(); });

        startGame();
    }

    ~MazeGameWidget() override
    {
        stopZyroPolling();
    }

    // Callback: set by MissionPage to trigger showResultPopup
    std::function<void()> onGoalReached;

private:
    QGraphicsScene *m_scene;
    QGraphicsView  *m_view;
    Player         *m_player;
    Goal           *m_goal;
    QTimer         *m_gameTimer;
    QTimer         *m_zyroTimer;
    bool            m_zyroReady;
    bool            m_completed;
    double          m_pitchInput;
    double          m_rollInput;
    QLabel         *m_zyroLabel;
    QSlider        *m_sliderX;
    QSlider        *m_sliderY;

    // ── UI construction ─────────────────────────────────────────────
    void buildUi()
    {
        setStyleSheet(QStringLiteral("background: transparent; border: none;"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(4);

        m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setStyleSheet(QStringLiteral(
            "QGraphicsView { background: #05070b; border: 1px solid #1a3a1a; "
            "border-radius: 4px; }"));
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setAlignment(Qt::AlignCenter);
        m_scene->setBackgroundBrush(QColor("#05070b"));

        root->addWidget(m_view, 1);

        // status bar
        auto *statusRow = new QHBoxLayout();
        statusRow->setContentsMargins(4, 0, 4, 0);

        m_zyroLabel = new QLabel(QStringLiteral("ZYRO: X=0  Y=0"), this);
        m_zyroLabel->setAlignment(Qt::AlignRight);
        m_zyroLabel->setStyleSheet(QStringLiteral(
            "color: #7dd3fc; font-size: 12px; font-weight: 600; "
            "font-family: 'Consolas', monospace; background: transparent; border: none;"));
        statusRow->addWidget(m_zyroLabel);
        root->addLayout(statusRow);

        // ── Slider mock (PC testing) ────────────────────────────────
#ifndef Q_OS_LINUX
        auto *sliderRow = new QHBoxLayout();
        sliderRow->setContentsMargins(4, 0, 4, 2);
        sliderRow->setSpacing(8);

        auto *lblX = new QLabel(QStringLiteral("Pitch(X)"), this);
        lblX->setStyleSheet(QStringLiteral(
            "color: #888; font-size: 11px; font-family: Consolas; "
            "background: transparent; border: none;"));
        sliderRow->addWidget(lblX);

        m_sliderX = new QSlider(Qt::Horizontal, this);
        m_sliderX->setRange(-90, 90);
        m_sliderX->setValue(0);
        m_sliderX->setFixedHeight(20);
        m_sliderX->setStyleSheet(sliderStyle());
        sliderRow->addWidget(m_sliderX, 1);

        auto *lblY = new QLabel(QStringLiteral("Roll(Y)"), this);
        lblY->setStyleSheet(QStringLiteral(
            "color: #888; font-size: 11px; font-family: Consolas; "
            "background: transparent; border: none;"));
        sliderRow->addWidget(lblY);

        m_sliderY = new QSlider(Qt::Horizontal, this);
        m_sliderY->setRange(-90, 90);
        m_sliderY->setValue(0);
        m_sliderY->setFixedHeight(20);
        m_sliderY->setStyleSheet(sliderStyle());
        sliderRow->addWidget(m_sliderY, 1);

        root->addLayout(sliderRow);

        QObject::connect(m_sliderX, &QSlider::valueChanged, this, [this](int v) {
            m_pitchInput = static_cast<double>(v);
            m_zyroLabel->setText(QStringLiteral("ZYRO: X=%1  Y=%2")
                .arg(static_cast<int>(m_pitchInput))
                .arg(static_cast<int>(m_rollInput)));
        });
        QObject::connect(m_sliderY, &QSlider::valueChanged, this, [this](int v) {
            m_rollInput = static_cast<double>(v);
            m_zyroLabel->setText(QStringLiteral("ZYRO: X=%1  Y=%2")
                .arg(static_cast<int>(m_pitchInput))
                .arg(static_cast<int>(m_rollInput)));
        });
#endif

    }

    void fitViewToScene()
    {
        if (m_view && m_scene) {
            m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
        }
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        fitViewToScene();
    }

    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        QTimer::singleShot(0, this, [this]() { fitViewToScene(); });
    }

    static QString sliderStyle()
    {
        return QStringLiteral(
            "QSlider::groove:horizontal { background: #1a1a2e; height: 6px; "
            "border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #00bfff; width: 14px; "
            "margin: -4px 0; border-radius: 7px; }"
            "QSlider::sub-page:horizontal { background: #00546e; "
            "border-radius: 3px; }");
    }

    // ── Maze layout ─────────────────────────────────────────────────
    void buildMaze()
    {
        // Boundary walls (visual only — clamping handles collision)
        auto addWall = [this](double x, double y, double w, double h) {
            auto *wall = new QGraphicsRectItem(x, y, w, h);
            wall->setBrush(QBrush(QColor(0, 255, 65, 60)));
            wall->setPen(QPen(QColor(0, 255, 65, 100), 0.5));
            wall->setZValue(5);
            m_scene->addItem(wall);
        };
        addWall(0, 0, SCENE_W, WALL_T);                           // top
        addWall(0, SCENE_H - WALL_T, SCENE_W, WALL_T);           // bottom
        addWall(0, 0, WALL_T, SCENE_H);                           // left
        addWall(SCENE_W - WALL_T, 0, WALL_T, SCENE_H);           // right

        // ── Decorative grid lines (faint) ───────────────────────────
        for (double gx = 60; gx < SCENE_W; gx += 60) {
            auto *line = new QGraphicsRectItem(gx, 0, 0.5, SCENE_H);
            line->setBrush(Qt::NoBrush);
            line->setPen(QPen(QColor(0, 255, 65, 15), 0.5));
            line->setZValue(1);
            m_scene->addItem(line);
        }
        for (double gy = 60; gy < SCENE_H; gy += 60) {
            auto *line = new QGraphicsRectItem(0, gy, SCENE_W, 0.5);
            line->setBrush(Qt::NoBrush);
            line->setPen(QPen(QColor(0, 255, 65, 15), 0.5));
            line->setZValue(1);
            m_scene->addItem(line);
        }

        // ── Labels ──────────────────────────────────────────────────
        auto *startLabel = new QGraphicsTextItem(QStringLiteral("START"));
        startLabel->setDefaultTextColor(QColor(0, 191, 255, 160));
        startLabel->setFont(QFont("Consolas", 9, QFont::Bold));
        startLabel->setPos(START_X - 8, START_Y + PLAYER_R + 4);
        startLabel->setZValue(2);
        m_scene->addItem(startLabel);

        auto *goalLabel = new QGraphicsTextItem(QStringLiteral("GOAL"));
        goalLabel->setDefaultTextColor(QColor(0, 255, 65, 180));
        goalLabel->setFont(QFont("Consolas", 9, QFont::Bold));
        goalLabel->setPos(GOAL_X + 2, GOAL_Y - 16);
        goalLabel->setZValue(2);
        m_scene->addItem(goalLabel);
    }

    // ── Start / Stop ────────────────────────────────────────────────
    void startGame()
    {
        m_completed = false;
        m_player->resetTo(START_X, START_Y);
        m_gameTimer->start(TICK_MS);
        startZyroPolling();
    }

    void startZyroPolling()
    {
#ifdef Q_OS_LINUX
        if (board_sync() != 0) {
            if (m_zyroLabel)
                m_zyroLabel->setText(QStringLiteral("ZYRO: SYNC FAILED"));
            return;
        }
        if (init_event_thread() != 0) {
            if (m_zyroLabel)
                m_zyroLabel->setText(QStringLiteral("ZYRO: THREAD FAILED"));
            return;
        }
        m_zyroReady = true;
        m_zyroTimer = new QTimer(this);
        m_zyroTimer->setInterval(50);  // 20 Hz polling
        QObject::connect(m_zyroTimer, &QTimer::timeout, this, [this]() {
            int x = 0, y = 0;
            if (zyro_get_value(&x, &y) == 0) {
                m_pitchInput = static_cast<double>(x);
                m_rollInput  = static_cast<double>(y);
                if (m_zyroLabel)
                    m_zyroLabel->setText(
                        QStringLiteral("ZYRO: X=%1  Y=%2").arg(x).arg(y));
            }
        });
        m_zyroTimer->start();
#endif
    }

    void stopZyroPolling()
    {
#ifdef Q_OS_LINUX
        if (m_zyroTimer) {
            m_zyroTimer->stop();
            m_zyroTimer->deleteLater();
            m_zyroTimer = nullptr;
        }
        if (m_zyroReady) {
            close_event_thread();
            m_zyroReady = false;
        }
#endif
    }

    // ── Game loop ───────────────────────────────────────────────────
    void updateGame()
    {
        if (m_completed) return;

        // 1) gyro input → acceleration → velocity (inverted)
        const double ax = -m_rollInput  * ACCEL_SCALE;
        const double ay = -m_pitchInput * ACCEL_SCALE;

        m_player->vx = qBound(-MAX_SPEED,
                               (m_player->vx + ax) * FRICTION, MAX_SPEED);
        m_player->vy = qBound(-MAX_SPEED,
                               (m_player->vy + ay) * FRICTION, MAX_SPEED);

        // 2) move
        double newX = m_player->pos().x() + m_player->vx;
        double newY = m_player->pos().y() + m_player->vy;

        newX = qBound(WALL_T + PLAYER_R, newX, SCENE_W - WALL_T - PLAYER_R);
        newY = qBound(WALL_T + PLAYER_R, newY, SCENE_H - WALL_T - PLAYER_R);

        m_player->setPos(newX, newY);

        // 3) collision — only check Goal
        const auto collisions = m_scene->collidingItems(m_player);
        for (QGraphicsItem *item : collisions) {
            if (dynamic_cast<Goal *>(item)) {
                onGoalReachedInternal();
                return;
            }
        }
    }

    // ── Goal reached ────────────────────────────────────────────────
    void onGoalReachedInternal()
    {
        m_completed = true;
        m_gameTimer->stop();
        stopZyroPolling();

        auto *successText = new QGraphicsTextItem(QStringLiteral("SYSTEM UNLOCKED"));
        successText->setDefaultTextColor(QColor(0, 255, 65));
        successText->setFont(QFont("Consolas", 20, QFont::Bold));
        successText->setPos(SCENE_W / 2 - 130, SCENE_H / 2 - 20);
        successText->setZValue(50);
        m_scene->addItem(successText);

        QTimer::singleShot(1200, this, [this]() {
            if (onGoalReached) onGoalReached();
        });
    }
};

} // anonymous namespace

// ════════════════════════════════════════════════════════════════════════════
// MissionPage::setupMission5
// ════════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission5()
{
    m_correctAnswer = QStringLiteral("MAZE_CLEAR");

    auto *page = new QWidget(this);
    page->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(10, 4, 10, 4);
    pageLayout->setSpacing(6);

    // Header
    auto *header = new QLabel(
        QStringLiteral("[MISSION 5] - 레이저 미로 탈출"), page);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 22px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; "
        "border: none;"));
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
    panelLayout->setContentsMargins(10, 8, 10, 8);
    panelLayout->setSpacing(4);

    // Instruction
    auto *instrLabel = new QLabel(QStringLiteral(
        "보드를 기울여 데이터 코어(공)를 GOAL 지점까지 이동시키세요."),
        mainPanel);
    instrLabel->setAlignment(Qt::AlignCenter);
    instrLabel->setWordWrap(true);
    instrLabel->setStyleSheet(QStringLiteral(
        "color: #9ca3af; font-size: 13px; "
        "font-family: 'Consolas', monospace; background: transparent; "
        "border: none;"));
    panelLayout->addWidget(instrLabel);

    // Maze game widget
    auto *mazeGame = new MazeGameWidget(mainPanel);
    mazeGame->onGoalReached = [this]() {
        showResultPopup(true);
    };
    panelLayout->addWidget(mazeGame, 1);

    pageLayout->addWidget(mainPanel, 1);
    m_contentLayout->addWidget(page);
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 5 — Story popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission5Story()
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;
    storyLines
        << QStringLiteral("<span style='color:#666; %1'>[17:29:10]</span>&nbsp;&nbsp;"
                          "<span style='color:#ff4444; %1'>[5단계 인증] 최종 관문 — 레이저 미로</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:11]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>갓찌가 마지막으로 설치한 물리 잠금장치입니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:12]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>단말기를 기울여 데이터 코어를 레이저 사이로</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:13]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>통과시켜 GOAL 지점까지 이동시키세요.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:14]</span>&nbsp;&nbsp;"
                          "<span style='color:#888; %1'>...</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:15]</span>&nbsp;&nbsp;"
                          "<span style='color:#eab308; %1'>레이저에 닿으면 시작 지점으로 돌아갑니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:16]</span>&nbsp;&nbsp;"
                          "<span style='color:#eab308; %1'>손이 떨리면 갓찌가 웃습니다.</span>").arg(sf);

    showTerminalPopup(
        QStringLiteral("SECURITY_TERMINAL.exe"),
        storyLines,
        QStringLiteral("\u25b6 PROCEED"),
        QStringLiteral("#00ff41"),
        QColor(0, 255, 65, 140));
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 5 — Result popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission5Result(bool correct)
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (correct) {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:30:20]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>레이저 미로 스캔 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:30:21]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>데이터 코어 GOAL 도달 확인</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:30:22]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>물리 잠금장치 해제 완료</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:30:23]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>금고 잠금 해제됨</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:30:24]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>퇴근 코드 생성 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:30:25]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>전 단계 인증 완료</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 확인"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));

        emit missionCompleted(m_missionNumber);
    }
}
