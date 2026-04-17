#ifndef NOTICEDIALOG_H
#define NOTICEDIALOG_H

class QWidget;

class NoticeDialog
{
public:
    /**
     * Show the system notices popup dialog.
     * @param parent  Parent widget for centering and modality.
     */
    static void show(QWidget *parent);
};

#endif // NOTICEDIALOG_H
