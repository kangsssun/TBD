#include "noticedialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QDialog>
#include <QObject>

void NoticeDialog::show(QWidget *parent, const QStringList &notices)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("systemNoticesDialog"));
    dialog.setWindowTitle(QStringLiteral("SYSTEM NOTICES"));
    dialog.setModal(true);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_StyledBackground, true);
    dialog.setFixedSize(760, 520);
    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog#systemNoticesDialog {
            background-color: #0b1220;
            border: 1px solid #10b981;
            border-radius: 12px;
        }
        QLabel#noticeTitle {
            background: transparent;
            color: #ffffff;
            font-size: 28px;
            font-weight: 800;
            letter-spacing: 2px;
        }
        QLabel#noticeSubtitle {
            background: transparent;
            color: #94a3b8;
            font-size: 15px;
        }
        QScrollArea#noticeScroll {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #0f172a;
            width: 14px;
            margin: 4px 0 4px 0;
            border-radius: 7px;
        }
        QScrollBar::handle:vertical {
            background: #475569;
            min-height: 36px;
            border-radius: 7px;
        }
        QScrollBar::handle:vertical:hover {
            background: #64748b;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            background: transparent;
            border: none;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QWidget#noticeListContainer {
            background: transparent;
        }
        QWidget#noticeCard {
            background-color: #111827;
            border: 1px solid #1f2937;
            border-radius: 10px;
        }
        QLabel#noticeCardHeader {
            background: transparent;
            color: #10b981;
            font-size: 16px;
            font-weight: 800;
        }
        QLabel#noticeCardBody {
            background: transparent;
            color: #e5e7eb;
            font-size: 15px;
            line-height: 1.4em;
        }
        QPushButton#noticeCloseButton {
            background-color: #142338;
            color: #bfdbfe;
            border: 1px solid #2563eb;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#noticeCloseButton:hover {
            background-color: #1a3150;
            color: #ffffff;
        }
        QPushButton#noticeCloseButton:pressed {
            background-color: #2563eb;
            color: #ffffff;
        }
    )"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(16);

    auto *titleLabel = new QLabel(QStringLiteral("SYSTEM NOTICES"), &dialog);
    titleLabel->setObjectName(QStringLiteral("noticeTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);

    auto *subtitleLabel = new QLabel(QStringLiteral("운영자가 전달한 공지사항을 확인할 수 있습니다."), &dialog);
    subtitleLabel->setObjectName(QStringLiteral("noticeSubtitle"));
    subtitleLabel->setAlignment(Qt::AlignCenter);

    auto *noticeScroll = new QScrollArea(&dialog);
    noticeScroll->setObjectName(QStringLiteral("noticeScroll"));
    noticeScroll->setWidgetResizable(true);
    noticeScroll->setFrameShape(QFrame::NoFrame);
    noticeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    noticeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *noticeListContainer = new QWidget();
    noticeListContainer->setObjectName(QStringLiteral("noticeListContainer"));
    auto *noticeListLayout = new QVBoxLayout(noticeListContainer);
    noticeListLayout->setContentsMargins(0, 0, 0, 0);
    noticeListLayout->setSpacing(12);

    const QStringList displayNotices = notices.isEmpty()
        ? QStringList{ QStringLiteral("현재 등록된 운영자 공지가 없습니다.") }
        : notices;

    int idx = 1;
    for (const auto &text : displayNotices) {
        auto *card = new QWidget(noticeListContainer);
        card->setObjectName(QStringLiteral("noticeCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 16);
        cardLayout->setSpacing(8);

        auto *header = new QLabel(QStringLiteral("[공지 %1]").arg(idx, 2, 10, QLatin1Char('0')), card);
        header->setObjectName(QStringLiteral("noticeCardHeader"));

        auto *body = new QLabel(text, card);
        body->setObjectName(QStringLiteral("noticeCardBody"));
        body->setWordWrap(true);

        cardLayout->addWidget(header);
        cardLayout->addWidget(body);
        noticeListLayout->addWidget(card);
        ++idx;
    }

    noticeListLayout->addStretch();
    noticeScroll->setWidget(noticeListContainer);

    auto *closeButton = new QPushButton(QStringLiteral("닫기"), &dialog);
    closeButton->setObjectName(QStringLiteral("noticeCloseButton"));
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFixedHeight(46);

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addWidget(noticeScroll, 1);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (parent) {
        const QPoint centerGlobal = parent->mapToGlobal(parent->rect().center());
        dialog.move(centerGlobal - QPoint(dialog.width() / 2, dialog.height() / 2));
    }
    dialog.exec();
}
