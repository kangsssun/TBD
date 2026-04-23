#ifndef READYPAGE_H
#define READYPAGE_H

#include <QWidget>
#include <QStringList>
#include <QJsonObject>
#include <functional>

class QLabel;
class QProgressBar;
class QTimer;
class QVBoxLayout;
class QPushButton;
class MissionPage;

class ReadyPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReadyPage(QWidget *parent = nullptr);

    void setTeamName(const QString &name);
    void resetCountdown(int seconds);
    void pauseCountdown();
    void resumeCountdown();
    void syncTimer(int seconds, bool running);
    void setMissionWidget(MissionPage *mission);
    MissionPage *currentMission() const;

    void addSystemNotice(const QString &text);
    void addGmDirectMessage(const QString &text);

    void setMissionProgress(int percent);
    void setGlobalProgress(int percent);

signals:
    void gmMessageReceived(const QString &text);
    void returnToTitleRequested();

public:
    void setSendMessageCallback(const std::function<void(const QString &)> &cb);
    void setProgressUpdateCallback(const std::function<void(int missionNumber, int progress)> &cb);
    void setQrSubmitCallback(const std::function<void(const QByteArray &, const std::function<void(const QString &, const QString &)> &)> &cb);
    void setServerMessageCallback(const std::function<void(const QJsonObject &)> &cb);
    int missionProgress() const { return m_currentProgress; }
    void restoreProgress(int percent) { m_currentProgress = percent; setMissionProgress(percent); }
    void showEndingSequence(const QString &recoveryCode = QString(),
                            const QString &serverClearTime = QString(),
                            int rank = 0, int totalTeams = 0, bool isLast = false);

private:
    void setupUi();
    void updateHeader();
    void showEvent1();
    QString formatRemainingTime() const;

    QLabel *m_systemUserLabel;
    QLabel *m_timeRemainingLabel;
    QProgressBar *m_missionProgressBar;
    QProgressBar *m_globalProgressBar;
    QTimer *m_readyTimer;
    int m_remainingSeconds;
    QString m_teamName;

    QVBoxLayout *m_eventLayout;
    QWidget *m_currentMissionWidget;
    QLabel *m_eventTitleLabel;

    QPushButton *m_systemNoticesButton;
    QPushButton *m_contactGmButton;
    QLabel *m_missionPercentLabel;
    QLabel *m_globalPercentLabel;
    int m_unreadNotices;
    int m_unreadMessages;
    QStringList m_notices;
    QStringList m_directMessages;
    std::function<void(const QString &)> m_sendMessageCb;
    std::function<void(int, int)> m_progressUpdateCb;
    std::function<void(const QByteArray &, const std::function<void(const QString &, const QString &)> &)> m_qrSubmitCb;
    std::function<void(const QJsonObject &)> m_serverMessageCb;
    int m_currentProgress;
};

#endif // READYPAGE_H
