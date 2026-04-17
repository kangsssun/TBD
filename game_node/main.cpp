#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include "game_node.h"

namespace {
QString loadPreferredUiFontFamily()
{
    const int fontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/NotoSansKR-Regular.ttf"));
    if (fontId < 0) {
        return QString();
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    return families.isEmpty() ? QString() : families.first();
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QString uiFontFamily = loadPreferredUiFontFamily();
    if (!uiFontFamily.isEmpty()) {
        QFont uiFont = app.font();
        uiFont.setFamily(uiFontFamily);
        app.setFont(uiFont);
    }

    GameNode window;
    window.setWindowTitle(QStringLiteral("Escape Room Game"));
    window.resize(1024, 600);  // 임베디드 디스플레이 권장 해상도
    window.show();

    int ret = app.exec();

    // 종료 시 화면을 검은색으로 클리어 (임베디드 프레임버퍼 잔상 방지)
    window.hide();
    window.setStyleSheet(QStringLiteral("background-color: black;"));
    window.repaint();
    app.processEvents();

    return ret;
}
