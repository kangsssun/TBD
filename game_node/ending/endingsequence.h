#ifndef ENDINGSEQUENCE_H
#define ENDINGSEQUENCE_H

#include <QWidget>
#include <QString>

class EndingSequence
{
public:
    /**
     * @brief Run the full ending sequence (blocking, modal dialogs).
     * @param parent   The top-level widget (used for overlay + dialog parenting)
     * @param teamName Team name displayed on the Mission Clear screen
     * @param remainingSeconds  Countdown seconds left (used to compute clear time)
     * @param totalSeconds      Total game time in seconds (default 45 min)
     */
    static void show(QWidget *parent,
                     const QString &teamName,
                     int remainingSeconds,
                     int totalSeconds = 45 * 60);

private:
    static void showBootLog(QWidget *topLevel, const QSize &screenSize);
    static void showImageSequence(QWidget *topLevel, const QSize &screenSize);
    static void showExitConfirmation(QWidget *topLevel);
    static void showRetrospective(QWidget *topLevel);
    static void showRetrospectiveConfirm(QWidget *topLevel);
    static void showMissionClear(QWidget *topLevel, const QSize &screenSize,
                                 const QString &teamName, int remainingSeconds,
                                 int totalSeconds);
};

#endif // ENDINGSEQUENCE_H
