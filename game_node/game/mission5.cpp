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
#include <QGraphicsPolygonItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSlider>
#include <QBrush>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
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
constexpr double SCENE_W  = 1200.0;
constexpr double SCENE_H  = 500.0;
constexpr double WALL_T   = 6.0;      // wall / laser thickness
constexpr double PLAYER_R = 12.0;     // player radius
constexpr double GOAL_SIZE = 30.0;

// ── Physics constants ───────────────────────────────────────────────────
constexpr double ACCEL_SCALE  = 0.004;
constexpr double FRICTION     = 0.87;
constexpr double MAX_SPEED    = 2.2;
constexpr int    TICK_MS      = 16;    // ~60 FPS

// ── Start / Goal positions ──────────────────────────────────────────────
constexpr double START_X = 30.0;
constexpr double START_Y = 30.0;
constexpr double GOAL_X  = 10.0;
constexpr double GOAL_Y  = SCENE_H - GOAL_SIZE - 10.0;

// ── Hearts / HP ─────────────────────────────────────────────────────────
constexpr int    MAX_HEARTS = 3;
constexpr int    INVINCIBLE_FRAMES = 63;   // ~1.0 s at 60 FPS

// ─────────────────────────────────────────────────────────────────────────
// Player (ball image)
// ─────────────────────────────────────────────────────────────────────────
class Player : public QGraphicsPixmapItem
{
public:
    double vx = 0.0;
    double vy = 0.0;

