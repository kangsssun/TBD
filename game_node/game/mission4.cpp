#include "missionpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QtMath>
#include <cmath>

#ifdef Q_OS_LINUX
#include "dd_api/thermal_api.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Mission 4 helper widgets (file-local)
// ═══════════════════════════════════════════════════════════════════════════
namespace {

class TemperatureGraphWidget : public QWidget
{
public:
    explicit TemperatureGraphWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_targetTemp(25.2)
        , m_minTemp(0.0)
        , m_maxTemp(50.0)
        , m_timeWindowSec(30.0)
        , m_showTargetLine(true)
    {
        setMinimumHeight(140);
    }

    void setRanges(double minTemp, double maxTemp, double timeWindowSec)
    {
        m_minTemp = minTemp;
        m_maxTemp = maxTemp;
        m_timeWindowSec = qMax(5.0, timeWindowSec);
        update();
    }

    void setTargetTemperature(double targetTemp)
    {
        m_targetTemp = targetTemp;
        update();
    }

    void setShowTargetLine(bool show)
    {
        m_showTargetLine = show;
        update();
    }

    void appendSample(double elapsedSec, double temp)
    {
        m_points.append(QPointF(elapsedSec, temp));

        const double startTime = qMax(0.0, elapsedSec - m_timeWindowSec);
        while (!m_points.isEmpty() && m_points.first().x() < startTime) {
            m_points.removeFirst();
        }

        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#05070b"));

        const QRectF plotRect = rect().adjusted(52, 14, -14, -36);
        if (plotRect.width() < 40.0 || plotRect.height() < 40.0) {
            return;
        }

        painter.setPen(QPen(QColor("#1f3a25"), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(plotRect, 6, 6);

        const double now = m_points.isEmpty() ? 0.0 : m_points.last().x();
        const double xStart = qMax(0.0, now - m_timeWindowSec);
        const double xEnd = qMax(m_timeWindowSec, now);

        auto toX = [&](double sec) {
            const double ratio = (sec - xStart) / qMax(0.001, xEnd - xStart);
            return plotRect.left() + ratio * plotRect.width();
        };
        auto toY = [&](double temp) {
            const double ratio = (temp - m_minTemp) / qMax(0.001, m_maxTemp - m_minTemp);
            return plotRect.bottom() - ratio * plotRect.height();
        };

        painter.setPen(QPen(QColor("#112417"), 1));
        for (int i = 0; i <= 5; ++i) {
            const double ratio = static_cast<double>(i) / 5.0;
            const double y = plotRect.top() + ratio * plotRect.height();
            painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));

            const double tempLabel = m_maxTemp - ratio * (m_maxTemp - m_minTemp);
            painter.setPen(QColor("#40664a"));
            painter.drawText(QRectF(0, y - 8, 46, 16), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(tempLabel, 'f', 0));
            painter.setPen(QPen(QColor("#112417"), 1));
        }

        if (m_showTargetLine && m_targetTemp >= m_minTemp && m_targetTemp <= m_maxTemp) {
            const double yTarget = toY(m_targetTemp);
            painter.setPen(QPen(QColor("#ff5555"), 1, Qt::DashLine));
            painter.drawLine(QPointF(plotRect.left(), yTarget), QPointF(plotRect.right(), yTarget));
            painter.setPen(QColor("#ff7b7b"));
            painter.drawText(QRectF(plotRect.left() + 8, yTarget - 18, 180, 16), Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("정답 %1°C").arg(m_targetTemp, 0, 'f', 1));
        }

        painter.setPen(QColor("#40664a"));
        const int xTickStart = static_cast<int>(std::ceil(xStart / 2.0) * 2.0);
        for (int tick = xTickStart; tick <= static_cast<int>(std::floor(xEnd)); tick += 2) {
            const double x = toX(static_cast<double>(tick));
            painter.drawLine(QPointF(x, plotRect.bottom()), QPointF(x, plotRect.bottom() + 4));
            painter.drawText(QRectF(x - 20, plotRect.bottom() + 6, 40, 16), Qt::AlignCenter,
                             QString::number(tick));
        }

        if (m_points.size() >= 2) {
            QPolygonF polyline;
            polyline.reserve(m_points.size());
            for (const QPointF &point : std::as_const(m_points)) {
                polyline.append(QPointF(toX(point.x()), toY(point.y())));
            }
            painter.setPen(QPen(QColor("#00d4ff"), 2));
            painter.drawPolyline(polyline);
        }

        if (!m_points.isEmpty()) {
            const QPointF last = m_points.last();
            const QPointF mapped(toX(last.x()), toY(last.y()));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#00ff88"));
            painter.drawEllipse(mapped, 4.0, 4.0);
        }

        painter.setPen(QColor("#6fa87f"));
        painter.drawText(QRectF(plotRect.left(), rect().height() - 20, plotRect.width(), 16), Qt::AlignCenter,
                         QStringLiteral("s"));
    }

private:
    QVector<QPointF> m_points;
    double m_targetTemp;
    double m_minTemp;
    double m_maxTemp;
    double m_timeWindowSec;
    bool m_showTargetLine;
};

class FanWidget : public QWidget
{
public:
    explicit FanWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_angle(0.0)
        , m_speedRatio(0.3)
    {
        setFixedSize(92, 92);
    }

