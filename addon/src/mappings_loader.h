#ifndef SCHNELLE_UMLAUTE_MAPPINGS_LOADER_H
#define SCHNELLE_UMLAUTE_MAPPINGS_LOADER_H

// Runtime mappings layer on top of the format-level parser in mappings-io.h.
// parseMappings returns raw input→output strings; the loader also expands
// comma-separated cycling variants (respecting the double-comma escape) and
// knows how to find the file via fcitx5's StandardPaths lookup. It is also the
// one place that resolves the addon config dir for the engine, so the merge
// manifest and the usage-counter file IO live here too (the editor resolves
// the same files through its own Qt path helper, but both sides share the
// format headers below).

#include "mappings-io.h"        // splitOutputs (format-level output splitting)
#include "merge_manifest_io.h"  // MergeManifest (shared format)
#include "usage_io.h"           // UsageCounts (shared format)

#include <string>
#include <unordered_map>
#include <vector>

namespace schnelle_umlaute {

// Runtime mapping table: input UTF-8 character → cycling output variants.
// Order within the variant list defines the cycling sequence.
using UmlautMap = std::unordered_map<std::string, std::vector<std::string>>;

// Load mappings from a config file relative to the addon's config dir
// ($XDG_CONFIG_HOME/fcitx5/), e.g. "schnelle-umlaute/mappings.txt" for the
// Standard profile or "schnelle-umlaute/profiles/<slug>.txt" for another
// profile. Falls back to defaultMappings() when the file is absent, empty, or
// every parsed entry splits into zero variants. Individual malformed entries
// are skipped with an FCITX_WARN but do not abort the load.
UmlautMap loadMappingsFromFile(const std::string &relPath);

// Convenience overload for the Standard profile
// ("schnelle-umlaute/mappings.txt").
UmlautMap loadMappingsFromFile();

// Load the single global merge manifest (schnelle-umlaute/merge.conf). Returns
// an empty manifest (no base) when the file is absent, which the engine reads
// as "no merge".
MergeManifest loadMergeManifest();

// Load the per-(base, variant) usage counters (schnelle-umlaute/usage.conf).
// Returns an empty table when the file is absent.
UsageCounts loadUsage();

// Atomically write the usage counters (schnelle-umlaute/usage.conf) via
// fcitx StandardPaths safeSave (temp file + rename). Returns false on failure.
bool saveUsage(const UsageCounts &counts);

} // namespace schnelle_umlaute

#endif