    explicit Player(QGraphicsItem *parent = nullptr)
        : QGraphicsPixmapItem(parent)
    {
        const int diameter = static_cast<int>(PLAYER_R * 2);
        QPixmap pix(QStringLiteral(":/images/ball.png"));
        if (!pix.isNull()) {
            pix = pix.scaled(diameter, diameter, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        setPixmap(pix);
        setOffset(-PLAYER_R, -PLAYER_R);
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
        buildHearts();

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
    QList<QGraphicsItem*> m_innerWalls;
    int  m_hearts = MAX_HEARTS;
    QList<QGraphicsPixmapItem*> m_heartItems;
    bool m_invincible = false;
    int  m_invincibleCounter = 0;
    int  m_stunCounter = 0;

    // Moving obstacles
    struct MovingObs {
        QGraphicsEllipseItem *item;
        double minY, maxY;
        double speed;
        bool movingDown;
    };
    QList<MovingObs> m_movingObs;

    // Rotating obstacles (at turning points)
    struct RotatingObs {
        QGraphicsEllipseItem *item;
        double cx, cy;       // orbit center
        double orbitR;       // orbit radius
        double angle;        // current angle (radians)
        double angularSpeed; // radians per frame
    };
    QList<RotatingObs> m_rotatingObs;

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
            "QGraphicsView { background: #05070b; border: none; }"));
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setAlignment(Qt::AlignCenter);
        m_scene->setBackgroundBrush(QColor("#05070b"));

        root->addWidget(m_view, 1);

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
        });
        QObject::connect(m_sliderY, &QSlider::valueChanged, this, [this](int v) {
            m_rollInput = static_cast<double>(v);
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
        goalLabel->setPos(GOAL_X + GOAL_SIZE + 4, GOAL_Y + 6);
        goalLabel->setZValue(2);
        m_scene->addItem(goalLabel);

        // ── Internal laser walls (serpentine maze) ──────────────────
        auto addLaser = [this](double x, double y, double w, double h, bool flip = false) {
            const bool horizontal = (w >= h);

            if (horizontal) {
                // Horizontal: straight rectangle
                auto *rect = new QGraphicsRectItem(x, y, w, h);
                QLinearGradient grad(x, y, x, y + h);
                grad.setColorAt(0.0, QColor(255, 40, 40, 160));
                grad.setColorAt(0.5, QColor(255, 100, 100, 255));
                grad.setColorAt(1.0, QColor(255, 40, 40, 160));
                rect->setBrush(QBrush(grad));
                rect->setPen(QPen(QColor(255, 80, 80, 180), 0.5));
                rect->setZValue(5);
                m_scene->addItem(rect);
                m_innerWalls.append(rect);
            } else {
                // Vertical: triangle spike
                const double cx = x + w * 0.5;
                const double baseW = 24.0;
                QPolygonF poly;
                if (!flip) {
                    // Normal: base at top, tip at bottom ▽
                    poly << QPointF(cx - baseW, y)
                         << QPointF(cx + baseW, y)
                         << QPointF(cx, y + h);
                } else {
                    // Flipped: tip at top, base at bottom △
                    poly << QPointF(cx, y)
                         << QPointF(cx + baseW, y + h)
                         << QPointF(cx - baseW, y + h);
                }
                auto *item = new QGraphicsPolygonItem(poly);
                QLinearGradient grad(cx, y, cx, y + h);
                grad.setColorAt(0.0, QColor(255, 80, 80, 200));
                grad.setColorAt(0.5, QColor(255, 120, 120, 255));
                grad.setColorAt(1.0, QColor(200, 20, 20, 180));
                item->setBrush(QBrush(grad));
                item->setPen(QPen(QColor(255, 80, 80, 200), 0.5));
                item->setZValue(5);
                m_scene->addItem(item);
                m_innerWalls.append(item);
            }
        };

        // Main horizontal laser walls (serpentine pattern)
        // Wall 1: y=125, gap on right
        addLaser(WALL_T, 125, 1044, WALL_T);
        // Wall 2: y=250, gap on left
        addLaser(150, 250, 1044, WALL_T);
        // Wall 3: y=375, gap on right
        addLaser(WALL_T, 375, 1044, WALL_T);

        // Small vertical laser obstacles in corridors
        // Top corridor (y=6..125)
        addLaser(380, WALL_T, WALL_T, 60);              // #1 normal ▽
        addLaser(750, 65, WALL_T, 60, true);             // #2 flipped △
        // Corridor 1 (y=131..250)
        addLaser(500, 131, WALL_T, 60);                  // #3 normal ▽
        addLaser(250, 190, WALL_T, 60, true);            // #4 flipped △
        // Corridor 2 (y=256..375)
        addLaser(650, 256, WALL_T, 60);                  // #5 normal ▽
        addLaser(950, 315, WALL_T, 60, true);            // #6 flipped △
        // Bottom corridor (y=381..494)
        addLaser(200, 381, WALL_T, 60);                  // #7 normal ▽
        addLaser(520, SCENE_H - WALL_T - 60, WALL_T, 60, true); // #8 flipped △
        addLaser(880, 381, WALL_T, 60);                  // #9 normal ▽

        // ── Moving obstacles (in gaps between spikes) ───────────
        auto addMovingObs = [this](double x, double corridorTop, double corridorBot, double speed) {
            const double r = 10.0;
            auto *obs = new QGraphicsEllipseItem(-r, -r, r * 2, r * 2);
            QRadialGradient grad(0, 0, r);
            grad.setColorAt(0.0, QColor(255, 180, 0, 255));
            grad.setColorAt(0.7, QColor(255, 120, 0, 200));
            grad.setColorAt(1.0, QColor(200, 60, 0, 140));
            obs->setBrush(QBrush(grad));
            obs->setPen(QPen(QColor(255, 160, 0, 200), 1));
            obs->setZValue(8);
            const double startY = (corridorTop + corridorBot) / 2.0;
            obs->setPos(x, startY);
            m_scene->addItem(obs);
            m_movingObs.append({obs, corridorTop + r + 2, corridorBot - r - 2, speed, true});
        };

        // Top corridor: spike-free zone around x=580
        addMovingObs(580, WALL_T, 125, 0.8);
        // Corridor 1: spike-free zone around x=850
        addMovingObs(850, 131, 250, 1.0);
        // Corridor 2: spike-free zone around x=350
        addMovingObs(350, 256, 375, 0.9);
        // Bottom corridor: spike-free zone around x=700
        addMovingObs(700, 381, SCENE_H - WALL_T, 1.1);

        // ── Rotating obstacles at turning points ────────────
        auto addRotatingObs = [this](double cx, double cy, double orbitR, double angularSpeed) {
            const double r = 10.0;
            auto *obs = new QGraphicsEllipseItem(-r, -r, r * 2, r * 2);
            QRadialGradient grad(0, 0, r);
            grad.setColorAt(0.0, QColor(255, 60, 200, 255));
            grad.setColorAt(0.7, QColor(200, 30, 160, 200));
            grad.setColorAt(1.0, QColor(140, 10, 100, 140));
            obs->setBrush(QBrush(grad));
            obs->setPen(QPen(QColor(255, 80, 200, 200), 1));
            obs->setZValue(8);
            obs->setPos(cx + orbitR, cy);  // start at angle=0
            m_scene->addItem(obs);
            m_rotatingObs.append({obs, cx, cy, orbitR, 0.0, angularSpeed});
        };

        // Turn 1: right gap (wall1 ends at x=1050, corridor y=125..250)
        addRotatingObs(1120, 187.5, 40.0, 0.03);
        // Turn 2: left gap (wall2 starts at x=150, corridor y=250..375)
        addRotatingObs(78, 312.5, 40.0, -0.035);
        // Turn 3: right gap (wall3 ends at x=1050, corridor y=375..494)
        addRotatingObs(1120, 437.0, 40.0, 0.04);
    }

    // ── Hearts ──────────────────────────────────────────────────────
    void buildHearts()
    {
        const double baseX = SCENE_W - 180.0;
        const double baseY = 10.0;
        const int heartSize = 50;
        QPixmap devilPix(QStringLiteral(":/images/devil.png"));
        if (!devilPix.isNull()) {
            devilPix = devilPix.scaled(heartSize, heartSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        for (int i = 0; i < MAX_HEARTS; ++i) {
            auto *heart = new QGraphicsPixmapItem(devilPix);
            heart->setPos(baseX + i * 58.0, baseY);
            heart->setZValue(20);
            m_scene->addItem(heart);
            m_heartItems.append(heart);
        }
    }

    void updateHeartsDisplay()
    {
        for (int i = 0; i < m_heartItems.size(); ++i) {
            m_heartItems[i]->setOpacity(i < m_hearts ? 1.0 : 0.15);
        }
    }

    void onWallHit()
    {
        if (m_invincible) return;

        m_hearts--;
        updateHeartsDisplay();
        m_invincible = true;
        m_invincibleCounter = 0;

        // Bounce player back
        m_player->vx = -m_player->vx * 1.5;
        m_player->vy = -m_player->vy * 1.5;

        if (m_hearts <= 0) {
            // All hearts lost → full reset
            m_hearts = MAX_HEARTS;
            updateHeartsDisplay();
            m_player->resetTo(START_X, START_Y);
        }
    }

    // ── Start / Stop ────────────────────────────────────────────────
    void startGame()
    {
        m_completed = false;
        m_hearts = MAX_HEARTS;
        m_invincible = false;
        m_invincibleCounter = 0;
        updateHeartsDisplay();
        m_player->resetTo(START_X, START_Y);
        m_gameTimer->start(TICK_MS);
        startZyroPolling();
    }

    void startZyroPolling()
    {
#ifdef Q_OS_LINUX
        if (board_sync() != 0) {
            return;
        }
        if (init_event_thread() != 0) {
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

        // Stun: skip movement while stunned
        if (m_stunCounter > 0) {
            m_stunCounter--;
            if (m_invincible) {
                m_invincibleCounter++;
                m_player->setOpacity((m_invincibleCounter / 4) % 2 ? 0.25 : 1.0);
                if (m_invincibleCounter >= INVINCIBLE_FRAMES) {
                    m_invincible = false;
                    m_invincibleCounter = 0;
                    m_player->setOpacity(1.0);
                }
            }
            return;
        }

        // Move obstacles
        for (auto &obs : m_movingObs) {
            double y = obs.item->pos().y();
            y += obs.movingDown ? obs.speed : -obs.speed;
            if (y >= obs.maxY) { y = obs.maxY; obs.movingDown = false; }
            if (y <= obs.minY) { y = obs.minY; obs.movingDown = true; }
            obs.item->setPos(obs.item->pos().x(), y);
        }

        // Rotate obstacles
        for (auto &rot : m_rotatingObs) {
            rot.angle += rot.angularSpeed;
            const double nx = rot.cx + std::cos(rot.angle) * rot.orbitR;
            const double ny = rot.cy + std::sin(rot.angle) * rot.orbitR;
            rot.item->setPos(nx, ny);
        }

        // 1) gyro input → acceleration → velocity (inverted)
        const double ax = -m_rollInput  * ACCEL_SCALE;
        const double ay = -m_pitchInput * ACCEL_SCALE;

        m_player->vx = qBound(-MAX_SPEED,
                               (m_player->vx + ax) * FRICTION, MAX_SPEED);
        m_player->vy = qBound(-MAX_SPEED,
                               (m_player->vy + ay) * FRICTION, MAX_SPEED);

        // 2) save previous position, then move
        const double prevX = m_player->pos().x();
        const double prevY = m_player->pos().y();

        double newX = prevX + m_player->vx;
        double newY = prevY + m_player->vy;

        newX = qBound(WALL_T + PLAYER_R, newX, SCENE_W - WALL_T - PLAYER_R);
        newY = qBound(WALL_T + PLAYER_R, newY, SCENE_H - WALL_T - PLAYER_R);

        m_player->setPos(newX, newY);

        // 3) invincibility blink
        if (m_invincible) {
            m_invincibleCounter++;
            m_player->setOpacity((m_invincibleCounter / 4) % 2 ? 0.25 : 1.0);
            if (m_invincibleCounter >= INVINCIBLE_FRAMES) {
                m_invincible = false;
                m_invincibleCounter = 0;
                m_player->setOpacity(1.0);
            }
        }

        // 4) collision — check laser walls (push back + damage if not invincible)
        for (auto *wall : m_innerWalls) {
            if (m_player->collidesWithItem(wall)) {
                // Push back in the opposite direction of movement
                double dx = prevX - newX;
                double dy = prevY - newY;
                const double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.01) { dx /= dist; dy /= dist; }
                else { dx = 0.0; dy = -1.0; }
                const double pushBack = 20.0;
                double safeX = prevX + dx * pushBack;
                double safeY = prevY + dy * pushBack;
                safeX = qBound(WALL_T + PLAYER_R, safeX, SCENE_W - WALL_T - PLAYER_R);
                safeY = qBound(WALL_T + PLAYER_R, safeY, SCENE_H - WALL_T - PLAYER_R);
                m_player->setPos(safeX, safeY);

                m_player->vx = 0.0;
                m_player->vy = 0.0;
                m_stunCounter = 60;  // ~1s freeze at 60fps
                // Take damage only when not invincible
                if (!m_invincible) {
                    onWallHit();
                }
                return;
            }
        }

        // 5) check moving obstacles
        for (const auto &obs : m_movingObs) {
            if (m_player->collidesWithItem(obs.item)) {
                double dx = prevX - newX;
                double dy = prevY - newY;
                const double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.01) { dx /= dist; dy /= dist; }
                else { dx = 0.0; dy = -1.0; }
                const double pushBack = 20.0;
                double safeX = prevX + dx * pushBack;
                double safeY = prevY + dy * pushBack;
                safeX = qBound(WALL_T + PLAYER_R, safeX, SCENE_W - WALL_T - PLAYER_R);
                safeY = qBound(WALL_T + PLAYER_R, safeY, SCENE_H - WALL_T - PLAYER_R);
                m_player->setPos(safeX, safeY);
                m_player->vx = 0.0;
                m_player->vy = 0.0;
                m_stunCounter = 60;
                if (!m_invincible) {
                    onWallHit();
                }
                return;
            }
        }

        // 6) check rotating obstacles
        for (const auto &rot : m_rotatingObs) {
            if (m_player->collidesWithItem(rot.item)) {
                double dx = prevX - newX;
                double dy = prevY - newY;
                const double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.01) { dx /= dist; dy /= dist; }
                else { dx = 0.0; dy = -1.0; }
                const double pushBack = 20.0;
                double safeX = prevX + dx * pushBack;
                double safeY = prevY + dy * pushBack;
                safeX = qBound(WALL_T + PLAYER_R, safeX, SCENE_W - WALL_T - PLAYER_R);
                safeY = qBound(WALL_T + PLAYER_R, safeY, SCENE_H - WALL_T - PLAYER_R);
                m_player->setPos(safeX, safeY);
                m_player->vx = 0.0;
                m_player->vy = 0.0;
                m_stunCounter = 60;
                if (!m_invincible) {
                    onWallHit();
                }
                return;
            }
        }

        // 7) check Goal
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
    pageLayout->setContentsMargins(4, 2, 4, 2);
    pageLayout->setSpacing(2);

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

    // Maze game widget (directly under divider, no wrapper panel)
    auto *mazeGame = new MazeGameWidget(page);
    mazeGame->onGoalReached = [this]() {
        showResultPopup(true);
    };
    pageLayout->addWidget(mazeGame, 1);

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
                          "<span style='color:#eab308; %1'>레이저에 닿으면 체력(♥)이 1 감소합니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:16]</span>&nbsp;&nbsp;"
                          "<span style='color:#eab308; %1'>체력이 모두 소진되면 처음부터 재시작됩니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:29:17]</span>&nbsp;&nbsp;"
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
