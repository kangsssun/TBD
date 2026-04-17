#ifndef KEYBOARDPANEL_H
#define KEYBOARDPANEL_H

#include <QString>
#include <functional>

class QWidget;
class QScrollArea;
class QDialog;

/**
 * @brief Result of buildKeyboardPanel(): the scroll area containing the
 *        keyboard pages and a separate special-button bar (DEL, Shift, …).
 */
struct KeyboardPanel
{
    QScrollArea *scrollArea = nullptr;
    QWidget *specialButtonBar = nullptr;
};

/**
 * Compose raw Korean jamo sequence into proper Hangul syllables.
 * Also passes through non-Korean characters unchanged.
 */
QString composeHangulText(const QString &source);

/**
 * Build a full on-screen Korean/English keyboard panel.
 *
 * @param parent            The parent dialog (used for widget ownership).
 * @param scaledSize        A scaling helper: scaledSize(value, min) → scaled int.
 * @param appendCharacter   Called when a character key is pressed.
 * @param removeLastCharacter Called when DEL is pressed.
 * @param clearInput        Called when CLEAR is pressed.
 * @return A KeyboardPanel with the scroll area and the special-button bar.
 */
KeyboardPanel buildKeyboardPanel(
    QDialog *parent,
    const std::function<int(int, int)> &scaledSize,
    const std::function<void(const QString &)> &appendCharacter,
    const std::function<void()> &removeLastCharacter,
    const std::function<void()> &clearInput);

#endif // KEYBOARDPANEL_H
