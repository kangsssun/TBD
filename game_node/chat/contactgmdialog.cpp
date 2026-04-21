#include "contactgmdialog.h"
#include "../game/readypage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QDialog>
#include <QObject>
#include <QScrollArea>
#include <QStackedWidget>
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QMouseEvent>
#include <QDateTime>
#include <functional>

// ── Hangul compose helpers ─────────────────────────────────────────────────
namespace {

bool isKoreanConsonant(const QChar &ch)
{
    return QStringLiteral("ㄱㄲㄳㄴㄵㄶㄷㄸㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅃㅄㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ").contains(ch);
}

bool isKoreanVowel(const QChar &ch)
{
    return QStringLiteral("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅛㅜㅠㅡㅣ").contains(ch);
}

QString combineInitial(const QChar &a, const QChar &b)
{
    const QString p = QString(a) + b;
    if (p == QStringLiteral("ㄱㄱ")) return QStringLiteral("ㄲ");
    if (p == QStringLiteral("ㄷㄷ")) return QStringLiteral("ㄸ");
    if (p == QStringLiteral("ㅂㅂ")) return QStringLiteral("ㅃ");
    if (p == QStringLiteral("ㅅㅅ")) return QStringLiteral("ㅆ");
    if (p == QStringLiteral("ㅈㅈ")) return QStringLiteral("ㅉ");
    return QString();
}
QString combineMedial(const QChar &a, const QChar &b)
{
    const QString p = QString(a) + b;
    if (p==QStringLiteral("ㅗㅏ")) return QStringLiteral("ㅘ");
    if (p==QStringLiteral("ㅗㅐ")) return QStringLiteral("ㅙ");
    if (p==QStringLiteral("ㅗㅣ")) return QStringLiteral("ㅚ");
    if (p==QStringLiteral("ㅜㅓ")) return QStringLiteral("ㅝ");
    if (p==QStringLiteral("ㅜㅔ")) return QStringLiteral("ㅞ");
    if (p==QStringLiteral("ㅜㅣ")) return QStringLiteral("ㅟ");
    if (p==QStringLiteral("ㅡㅣ")) return QStringLiteral("ㅢ");
    if (p==QStringLiteral("ㅏㅣ")) return QStringLiteral("ㅐ");
    if (p==QStringLiteral("ㅑㅣ")) return QStringLiteral("ㅒ");
    if (p==QStringLiteral("ㅓㅣ")) return QStringLiteral("ㅔ");
    if (p==QStringLiteral("ㅕㅣ")) return QStringLiteral("ㅖ");
    return QString();
}
QString combineFinal(const QChar &a, const QChar &b)
{
    const QString p = QString(a) + b;
    if (p==QStringLiteral("ㄱㅅ")) return QStringLiteral("ㄳ");
    if (p==QStringLiteral("ㄴㅈ")) return QStringLiteral("ㄵ");
    if (p==QStringLiteral("ㄴㅎ")) return QStringLiteral("ㄶ");
    if (p==QStringLiteral("ㄹㄱ")) return QStringLiteral("ㄺ");
    if (p==QStringLiteral("ㄹㅁ")) return QStringLiteral("ㄻ");
    if (p==QStringLiteral("ㄹㅂ")) return QStringLiteral("ㄼ");
    if (p==QStringLiteral("ㄹㅅ")) return QStringLiteral("ㄽ");
    if (p==QStringLiteral("ㄹㅌ")) return QStringLiteral("ㄾ");
    if (p==QStringLiteral("ㄹㅍ")) return QStringLiteral("ㄿ");
    if (p==QStringLiteral("ㄹㅎ")) return QStringLiteral("ㅀ");
    if (p==QStringLiteral("ㅂㅅ")) return QStringLiteral("ㅄ");
    return QString();
}

QString composeHangul(const QString &src)
{
    static const QString cho = QStringLiteral("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ");
    static const QStringList jung = {
        QStringLiteral("ㅏ"),QStringLiteral("ㅐ"),QStringLiteral("ㅑ"),QStringLiteral("ㅒ"),
        QStringLiteral("ㅓ"),QStringLiteral("ㅔ"),QStringLiteral("ㅕ"),QStringLiteral("ㅖ"),
        QStringLiteral("ㅗ"),QStringLiteral("ㅘ"),QStringLiteral("ㅙ"),QStringLiteral("ㅚ"),
        QStringLiteral("ㅛ"),QStringLiteral("ㅜ"),QStringLiteral("ㅝ"),QStringLiteral("ㅞ"),
        QStringLiteral("ㅟ"),QStringLiteral("ㅠ"),QStringLiteral("ㅡ"),QStringLiteral("ㅢ"),
        QStringLiteral("ㅣ")
    };
    static const QStringList jong = {
        QString(),QStringLiteral("ㄱ"),QStringLiteral("ㄲ"),QStringLiteral("ㄳ"),
        QStringLiteral("ㄴ"),QStringLiteral("ㄵ"),QStringLiteral("ㄶ"),QStringLiteral("ㄷ"),
        QStringLiteral("ㄹ"),QStringLiteral("ㄺ"),QStringLiteral("ㄻ"),QStringLiteral("ㄼ"),
        QStringLiteral("ㄽ"),QStringLiteral("ㄾ"),QStringLiteral("ㄿ"),QStringLiteral("ㅀ"),
        QStringLiteral("ㅁ"),QStringLiteral("ㅂ"),QStringLiteral("ㅄ"),QStringLiteral("ㅅ"),
        QStringLiteral("ㅆ"),QStringLiteral("ㅇ"),QStringLiteral("ㅈ"),QStringLiteral("ㅊ"),
        QStringLiteral("ㅋ"),QStringLiteral("ㅌ"),QStringLiteral("ㅍ"),QStringLiteral("ㅎ")
    };
    QString out;
    for (int i = 0; i < src.size();) {
        const QChar c = src.at(i);
        if (c.isSpace() || (!isKoreanConsonant(c) && !isKoreanVowel(c))) { out += c; ++i; continue; }
        if (isKoreanVowel(c)) {
            QString med(1, c); int cm = 1;
            if (i+1 < src.size() && isKoreanVowel(src.at(i+1))) { auto x = combineMedial(c, src.at(i+1)); if (!x.isEmpty()) { med = x; cm = 2; } }
            out += med; i += cm; continue;
        }
        QString ini(1, c); int ci = 1;
        if (i+2 < src.size() && isKoreanConsonant(src.at(i+1)) && isKoreanVowel(src.at(i+2))) { auto x = combineInitial(c, src.at(i+1)); if (!x.isEmpty()) { ini = x; ci = 2; } }
        int choIdx = cho.indexOf(ini.at(0)); int vs = i + ci;
        if (choIdx < 0 || vs >= src.size() || !isKoreanVowel(src.at(vs))) { out += ini; i += ci; continue; }
        QString med(1, src.at(vs)); int cm = 1;
        if (vs+1 < src.size() && isKoreanVowel(src.at(vs+1))) { auto x = combineMedial(src.at(vs), src.at(vs+1)); if (!x.isEmpty()) { med = x; cm = 2; } }
        int jungIdx = jung.indexOf(med); if (jungIdx < 0) { out += ini; i += ci; continue; }
        int jongIdx = 0, cf = 0; int fs = vs + cm;
        if (fs < src.size() && isKoreanConsonant(src.at(fs))) {
            if (fs+1 < src.size() && isKoreanConsonant(src.at(fs+1))) {
                auto x = combineFinal(src.at(fs), src.at(fs+1));
                if (!x.isEmpty() && (fs+2 >= src.size() || !isKoreanVowel(src.at(fs+2)))) { int fi = jong.indexOf(x); if (fi > 0) { jongIdx = fi; cf = 2; } }
            }
            if (cf == 0 && (fs+1 >= src.size() || !isKoreanVowel(src.at(fs+1)))) { int fi = jong.indexOf(QString(1, src.at(fs))); if (fi > 0) { jongIdx = fi; cf = 1; } }
        }
        out += QChar(static_cast<ushort>(0xAC00 + ((choIdx*21)+jungIdx)*28 + jongIdx)); i = fs + cf;
    }
    return out;
}

QString koreanShiftChar(const QString &b)
{
    if (b==QStringLiteral("ㅂ")) return QStringLiteral("ㅃ");
    if (b==QStringLiteral("ㅈ")) return QStringLiteral("ㅉ");
    if (b==QStringLiteral("ㄷ")) return QStringLiteral("ㄸ");
    if (b==QStringLiteral("ㄱ")) return QStringLiteral("ㄲ");
    if (b==QStringLiteral("ㅅ")) return QStringLiteral("ㅆ");
    if (b==QStringLiteral("ㅐ")) return QStringLiteral("ㅒ");
    if (b==QStringLiteral("ㅔ")) return QStringLiteral("ㅖ");
    return b;
}

void connectDebounced(QPushButton *btn, const std::function<void()> &handler, int ms = 80)
{
    QObject::connect(btn, &QPushButton::released, btn, [btn, handler, ms]() {
        if (!btn->isEnabled()) return;
        btn->setEnabled(false);
        QTimer::singleShot(ms, btn, [btn]() { btn->setEnabled(true); });
        handler();
    });
}

// Event filter to show/hide keyboard on touch
class KbToggleFilter : public QObject {
public:
    QWidget *kb;
    QLineEdit *input;
    QWidget *root;

