#include "contactgmdialog.h"

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
#include <QScreen>
#include <QTimer>
#include <QMouseEvent>
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

void connectDebounced(QPushButton *btn, const std::function<void()> &handler, int ms = 120)
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
    QWidget *kb; QLineEdit *input;
    KbToggleFilter(QWidget *k, QLineEdit *i, QObject *p) : QObject(p), kb(k), input(i) {}
    bool eventFilter(QObject *, QEvent *ev) override {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(ev);
            QPoint ip = input->mapFromGlobal(me->globalPos());
            if (input->rect().contains(ip)) { kb->setVisible(true); return false; }
            QPoint kp = kb->mapFromGlobal(me->globalPos());
            if (kb->isVisible() && kb->rect().contains(kp)) { return false; }
            kb->setVisible(false);
        }
        return false;
    }
};

} // anon namespace

void ContactGmDialog::show(QWidget *parent, const QString &teamName,
                           const QStringList &messages,
                           const std::function<void(const QString &)> &sendCb)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("contactGmDialog"));
    dialog.setWindowTitle(QStringLiteral("CONTACT GM"));
    dialog.setModal(true);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_StyledBackground, true);

    QScreen *scr = parent ? parent->screen() : QGuiApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0,0,760,600);
    int dw = qMin(760, avail.width() - 20);
    int dh = qMin(600, avail.height() - 20);
    dialog.setFixedSize(dw, dh);
    qreal sc = qMin(1.0, qMin(dw/760.0, dh/600.0));
    auto sz = [sc](int v, int m) { return qMax(m, qRound(v * sc)); };

    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog#contactGmDialog { background-color: #0b1220; border: 1px solid #10b981; border-radius: 12px; }
        QLabel#contactTitle { background:transparent; color:#fff; font-size:22px; font-weight:800; }
        QListWidget#chatList { background-color:#111827; color:#e5e7eb; border:1px solid #1f2937; border-radius:8px; padding:6px; font-size:14px; }
        QLineEdit#chatInput { background-color:#111827; color:#fff; border:1px solid #334155; border-radius:8px; padding:6px 8px; font-size:14px; }
        QPushButton#chatSendButton { background-color:#14532d; color:#fff; border:1px solid #10b981; border-radius:8px; padding:6px 12px; font-size:14px; font-weight:700; }
        QPushButton#chatSendButton:hover { background-color:#166534; }
        QPushButton#chatCloseButton { background-color:#142338; color:#bfdbfe; border:1px solid #2563eb; border-radius:8px; padding:8px 18px; font-size:14px; font-weight:700; }
        QPushButton#chatCloseButton:hover { background-color:#1a3150; color:#fff; }
        QPushButton#kbKey { background-color:#1e293b; color:#e2e8f0; border:1px solid #334155; border-radius:4px; font-size:13px; font-weight:600; }
        QPushButton#kbKey:pressed { background-color:#2563eb; }
        QPushButton#kbSpecial { background-color:#0f172a; color:#94a3b8; border:1px solid #334155; border-radius:4px; font-size:11px; font-weight:700; }
        QPushButton#kbSpecial:pressed { background-color:#334155; }
        QScrollArea#kbScroll { background:transparent; border:none; }
    )"));

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 14, 16, 12);
    mainLayout->setSpacing(8);

    auto *titleLabel = new QLabel(QStringLiteral("CONTACT GM"), &dialog);
    titleLabel->setObjectName(QStringLiteral("contactTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // ── Chat list ──────────────────────────────────────────────────────
    auto *chatList = new QListWidget(&dialog);
    chatList->setObjectName(QStringLiteral("chatList"));
    chatList->addItem(QStringLiteral("[SYSTEM] CONTACT GM 채널이 연결되었습니다."));
    for (const auto &msg : messages)
        chatList->addItem(QStringLiteral("[GM] %1").arg(msg));
    if (messages.isEmpty())
        chatList->addItem(QStringLiteral("[GM] 문의사항을 입력해 주세요."));
    mainLayout->addWidget(chatList, 1);

    // ── Input row ──────────────────────────────────────────────────────
    QString inputRaw;

    auto *chatInput = new QLineEdit(&dialog);
    chatInput->setObjectName(QStringLiteral("chatInput"));
    chatInput->setPlaceholderText(QStringLiteral("질문 또는 요청사항을 입력하세요."));
    chatInput->setReadOnly(true);

    auto refreshInput = [&inputRaw, chatInput]() {
        chatInput->setText(composeHangul(inputRaw));
    };

    auto *sendButton = new QPushButton(QStringLiteral("전송"), &dialog);
    sendButton->setObjectName(QStringLiteral("chatSendButton"));
    sendButton->setCursor(Qt::PointingHandCursor);
    sendButton->setMinimumWidth(72);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);
    inputRow->addWidget(chatInput, 1);
    inputRow->addWidget(sendButton);
    mainLayout->addLayout(inputRow);

    // ── On-screen keyboard ─────────────────────────────────────────────
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

    auto *kbContainer = new QWidget(&dialog);
    auto *kbLayout = new QVBoxLayout(kbContainer);
    kbLayout->setContentsMargins(0, 0, 0, 0);
    kbLayout->setSpacing(sz(2, 1));

    auto *kbPages = new QStackedWidget(kbContainer);

    auto buildPage = [&](const QStringList &rows, bool isEng) -> QWidget* {
        auto *pg = new QWidget();
        auto *pl = new QVBoxLayout(pg); pl->setContentsMargins(0,0,0,0); pl->setSpacing(sz(2,1));
        for (const QString &row : rows) {
            auto *rl = new QHBoxLayout(); rl->setContentsMargins(0,0,0,0); rl->setSpacing(sz(2,1)); rl->addStretch();
            for (const QChar &ch : row) {
                auto *k = new QPushButton(QString(ch));
                k->setObjectName(QStringLiteral("kbKey"));
                k->setFixedSize(sz(40, 20), sz(30, 16));
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
            rl->addStretch(); pl->addLayout(rl);
        }
        return pg;
    };

    kbPages->addWidget(buildPage(engRows, true));
    kbPages->addWidget(buildPage(korRows, false));
    kbPages->setCurrentIndex(1);
    kbLayout->addWidget(kbPages);

    // Special buttons row
    auto *spBar = new QWidget(kbContainer);
    auto *spLay = new QHBoxLayout(spBar); spLay->setContentsMargins(0,0,0,0); spLay->setSpacing(sz(2,1)); spLay->addStretch();

    auto makeSpecial = [&](const QString &label, int w) {
        auto *b = new QPushButton(label);
        b->setObjectName(QStringLiteral("kbSpecial"));
        b->setFixedSize(sz(w, 40), sz(28, 16));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    auto *btnClear = makeSpecial(QStringLiteral("CLR"), 56);
    auto *btnShift = makeSpecial(QStringLiteral("Shift"), 64);
    auto *btnSpace = makeSpecial(QStringLiteral("SPACE"), 100);
    auto *btnLang  = makeSpecial(QStringLiteral("A/H"), 56);
    auto *btnDel   = makeSpecial(QStringLiteral("DEL"), 56);

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
        btnLang->setText(koreanMode ? QStringLiteral("A/H") : QStringLiteral("H/A"));
        if (koreanMode) { shiftOn = false; for (auto *k : korKeys) k->setText(k->property("base").toString()); btnShift->setText(QStringLiteral("Shift")); }
    });

    spLay->addWidget(btnClear); spLay->addWidget(btnShift); spLay->addWidget(btnSpace);
    spLay->addWidget(btnLang); spLay->addWidget(btnDel); spLay->addStretch();
    kbLayout->addWidget(spBar);

    auto *kbScroll = new QScrollArea(&dialog);
    kbScroll->setObjectName(QStringLiteral("kbScroll"));
    kbScroll->setFrameShape(QFrame::NoFrame);
    kbScroll->setWidgetResizable(true);
    kbScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    kbScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    kbScroll->setWidget(kbContainer);
    kbScroll->setVisible(false); // Initially hidden

    mainLayout->addWidget(kbScroll);

    // ── Keyboard toggle filter ─────────────────────────────────────────
    auto *filter = new KbToggleFilter(kbScroll, chatInput, &dialog);
    dialog.installEventFilter(filter);

    // ── Send message ───────────────────────────────────────────────────
    auto doSend = [&inputRaw, refreshInput, chatList, &teamName, &sendCb]() {
        const QString composed = composeHangul(inputRaw).trimmed();
        if (composed.isEmpty()) return;

        const QString sender = teamName.isEmpty() ? QStringLiteral("TEAM 1") : teamName;
        chatList->addItem(QStringLiteral("[%1] %2").arg(sender.toUpper(), composed));
        chatList->scrollToBottom();

        if (sendCb) sendCb(composed);

        inputRaw.clear();
        refreshInput();
    };

    QObject::connect(sendButton, &QPushButton::clicked, &dialog, doSend);

    // ── Close button ───────────────────────────────────────────────────
    auto *closeButton = new QPushButton(QStringLiteral("닫기"), &dialog);
    closeButton->setObjectName(QStringLiteral("chatCloseButton"));
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFixedHeight(38);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    mainLayout->addWidget(closeButton, 0, Qt::AlignRight);

    if (parent) {
        const QPoint c = parent->mapToGlobal(parent->rect().center());
        dialog.move(c - QPoint(dialog.width()/2, dialog.height()/2));
    }
    dialog.exec();
}
