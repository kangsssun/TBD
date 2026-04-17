#ifndef MISSIONPAGE_H
#define MISSIONPAGE_H

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
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
    void startTypingAnimation();
    void typeNextLine();

    int m_missionNumber;
    QVBoxLayout *m_contentLayout;

    // Mission 1 story terminal
    QStackedWidget *m_missionStack;
    QLabel *m_terminalOutput;
    QPushButton *m_nextButton;
    QTimer *m_typeTimer;
    QStringList m_storyLines;
    int m_currentLineIndex;
};

#endif // MISSIONPAGE_H
