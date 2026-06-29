// Round-trip test for the editor's Qt-to-fcitx combo conversion
// (qtKeyComboToPortable): the strings it emits must parse with fcitx Key() and,
// after normalization, match a real keypress of the same combo. This is the
// main risk of the shortcut-capture UI, so it is pinned against the actual
// fcitx Key implementation (linked here) rather than assumed.

#include "keycombo.h"
#include "test_expect.h"

#include <QtCore/qnamespace.h>
#include <fcitx-utils/key.h>

#include <cstdio>

using fcitx::Key;

int main() {
    // A real (non-Shift) modifier is required.
    EXPECT(qtKeyComboToPortable(Qt::Key_J, Qt::NoModifier).isEmpty());
    EXPECT(qtKeyComboToPortable(Qt::Key_J, Qt::ShiftModifier).isEmpty());

    // Letter: emitted uppercase, parses, and matches a lowercase press after
    // normalization (Key::check does no case folding on its own).
    {
        QString s = qtKeyComboToPortable(
            Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
        EXPECT(s == QStringLiteral("Control+Alt+J"));
        Key bound(s.toStdString());
        EXPECT(bound.isValid());
        Key press("Control+Alt+j"); // what the OS delivers for Ctrl+Alt+j
        EXPECT(press.normalize().check(bound.normalize()));
    }

    // Digit: pin the keysym name and the real-press match.
    {
        QString s = qtKeyComboToPortable(
            Qt::Key_1, Qt::ControlModifier | Qt::AltModifier);
        EXPECT(s == QStringLiteral("Control+Alt+1"));
        Key bound(s.toStdString());
        EXPECT(bound.isValid());
        EXPECT(bound.sym() == FcitxKey_1);
        Key press("Control+Alt+1");
        EXPECT(press.normalize().check(bound.normalize()));
    }

    // Symbol: X keysym name, parses to the matching sym.
    {
        QString s = qtKeyComboToPortable(
            Qt::Key_Period, Qt::ControlModifier | Qt::AltModifier);
        EXPECT(s == QStringLiteral("Control+Alt+period"));
        Key k(s.toStdString());
        EXPECT(k.isValid());
        EXPECT(k.sym() == FcitxKey_period);
    }

    // Meta maps to Super; matches a real Super+k press after normalization.
    {
        QString s = qtKeyComboToPortable(Qt::Key_K, Qt::MetaModifier);
        EXPECT(s == QStringLiteral("Super+K"));
        Key bound(s.toStdString());
        EXPECT(bound.isValid());
        Key press("Super+k");
        EXPECT(press.normalize().check(bound.normalize()));
    }

    // Shift combines with a real modifier; pin the sym and the real-press match.
    {
        QString s = qtKeyComboToPortable(
            Qt::Key_F5,
            Qt::ControlModifier | Qt::ShiftModifier);
        EXPECT(s == QStringLiteral("Control+Shift+F5"));
        Key bound(s.toStdString());
        EXPECT(bound.isValid());
        EXPECT(bound.sym() == FcitxKey_F5);
        Key press("Control+Shift+F5");
        EXPECT(press.normalize().check(bound.normalize()));
    }

    // Unsupported base key -> empty (user picks another).
    EXPECT(qtKeyComboToPortable(Qt::Key_CapsLock, Qt::ControlModifier).isEmpty());

    // Numpad keys are rejected: they deliver KP_* syms at runtime that wouldn't
    // match the main-row syms emitted here, so they'd be dead bindings.
    EXPECT(qtKeyComboToPortable(Qt::Key_1,
                                Qt::ControlModifier | Qt::KeypadModifier)
               .isEmpty());
    EXPECT(qtKeyComboToPortable(Qt::Key_Enter,
                                Qt::ControlModifier | Qt::KeypadModifier)
               .isEmpty());

    // A shifted number-row key (Shift+1 -> Qt::Key_Exclam) is unmapped, so the
    // combo is rejected instead of stored as a dead binding.
    EXPECT(qtKeyComboToPortable(Qt::Key_Exclam, Qt::ControlModifier |
                                                    Qt::AltModifier |
                                                    Qt::ShiftModifier)
               .isEmpty());

    std::fprintf(stderr, "ok testkeycombo\n");
    return 0;
}
