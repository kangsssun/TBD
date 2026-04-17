#ifndef GAME_NODE_H
#define GAME_NODE_H

#include <QWidget>
#include <QTimer>

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyStyles();

    Ui::GameNode *ui;
    QString m_teamName;

    int m_introPageIndex;
    int m_readyPageIndex;

    EmergencyPage *m_emergencyPage;
    ReadyPage *m_readyPage;

    QTimer *m_blinkTimer;
    bool m_teamDialogOpen;
    bool m_ignoreTitleTap;
};

#endif // GAME_NODE_H
