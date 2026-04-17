#ifndef NOTICEDIALOG_H
#define NOTICEDIALOG_H

#include <QStringList>

class QWidget;

class NoticeDialog
{
public:
    static void show(QWidget *parent, const QStringList &notices = {});
};

#endif // NOTICEDIALOG_H
