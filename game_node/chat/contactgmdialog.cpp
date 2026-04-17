#include "contactgmdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QDialog>
#include <QObject>

void ContactGmDialog::show(QWidget *parent, const QString &teamName)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("contactGmDialog"));
    dialog.setWindowTitle(QStringLiteral("CONTACT GM"));
    dialog.setModal(true);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_StyledBackground, true);
    dialog.setFixedSize(760, 520);

    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog#contactGmDialog {
            background-color: #0b1220;
            border: 1px solid #10b981;
            border-radius: 12px;
        }
        QLabel#contactTitle {
            background: transparent;
            color: #ffffff;
            font-size: 28px;
            font-weight: 800;
            letter-spacing: 2px;
        }
        QLabel#contactSubtitle {
            background: transparent;
            color: #94a3b8;
            font-size: 15px;
        }
        QListWidget#chatList {
            background-color: #111827;
            color: #e5e7eb;
            border: 1px solid #1f2937;
            border-radius: 10px;
            padding: 8px;
            font-size: 15px;
        }
        QLineEdit#chatInput {
            background-color: #111827;
            color: #ffffff;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 8px 10px;
            font-size: 15px;
        }
        QPushButton#chatSendButton {
            background-color: #14532d;
            color: #ffffff;
            border: 1px solid #10b981;
            border-radius: 8px;
            padding: 8px 14px;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#chatSendButton:hover {
            background-color: #166534;
        }
        QPushButton#chatCloseButton {
            background-color: #142338;
            color: #bfdbfe;
            border: 1px solid #2563eb;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#chatCloseButton:hover {
            background-color: #1a3150;
            color: #ffffff;
        }
    )"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("CONTACT GM"), &dialog);
    titleLabel->setObjectName(QStringLiteral("contactTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);

    auto *subtitleLabel = new QLabel(QStringLiteral("운영자(Game Master)에게 질문 또는 요청사항을 전송할 수 있습니다."), &dialog);
    subtitleLabel->setObjectName(QStringLiteral("contactSubtitle"));
    subtitleLabel->setAlignment(Qt::AlignCenter);

    auto *chatList = new QListWidget(&dialog);
    chatList->setObjectName(QStringLiteral("chatList"));
    chatList->addItem(QStringLiteral("[SYSTEM] CONTACT GM 채널이 연결되었습니다."));
    chatList->addItem(QStringLiteral("[GM] 문의사항을 입력해 주세요."));

    auto *inputRow = new QHBoxLayout();
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(10);

    auto *chatInput = new QLineEdit(&dialog);
    chatInput->setObjectName(QStringLiteral("chatInput"));
    chatInput->setPlaceholderText(QStringLiteral("질문 또는 요청사항을 입력하세요."));

    auto *sendButton = new QPushButton(QStringLiteral("전송"), &dialog);
    sendButton->setObjectName(QStringLiteral("chatSendButton"));
    sendButton->setCursor(Qt::PointingHandCursor);
    sendButton->setMinimumWidth(96);

    inputRow->addWidget(chatInput, 1);
    inputRow->addWidget(sendButton);

    auto appendMessage = [teamName, chatList, chatInput]() {
        const QString text = chatInput->text().trimmed();
        if (text.isEmpty()) return;

        const QString sender = teamName.isEmpty() ? QStringLiteral("TEAM 1") : teamName;
        chatList->addItem(QStringLiteral("[%1] %2").arg(sender.toUpper(), text));
        chatList->addItem(QStringLiteral("[GM SERVER] 메시지가 접수되었습니다. 확인 후 답변드리겠습니다."));
        chatInput->clear();
        chatList->scrollToBottom();
    };

    QObject::connect(sendButton, &QPushButton::clicked, &dialog, appendMessage);
    QObject::connect(chatInput, &QLineEdit::returnPressed, &dialog, appendMessage);

    auto *closeButton = new QPushButton(QStringLiteral("닫기"), &dialog);
    closeButton->setObjectName(QStringLiteral("chatCloseButton"));
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFixedHeight(46);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addWidget(chatList, 1);
    layout->addLayout(inputRow);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    if (parent) {
        const QPoint centerGlobal = parent->mapToGlobal(parent->rect().center());
        dialog.move(centerGlobal - QPoint(dialog.width() / 2, dialog.height() / 2));
    }
    dialog.exec();
}
