#ifndef TEAMNAMEDIALOG_H
#define TEAMNAMEDIALOG_H

#include <QDialog>
#include <QString>

class QWidget;

class TeamNameDialog
{
public:
    /**
     * Show the cyberpunk team-name dialog with on-screen Korean/English keyboard.
     * @param parent   Parent widget for centering and modality.
     * @param accepted Output – set to true when the user confirms.
     * @return The composed team name (empty if cancelled).
     */
    static QString getTeamName(QWidget *parent, bool *accepted = nullptr);
};

#endif // TEAMNAMEDIALOG_H
