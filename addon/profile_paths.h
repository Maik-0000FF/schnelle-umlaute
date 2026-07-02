#ifndef SCHNELLE_UMLAUTE_PROFILE_PATHS_H
#define SCHNELLE_UMLAUTE_PROFILE_PATHS_H

#include <string>
#include <string_view>

// Single source of truth for the file names that cross the editor<->engine
// boundary. The editor (Qt, addon/editor) and the engine (fcitx, addon/src)
// both read/write the same files, so these literals must not be duplicated in
// each: if one side drifted, the two would silently disagree on where the
// mappings and profiles live. Plain C-string constants so both QString and
// std::string can consume them. Shared via the addon/ include dir, like
// mappings-io.h.

namespace schnelle_umlaute {

// All paths below are relative to fcitx5's per-user config dir
// (~/.config/fcitx5, fcitx StandardPaths PkgConfig / QStandardPaths
// GenericConfigLocation + "/fcitx5").

// Per-addon config subdirectory.
inline constexpr const char *kConfigSubdir = "schnelle-umlaute";
// The Standard profile's mappings file (relative to kConfigSubdir). This is
// the pre-profiles file, kept as-is so existing mappings are never lost.
inline constexpr const char *kMappingsFile = "mappings.txt";
// Subdirectory holding every non-Standard profile's mappings file.
inline constexpr const char *kProfilesSubdir = "profiles";
// Profile metadata (list, active name, cycle hotkeys), relative to
// kConfigSubdir. Owned by the editor's ProfileListModel; read by the engine.
inline constexpr const char *kProfilesConf = "profiles.conf";
// Display name of the protected Standard profile.
inline constexpr const char *kStandardProfile = "Standard";

// A profile's File field must be either the Standard mappings file or a plain
// file directly under the profiles/ subdir. Rejects path traversal / absolute
// paths / nested dirs from a hand-edited or migrated profiles.conf, so neither
// the engine loader nor the editor's delete ever reaches outside the addon
// config dir. Shared by both sides so the rule lives in one place.
inline bool isSafeProfileFile(std::string_view file) {
    if (file == kMappingsFile) {
        return true;
    }
    const std::string prefix = std::string(kProfilesSubdir) + "/";
    if (file.size() <= prefix.size() ||
        file.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    const std::string_view rest = file.substr(prefix.size());
    return !rest.empty() && rest.find('/') == std::string_view::npos &&
           rest.find("..") == std::string_view::npos;
}

// True when File refers to the protected Standard profile. Accepts both the
// editor's bare File field ("mappings.txt") and the engine's config-dir-
// relative path ("schnelle-umlaute/mappings.txt"), so both sides share one
// rule (e.g. "seed the German defaults only for Standard") instead of
// comparing the name in two diverging forms.
inline bool isStandardProfile(std::string_view file) {
    return file == kMappingsFile ||
           file == std::string(kConfigSubdir) + "/" + kMappingsFile;
}

} // namespace schnelle_umlaute

#endif
