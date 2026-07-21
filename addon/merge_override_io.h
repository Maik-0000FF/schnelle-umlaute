#ifndef SCHNELLE_UMLAUTE_MERGE_OVERRIDE_IO_H
#define SCHNELLE_UMLAUTE_MERGE_OVERRIDE_IO_H

// Parse and serialize a profile's merge-override sidecar (issue #112): the
// per-base tweaks the user applies to the COMPOSED result (active profile +
// merge overlay) without touching any source profile. `add` is not stored here
// (own variants live in the profile's own .txt); this file carries `remove`,
// `order`, and whole-base removals. Shared by the engine and the editor so both
// read the same format. One directive per line:
//   -<base>            drop <base> from inheritance entirely (removedBases)
//   !<base>=v1,v2,...   remove these inherited variants from <base>
//   ~<base>=v1,v2,...   arrange <base>'s variants in this order (live-friendly)
// The value lists use the same comma / double-comma escaping as the mapping
// output field (splitOutputs/joinOutputs). Lines starting with '#' are comments;
// empty and unrecognized lines are skipped.

#include "mappings-io.h"     // utf8FirstCharBytes, splitOutputs, joinOutputs
#include "profile_compose.h" // OverrideLayer, BaseOverride

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace schnelle_umlaute {

inline OverrideLayer parseMergeOverride(FILE *fp) {
    OverrideLayer layer;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const char kind = line[0];
        if (kind != '-' && kind != '!' && kind != '~')
            continue; // unknown directive
        const std::string rest = line.substr(1);
        // The base is the first UTF-8 character of the rest.
        const size_t bl = utf8FirstCharBytes(rest.data(), rest.size());
        if (bl == 0)
            continue;
        const std::string base = rest.substr(0, bl);
        if (kind == '-') {
            layer.removedBases.insert(base);
            continue;
        }
        // '!' / '~' need "=<value>" after the base.
        if (rest.size() <= bl || rest[bl] != '=')
            continue;
        auto vars = splitOutputs(rest.substr(bl + 1));
        if (kind == '!')
            layer.perBase[base].remove = std::move(vars);
        else // '~'
            layer.perBase[base].order = std::move(vars);
    }
    return layer;
}

inline std::string serializeMergeOverride(const OverrideLayer &layer) {
    std::string out;
    // Both containers are unordered, so sort the bases before emitting: a
    // rewrite with unchanged content then produces a byte-identical file
    // instead of churning the line order on every save.
    std::vector<std::string> removed(layer.removedBases.begin(),
                                     layer.removedBases.end());
    std::sort(removed.begin(), removed.end());
    for (const auto &base : removed) {
        out += '-';
        out += base;
        out += '\n';
    }
    std::vector<std::string> bases;
    bases.reserve(layer.perBase.size());
    for (const auto &kv : layer.perBase)
        bases.push_back(kv.first);
    std::sort(bases.begin(), bases.end());
    for (const auto &base : bases) {
        const auto &ops = layer.perBase.at(base);
        if (!ops.remove.empty()) {
            out += '!';
            out += base;
            out += '=';
            out += joinOutputs(ops.remove);
            out += '\n';
        }
        if (!ops.order.empty()) {
            out += '~';
            out += base;
            out += '=';
            out += joinOutputs(ops.order);
            out += '\n';
        }
    }
    return out;
}

} // namespace schnelle_umlaute

#endif
