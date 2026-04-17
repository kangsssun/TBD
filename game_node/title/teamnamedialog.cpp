#include "teamnamedialog.h"
#include "keyboard/keyboardpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QScreen>

QString TeamNameDialog::getTeamName(QWidget *parent, bool *accepted)
{
    return getInput(parent,
                    QStringLiteral("ENTER TEAM NAME"),
                    QStringLiteral("본인의 팀 이름을 12자 이하로 입력해주세요."),
                    12,
                    accepted);
}

QString TeamNameDialog::getInput(QWidget *parent,
                                 const QString &title,
                                 const QString &subtitle,
                                 int maxLen,
                                 bool *accepted)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("teamDialog"));
    dialog.setWindowTitle(title);

    const QSize baseDialogSize(760, 520);
    QScreen *targetScreen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }

    QRect availableGeometry;
    if (targetScreen) {
        availableGeometry = targetScreen->availableGeometry();
    }

    const int maxDialogWidth = availableGeometry.isValid()
                                   ? qMax(220, availableGeometry.width() - 24)
                                   : baseDialogSize.width();
    const int maxDialogHeight = availableGeometry.isValid()
                                    ? qMax(220, availableGeometry.height() - 24)
                                    : baseDialogSize.height();

    const qreal widthScale = static_cast<qreal>(maxDialogWidth) / static_cast<qreal>(baseDialogSize.width());
    const qreal heightScale = static_cast<qreal>(maxDialogHeight) / static_cast<qreal>(baseDialogSize.height());
    const qreal uiScale = qMin(1.0, qMin(widthScale, heightScale));
    const int scaledTitleFont = qMax(18, qRound(28.0 * uiScale));
    const int scaledTitleTopPad = qMax(1, qRound(4.0 * uiScale));
    const int scaledTitleBottomPad = qMax(2, qRound(8.0 * uiScale));
    const int scaledInputFont = qMax(14, qRound(18.0 * uiScale));
    const int scaledInputVPad = qMax(4, qRound(8.0 * uiScale));
    const int scaledInputHPad = qMax(4, qRound(10.0 * uiScale));

    auto scaledSize = [uiScale](int value, int minValue) {
        return qMax(minValue, qRound(static_cast<qreal>(value) * uiScale));
    };

    dialog.setFixedSize(scaledSize(baseDialogSize.width(), 220), scaledSize(baseDialogSize.height(), 220));
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_NoMousePropagation);
    dialog.setAttribute(Qt::WA_StyledBackground, true);
    dialog.setAutoFillBackground(true);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(scaledSize(24, 10), scaledSize(24, 10), scaledSize(24, 10), scaledSize(20, 8));
    layout->setSpacing(scaledSize(10, 4));

    const int sectionGapTitleToInput = scaledSize(4, 1);
    const int sectionGapInputToKeypad = scaledSize(8, 2);
    const int sectionGapKeypadToButtons = scaledSize(4, 1);

    // Title label
    auto *label = new QLabel(title);
    label->setObjectName(QStringLiteral("teamDialogTitle"));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("font-size:%1px; font-weight:900; padding:%2px 0 %3px 0;")
                             .arg(scaledTitleFont)
                             .arg(scaledTitleTopPad)
                             .arg(scaledTitleBottomPad));
    layout->addWidget(label);

    auto *subtitleLabel = new QLabel(subtitle);
    subtitleLabel->setObjectName(QStringLiteral("teamDialogSubtitle"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitleLabel);

    layout->addSpacing(sectionGapTitleToInput);

    // Input
    const int maxInputLength = maxLen;
    QString inputRaw;
    auto *lineEdit = new QLineEdit();
    lineEdit->setObjectName(QStringLiteral("teamNameLineEdit"));
    lineEdit->setAlignment(Qt::AlignCenter);
    lineEdit->setReadOnly(true);
    lineEdit->setMinimumHeight(scaledSize(48, 32));
    lineEdit->setFixedHeight(scaledSize(48, 32));
    lineEdit->setMaximumWidth(scaledSize(520, 300));
    lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lineEdit->setStyleSheet(QStringLiteral("font-size:%1px; padding:%2px %3px;")
                                .arg(qMax(18, scaledInputFont))
                                .arg(scaledInputVPad)
                                .arg(scaledInputHPad));
    layout->addWidget(lineEdit, 0, Qt::AlignHCenter);

    auto composeDisplayText = [](const QString &source) {
        return composeHangulText(source);
    };

    auto refreshInputDisplay = [&inputRaw, &composeDisplayText, lineEdit]() {
        lineEdit->setText(composeDisplayText(inputRaw) + QStringLiteral("_"));
    };
    refreshInputDisplay();
    layout->addSpacing(sectionGapInputToKeypad);

    // ── On-screen keyboard (touch-friendly) ────────────────────────────
    auto appendCharacter = [&inputRaw, maxInputLength, refreshInputDisplay, &composeDisplayText](const QString &text) {
        const QString candidateRaw = inputRaw + text;
        if (composeDisplayText(candidateRaw).length() <= maxInputLength) {
            inputRaw = candidateRaw;
            refreshInputDisplay();
        }
    };

    auto removeLastCharacter = [&inputRaw, refreshInputDisplay]() {
        if (!inputRaw.isEmpty()) {
            inputRaw.chop(1);
            refreshInputDisplay();
        }
    };

    auto clearInput = [&inputRaw, refreshInputDisplay]() {
        inputRaw.clear();
        refreshInputDisplay();
    };

    const KeyboardPanel keyboardPanel = buildKeyboardPanel(
        &dialog,
        scaledSize,
        appendCharacter,
        removeLastCharacter,
        clearInput);

    layout->addWidget(keyboardPanel.scrollArea, 1);
    layout->addWidget(keyboardPanel.specialButtonBar, 0, Qt::AlignHCenter);
    layout->addSpacing(sectionGapKeypadToButtons);

    // ── Confirm / Cancel buttons ───────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(scaledSize(20, 4));

    auto *btnConfirm = new QPushButton(QStringLiteral("CONFIRM"));
    btnConfirm->setObjectName(QStringLiteral("btnConfirm"));
    btnConfirm->setCursor(Qt::PointingHandCursor);
    btnConfirm->setMinimumHeight(scaledSize(46, 24));
    btnConfirm->setMinimumWidth(scaledSize(250, 140));
    btnConfirm->setFocusPolicy(Qt::NoFocus);
    btnConfirm->setAutoRepeat(false);

    auto *confirmGlow = new QGraphicsDropShadowEffect(btnConfirm);
    confirmGlow->setBlurRadius(20);
    confirmGlow->setColor(QColor(0, 255, 255, 150));
    confirmGlow->setOffset(0, 0);
    btnConfirm->setGraphicsEffect(confirmGlow);

    auto *btnCancel = new QPushButton(QStringLiteral("CANCEL"));
    btnCancel->setObjectName(QStringLiteral("btnCancel"));
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setMinimumHeight(scaledSize(46, 24));
    btnCancel->setMinimumWidth(scaledSize(250, 140));
    btnCancel->setFocusPolicy(Qt::NoFocus);
    btnCancel->setAutoRepeat(false);

    auto *cancelGlow = new QGraphicsDropShadowEffect(btnCancel);
    cancelGlow->setBlurRadius(20);
    cancelGlow->setColor(QColor(255, 0, 255, 150));
    cancelGlow->setOffset(0, 0);
    btnCancel->setGraphicsEffect(cancelGlow);

    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnConfirm);
    layout->addLayout(btnRow);

    QObject::connect(btnConfirm, &QPushButton::clicked, &dialog, [&dialog]() {
        dialog.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dialog, [&dialog]() {
        dialog.reject();
    });

    QPoint dialogPos;
    if (availableGeometry.isValid()) {
        dialogPos = QPoint(
            availableGeometry.left() + (availableGeometry.width() - dialog.width()) / 2,
            availableGeometry.top() + (availableGeometry.height() - dialog.height()) / 2);
    } else if (parent) {
        const QPoint centerGlobal = parent->mapToGlobal(parent->rect().center());
        dialogPos = centerGlobal - QPoint(dialog.width() / 2, dialog.height() / 2);
    } else {
        dialogPos = QPoint(100, 100);
    }
    if (availableGeometry.isValid()) {
        const int minX = availableGeometry.left();
        const int maxX = availableGeometry.right() - dialog.width() + 1;
        const int minY = availableGeometry.top();
        const int maxY = availableGeometry.bottom() - dialog.height() + 1;
        dialogPos.setX(qBound(minX, dialogPos.x(), qMax(minX, maxX)));
        dialogPos.setY(qBound(minY, dialogPos.y(), qMax(minY, maxY)));
    }
    dialog.move(dialogPos);

    const bool isAccepted = (dialog.exec() == QDialog::Accepted);

    if (accepted) {
        *accepted = isAccepted;
    }
    if (isAccepted) {
        return composeDisplayText(inputRaw).trimmed();
    }
    return QString();
}
