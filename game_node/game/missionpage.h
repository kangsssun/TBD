#ifndef MISSIONPAGE_H
#define MISSIONPAGE_H

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QTimer;

/**
 * @brief Base class for all mission pages.
 */
class MissionPage : public QWidget
{
    Q_OBJECT

public:
    explicit MissionPage(int missionNumber, QWidget *parent = nullptr);
    virtual ~MissionPage() = default;

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
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

private:
    void setupMission1();
    void setupMission2();
    void showStoryPopup();
    void showResultPopup(bool correct);
    void showImagePopup(const QString &imagePath,
                        const QString &btnText,
                        const QString &btnColor,
                        const QColor &glowColor);
    void showTerminalPopup(const QString &title,
                           const QStringList &lines,
                           const QString &btnText,
                           const QString &btnColor,
                           const QColor &glowColor);

    int m_missionNumber;
    QVBoxLayout *m_contentLayout;

    // Mission 1 answer input
    QString m_answerInputRaw;
    QString m_correctAnswer;
};

#endif // MISSIONPAGE_H
