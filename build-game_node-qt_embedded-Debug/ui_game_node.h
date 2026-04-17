/********************************************************************************
** Form generated from reading UI file 'game_node.ui'
**
** Created by: Qt User Interface Compiler version 5.15.11
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_GAME_NODE_H
#define UI_GAME_NODE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_GameNode
{
public:
    QVBoxLayout *rootLayout;
    QStackedWidget *stackedWidget;
    QWidget *titlePage;
    QVBoxLayout *titleLayout;
    QSpacerItem *titleTopSpacer;
    QSpacerItem *titleMidSpacer;
    QLabel *labelStartGame;
    QSpacerItem *titleBottomSpacer;
    QWidget *answerPage;
    QVBoxLayout *answerLayout;
    QSpacerItem *answerTopSpacer;
    QLabel *answerStageLabel;
    QSpacerItem *answerMidSpacer1;
    QLineEdit *answerLineEdit;
    QSpacerItem *answerMidSpacer2;
    QPushButton *btnSubmitAnswer;
    QSpacerItem *answerBottomSpacer;
    QPushButton *btnBackFromAnswer;
    QWidget *settingPage;
    QVBoxLayout *settingLayout;
    QSpacerItem *settingTopSpacer;
    QLabel *settingLabel;
    QSpacerItem *settingBottomSpacer;
    QPushButton *btnBackFromSetting;

    void setupUi(QWidget *GameNode)
    {
        if (GameNode->objectName().isEmpty())
            GameNode->setObjectName(QString::fromUtf8("GameNode"));
        GameNode->resize(1024, 600);
        rootLayout = new QVBoxLayout(GameNode);
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setContentsMargins(0, 0, 0, 0);
        stackedWidget = new QStackedWidget(GameNode);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));
        titlePage = new QWidget();
        titlePage->setObjectName(QString::fromUtf8("titlePage"));
        titleLayout = new QVBoxLayout(titlePage);
        titleLayout->setObjectName(QString::fromUtf8("titleLayout"));
        titleLayout->setContentsMargins(40, 40, 40, 40);
        titleTopSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        titleLayout->addItem(titleTopSpacer);

        titleMidSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        titleLayout->addItem(titleMidSpacer);

        labelStartGame = new QLabel(titlePage);
        labelStartGame->setObjectName(QString::fromUtf8("labelStartGame"));
        labelStartGame->setMinimumSize(QSize(400, 60));
        labelStartGame->setAlignment(Qt::AlignCenter);

        titleLayout->addWidget(labelStartGame, 0, Qt::AlignHCenter);

        titleBottomSpacer = new QSpacerItem(20, 80, QSizePolicy::Minimum, QSizePolicy::Fixed);

        titleLayout->addItem(titleBottomSpacer);

        stackedWidget->addWidget(titlePage);
        answerPage = new QWidget();
        answerPage->setObjectName(QString::fromUtf8("answerPage"));
        answerLayout = new QVBoxLayout(answerPage);
        answerLayout->setObjectName(QString::fromUtf8("answerLayout"));
        answerLayout->setContentsMargins(40, 40, 40, 40);
        answerTopSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        answerLayout->addItem(answerTopSpacer);

        answerStageLabel = new QLabel(answerPage);
        answerStageLabel->setObjectName(QString::fromUtf8("answerStageLabel"));
        answerStageLabel->setAlignment(Qt::AlignCenter);

        answerLayout->addWidget(answerStageLabel);

        answerMidSpacer1 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

        answerLayout->addItem(answerMidSpacer1);

        answerLineEdit = new QLineEdit(answerPage);
        answerLineEdit->setObjectName(QString::fromUtf8("answerLineEdit"));
        answerLineEdit->setMinimumSize(QSize(0, 44));
        answerLineEdit->setAlignment(Qt::AlignCenter);

        answerLayout->addWidget(answerLineEdit);

        answerMidSpacer2 = new QSpacerItem(20, 12, QSizePolicy::Minimum, QSizePolicy::Fixed);

        answerLayout->addItem(answerMidSpacer2);

        btnSubmitAnswer = new QPushButton(answerPage);
        btnSubmitAnswer->setObjectName(QString::fromUtf8("btnSubmitAnswer"));
        btnSubmitAnswer->setMinimumSize(QSize(0, 50));

        answerLayout->addWidget(btnSubmitAnswer);

        answerBottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        answerLayout->addItem(answerBottomSpacer);

        btnBackFromAnswer = new QPushButton(answerPage);
        btnBackFromAnswer->setObjectName(QString::fromUtf8("btnBackFromAnswer"));
        btnBackFromAnswer->setMinimumSize(QSize(0, 44));

        answerLayout->addWidget(btnBackFromAnswer);

        stackedWidget->addWidget(answerPage);
        settingPage = new QWidget();
        settingPage->setObjectName(QString::fromUtf8("settingPage"));
        settingLayout = new QVBoxLayout(settingPage);
        settingLayout->setObjectName(QString::fromUtf8("settingLayout"));
        settingLayout->setContentsMargins(40, 40, 40, 40);
        settingTopSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        settingLayout->addItem(settingTopSpacer);

        settingLabel = new QLabel(settingPage);
        settingLabel->setObjectName(QString::fromUtf8("settingLabel"));
        settingLabel->setAlignment(Qt::AlignCenter);

        settingLayout->addWidget(settingLabel);

        settingBottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        settingLayout->addItem(settingBottomSpacer);

        btnBackFromSetting = new QPushButton(settingPage);
        btnBackFromSetting->setObjectName(QString::fromUtf8("btnBackFromSetting"));
        btnBackFromSetting->setMinimumSize(QSize(0, 44));

        settingLayout->addWidget(btnBackFromSetting);

        stackedWidget->addWidget(settingPage);

        rootLayout->addWidget(stackedWidget);


        retranslateUi(GameNode);

        stackedWidget->setCurrentIndex(2);


        QMetaObject::connectSlotsByName(GameNode);
    } // setupUi

    void retranslateUi(QWidget *GameNode)
    {
        GameNode->setWindowTitle(QCoreApplication::translate("GameNode", "Escape Room Game", nullptr));
        labelStartGame->setText(QCoreApplication::translate("GameNode", "\342\226\266  START GAME", nullptr));
        answerStageLabel->setText(QCoreApplication::translate("GameNode", "Answer Input (Stage 1)", nullptr));
        answerLineEdit->setPlaceholderText(QCoreApplication::translate("GameNode", "Enter your answer...", nullptr));
        btnSubmitAnswer->setText(QCoreApplication::translate("GameNode", "Submit", nullptr));
        btnBackFromAnswer->setText(QCoreApplication::translate("GameNode", "Back to Title", nullptr));
        settingLabel->setText(QCoreApplication::translate("GameNode", "Settings", nullptr));
        btnBackFromSetting->setText(QCoreApplication::translate("GameNode", "Back to Title", nullptr));
    } // retranslateUi

};

namespace Ui {
    class GameNode: public Ui_GameNode {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_GAME_NODE_H
