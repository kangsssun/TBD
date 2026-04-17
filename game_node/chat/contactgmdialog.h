#ifndef CONTACTGMDIALOG_H
#define CONTACTGMDIALOG_H

#include <QString>
#include <QStringList>
#include <functional>

class QWidget;

class ContactGmDialog
{
public:
    static void show(QWidget *parent, const QString &teamName,
                     const QStringList &messages = {},
                     const std::function<void(const QString &)> &sendCb = nullptr);
};

#endif // CONTACTGMDIALOG_H
