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

    /**
     * Show a generic input popup reusing the same keyboard UI.
     * @param parent   Parent widget for centering and modality.
     * @param title    Dialog title text (e.g. "ENTER ANSWER").
     * @param subtitle Subtitle / instruction text.
     * @param maxLen   Maximum input length.
     * @param accepted Output – set to true when the user confirms.
     * @return The composed input string (empty if cancelled).
     */
    static QString getInput(QWidget *parent,
                            const QString &title,
                            const QString &subtitle,
                            int maxLen = 20,
                            bool *accepted = nullptr);
};

#endif // TEAMNAMEDIALOG_H
