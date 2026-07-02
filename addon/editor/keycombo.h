#ifndef SCHNELLE_UMLAUTE_EDITOR_KEYCOMBO_H
#define SCHNELLE_UMLAUTE_EDITOR_KEYCOMBO_H

#include <QString>

// Convert a captured Qt key combo (a Qt::Key value plus Qt::KeyboardModifiers)
// into the fcitx portable combo string the engine matches, e.g.
// "Control+Alt+J" or "Control+Alt+period".
//
// Returns an empty string when the combo is unusable as a shortcut: no real
// (non-Shift) modifier is held, or the base key is one we don't map (the
// engine also rejects modifier-less combos, see parseShortcut). Letters are
// emitted uppercase; both the engine and this writer rely on fcitx Key
// normalization so a Ctrl+Alt+j press matches a "Control+Alt+J" binding.
//
// Kept free of QML/QObject so it can be unit-tested directly against fcitx
// Key() (see tests/testkeycombo.cpp); KeyComboUtil wraps it for QML.
QString qtKeyComboToPortable(int qtKey, int qtModifiers);

#endif
