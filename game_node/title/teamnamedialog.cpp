#include "teamnamedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QStackedWidget>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <functional>
#include <memory>

namespace {

bool isKoreanConsonant(const QChar &ch)
{
    return QStringLiteral("ㄱㄲㄳㄴㄵㄶㄷㄸㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅃㅄㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ").contains(ch);
}

bool isKoreanVowel(const QChar &ch)
{
    return QStringLiteral("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅛㅜㅠㅡㅣ").contains(ch);
}

QString combineInitial(const QChar &first, const QChar &second)
{
    const QString pair = QString(first) + second;
    if (pair == QStringLiteral("ㄱㄱ")) return QStringLiteral("ㄲ");
    if (pair == QStringLiteral("ㄷㄷ")) return QStringLiteral("ㄸ");
    if (pair == QStringLiteral("ㅂㅂ")) return QStringLiteral("ㅃ");
    if (pair == QStringLiteral("ㅅㅅ")) return QStringLiteral("ㅆ");
    if (pair == QStringLiteral("ㅈㅈ")) return QStringLiteral("ㅉ");
    return QString();
}

QString combineMedial(const QChar &first, const QChar &second)
{
    const QString pair = QString(first) + second;
    if (pair == QStringLiteral("ㅗㅏ")) return QStringLiteral("ㅘ");
    if (pair == QStringLiteral("ㅗㅐ")) return QStringLiteral("ㅙ");
    if (pair == QStringLiteral("ㅗㅣ")) return QStringLiteral("ㅚ");
    if (pair == QStringLiteral("ㅜㅓ")) return QStringLiteral("ㅝ");
    if (pair == QStringLiteral("ㅜㅔ")) return QStringLiteral("ㅞ");
    if (pair == QStringLiteral("ㅜㅣ")) return QStringLiteral("ㅟ");
    if (pair == QStringLiteral("ㅡㅣ")) return QStringLiteral("ㅢ");
    if (pair == QStringLiteral("ㅏㅣ")) return QStringLiteral("ㅐ");
    if (pair == QStringLiteral("ㅑㅣ")) return QStringLiteral("ㅒ");
    if (pair == QStringLiteral("ㅓㅣ")) return QStringLiteral("ㅔ");
    if (pair == QStringLiteral("ㅕㅣ")) return QStringLiteral("ㅖ");
    return QString();
}

QString combineFinal(const QChar &first, const QChar &second)
{
    const QString pair = QString(first) + second;
    if (pair == QStringLiteral("ㄱㅅ")) return QStringLiteral("ㄳ");
    if (pair == QStringLiteral("ㄴㅈ")) return QStringLiteral("ㄵ");
    if (pair == QStringLiteral("ㄴㅎ")) return QStringLiteral("ㄶ");
    if (pair == QStringLiteral("ㄹㄱ")) return QStringLiteral("ㄺ");
    if (pair == QStringLiteral("ㄹㅁ")) return QStringLiteral("ㄻ");
    if (pair == QStringLiteral("ㄹㅂ")) return QStringLiteral("ㄼ");
    if (pair == QStringLiteral("ㄹㅅ")) return QStringLiteral("ㄽ");
    if (pair == QStringLiteral("ㄹㅌ")) return QStringLiteral("ㄾ");
    if (pair == QStringLiteral("ㄹㅍ")) return QStringLiteral("ㄿ");
    if (pair == QStringLiteral("ㄹㅎ")) return QStringLiteral("ㅀ");
    if (pair == QStringLiteral("ㅂㅅ")) return QStringLiteral("ㅄ");
    return QString();
}

QString composeHangulText(const QString &source)
{
    static const QString choseongList = QStringLiteral("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ");
    static const QStringList jungseongList = {
        QStringLiteral("ㅏ"), QStringLiteral("ㅐ"), QStringLiteral("ㅑ"), QStringLiteral("ㅒ"), QStringLiteral("ㅓ"), QStringLiteral("ㅔ"), QStringLiteral("ㅕ"), QStringLiteral("ㅖ"),
        QStringLiteral("ㅗ"), QStringLiteral("ㅘ"), QStringLiteral("ㅙ"), QStringLiteral("ㅚ"), QStringLiteral("ㅛ"), QStringLiteral("ㅜ"), QStringLiteral("ㅝ"), QStringLiteral("ㅞ"),
        QStringLiteral("ㅟ"), QStringLiteral("ㅠ"), QStringLiteral("ㅡ"), QStringLiteral("ㅢ"), QStringLiteral("ㅣ")
    };
    static const QStringList jongseongList = {
        QString(), QStringLiteral("ㄱ"), QStringLiteral("ㄲ"), QStringLiteral("ㄳ"), QStringLiteral("ㄴ"), QStringLiteral("ㄵ"), QStringLiteral("ㄶ"), QStringLiteral("ㄷ"),
        QStringLiteral("ㄹ"), QStringLiteral("ㄺ"), QStringLiteral("ㄻ"), QStringLiteral("ㄼ"), QStringLiteral("ㄽ"), QStringLiteral("ㄾ"), QStringLiteral("ㄿ"), QStringLiteral("ㅀ"),
        QStringLiteral("ㅁ"), QStringLiteral("ㅂ"), QStringLiteral("ㅄ"), QStringLiteral("ㅅ"), QStringLiteral("ㅆ"), QStringLiteral("ㅇ"), QStringLiteral("ㅈ"), QStringLiteral("ㅊ"),
        QStringLiteral("ㅋ"), QStringLiteral("ㅌ"), QStringLiteral("ㅍ"), QStringLiteral("ㅎ")
    };

    QString output;

    for (int i = 0; i < source.size();) {
        const QChar current = source.at(i);

        if (current.isSpace()) {
            output += current;
            ++i;
            continue;
        }

        if (!isKoreanConsonant(current) && !isKoreanVowel(current)) {
            output += current;
            ++i;
            continue;
        }

        if (isKoreanVowel(current)) {
            QString medial(1, current);
            int consumedMedial = 1;
            if (i + 1 < source.size() && isKoreanVowel(source.at(i + 1))) {
                const QString combined = combineMedial(current, source.at(i + 1));
                if (!combined.isEmpty()) {
                    medial = combined;
                    consumedMedial = 2;
                }
            }
            output += medial;
            i += consumedMedial;
            continue;
        }

        QString initial(1, current);
        int consumedInitial = 1;
        if (i + 2 < source.size() && isKoreanConsonant(source.at(i + 1)) && isKoreanVowel(source.at(i + 2))) {
            const QString combined = combineInitial(current, source.at(i + 1));
            if (!combined.isEmpty()) {
                initial = combined;
                consumedInitial = 2;
            }
        }

        const int choseongIndex = choseongList.indexOf(initial.at(0));
        const int vowelStart = i + consumedInitial;
        if (choseongIndex < 0 || vowelStart >= source.size() || !isKoreanVowel(source.at(vowelStart))) {
            output += initial;
            i += consumedInitial;
            continue;
        }

        QString medial(1, source.at(vowelStart));
        int consumedMedial = 1;
        if (vowelStart + 1 < source.size() && isKoreanVowel(source.at(vowelStart + 1))) {
            const QString combined = combineMedial(source.at(vowelStart), source.at(vowelStart + 1));
            if (!combined.isEmpty()) {
                medial = combined;
                consumedMedial = 2;
            }
        }

        const int jungseongIndex = jungseongList.indexOf(medial);
        if (jungseongIndex < 0) {
            output += initial;
            i += consumedInitial;
            continue;
        }

        int jongseongIndex = 0;
        int consumedFinal = 0;
        const int finalStart = vowelStart + consumedMedial;
        if (finalStart < source.size() && isKoreanConsonant(source.at(finalStart))) {
            if (finalStart + 1 < source.size() && isKoreanConsonant(source.at(finalStart + 1))) {
                const QString combined = combineFinal(source.at(finalStart), source.at(finalStart + 1));
                if (!combined.isEmpty() && (finalStart + 2 >= source.size() || !isKoreanVowel(source.at(finalStart + 2)))) {
                    const int finalIndex = jongseongList.indexOf(combined);
                    if (finalIndex > 0) {
                        jongseongIndex = finalIndex;
                        consumedFinal = 2;
                    }
                }
            }

            if (consumedFinal == 0 && (finalStart + 1 >= source.size() || !isKoreanVowel(source.at(finalStart + 1)))) {
                const QString singleFinal(1, source.at(finalStart));
                const int finalIndex = jongseongList.indexOf(singleFinal);
                if (finalIndex > 0) {
                    jongseongIndex = finalIndex;
                    consumedFinal = 1;
                }
            }
        }

        const ushort syllable = static_cast<ushort>(0xAC00 + ((choseongIndex * 21) + jungseongIndex) * 28 + jongseongIndex);
        output += QChar(syllable);
        i = finalStart + consumedFinal;
    }

    return output;
}

QString koreanShiftChar(const QString &baseChar)
{
    if (baseChar == QStringLiteral("ㅂ")) return QStringLiteral("ㅃ");
    if (baseChar == QStringLiteral("ㅈ")) return QStringLiteral("ㅉ");
    if (baseChar == QStringLiteral("ㄷ")) return QStringLiteral("ㄸ");
    if (baseChar == QStringLiteral("ㄱ")) return QStringLiteral("ㄲ");
    if (baseChar == QStringLiteral("ㅅ")) return QStringLiteral("ㅆ");
    if (baseChar == QStringLiteral("ㅐ")) return QStringLiteral("ㅒ");
    if (baseChar == QStringLiteral("ㅔ")) return QStringLiteral("ㅖ");
    return baseChar;
}

void connectDebouncedReleased(QPushButton *button, const std::function<void()> &handler, int debounceMs = 120)
{
    QObject::connect(button, &QPushButton::released, button, [button, handler, debounceMs]() {
        if (!button->isEnabled()) return;
        button->setEnabled(false);
        QTimer::singleShot(debounceMs, button, [button]() { button->setEnabled(true); });
        handler();
    });
}

QWidget *createKeyboardPage(
    QWidget *keypadContainer,
    const QStringList &rowsForPage,
    const std::function<int(int, int)> &scaledSize,
    const std::function<void(const QString &)> &appendCharacter,
    bool collectAlphabetKeys,
    bool collectKoreanShiftKeys,
    QList<QPushButton *> &alphabetKeys,
    QList<QPushButton *> &koreanShiftKeys)
{
    auto *page = new QWidget(keypadContainer);
    page->setObjectName(QStringLiteral("keyboardPage"));
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(scaledSize(2, 1));

    for (const QString &row : rowsForPage) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(scaledSize(2, 1));
        rowLayout->addStretch();

        for (const QChar &ch : row) {
            auto *key = new QPushButton(QString(ch));
            key->setObjectName(QStringLiteral("kbKey"));
            key->setFixedSize(scaledSize(46, 22), scaledSize(34, 18));
            key->setCursor(Qt::PointingHandCursor);
            key->setFocusPolicy(Qt::NoFocus);
            key->setAutoRepeat(false);

            if (collectAlphabetKeys && ch.isLetter()) {
                key->setProperty("upperChar", QString(ch.toUpper()));
                key->setProperty("lowerChar", QString(ch.toLower()));
                alphabetKeys.append(key);
            }

            if (collectKoreanShiftKeys) {
                const QString baseChar = QString(ch);
                key->setProperty("baseChar", baseChar);
                key->setProperty("shiftChar", koreanShiftChar(baseChar));
                koreanShiftKeys.append(key);
            }

            connectDebouncedReleased(key, [appendCharacter, key]() {
                appendCharacter(key->text());
            });

            rowLayout->addWidget(key);
        }

        rowLayout->addStretch();
        pageLayout->addLayout(rowLayout);
    }

