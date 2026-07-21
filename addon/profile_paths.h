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

// A merge-overlay entry (issue #112) references a profile by its File field,
// with this prefix so the ref format can later distinguish other kinds. The
// overlay is a comma-joined list of these in profiles.conf's MergeOverlay key;
// both the editor and the engine build and split refs, so the prefix and the
// two conversions below live here once instead of as scattered literals.
inline constexpr const char *kProfileRefPrefix = "profile:";

// Build a merge ref from a profile File ("profiles/x.txt" ->
// "profile:profiles/x.txt").
inline std::string profileRefForFile(std::string_view file) {
    return std::string(kProfileRefPrefix) + std::string(file);
}

// The File a merge ref names, or an empty string when it is not a profile ref.
inline std::string fileForProfileRef(std::string_view ref) {
    const std::string_view prefix(kProfileRefPrefix);
    if (ref.size() < prefix.size() ||
        ref.compare(0, prefix.size(), prefix) != 0) {
        return std::string();
    }
    return std::string(ref.substr(prefix.size()));
}

// A profile's merge-override sidecar path: the profile File with a ".txt"
// suffix swapped for ".merge" (a suffix-less name just gains ".merge"). The
// engine and the editor derive it identically so they never read from and write
// to different sidecars for the same profile.
inline std::string sidecarRelPathForProfile(std::string_view file) {
    const std::string_view kTxt(".txt");
    std::string rel(file);
    if (rel.size() >= kTxt.size() &&
        rel.compare(rel.size() - kTxt.size(), kTxt.size(), kTxt) == 0) {
        rel.replace(rel.size() - kTxt.size(), kTxt.size(), ".merge");
    } else {
        rel += ".merge";
    }
    return rel;
}

} // namespace schnelle_umlaute

#endif