    void setSpeedRatio(double ratio)
    {
        m_speedRatio = qBound(0.0, ratio, 1.0);
    }

    void advance()
    {
        const double baseStep = 2.2;
        const double dynamicStep = 10.0 * m_speedRatio;
        m_angle = std::fmod(m_angle + baseStep + dynamicStep, 360.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#05070b"));

        const qreal side = qMin(width(), height());
        const QPointF center(width() / 2.0, height() / 2.0);
        const qreal radius = side * 0.44;

        painter.setPen(QPen(QColor("#1e4a29"), 2));
        painter.setBrush(QColor("#0b1110"));
        painter.drawEllipse(center, radius, radius);

        painter.save();
        painter.translate(center);
        painter.rotate(m_angle);
        for (int i = 0; i < 4; ++i) {
            painter.rotate(90.0);
            QPainterPath blade;
            blade.moveTo(radius * 0.08, 0);
            blade.quadTo(radius * 0.62, -radius * 0.30, radius * 0.80, 0);
            blade.quadTo(radius * 0.62, radius * 0.30, radius * 0.08, 0);
            painter.fillPath(blade, QColor("#00c4ff"));
        }
        painter.restore();

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#00ff88"));
        painter.drawEllipse(center, radius * 0.18, radius * 0.18);
    }

private:
    double m_angle;
    double m_speedRatio;
};

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// Mission 4 — Server temperature stabilization
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::setupMission4()
{
    const double targetTemp = 23.0;
    const double tempMin = 20.0;
    const double tempMax = 26.0;
    const double clearThreshold = 23.0;
    const double graphWindowSec = 30.0;

    m_correctAnswer = QStringLiteral("23.0");

    auto *page = new QWidget(this);
    page->setStyleSheet(QStringLiteral("background-color: #0c0c0c;"));
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(10, 4, 10, 4);
    pageLayout->setSpacing(6);

    auto *header = new QLabel(QStringLiteral("[MISSION 4] - 서버 온도 안정화"), page);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(QStringLiteral(
        "color: #00ff41; font-size: 22px; font-weight: 800; "
        "font-family: 'Consolas', monospace; background: transparent; border: none;"));
    pageLayout->addWidget(header);

    auto *divider = new QFrame(page);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("background-color: #00ff41; max-height: 1px; border: none;"));
    pageLayout->addWidget(divider);
    pageLayout->addSpacing(4);

    auto *mainPanel = new QWidget(page);
    mainPanel->setStyleSheet(QStringLiteral(
        "background: #111; border: 1px solid #00ff41; border-radius: 8px;"));
    auto *panelLayout = new QVBoxLayout(mainPanel);
    panelLayout->setContentsMargins(14, 10, 14, 10);
    panelLayout->setSpacing(6);

    // ── Top strip: [Temperature + Status] | [Fan] ────────────────────
    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(10);

    // Left: temperature + status stacked
    auto *infoFrame = new QFrame(mainPanel);
    infoFrame->setStyleSheet(QStringLiteral(
        "background: #070b0e; border: 1px solid #163848; border-radius: 6px;"));
    auto *infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(12, 8, 12, 8);
    infoLayout->setSpacing(4);

    auto *currentFrame = new QFrame(infoFrame);
    currentFrame->setStyleSheet(QStringLiteral(
        "background: #071017; border: 1px solid #1e4f7a; border-radius: 4px;"));
    auto *currentLayout = new QVBoxLayout(currentFrame);
    currentLayout->setContentsMargins(10, 6, 10, 6);
    currentLayout->setSpacing(0);

