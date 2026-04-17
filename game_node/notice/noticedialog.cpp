#include "noticedialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QDialog>
#include <QObject>

void NoticeDialog::show(QWidget *parent)
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

    const QList<QPair<QString, QString>> notices = {
        {
            QStringLiteral("[공지 01] 시스템 안내"),
            QStringLiteral("현재 등록된 운영자 공지가 없습니다. 운영자가 공지를 추가하면 이 창에서 바로 확인할 수 있습니다.")
        },
        {
            QStringLiteral("[공지 02] 미션 진행 안내"),
            QStringLiteral("각 팀은 현재 진행 중인 이벤트를 먼저 완료한 뒤 다음 단계로 이동해 주세요. 입력값 제출 전 팀원 간 최종 확인을 권장합니다.")
        },
        {
            QStringLiteral("[공지 03] 장비 사용 주의"),
            QStringLiteral("테이블 위 장비와 소품은 지정된 용도로만 사용해 주세요. 무리하게 분해하거나 강한 힘을 가하면 진행이 제한될 수 있습니다.")
        },
        {
            QStringLiteral("[공지 04] 패널 점검"),
            QStringLiteral("일부 팀의 메인 패널에서 지연 현상이 보고되었습니다. 화면 전환이 늦더라도 잠시 기다린 후 다시 입력해 주세요.")
        },
        {
            QStringLiteral("[공지 05] 힌트 요청 규칙"),
            QStringLiteral("힌트가 필요한 경우 CONTACT GM 버튼을 통해 요청해 주세요. 힌트는 단계별로 제공되며 과도한 요청은 제한될 수 있습니다.")
        },
        {
            QStringLiteral("[공지 06] 제한 시간 안내"),
            QStringLiteral("상단 타이머는 실시간으로 남은 시간을 표시합니다. 종료 10분 전부터는 공용 안내 방송이 추가로 제공될 예정입니다.")
        },
        {
            QStringLiteral("[공지 07] 안전 공지"),
            QStringLiteral("비상 상황이나 컨디션 이상이 있을 경우 즉시 운영자에게 알려 주세요. 안전과 관련된 요청은 우선 처리됩니다.")
        },
        {
            QStringLiteral("[공지 08] 최종 점검"),
            QStringLiteral("현재 공지 목록은 스크롤 테스트용 예시 데이터입니다. 실제 운영 시에는 운영자가 등록한 공지가 이 영역에 순차적으로 표시됩니다.")
        }
    };

    for (const auto &notice : notices) {
        auto *card = new QWidget(noticeListContainer);
        card->setObjectName(QStringLiteral("noticeCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 16);
        cardLayout->setSpacing(8);

        auto *header = new QLabel(notice.first, card);
        header->setObjectName(QStringLiteral("noticeCardHeader"));

        auto *body = new QLabel(notice.second, card);
        body->setObjectName(QStringLiteral("noticeCardBody"));
        body->setWordWrap(true);

        cardLayout->addWidget(header);
        cardLayout->addWidget(body);
        noticeListLayout->addWidget(card);
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
