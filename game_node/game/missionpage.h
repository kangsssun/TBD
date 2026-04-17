#ifndef MISSIONPAGE_H
#define MISSIONPAGE_H

#include <QWidget>

class QVBoxLayout;

/**
 * @brief Base class for all mission pages.
 *
 * Subclass this to create Mission1, Mission2, Mission3, etc.
 * Each mission should override setupMission() to build its own UI
 * and implement checkAnswer() for answer validation.
 */
class MissionPage : public QWidget
{
    Q_OBJECT

public:
    explicit MissionPage(int missionNumber, QWidget *parent = nullptr);
    virtual ~MissionPage() = default;

    int missionNumber() const { return m_missionNumber; }

signals:
    /** Emitted when the player successfully completes this mission. */
    void missionCompleted(int missionNumber);

    /** Emitted when the player fails this mission. */
    void missionFailed(int missionNumber);

protected:
    /**
     * Override this to build mission-specific UI.
     * Called automatically from the constructor.
     * Use contentLayout() to add widgets.
     */
    virtual void setupMission();

    /** Returns the layout inside the mission content area. */
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

private:
    int m_missionNumber;
    QVBoxLayout *m_contentLayout;
};

#endif // MISSIONPAGE_H
