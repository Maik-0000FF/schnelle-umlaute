#include "hand_classifier.h"

#include <fcitx-utils/utf8.h>
#include <xkbcommon/xkbcommon.h>

#include <cstdint>
#include <utility>

namespace fcitx {

namespace {
constexpr uint32_t kMaxUnicodeCodepoint = 0x10FFFF;
}

bool HandClassifier::isLeftHandKeycode(int keycode) {
    return (keycode >= 24 && keycode <= 28) || // Q W E R T row
           (keycode >= 38 && keycode <= 42) || // A S D F G row
           (keycode >= 52 && keycode <= 56) || // Z X C V B row
           keycode == 49 ||                    // ` ~
           (keycode >= 10 && keycode <= 14);   // 1 2 3 4 5
}

bool HandClassifier::isLeftHand(const std::string &key) const {
    std::string lookup = key;
    if (lookup.size() == 1 && lookup[0] >= 'A' && lookup[0] <= 'Z')
        lookup[0] = static_cast<char>(lookup[0] - 'A' + 'a');
    auto it = charToKeycode_.find(lookup);
    if (it == charToKeycode_.end())
        return false;
    return isLeftHandKeycode(it->second);
}

void HandClassifier::build(struct xkb_keymap *keymap) {
    charToKeycode_.clear();
    if (!keymap)
        return;
    xkb_keycode_t min = xkb_keymap_min_keycode(keymap);
    xkb_keycode_t max = xkb_keymap_max_keycode(keymap);
    for (xkb_keycode_t code = min; code <= max; ++code) {
        const xkb_keysym_t *syms;
        int n = xkb_keymap_key_get_syms_by_level(keymap, code, 0, 0, &syms);
        if (n > 0) {
            uint32_t uc = xkb_keysym_to_utf32(syms[0]);
            if (uc > 0 && uc <= kMaxUnicodeCodepoint) {
                std::string ch = utf8::UCS4ToUTF8(uc);
                charToKeycode_.emplace(std::move(ch), static_cast<int>(code));
            }
        }
    }
}

} // namespace fcitx