    KbToggleFilter(QWidget *k, QLineEdit *i, QWidget *r, QObject *p)
        : QObject(p), kb(k), input(i), root(r) {}

    bool eventFilter(QObject *watched, QEvent *ev) override {
        if (!kb || !input || !root || ev->type() != QEvent::MouseButtonPress) {
            return QObject::eventFilter(watched, ev);
        }

        auto *me = static_cast<QMouseEvent *>(ev);

        // 입력창 클릭 → 키패드 토글
        const QRect inputRect(input->mapToGlobal(QPoint(0, 0)), input->size());
        if (inputRect.contains(me->globalPos())) {
            kb->setVisible(!kb->isVisible());
            return false;
        }

        // 팝업(dialog) 외부 클릭 → 키패드 닫기
        if (kb->isVisible()) {
            const QRect rootRect(root->mapToGlobal(QPoint(0, 0)), root->size());
            if (!rootRect.contains(me->globalPos())) {
                kb->setVisible(false);
            }
        }

        return QObject::eventFilter(watched, ev);
    }
};

} // anon namespace

void ContactGmDialog::show(QWidget *parent, const QString &teamName,
                           const QStringList &messages,
                           const std::function<void(const QString &)> &sendCb)
{
    // ════════════════════════════════════════════════════════════════════
    // Dialog setup — 760 × 520, dark slate + cyan accent
    // ════════════════════════════════════════════════════════════════════
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("contactGmDialog"));
    dialog.setWindowTitle(QStringLiteral("CONTACT GM"));
    dialog.setModal(true);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_StyledBackground, true);
    dialog.setFixedSize(760, 520);

    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog#contactGmDialog {
            background-color: #0f172a;
            border: 2px solid #06b6d4;
            border-radius: 12px;
        }
        QListWidget#chatList {
            background-color: #1e293b;
            color: #e2e8f0;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 4px;
            font-size: 14px;
            outline: none;
        }
        QLineEdit#chatInput {
            background-color: #1e293b;
            color: #fff;
            border: 1px solid #06b6d4;
            border-radius: 6px;
            padding: 4px 10px;
            font-size: 15px;
        }
        QPushButton#chatSendButton {
            background-color: #0a1628;
            color: #00ffff;
            border: 1px solid #00cccc;
            border-radius: 6px;
            font-size: 14px;
            font-weight: 700;
            font-family: 'Consolas', 'Courier New', monospace;
            padding: 2px 8px;
        }
        QPushButton#chatSendButton:hover { background-color: rgba(0, 255, 255, 40); color: #fff; }
        QPushButton#chatCloseButton {
            background-color: #0a1628;
            color: #ff00ff;
            border: 1px solid #cc00cc;
            border-radius: 6px;
            font-size: 14px;
            font-weight: 700;
            font-family: 'Consolas', 'Courier New', monospace;
            padding: 2px 8px;
        }
        QPushButton#chatCloseButton:hover { background-color: rgba(255, 0, 255, 40); color: #fff; }
        QPushButton#kbKey {
            background-color: #1a1a2e;
            color: #00ffff;
            border: 1px solid #00cccc;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
            font-family: 'Consolas', 'Courier New', monospace;
            padding: 2px;
            outline: none;
        }
        QPushButton#kbKey:hover { background-color: rgba(0, 255, 255, 40); color: #ffffff; }
        QPushButton#kbKey:pressed { background-color: rgba(0, 255, 255, 120); color: #0a0a0f; border-color: #00ffff; }
        QPushButton#kbSpecial {
            background-color: #1a1a2e;
            color: #ff00ff;
            border: 1px solid #cc00cc;
            border-radius: 4px;
            font-size: 13px;
            font-weight: bold;
            font-family: 'Consolas', 'Courier New', monospace;
            padding: 1px;
            outline: none;
        }
        QPushButton#kbSpecial:hover { background-color: rgba(255, 0, 255, 40); color: #ffffff; }
        QPushButton#kbSpecial:pressed { background-color: rgba(255, 0, 255, 120); color: #0a0a0f; border-color: #ff00ff; }
    )"));

    // ════════════════════════════════════════════════════════════════════
    // Pixel budget (520 total height):
    //   margins top 10 + bottom 8 = 18  → usable 502
    //   spacing 6 × 3 gaps = 18         → content 484
    //   chatList  = 180  (fixed)
    //   inputRow  = 32   (fixed)
    //   keyboard  = 272  (remaining, generous)
    // ════════════════════════════════════════════════════════════════════

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(6);

    // ── Chat list (fixed height, scrollable) ───────────────────────────
    auto *chatList = new QListWidget(&dialog);
    chatList->setObjectName(QStringLiteral("chatList"));
    chatList->setFixedHeight(180);
    chatList->viewport()->setStyleSheet(QStringLiteral("background-color:#1e293b;"));

    auto addChatEntry = [chatList](const QString &sender,
                                   const QString &text,
                                   Qt::Alignment bubbleAlign,
                                   const QString &bubbleStyle,
                                   const QString &timeText,
                                   bool showSender,
                                   bool centerOnly = false) {
        auto *item = new QListWidgetItem(chatList);
        auto *wrapper = new QWidget(chatList);
        wrapper->setStyleSheet(QStringLiteral("background:transparent;"));
        auto *outerLayout = new QHBoxLayout(wrapper);
        outerLayout->setContentsMargins(4, 3, 4, 3);
        outerLayout->setSpacing(0);

        if (centerOnly) {
            auto *lbl = new QLabel(text, wrapper);
            lbl->setWordWrap(true);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(QStringLiteral(
                "color:#67e8f9; font-size:12px; background:transparent; border:none; padding:2px 6px;"));
            outerLayout->addWidget(lbl, 1);
        } else {
            auto *area = new QWidget(wrapper);
            area->setStyleSheet(QStringLiteral("background:transparent;"));
            auto *aLayout = new QVBoxLayout(area);
            aLayout->setContentsMargins(0, 0, 0, 0);
            aLayout->setSpacing(0);

            auto *bubble = new QLabel(text, area);
            bubble->setWordWrap(true);
            bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
            bubble->setMaximumWidth(420);
            bubble->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            bubble->setStyleSheet(bubbleStyle);
            aLayout->addWidget(bubble, 0, bubbleAlign);

            if (bubbleAlign == Qt::AlignRight) {
                outerLayout->addStretch();
                outerLayout->addWidget(area, 0, Qt::AlignRight);
            } else {
                outerLayout->addWidget(area, 0, Qt::AlignLeft);
                outerLayout->addStretch();
            }
        }

        item->setSizeHint(wrapper->sizeHint());
        chatList->addItem(item);
        chatList->setItemWidget(item, wrapper);
        chatList->scrollToBottom();
    };

    // Seed initial messages
    addChatEntry(QString(), QString::fromUtf8("GM \xec\xb1\x84\xeb\x84\x90\xec\x9d\xb4 \xec\x97\xb0\xea\xb2\xb0\xeb\x90\x98\xec\x97\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4."),
                 Qt::AlignCenter, QString(), QString(), false, true);

    const QString gmBubble = QStringLiteral(
        "background-color:#164e63; color:#ecfeff; border:1px solid #0e7490; "
        "border-radius:10px; padding:6px 10px; font-size:14px;");
    const QString myBubble = QStringLiteral(
        "background-color:#14532d; color:#fff; border:1px solid #10b981; "
        "border-radius:10px; padding:6px 10px; font-size:14px;");
    for (const auto &msg : messages) {
        if (msg.startsWith(QStringLiteral("[ME] "))) {
            const QString myText = msg.mid(5);
            const QString sender = teamName.isEmpty() ? QStringLiteral("TEAM 1") : teamName;
            addChatEntry(sender.toUpper(), myText, Qt::AlignRight, myBubble,
                         QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")), true);
        } else {
            addChatEntry(QStringLiteral("GM"), msg, Qt::AlignLeft, gmBubble,
                         QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")), true);
        }
    }
    if (messages.isEmpty()) {
        addChatEntry(QStringLiteral("GM"), QString::fromUtf8("\xeb\xac\xb8\xec\x9d\x98\xec\x82\xac\xed\x95\xad\xec\x9d\x84 \xec\x9e\x85\xeb\xa0\xa5\xed\x95\xb4 \xec\xa3\xbc\xec\x84\xb8\xec\x9a\x94."),
                     Qt::AlignLeft, gmBubble,
                     QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")), true);
    }
    mainLayout->addWidget(chatList);

    // ── Input row (fixed height) ───────────────────────────────────────
    QString inputRaw;

    auto *chatInput = new QLineEdit(&dialog);
    chatInput->setObjectName(QStringLiteral("chatInput"));
    chatInput->setPlaceholderText(QString::fromUtf8("\xeb\xa9\x94\xec\x8b\x9c\xec\xa7\x80\xeb\xa5\xbc \xec\x9e\x85\xeb\xa0\xa5\xed\x95\x98\xec\x84\xb8\xec\x9a\x94..."));
    chatInput->setReadOnly(true);
    chatInput->setFixedHeight(32);

    auto refreshInput = [&inputRaw, chatInput]() {
        chatInput->setText(composeHangul(inputRaw));
        chatInput->setCursorPosition(chatInput->text().size());
    };

    auto *sendButton = new QPushButton(QString::fromUtf8("\xec\xa0\x84\xec\x86\xa1"), &dialog);
    sendButton->setObjectName(QStringLiteral("chatSendButton"));
    sendButton->setCursor(Qt::PointingHandCursor);
    sendButton->setFixedSize(72, 36);

    auto *closeButton = new QPushButton(QString::fromUtf8("\xeb\x8b\xab\xea\xb8\xb0"), &dialog);
    closeButton->setObjectName(QStringLiteral("chatCloseButton"));
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFixedSize(72, 36);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    auto *inputRow = new QHBoxLayout();
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(6);
    inputRow->addWidget(chatInput, 1);
    inputRow->addWidget(sendButton);
    inputRow->addWidget(closeButton);
    mainLayout->addLayout(inputRow);

    // ── On-screen keyboard (stretch 1 — takes ALL remaining space) ─────
    const QStringList engRows = {
        QStringLiteral("1234567890"),
        QStringLiteral("QWERTYUIOP"),
        QStringLiteral("ASDFGHJKL"),
        QStringLiteral("ZXCVBNM")
    };
    const QStringList korRows = {
        QStringLiteral("1234567890"),
        QStringLiteral("ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔ"),
        QStringLiteral("ㅁㄴㅇㄹㅎㅗㅓㅏㅣ"),
        QStringLiteral("ㅋㅌㅊㅍㅠㅜㅡ")
    };

    bool koreanMode = true;
    bool shiftOn = false;
    bool upperCase = true;
    QList<QPushButton *> engKeys, korKeys;

    auto *kbArea = new QWidget(&dialog);
    kbArea->setStyleSheet(QStringLiteral("background-color:#0f172a;"));
    auto *kbOuterLayout = new QVBoxLayout(kbArea);
    kbOuterLayout->setContentsMargins(0, 2, 0, 0);
    kbOuterLayout->setSpacing(2);

    auto *kbPages = new QStackedWidget(kbArea);
    kbPages->setStyleSheet(QStringLiteral("background:transparent;"));

    auto buildPage = [&](const QStringList &rows, bool isEng) -> QWidget* {
        auto *pg = new QWidget();
        pg->setStyleSheet(QStringLiteral("background:transparent;"));
        auto *pl = new QVBoxLayout(pg);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(0);
        for (const QString &row : rows) {
            auto *rl = new QHBoxLayout();
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(0);
            rl->addStretch();
            for (const QChar &ch : row) {
                auto *k = new QPushButton(QString(ch));
                k->setObjectName(QStringLiteral("kbKey"));
                k->setFixedSize(46, 34);
                k->setCursor(Qt::PointingHandCursor);
                k->setFocusPolicy(Qt::NoFocus);
                if (isEng && ch.isLetter()) {
                    k->setProperty("up", QString(ch.toUpper()));
                    k->setProperty("lo", QString(ch.toLower()));
                    engKeys.append(k);
                }
                if (!isEng) {
                    k->setProperty("base", QString(ch));
                    k->setProperty("shift", koreanShiftChar(QString(ch)));
                    korKeys.append(k);
                }
                connectDebounced(k, [&inputRaw, refreshInput, k]() {
                    inputRaw += k->text();
                    refreshInput();
                });
                rl->addWidget(k);
            }
            rl->addStretch();
            pl->addLayout(rl);
        }
        return pg;
    };

    kbPages->addWidget(buildPage(engRows, true));
    kbPages->addWidget(buildPage(korRows, false));
    kbPages->setCurrentIndex(1);
    kbOuterLayout->addWidget(kbPages, 1);

    // Special buttons row
    auto *spBar = new QWidget(kbArea);
    spBar->setStyleSheet(QStringLiteral("background:transparent;"));
    auto *spLay = new QHBoxLayout(spBar);
    spLay->setContentsMargins(0, 0, 0, 0);
    spLay->setSpacing(0);
    spLay->addStretch();

    auto makeSpecial = [](const QString &label, int w) {
        auto *b = new QPushButton(label);
        b->setObjectName(QStringLiteral("kbSpecial"));
        b->setFixedSize(w, 32);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    auto *btnClear = makeSpecial(QStringLiteral("CLEAR"), 80);
    auto *btnShift = makeSpecial(QStringLiteral("Shift"), 80);
    auto *btnSpace = makeSpecial(QStringLiteral("SPACE"), 136);
    auto *btnLang  = makeSpecial(QString::fromUtf8("\xed\x95\x9c/\xec\x98\x81"), 80);
    auto *btnDel   = makeSpecial(QStringLiteral("<< DEL"), 80);

    connectDebounced(btnClear, [&inputRaw, refreshInput]() { inputRaw.clear(); refreshInput(); });
    connectDebounced(btnDel,   [&inputRaw, refreshInput]() { if (!inputRaw.isEmpty()) { inputRaw.chop(1); refreshInput(); } });
    connectDebounced(btnSpace, [&inputRaw, refreshInput]() { inputRaw += QStringLiteral(" "); refreshInput(); });

    connectDebounced(btnShift, [&shiftOn, &upperCase, &koreanMode, &korKeys, &engKeys, btnShift]() {
        if (koreanMode) {
            shiftOn = !shiftOn;
            for (auto *k : korKeys) k->setText(shiftOn ? k->property("shift").toString() : k->property("base").toString());
            btnShift->setText(shiftOn ? QStringLiteral("SHIFT") : QStringLiteral("Shift"));
        } else {
            upperCase = !upperCase;
            for (auto *k : engKeys) k->setText(upperCase ? k->property("up").toString() : k->property("lo").toString());
        }
    });

    connectDebounced(btnLang, [&koreanMode, &shiftOn, kbPages, &korKeys, btnLang, btnShift]() {
        koreanMode = !koreanMode;
        kbPages->setCurrentIndex(koreanMode ? 1 : 0);
        btnLang->setText(koreanMode ? QString::fromUtf8("\xed\x95\x9c/\xec\x98\x81") : QString::fromUtf8("\xec\x98\x81/\xed\x95\x9c"));
        if (koreanMode) {
            shiftOn = false;
            for (auto *k : korKeys) k->setText(k->property("base").toString());
            btnShift->setText(QStringLiteral("Shift"));
        }
    });

    spLay->addWidget(btnClear);
    spLay->addWidget(btnShift);
    spLay->addWidget(btnSpace);
    spLay->addWidget(btnLang);
    spLay->addWidget(btnDel);
    spLay->addStretch();
    kbOuterLayout->addWidget(spBar);

    // Keyboard area takes ALL remaining vertical space (stretch 1)
    kbArea->setVisible(false);
    mainLayout->addWidget(kbArea, 1);

    auto *kbToggleFilter = new KbToggleFilter(kbArea, chatInput, &dialog, &dialog);
    qApp->installEventFilter(kbToggleFilter);

    // ── Send message ───────────────────────────────────────────────────
    auto doSend = [&inputRaw, refreshInput, chatList, &teamName, &sendCb, addChatEntry]() {
        const QString composed = composeHangul(inputRaw).trimmed();
        if (composed.isEmpty()) return;

        const QString sender = teamName.isEmpty() ? QStringLiteral("TEAM 1") : teamName;
        addChatEntry(sender.toUpper(), composed, Qt::AlignRight,
                     QStringLiteral("background-color:#14532d; color:#fff; border:1px solid #10b981; "
                                    "border-radius:10px; padding:6px 10px; font-size:14px;"),
                     QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")), true);

        if (sendCb) sendCb(composed);
        inputRaw.clear();
        refreshInput();
    };

    QObject::connect(sendButton, &QPushButton::clicked, &dialog, doSend);

    // ── Position & show ────────────────────────────────────────────────
    if (parent) {
        const QPoint c = parent->mapToGlobal(parent->rect().center());
        dialog.move(c - QPoint(dialog.width() / 2, dialog.height() / 2));
    }

    // ── GM 메시지 실시간 수신 연결 ──────────────────────────────────────
    QMetaObject::Connection gmConn;
    auto *readyPage = qobject_cast<ReadyPage *>(parent);
    if (readyPage) {
        gmConn = QObject::connect(readyPage, &ReadyPage::gmMessageReceived,
                                  &dialog, [addChatEntry](const QString &text) {
            addChatEntry(QStringLiteral("GM"), text, Qt::AlignLeft,
                         QStringLiteral("background-color:#164e63; color:#ecfeff; border:1px solid #0e7490; "
                                        "border-radius:10px; padding:6px 10px; font-size:14px;"),
                         QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")), true);
        });
    }

    dialog.exec();
    if (gmConn) QObject::disconnect(gmConn);
    qApp->removeEventFilter(kbToggleFilter);
}