    auto *currentValue = new QLabel(QStringLiteral("현재: --.-°C"), currentFrame);
    currentValue->setAlignment(Qt::AlignCenter);
    currentValue->setStyleSheet(QStringLiteral(
        "color: #00d4ff; font-size: 28px; font-weight: 800; "
        "font-family: 'Consolas', monospace; border: none;"));
    currentLayout->addWidget(currentValue);
    infoLayout->addWidget(currentFrame);

    auto *statusLabel = new QLabel(QStringLiteral("STATUS: ADJUSTING..."), infoFrame);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet(QStringLiteral(
        "color: #eab308; font-size: 13px; font-weight: 700; "
        "font-family: 'Consolas', monospace; border: none;"));
    infoLayout->addWidget(statusLabel);

    auto *modeLabel = new QLabel(QStringLiteral("THERMAL CONTROL · AUTO MODE"), infoFrame);
    modeLabel->setAlignment(Qt::AlignCenter);
    modeLabel->setStyleSheet(QStringLiteral(
        "color: #6b7280; font-size: 11px; font-family: 'Consolas', monospace; border: none;"));
    infoLayout->addWidget(modeLabel);

    topRow->addWidget(infoFrame, 1);

    // Right: compact fan
    auto *fanPanel = new QFrame(mainPanel);
    fanPanel->setStyleSheet(QStringLiteral(
        "background: #070b0e; border: 1px solid #163848; border-radius: 6px;"));
    auto *fanLayout = new QVBoxLayout(fanPanel);
    fanLayout->setContentsMargins(8, 6, 8, 6);
    fanLayout->setSpacing(3);

    auto *fanTitle = new QLabel(QStringLiteral("FAN"), fanPanel);
    fanTitle->setAlignment(Qt::AlignCenter);
    fanTitle->setStyleSheet(QStringLiteral(
        "color: #6fb7d1; font-size: 10px; font-family: 'Consolas', monospace; border: none;"));
    auto *fanWidget = new FanWidget(fanPanel);
    auto *fanState = new QLabel(QStringLiteral("RPM: --"), fanPanel);
    fanState->setAlignment(Qt::AlignCenter);
    fanState->setStyleSheet(QStringLiteral(
        "color: #7dd3fc; font-size: 10px; font-family: 'Consolas', monospace; border: none;"));
    fanLayout->addWidget(fanTitle);
    fanLayout->addWidget(fanWidget, 0, Qt::AlignCenter);
    fanLayout->addWidget(fanState);

    topRow->addWidget(fanPanel, 0);
    panelLayout->addLayout(topRow);

    // ── Graph: fills remaining vertical space ────────────────────────
    auto *graphFrame = new QFrame(mainPanel);
    graphFrame->setStyleSheet(QStringLiteral(
        "background: #05070b; border: 1px solid #1e4a29; border-radius: 6px;"));
    auto *graphFrameLayout = new QVBoxLayout(graphFrame);
    graphFrameLayout->setContentsMargins(6, 4, 6, 4);
    graphFrameLayout->setSpacing(2);

    auto *graphLabel = new QLabel(QStringLiteral("실시간 서버 온도 추이"), graphFrame);
    graphLabel->setAlignment(Qt::AlignCenter);
    graphLabel->setStyleSheet(QStringLiteral(
        "color: #89d3ff; font-size: 11px; font-family: 'Consolas', monospace; border: none;"));
    graphFrameLayout->addWidget(graphLabel);

    auto *graphWidget = new TemperatureGraphWidget(graphFrame);
    graphWidget->setRanges(tempMin, tempMax, graphWindowSec);
    graphWidget->setTargetTemperature(targetTemp);
    graphWidget->setShowTargetLine(false);
    graphFrameLayout->addWidget(graphWidget, 1);

    panelLayout->addWidget(graphFrame, 1);

    pageLayout->addWidget(mainPanel, 1);
    m_contentLayout->addWidget(page);

    constexpr int sampleIntervalMs = 2000;
    constexpr double sampleIntervalSec = 2.0;

    double *currentTemp = new double(35.0);  // default high; overwritten by sensor
    double *baselineTemp = new double(35.0);  // initial sensor reading for amplification
    double *elapsedSec = new double(0.0);
    double *inRangeSec = new double(0.0);
    bool *completed = new bool(false);
    bool *sensorReady = new bool(false);

    // Amplification: real sensor change * AMPLIFY_FACTOR = game temperature change
    // e.g., sensor drops 1°C → game drops 3°C
    constexpr double AMPLIFY_FACTOR = 3.0;

