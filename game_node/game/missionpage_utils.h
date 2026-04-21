#ifndef MISSIONPAGE_UTILS_H
#define MISSIONPAGE_UTILS_H

#include <QPushButton>
#include <QApplication>
#include <QEvent>
#include <QString>

inline void resetButtonHoverState(QPushButton *button)
{
    if (!button) return;

    button->setDown(false);
    button->clearFocus();
    button->setAttribute(Qt::WA_UnderMouse, false);

    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(button, &leaveEvent);

    // Force stylesheet re-evaluation to clear hover/pressed visual state
    const QString ss = button->styleSheet();
    button->setStyleSheet(QString());
    button->setStyleSheet(ss);

    button->update();
}

#endif // MISSIONPAGE_UTILS_H
