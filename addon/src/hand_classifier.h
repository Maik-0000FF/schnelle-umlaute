#ifndef SCHNELLE_UMLAUTE_HAND_CLASSIFIER_H
#define SCHNELLE_UMLAUTE_HAND_CLASSIFIER_H

// Layout-independent left/right-hand classification for physical keyboard
// keys. The physical key position determines the hand, not the character
// printed on the keycap — so QWERTY, QWERTZ, AZERTY, Dvorak and Colemak
// all produce the same answer for the same physical key.
//
// Used by the dual custom-leader split feature to decide which input keys
// a given leader may trigger (opposite-hand rule).

#include <string>
#include <unordered_map>

struct xkb_keymap;

namespace fcitx {

class HandClassifier {
public:
    // Build the reverse char→keycode map from an XKB keymap.
    // Intentionally level 0 (unshifted) only — shifted symbols like '@'
    // (Shift+2) or '?' (Shift+/) are not mapped, so isLeftHand() falls
    // back to right hand (false) for them. This is the safer default:
    // custom keyboards may place higher-level characters on arbitrary
    // physical positions (e.g. thumb clusters), so assuming the base
    // key's position would be incorrect.
    //
    // Safe to call repeatedly; the map is cleared on each call.
    // Passing nullptr clears the map without rebuilding.
    void build(struct xkb_keymap *keymap);

    // Physical keycode-based left-hand classification using standard
    // evdev codes. Pure function; no map lookup.
    static bool isLeftHandKeycode(int keycode);

    // Character-based lookup via the reverse map built from the keymap.
    // Falls back to right hand (false) for unknown characters or an
    // empty map, which has the effect of disabling dual split rather
    // than enforcing a wrong classification.
    bool isLeftHand(const std::string &key) const;

private:
    std::unordered_map<std::string, int> charToKeycode_;
};

} // namespace fcitx

#endif
