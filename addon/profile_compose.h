#ifndef SCHNELLE_UMLAUTE_PROFILE_COMPOSE_H
#define SCHNELLE_UMLAUTE_PROFILE_COMPOSE_H

#include <string>
#include <unordered_map>
#include <vector>

// Pure logic for the base-anchored profile merge: compose the effective
// per-base variant lists from a chosen base profile plus an ordered sequence
// of appended source profiles. Kept free of fcitx/Qt so it can be unit-tested
// directly and shared verbatim by the engine and the editor, which must
// resolve a merge identically.
//
// Duplicates are PRESERVED with per-instance provenance: the same variant
// coming from two source profiles yields two Variant instances, each tagged
// with its source. That is what the editor needs to colour/tag duplicates and
// to target the right source profile on delete. The RUNTIME projection
// (projectUnique) collapses duplicates by value so the engine's cycle never
// has a dead slot; only the editor keeps the full instance list.

namespace schnelle_umlaute {

// A profile's flat map: base char -> ordered cycling variant values. Same
// shape as the engine's UmlautMap (src/mappings_loader.h), spelled out here as
// a plain std type so this header stays dependency-free.
using VariantMap = std::unordered_map<std::string, std::vector<std::string>>;

// One composed variant instance: its value plus the source profile File it
// came from ("mappings.txt" or "profiles/<slug>.txt"). The File is the stable
// provenance identity; it is what the manifest stores and what a delete
// cascades to.
struct Variant {
    std::string value;
    std::string sourceRef;
    bool operator==(const Variant &o) const {
        return value == o.value && sourceRef == o.sourceRef;
    }
};

// One source in the merge: its File (identity for provenance) and its flat
// map. The base profile is the first source; appended profiles follow in
// click order.
struct ComposeSource {
    std::string ref;
    const VariantMap *map;
};

// Per-base order override: the user's manual instance arrangement, keyed by
// value+source (NOT by position), so it self-heals when a source profile is
// edited later. Unmatched entries are dropped; unlisted instances append in
// natural order. See composeBase.
using OrderOverride = std::unordered_map<std::string, std::vector<Variant>>;

// Compose one base char's instance list from the ordered sources, applying an
// optional order override. Contract:
//   1. natural = each source in order, all its variants for `base`, duplicates
//      preserved, each instance tagged with its source File.
//   2. if `order` is given: best-effort stable reorder to match it — each
//      override entry claims the next unclaimed natural instance with the same
//      (value, sourceRef); unmatched override entries are dropped; the
//      remaining naturals append in natural order.
inline std::vector<Variant>
composeBase(const std::string &base,
            const std::vector<ComposeSource> &sources,
            const std::vector<Variant> *order) {
    std::vector<Variant> natural;
    for (const auto &src : sources) {
        if (!src.map)
            continue;
        auto it = src.map->find(base);
        if (it == src.map->end())
            continue;
        for (const auto &value : it->second)
            natural.push_back({value, src.ref});
    }
    if (!order || order->empty())
        return natural;

    std::vector<bool> claimed(natural.size(), false);
    std::vector<Variant> ordered;
    ordered.reserve(natural.size());
    for (const auto &want : *order) {
        for (size_t i = 0; i < natural.size(); ++i) {
            if (!claimed[i] && natural[i] == want) {
                claimed[i] = true;
                ordered.push_back(natural[i]);
                break;
            }
        }
    }
    for (size_t i = 0; i < natural.size(); ++i) {
        if (!claimed[i])
            ordered.push_back(natural[i]);
    }
    return ordered;
}

// Compose the full instance map from the ordered sources and the override
// layer. Every base appearing in any source is included once. Row ordering
// across bases is the caller's concern (the editor emits own bases first, then
// appended-only ones); this map is unordered because the engine only does
// per-base lookups.
inline std::unordered_map<std::string, std::vector<Variant>>
compose(const std::vector<ComposeSource> &sources,
        const OrderOverride &overrides) {
    std::unordered_map<std::string, std::vector<Variant>> out;
    for (const auto &src : sources) {
        if (!src.map)
            continue;
        for (const auto &kv : *src.map) {
            if (out.find(kv.first) != out.end())
                continue;
            auto oIt = overrides.find(kv.first);
            out.emplace(kv.first,
                        composeBase(kv.first, sources,
                                    oIt != overrides.end() ? &oIt->second
                                                           : nullptr));
        }
    }
    return out;
}

// Runtime projection of one base's instances to a unique value list (first
// occurrence wins, order preserved). Removes the dead cycle slots that
// duplicate provenance would otherwise create at runtime.
inline std::vector<std::string>
projectUnique(const std::vector<Variant> &instances) {
    std::vector<std::string> values;
    for (const auto &inst : instances) {
        bool seen = false;
        for (const auto &v : values) {
            if (v == inst.value) {
                seen = true;
                break;
            }
        }
        if (!seen)
            values.push_back(inst.value);
    }
    return values;
}

// Project a whole composed instance map to a runtime VariantMap (unique
// values per base). A base whose instance list is empty is dropped.
inline VariantMap projectUnique(
    const std::unordered_map<std::string, std::vector<Variant>> &composed) {
    VariantMap out;
    for (const auto &kv : composed) {
        auto values = projectUnique(kv.second);
        if (!values.empty())
            out.emplace(kv.first, std::move(values));
    }
    return out;
}

} // namespace schnelle_umlaute

#endif
