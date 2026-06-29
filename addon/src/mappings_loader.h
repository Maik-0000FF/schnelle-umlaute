#ifndef SCHNELLE_UMLAUTE_MAPPINGS_LOADER_H
#define SCHNELLE_UMLAUTE_MAPPINGS_LOADER_H

// Runtime mappings layer on top of the format-level parser in mappings-io.h.
// parseMappings returns raw input→output strings; the loader also expands
// comma-separated cycling variants (respecting the double-comma escape) and
// knows how to find the file via fcitx5's StandardPaths lookup.

#include <string>
#include <unordered_map>
#include <vector>

namespace schnelle_umlaute {

// Runtime mapping table: input UTF-8 character → cycling output variants.
// Order within the variant list defines the cycling sequence.
using UmlautMap = std::unordered_map<std::string, std::vector<std::string>>;

// Split a raw output string into cycling variants.
// Comma separates variants: "a,b" → ["a", "b"].
// Double comma escapes a literal comma: "a,,b" → ["a,b"].
// Empty segments are skipped: "a,,,b" → ["a,", "b"] (greedy from left).
std::vector<std::string> splitOutputs(const std::string &output);

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

} // namespace schnelle_umlaute

#endif
