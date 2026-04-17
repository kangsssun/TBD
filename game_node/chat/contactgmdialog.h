#ifndef CONTACTGMDIALOG_H
#define CONTACTGMDIALOG_H

#include <QString>

class QWidget;

class ContactGmDialog
{
public:
    /**
     * Show the GM contact / chat popup dialog.
     * @param parent  Parent widget for centering and modality.
     * @param teamName  Current team name (used as chat sender).
     */
    static void show(QWidget *parent, const QString &teamName);
};

#endif // CONTACTGMDIALOG_H
