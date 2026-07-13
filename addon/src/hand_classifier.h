#ifndef SCHNELLE_UMLAUTE_HAND_CLASSIFIER_H
#define SCHNELLE_UMLAUTE_HAND_CLASSIFIER_H

// Left/right-hand classification for physical keyboard keys, used by the dual
// custom-leader split (opposite-hand rule).
//
// Classification is by physical key position alone; the character printed on
// the keycap never enters into it. QWERTY, QWERTZ, AZERTY, Dvorak and Colemak
// therefore all produce the same answer for the same physical key, and a layout
// switch changes nothing.
//
// This works on keycodes, never on characters, and that is what makes it
// layout-independent. Both keycodes a caller needs come straight from a key
// event: the leader's is captured as a real key press in the editor and stored
// in the config, the input key's arrives with the keypress itself. Nothing is
// resolved back from a character, so no keymap is involved anywhere.

namespace fcitx {

// Config sentinel for "no physical key captured for this leader". A leader
// without a captured key is not active: it cannot be matched, and it has no
// keyboard half, so it cannot take part in the split either.
inline constexpr int kNoKeyCode = 0;

// Left-hand classification for an evdev+8 keycode. That offset is the shared
// convention of XKB, X11, fcitx5's Key::code() and Qt's nativeScanCode (Qt adds
// the same 8 on both XCB and Wayland), so no translation is needed at any
// boundary.
//
// Covers the letter/number block a touch typist splits between the hands.
// Everything outside it (modifiers, function keys, numpad, the right-hand
// symbol cluster) counts as right-hand by omission. For the split that is the
// conservative direction: it restricts rather than over-permits.
constexpr bool isLeftHandKeycode(int keycode) {
    return (keycode >= 24 && keycode <= 28) || // Q W E R T row
           (keycode >= 38 && keycode <= 42) || // A S D F G row
           (keycode >= 52 && keycode <= 56) || // Z X C V B row
           keycode == 49 ||                    // ` ~
           (keycode >= 10 && keycode <= 14);   // 1 2 3 4 5
}

} // namespace fcitx

#endif
