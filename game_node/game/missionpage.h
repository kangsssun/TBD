#ifndef MISSIONPAGE_H
#define MISSIONPAGE_H

#include <QWidget>
#include <functional>

class QObject;
class QEvent;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QTimer;
class QShowEvent;
class QHideEvent;
class QSlider;
class QGraphicsOpacityEffect;
class QProgressBar;

/**
 * @brief Base class for all mission pages.
 */
class MissionPage : public QWidget
{
    Q_OBJECT

public:
    explicit MissionPage(int missionNumber, QWidget *parent = nullptr);
    ~MissionPage() override;
    static void setOperatorMode(bool enabled);
    static bool isOperatorMode();

    int missionNumber() const { return m_missionNumber; }

    /** Show the story popup and start the mission. */
    void startMission();

    /** Reset to story screen (for re-entry). */
    void resetToStory();

    /** Set QR fallback submit callback */
    void setQrSubmitCallback(const std::function<void(const QByteArray &, const std::function<void(const QString &, const QString &)> &)> &cb);

signals:
    void missionCompleted(int missionNumber);
    void missionFailed(int missionNumber);
    void missionProblemShown(int missionNumber);

private slots:
    void onFrequencyChanged(int value);

protected:
    virtual void setupMission();
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

private:
    void setupMission1();
    void setupMission2();
    void setupMission3();
    void setupMission4();
    void setupMission5();

    void showStoryPopup();
    void showResultPopup(bool correct);

    // Per-mission story / result popups (implemented in missionX.cpp)
    void showMission1Story();
    void showMission1Result(bool correct);
    void showMission2Story();
    void showMission2Result(bool correct);
    void showMission3Story();
    void showMission3Result(bool correct);
    void showMission4Story();
    void showMission4Result(bool correct);
    void showMission5Story();
    void showMission5Result(bool correct);
    void showImagePopup(const QString &imagePath,
                        const QString &btnText,
                        const QString &btnColor,
                        const QColor &glowColor);
    void showMission2HintPopup();
    bool isAnswerAccepted(const QString &answer) const;
    void showTerminalPopup(const QString &title,
                           const QStringList &lines,
                           const QString &btnText,
                           const QString &btnColor,
                           const QColor &glowColor);
    void startMission3CameraPreview();
    void stopMission3CameraPreview();
    void evaluateMission3CameraPreview();
    void refreshMission3PreviewPlaceholder();
    bool hasBlockingPopupOpen() const;
    QString buildMission3CapturePath() const;
    void syncOperatorModeUi();

    int m_missionNumber;
    QVBoxLayout *m_contentLayout;

    // Mission 1 answer input
    QString m_answerInputRaw;
    QString m_correctAnswer;

    // Mission 3 camera preview/capture state
    QWidget *m_mission3PreviewArea;
    QLabel *m_mission3CaptureStatus;
    QTimer *m_mission3PopupWatchTimer;
    bool m_mission3CameraActive;
    bool m_mission3CameraStarting;
    bool m_mission3StoryPopupDismissed;
    bool m_mission3CaptureInProgress;
    bool m_mission3PendingStop;
    QString m_mission3CapturedImagePath;
    qint64 m_mission3LastCameraAttemptMs;

    // QR fallback callback (Mission 3)
    std::function<void(const QByteArray &imageData, const std::function<void(const QString &status, const QString &result)> &callback)> m_qrSubmitCb;

    // Mission 2 noise filtering
    QLabel *m_mission2SecretLabel = nullptr;
    QLabel *m_mission2NoiseLabel = nullptr;
    QLabel *m_mission2FreqLabel = nullptr;
    QLabel *m_mission2ResultLabel = nullptr;
    QSlider *m_mission2Slider = nullptr;
    QGraphicsOpacityEffect *m_mission2NoiseEffect = nullptr;

    // Mission 2 wheel generator game
    void updateWheelGauge();
    void onChargeButtonClicked();
    void generateNewTarget();
    void checkGameClear();

    QProgressBar *m_wheelProgressBar = nullptr;
    QLabel       *m_wheelTargetLabel = nullptr;
    QLabel       *m_wheelStatusLabel = nullptr;
    QPushButton  *m_chargeButton = nullptr;
    QTimer       *m_wheelTimer = nullptr;
    int           m_gaugeValue = 0;
    bool          m_movingUp = true;
    int           m_targetMin = 0;
    int           m_targetMax = 0;
    int           m_successCount = 0;
    QWidget      *m_gaugeContainer = nullptr;
    QLabel       *m_safeZoneMarker = nullptr;
    QLabel       *m_safeZoneOverlay = nullptr;
    QLabel       *m_arrowTopLeft = nullptr;
    QLabel       *m_arrowTopRight = nullptr;
    QLabel       *m_arrowBottomLeft = nullptr;
    QLabel       *m_arrowBottomRight = nullptr;
    QWidget      *m_topArrowRow = nullptr;
    QWidget      *m_bottomArrowRow = nullptr;

    static constexpr int MAX_SUCCESS = 5;

    static bool s_operatorMode;
};

#endif // MISSIONPAGE_H
