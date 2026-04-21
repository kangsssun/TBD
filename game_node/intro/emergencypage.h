#ifndef EMERGENCYPAGE_H
#define EMERGENCYPAGE_H

#include <QWidget>

class QPushButton;
class QStackedLayout;
class QGraphicsOpacityEffect;
class QSequentialAnimationGroup;
class QTimer;
class QWidget;

class EmergencyPage : public QWidget
{
    Q_OBJECT

public:
    explicit EmergencyPage(QWidget *parent = nullptr);

    void reset();   // restart from story screen

signals:
    void confirmed();   // emitted when confirm button is clicked

private:
    void setupUi();

    QStackedLayout *m_stack;
    QWidget *m_storyLayer;
    QWidget *m_dimLayer;
    QWidget *m_flashLayer;

    QGraphicsOpacityEffect *m_storyFlashEffect;
    QSequentialAnimationGroup *m_storyFlashAnim;

    QGraphicsOpacityEffect *m_flashEffect;
    QSequentialAnimationGroup *m_pulseAnim;

    QTimer *m_storyTimer;

    QPushButton *m_confirmButton;
    QString m_confirmStyle;
};

#endif // EMERGENCYPAGE_H
