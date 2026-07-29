#ifndef SCHNELLE_UMLAUTE_CONFIG_KEYS_H
#define SCHNELLE_UMLAUTE_CONFIG_KEYS_H

// Key names in schnelle-umlaute.conf that BOTH the editor and the overlay
// daemon touch. The editor owns and writes the file; the daemon reads back the
// handful of keys it needs to derive its own theme, which it has to do when the
// desktop switches light/dark with the editor closed.
//
// They live here because a rename on the writing side is otherwise invisible to
// the reading side: the daemon would just miss the key and fall back to its
// default, with no error anywhere. Only the shared keys belong here; everything
// the editor alone reads and writes stays where it is used.
//
// Framework-free on purpose (plain char arrays, no QString), so a consumer that
// does not link Qt could still use it.

namespace schnelle_umlaute {
namespace keys {

// [Theme]
inline constexpr const char *kThemeSection = "Theme";
inline constexpr const char *kTheme = "Theme";
inline constexpr const char *kThemeAuto = "Auto";
inline constexpr const char *kThemeLight = "ThemeLight";
inline constexpr const char *kThemeDark = "ThemeDark";

// [Overlay]: only the two the daemon needs to decide whether an automatic
// switch also restyles the fcitx5 candidate window.
inline constexpr const char *kOverlaySection = "Overlay";
inline constexpr const char *kCaretTheme = "CaretTheme";
inline constexpr const char *kPlacement = "Placement";
// The one Placement value that turns the caret theme on; any other placement
// leaves the candidate window alone.
inline constexpr const char *kPlacementTextCaret = "TextCaret";

} // namespace keys
} // namespace schnelle_umlaute

#endif
