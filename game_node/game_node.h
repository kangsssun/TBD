#ifndef GAME_NODE_H
#define GAME_NODE_H

#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QHostAddress>
#include <QLabel>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class GameNode; }
QT_END_NAMESPACE

class EmergencyPage;  // intro/emergencypage.h
class ReadyPage;      // game/readypage.h

class GameNode : public QWidget
{
    Q_OBJECT

public:
    explicit GameNode(QWidget *parent = nullptr);
    ~GameNode() override;

private slots:
    void showTitleScreen();
    void showAnswerInputScreen();
    void showSettingScreen();
    void onStartClicked();

    // TCP Socket slots
    void onConnected();
    void onReadyRead();
    void onConnectionError(QAbstractSocket::SocketError socketError);
    void tryReconnect();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyStyles();
    void playTitleMusicIfNeeded();
    void stopTitleMusic();
    void setupAlsaEnvironment();
    QString findFirstSongFile() const;
    QString findAplayProgram() const;
    void initializeSocket();
    void registerWithServer();
    void handleServerMessage(const QJsonObject &json);
    void sendMessage(const QJsonObject &msg);
    void showGmNotice(const QString &text);

    Ui::GameNode *ui;
    QString m_teamName;
    int m_teamId; // 서버에서 팀을 식별할 ID (예: 1, 2, 3)

    int m_introPageIndex;
    int m_readyPageIndex;

    EmergencyPage *m_emergencyPage;
    ReadyPage *m_readyPage;

    QTimer *m_blinkTimer;
    bool m_teamDialogOpen;
    bool m_ignoreTitleTap;
    QProcess *m_titleAudioProcess;
    bool m_titleMusicStarted;
    bool m_operatorMode;

    QTcpSocket m_socket;
    QString m_serverIp;
    quint16 m_serverPort;
    QByteArray m_buffer;
    std::function<void(const QString &status, const QString &result)> m_qrResultCb;
};

#endif // GAME_NODE_H