    return page;
}

struct KeyboardPanel
{
    QScrollArea *scrollArea = nullptr;
    QWidget *specialButtonBar = nullptr;
};

KeyboardPanel buildKeyboardPanel(
    QDialog *dialog,
    const std::function<int(int, int)> &scaledSize,
    const std::function<void(const QString &)> &appendCharacter,
    const std::function<void()> &removeLastCharacter,
    const std::function<void()> &clearInput)
{
    const QStringList englishRows = {
        QStringLiteral("1234567890"),
        QStringLiteral("QWERTYUIOP"),
        QStringLiteral("ASDFGHJKL"),
        QStringLiteral("ZXCVBNM")
    };
    const QStringList koreanRows = {
        QStringLiteral("1234567890"),
        QStringLiteral("ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔ"),
        QStringLiteral("ㅁㄴㅇㄹㅎㅗㅓㅏㅣ"),
        QStringLiteral("ㅋㅌㅊㅍㅠㅜㅡ")
    };

    struct KeyboardState {
        bool uppercaseMode = true;
        bool koreanMode = true;
        bool koreanShiftMode = false;
        QList<QPushButton *> alphabetKeys;
        QList<QPushButton *> koreanShiftKeys;
    };
    auto ks = std::make_shared<KeyboardState>();

    auto *keypadContainer = new QWidget(dialog);
    keypadContainer->setObjectName(QStringLiteral("teamKeypadContainer"));
    keypadContainer->setMaximumWidth(scaledSize(680, 380));

    auto *keypadLayout = new QVBoxLayout(keypadContainer);
    keypadLayout->setContentsMargins(0, 0, 0, 0);
    keypadLayout->setSpacing(scaledSize(2, 1));

    auto *keyboardPages = new QStackedWidget(keypadContainer);
    keyboardPages->setObjectName(QStringLiteral("keyboardPages"));
    keyboardPages->addWidget(createKeyboardPage(
        keypadContainer,
        englishRows,
        scaledSize,
        appendCharacter,
        true,
        false,
        ks->alphabetKeys,
        ks->koreanShiftKeys));
    keyboardPages->addWidget(createKeyboardPage(
        keypadContainer,
        koreanRows,
        scaledSize,
        appendCharacter,
        false,
        true,
        ks->alphabetKeys,
        ks->koreanShiftKeys));
    keyboardPages->setCurrentIndex(1);
    keypadLayout->addWidget(keyboardPages);

    auto refreshKoreanShiftKeys = [ks](bool shiftOn) {
        for (auto *key : ks->koreanShiftKeys) {
            const QString nextText = shiftOn
                ? key->property("shiftChar").toString()
                : key->property("baseChar").toString();
            key->setText(nextText);
        }
    };
    refreshKoreanShiftKeys(false);

    auto *btnBackspace = new QPushButton(QStringLiteral("<< DEL"));
    btnBackspace->setObjectName(QStringLiteral("kbSpecial"));
    btnBackspace->setFixedSize(scaledSize(80, 52), scaledSize(32, 18));
    btnBackspace->setCursor(Qt::PointingHandCursor);
    btnBackspace->setFocusPolicy(Qt::NoFocus);
    btnBackspace->setAutoRepeat(false);
    connectDebouncedReleased(btnBackspace, [removeLastCharacter]() {
        removeLastCharacter();
    });

    auto *btnCaseToggle = new QPushButton(QStringLiteral("Shift"));
    btnCaseToggle->setObjectName(QStringLiteral("kbSpecial"));
    btnCaseToggle->setFixedSize(scaledSize(80, 52), scaledSize(32, 18));
    btnCaseToggle->setCursor(Qt::PointingHandCursor);
    btnCaseToggle->setFocusPolicy(Qt::NoFocus);
    btnCaseToggle->setAutoRepeat(false);
    connectDebouncedReleased(btnCaseToggle, [ks, btnCaseToggle, refreshKoreanShiftKeys]() {
        if (ks->koreanMode) {
            ks->koreanShiftMode = !ks->koreanShiftMode;
            refreshKoreanShiftKeys(ks->koreanShiftMode);
            btnCaseToggle->setText(ks->koreanShiftMode ? QStringLiteral("SHIFT") : QStringLiteral("Shift"));
        } else {
            ks->uppercaseMode = !ks->uppercaseMode;
            for (auto *alphabetKey : ks->alphabetKeys) {
                const QString nextText = ks->uppercaseMode
                    ? alphabetKey->property("upperChar").toString()
                    : alphabetKey->property("lowerChar").toString();
                alphabetKey->setText(nextText);
            }
            btnCaseToggle->setText(QStringLiteral("Aa"));
        }
    });

    auto *btnLanguageToggle = new QPushButton(QStringLiteral("영/한"));
    btnLanguageToggle->setObjectName(QStringLiteral("kbSpecial"));
    btnLanguageToggle->setFixedSize(scaledSize(80, 52), scaledSize(32, 18));
    btnLanguageToggle->setCursor(Qt::PointingHandCursor);
    btnLanguageToggle->setFocusPolicy(Qt::NoFocus);
    btnLanguageToggle->setAutoRepeat(false);
    connectDebouncedReleased(btnLanguageToggle, [ks, keyboardPages, btnLanguageToggle, btnCaseToggle, refreshKoreanShiftKeys]() {
        ks->koreanMode = !ks->koreanMode;
        keyboardPages->setCurrentIndex(ks->koreanMode ? 1 : 0);
        btnLanguageToggle->setText(ks->koreanMode ? QStringLiteral("영/한") : QStringLiteral("한/영"));
        if (ks->koreanMode) {
            ks->koreanShiftMode = false;
            refreshKoreanShiftKeys(false);
            btnCaseToggle->setText(QStringLiteral("Shift"));
        } else {
            btnCaseToggle->setText(QStringLiteral("Aa"));
        }
    });

    auto *btnSpace = new QPushButton(QStringLiteral("SPACE"));
    btnSpace->setObjectName(QStringLiteral("kbSpecial"));
    btnSpace->setFixedSize(scaledSize(136, 54), scaledSize(32, 18));
    btnSpace->setCursor(Qt::PointingHandCursor);
    btnSpace->setFocusPolicy(Qt::NoFocus);
    btnSpace->setAutoRepeat(false);
    connectDebouncedReleased(btnSpace, [appendCharacter]() {
        appendCharacter(QStringLiteral(" "));
    });

    auto *btnClear = new QPushButton(QStringLiteral("CLEAR"));
    btnClear->setObjectName(QStringLiteral("kbSpecial"));
    btnClear->setFixedSize(scaledSize(80, 52), scaledSize(32, 18));
    btnClear->setCursor(Qt::PointingHandCursor);
    btnClear->setFocusPolicy(Qt::NoFocus);
    btnClear->setAutoRepeat(false);
    connectDebouncedReleased(btnClear, [clearInput]() {
        clearInput();
    });

    btnCaseToggle->setEnabled(true);

    auto *keypadScroll = new QScrollArea(dialog);
    keypadScroll->setObjectName(QStringLiteral("teamKeypadScroll"));
    keypadScroll->setFrameShape(QFrame::NoFrame);
    keypadScroll->setWidgetResizable(true);
    keypadScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    keypadScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    keypadScroll->setWidget(keypadContainer);
    keypadScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    keypadScroll->setMaximumWidth(scaledSize(700, 400));

    auto *specialButtonBar = new QWidget(dialog);
    specialButtonBar->setObjectName(QStringLiteral("teamKeypadSpecialBar"));
    auto *specialButtonLayout = new QHBoxLayout(specialButtonBar);
    specialButtonLayout->setContentsMargins(0, 0, 0, 0);
    specialButtonLayout->setSpacing(scaledSize(2, 1));
    specialButtonLayout->addStretch();
    specialButtonLayout->addWidget(btnClear);
    specialButtonLayout->addWidget(btnCaseToggle);
    specialButtonLayout->addWidget(btnSpace);
    specialButtonLayout->addWidget(btnLanguageToggle);
    specialButtonLayout->addWidget(btnBackspace);
    specialButtonLayout->addStretch();

    return { keypadScroll, specialButtonBar };
}

} // namespace