    auto updateTelemetry = [=](bool appendGraphPoint) {
        const bool locked = *currentTemp <= clearThreshold;

        currentValue->setText(QStringLiteral("현재: %1°C").arg(*currentTemp, 0, 'f', 1));
        if (locked) {
            currentFrame->setStyleSheet(QStringLiteral(
                "background: #07160b; border: 1px solid #00ff88; border-radius: 6px;"));
            currentValue->setStyleSheet(QStringLiteral(
                "color: #00ff88; font-size: 26px; font-weight: 800; font-family: 'Consolas', monospace; border: none;"));
            statusLabel->setText(QStringLiteral("STATUS: TARGET LOCKED"));
            statusLabel->setStyleSheet(QStringLiteral(
                "color: #00ff88; font-size: 13px; font-weight: 700; font-family: 'Consolas', monospace; border: none;"));
        } else {
            currentFrame->setStyleSheet(QStringLiteral(
                "background: #170707; border: 1px solid #7a1e1e; border-radius: 6px;"));
            currentValue->setStyleSheet(QStringLiteral(
                "color: #ff6868; font-size: 26px; font-weight: 800; font-family: 'Consolas', monospace; border: none;"));
            statusLabel->setText(QStringLiteral("STATUS: OVERHEAT"));
            statusLabel->setStyleSheet(QStringLiteral(
                "color: #ff5555; font-size: 13px; font-weight: 700; font-family: 'Consolas', monospace; border: none;"));
        }

        const double diff = qAbs(*currentTemp - clearThreshold);
        const double fanRatio = qBound(0.15, diff / 8.0 + 0.15, 1.0);
        fanWidget->setSpeedRatio(fanRatio);
        fanState->setText(QStringLiteral("RPM: %1").arg(static_cast<int>(900 + fanRatio * 2400)));

        if (appendGraphPoint) {
            graphWidget->appendSample(*elapsedSec, *currentTemp);
        }
    };

    auto *fanTimer = new QTimer(page);
    fanTimer->setInterval(40);
    QObject::connect(fanTimer, &QTimer::timeout, fanWidget, [fanWidget]() {
        fanWidget->advance();
    });
    fanTimer->start();

    auto checkComplete = [=]() {
        if (*completed) {
            return;
        }

        if (*currentTemp <= clearThreshold) {
            *inRangeSec += sampleIntervalSec;
            if (*inRangeSec >= 3.0) {
                *completed = true;
                QTimer::singleShot(350, this, [this]() { showResultPopup(true); });
            }
        } else {
            *inRangeSec = 0.0;
        }
    };

    auto *thermalTimer = new QTimer(page);
    thermalTimer->setInterval(sampleIntervalMs);

    auto stopThermalPolling = [=]() {
        if (thermalTimer->isActive()) {
            thermalTimer->stop();
        }
#ifdef Q_OS_LINUX
        if (*sensorReady) {
            thermal_close();
            *sensorReady = false;
        }
#endif
    };

#ifdef Q_OS_LINUX
    if (thermal_init() == 0) {
        thermal_start_thread();
        *sensorReady = true;

        // Read initial sensor value as baseline
        float initTemp = 0.0f;
        if (thermal_get_value(&initTemp) == 0) {
            *baselineTemp = static_cast<double>(initTemp);
            // Start game temp at 25.5 (above 23 threshold, needs cooling)
            *currentTemp = 25.5;
        }

        statusLabel->setText(QStringLiteral("STATUS: SENSOR READY"));
    } else {
        statusLabel->setText(QStringLiteral("STATUS: SENSOR INIT FAILED"));
        statusLabel->setStyleSheet(QStringLiteral(
            "color: #ff5555; font-size: 13px; font-weight: 700; "
            "font-family: 'Consolas', monospace; border: none;"));
    }
#else
    statusLabel->setText(QStringLiteral("STATUS: SENSOR UNSUPPORTED"));
#endif

    QObject::connect(thermalTimer, &QTimer::timeout, this, [=]() {
        if (*completed) {
            return;
        }

#ifdef Q_OS_LINUX
        if (!*sensorReady) {
            return;
        }

        float temp = 0.0f;
        if (thermal_get_value(&temp) != 0) {
            return;
        }

        // Amplify: game_temp = 25.5 + (sensor_change_from_baseline * AMPLIFY_FACTOR)
        // If sensor drops, game temp drops faster
        const double sensorDelta = static_cast<double>(temp) - *baselineTemp;
        const double gameTemp = 25.5 + sensorDelta * AMPLIFY_FACTOR;
        *currentTemp = qMax(tempMin, gameTemp);  // no upper clamp for display
        *elapsedSec += sampleIntervalSec;
        updateTelemetry(true);
        checkComplete();
#endif
    });

