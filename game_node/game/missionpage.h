#ifndef MISSIONPAGE_H
#define MISSIONPAGE_H

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QTimer;
class QShowEvent;
class QHideEvent;

/**
 * @brief Base class for all mission pages.
 */
class MissionPage : public QWidget
{
    Q_OBJECT

public:
    explicit MissionPage(int missionNumber, QWidget *parent = nullptr);
    ~MissionPage() override;

    int missionNumber() const { return m_missionNumber; }

    /** Show the story popup and start the mission. */
    void startMission();

    /** Reset to story screen (for re-entry). */
    void resetToStory();

signals:
    void missionCompleted(int missionNumber);
    void missionFailed(int missionNumber);

protected:
    virtual void setupMission();
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
    bool hasBlockingPopupOpen() const;
    QString buildMission3CapturePath() const;

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
};

#endif // MISSIONPAGE_H