QString TeamNameDialog::getTeamName(QWidget *parent, bool *accepted)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("teamDialog"));
    dialog.setWindowTitle(QStringLiteral("TEAM REGISTRATION"));

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
    auto *label = new QLabel(QStringLiteral("ENTER TEAM NAME"));
    label->setObjectName(QStringLiteral("teamDialogTitle"));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("font-size:%1px; font-weight:900; padding:%2px 0 %3px 0;")
                             .arg(scaledTitleFont)
                             .arg(scaledTitleTopPad)
                             .arg(scaledTitleBottomPad));
    layout->addWidget(label);

    auto *subtitleLabel = new QLabel(QStringLiteral("본인의 팀 이름을 12자 이하로 입력해주세요."));
    subtitleLabel->setObjectName(QStringLiteral("teamDialogSubtitle"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitleLabel);

    layout->addSpacing(sectionGapTitleToInput);

    // Team name input
    const int maxTeamNameLength = 12;
    QString teamNameInputRaw;
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

    auto refreshInputDisplay = [&teamNameInputRaw, &composeDisplayText, lineEdit]() {
        lineEdit->setText(composeDisplayText(teamNameInputRaw) + QStringLiteral("_"));
    };
    refreshInputDisplay();
    layout->addSpacing(sectionGapInputToKeypad);

    // ── On-screen keyboard (touch-friendly) ────────────────────────────
    auto appendCharacter = [&teamNameInputRaw, maxTeamNameLength, refreshInputDisplay, &composeDisplayText](const QString &text) {
        const QString candidateRaw = teamNameInputRaw + text;
        if (composeDisplayText(candidateRaw).length() <= maxTeamNameLength) {
            teamNameInputRaw = candidateRaw;
            refreshInputDisplay();
        }
    };

    auto removeLastCharacter = [&teamNameInputRaw, refreshInputDisplay]() {
        if (!teamNameInputRaw.isEmpty()) {
            teamNameInputRaw.chop(1);
            refreshInputDisplay();
        }
    };

    auto clearInput = [&teamNameInputRaw, refreshInputDisplay]() {
        teamNameInputRaw.clear();
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
    if (parent) {
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
        return composeDisplayText(teamNameInputRaw).trimmed();
    }
    return QString();
}
