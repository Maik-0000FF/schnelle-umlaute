#ifndef SCHNELLE_UMLAUTE_PROFILE_PATHS_H
#define SCHNELLE_UMLAUTE_PROFILE_PATHS_H

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

} // namespace schnelle_umlaute

#endif
