#ifndef SCHNELLE_UMLAUTE_PROFILE_COMPOSE_H
#define SCHNELLE_UMLAUTE_PROFILE_COMPOSE_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Pure logic for the global merge overlay (issue #112): compose the effective
// per-base variant lists from an ordered sequence of source profiles plus a
// per-variant local override layer. Kept free of fcitx/Qt so it can be unit-
// tested directly and shared verbatim by the engine and the editor, which must
// resolve a profile identically. Callers build the ordered source list (the
// active profile first, then the overlay profiles in merge order) and the
// override layer, and feed them here. See the design in
// private/plan-1.8.0-profile-merging.md.

namespace schnelle_umlaute {

// A profile's base char -> ordered variant list. Same shape as the engine's
// UmlautMap (src/mappings_loader.h), spelled out here as a plain std type so
// this header stays dependency-free.
using VariantMap = std::unordered_map<std::string, std::vector<std::string>>;

// Per-base local override operations. Operation-based (not a final list) so it
// composes correctly as the sources change: a `remove` stays meaningful when a
// source later re-adds the variant, and `order` lists known variants first
// while new upstream ones still append.
struct BaseOverride {
    std::vector<std::string> add;    // own variants, not from any source
    std::vector<std::string> remove; // inherited variants to drop
    std::vector<std::string> order;  // optional explicit arrangement, by string
};

// The whole local override layer: per-base overrides plus whole-base removals.
struct OverrideLayer {
    std::unordered_map<std::string, BaseOverride> perBase;
    std::unordered_set<std::string> removedBases;
};

namespace compose_detail {

// Append `v` to `out` unless already present, preserving first-occurrence
// order. This is the dedup rule shared by the ordered union and the pool build.
inline void pushUnique(std::vector<std::string> &out, const std::string &v) {
    for (const auto &e : out) {
        if (e == v)
            return;
    }
    out.push_back(v);
}

inline bool contains(const std::vector<std::string> &v, const std::string &s) {
    for (const auto &e : v) {
        if (e == s)
            return true;
    }
    return false;
}

} // namespace compose_detail

// Compose one base char's effective variant list from the ordered sources and
// its override ops. Contract:
//   1. inherited = ordered union of the sources' lists (dedup, source order)
//   2. pool = (inherited minus `remove`) then `add` not already present
//   3. if `order` non-empty: listed-and-present variants first in that order,
//      then the remaining pool in default order
// Returns an empty list when nothing remains (the caller then drops the base).
inline std::vector<std::string>
composeBase(const std::string &base,
            const std::vector<const VariantMap *> &sources,
            const BaseOverride &ops) {
    using compose_detail::contains;
    using compose_detail::pushUnique;

    // 1. ordered union of the sources.
    std::vector<std::string> inherited;
    for (const VariantMap *src : sources) {
        auto it = src->find(base);
        if (it == src->end())
            continue;
        for (const auto &variant : it->second)
            pushUnique(inherited, variant);
    }

    // 2. pool = inherited minus `remove`, then own additions.
    std::vector<std::string> pool;
    for (const auto &variant : inherited) {
        if (!contains(ops.remove, variant))
            pushUnique(pool, variant);
    }
    for (const auto &variant : ops.add)
        pushUnique(pool, variant);

    // 3. optional explicit ordering: listed-and-present first, then the rest.
    if (ops.order.empty())
        return pool;

    std::vector<std::string> ordered;
    for (const auto &variant : ops.order) {
        if (contains(pool, variant))
            pushUnique(ordered, variant);
    }
    for (const auto &variant : pool)
        pushUnique(ordered, variant); // append the unlisted remainder
    return ordered;
}

// Compose the full effective VariantMap from the ordered sources and the
// override layer. A base is included iff its composed list is non-empty and it
// is not in `removedBases`. No recursion: `sources` are already-flat profile
// maps in merge order (active profile first, then the overlay in order).
inline VariantMap compose(const std::vector<const VariantMap *> &sources,
                          const OverrideLayer &overrides) {
    // Every base appearing in any source or the override layer, once each.
    std::vector<std::string> bases;
    for (const VariantMap *src : sources) {
        for (const auto &kv : *src)
            compose_detail::pushUnique(bases, kv.first);
    }
    for (const auto &kv : overrides.perBase)
        compose_detail::pushUnique(bases, kv.first);

    static const BaseOverride kNoOps;
    VariantMap effective;
    for (const auto &base : bases) {
        if (overrides.removedBases.count(base))
            continue;
        auto opIt = overrides.perBase.find(base);
        const BaseOverride &ops =
            (opIt != overrides.perBase.end()) ? opIt->second : kNoOps;
        std::vector<std::string> variants = composeBase(base, sources, ops);
        if (!variants.empty())
            effective.emplace(base, std::move(variants));
    }
    return effective;
}

} // namespace schnelle_umlaute

#endif
