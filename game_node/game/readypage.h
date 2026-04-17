#ifndef READYPAGE_H
#define READYPAGE_H

#include <QWidget>

class QLabel;
class QProgressBar;
class QTimer;
class QVBoxLayout;
class MissionPage;

class ReadyPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReadyPage(QWidget *parent = nullptr);

    void setTeamName(const QString &name);
    void resetCountdown(int seconds);
    void setMissionWidget(MissionPage *mission);

private:
    void setupUi();
    void updateHeader();
    QString formatRemainingTime() const;

    QLabel *m_systemUserLabel;
    QLabel *m_timeRemainingLabel;
    QProgressBar *m_missionProgressBar;
    QProgressBar *m_globalProgressBar;
    QTimer *m_readyTimer;
    int m_remainingSeconds;
    QString m_teamName;

    QVBoxLayout *m_eventLayout;    // layout inside eventContainer, for injecting missions
    QWidget *m_currentMissionWidget;
    QLabel *m_eventTitleLabel;
};

#endif // READYPAGE_H
