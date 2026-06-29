#include "keycombo.h"

#include <QChar>
#include <QLatin1Char>
#include <QtCore/qnamespace.h>

namespace {

// X keysym name for non-alphanumeric keys, the spelling fcitx
// Key::keySymFromString parses. nullptr for keys we don't support as
// shortcuts.
const char *symbolName(int key) {
    switch (key) {
    case Qt::Key_Space:
        return "space";
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return "Return";
    case Qt::Key_Tab:
        return "Tab";
    case Qt::Key_Backspace:
        return "BackSpace";
    case Qt::Key_Period:
        return "period";
    case Qt::Key_Comma:
        return "comma";
    case Qt::Key_Slash:
        return "slash";
    case Qt::Key_Backslash:
        return "backslash";
    case Qt::Key_Semicolon:
        return "semicolon";
    case Qt::Key_Apostrophe:
        return "apostrophe";
    case Qt::Key_BracketLeft:
        return "bracketleft";
    case Qt::Key_BracketRight:
        return "bracketright";
    case Qt::Key_Minus:
        return "minus";
    case Qt::Key_Equal:
        return "equal";
    case Qt::Key_QuoteLeft:
        return "grave";
    case Qt::Key_Left:
        return "Left";
    case Qt::Key_Right:
        return "Right";
    case Qt::Key_Up:
        return "Up";
    case Qt::Key_Down:
        return "Down";
    case Qt::Key_Home:
        return "Home";
    case Qt::Key_End:
        return "End";
    case Qt::Key_PageUp:
        return "Prior";
    case Qt::Key_PageDown:
        return "Next";
    case Qt::Key_Insert:
        return "Insert";
    case Qt::Key_Delete:
        return "Delete";
    default:
        return nullptr;
    }
}

} // namespace

QString qtKeyComboToPortable(int qtKey, int qtModifiers) {
    const bool ctrl = qtModifiers & Qt::ControlModifier;
    const bool alt = qtModifiers & Qt::AltModifier;
    const bool meta = qtModifiers & Qt::MetaModifier;
    const bool shift = qtModifiers & Qt::ShiftModifier;

    // A real (non-Shift) modifier is required; a bare or Shift-only key would
    // swallow ordinary typing and the engine rejects it anyway.
    if (!(ctrl || alt || meta))
        return QString();

    QString base;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        base = QChar(QLatin1Char(static_cast<char>('A' + (qtKey - Qt::Key_A))));
    else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        base = QChar(QLatin1Char(static_cast<char>('0' + (qtKey - Qt::Key_0))));
    else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35)
        base = QStringLiteral("F%1").arg(qtKey - Qt::Key_F1 + 1);
    else if (const char *name = symbolName(qtKey))
        base = QLatin1String(name);
    else
        return QString(); // unsupported base key

    QString out;
    if (ctrl)
        out += QStringLiteral("Control+");
    if (alt)
        out += QStringLiteral("Alt+");
    if (shift)
        out += QStringLiteral("Shift+");
    if (meta)
        out += QStringLiteral("Super+");
    out += base;
    return out;
}