    updateTelemetry(true);
    if (*sensorReady) {
        thermalTimer->start();
    }

    QObject::connect(page, &QObject::destroyed, [currentTemp, baselineTemp, elapsedSec, inRangeSec, completed, sensorReady, stopThermalPolling]() {
        stopThermalPolling();
        delete currentTemp;
        delete baselineTemp;
        delete elapsedSec;
        delete inRangeSec;
        delete completed;
        delete sensorReady;
    });
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 4 — Story popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission4Story()
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList storyLines;
    storyLines
        << QStringLiteral("<span style='color:#666; %1'>[17:27:10]</span>&nbsp;&nbsp;"
                          "<span style='color:#ff4444; %1'>[3단계 인증] 서버 쿨링 시스템 복구</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:27:11]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>전력이 복구되었지만, 서버 온도가 급상승 중입니다!</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:27:12]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>누군가 쿨링 팬이 돌아가지 못하도록 환풍구를 꽉 막아두었습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:27:13]</span>&nbsp;&nbsp;"
                          "<span style='color:#00ff41; %1'>이대로면 시스템이 녹아내려 영원히 탈출할 수 없습니다.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:27:14]</span>&nbsp;&nbsp;"
                          "<span style='color:#888; %1'>...</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:27:15]</span>&nbsp;&nbsp;"
                          "<span style='color:#00bfff; %1'>수동으로 쿨러를 조작하여 서버를 억지로 냉각시키십시오.</span>").arg(sf)
        << QStringLiteral("<span style='color:#666; %1'>[17:27:16]</span>&nbsp;&nbsp;"
                          "<span style='color:#00bfff; %1'>목표 조건 : 23.0°C 이하 · 3초 유지</span>").arg(sf);

    showTerminalPopup(
        QStringLiteral("SECURITY_TERMINAL.exe"),
        storyLines,
        QStringLiteral("\u25b6 확인"),
        QStringLiteral("#00ff41"),
        QColor(0, 255, 65, 140));
}

// ═══════════════════════════════════════════════════════════════════════════
// Mission 4 — Result popup
// ═══════════════════════════════════════════════════════════════════════════
void MissionPage::showMission4Result(bool correct)
{
    const QString sf = QStringLiteral(
        "font-size:16px; font-family:Consolas,Courier New,monospace; "
        "font-weight:500; letter-spacing:-1px;");

    QStringList resultLines;

    if (correct) {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:28:20]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>서버 온도 모니터링 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:21]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>현재 온도 : 22.8°C / 3초 유지 완료</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:22]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>3단계 인증 성공</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:23]</span>&nbsp;&nbsp;"
                              "<span style='color:#888; %1'>...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:24]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>온도가 안정화되며 환풍구를 막고 있던 이물질이 쏟아져 내렸습니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:25]</span>&nbsp;&nbsp;"
                              "<span style='color:#00ff41; %1'>엄청난 양의 '해바라기씨 껍질' 무더기입니다.</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:26]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>이 건물에 해바라기씨를 은닉할 만한 짐승은 단 하나뿐입니다... 설마?</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 확인"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));

        showImagePopup(
            QStringLiteral(":/images/mission4_complete.png"),
            QStringLiteral("\u25b6 확인"),
            QStringLiteral("#00ff41"),
            QColor(0, 255, 65, 140));

        emit missionCompleted(m_missionNumber);
    } else {
        resultLines
            << QStringLiteral("<span style='color:#666; %1'>[17:28:20]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>서버 온도 모니터링 중...</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:21]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>현재 온도 : 28.5°C</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:22]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>목표 온도 미달성</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:23]</span>&nbsp;&nbsp;"
                              "<span style='color:#ff4444; %1'>쓰로틀링(Throttling)이 발생하기 일보 직전입니다!</span>").arg(sf)
            << QStringLiteral("<span style='color:#666; %1'>[17:28:24]</span>&nbsp;&nbsp;"
                              "<span style='color:#eab308; %1'>신속히 온도를 23.0°C 이하로 낮추십시오.</span>").arg(sf);

        showTerminalPopup(
            QStringLiteral("SYSTEM_VERIFY.exe"),
            resultLines,
            QStringLiteral("\u25b6 다시 시도"),
            QStringLiteral("#ff4444"),
            QColor(255, 68, 68, 140));
    }
}
