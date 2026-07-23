#ifndef SCHNELLE_UMLAUTE_USAGE_SORT_H
#define SCHNELLE_UMLAUTE_USAGE_SORT_H

// The single frequency-sort comparator, shared by the engine (runtime cycle
// order) and the editor model (chip display order) so the preview can never
// diverge from what the cycle actually uses (Single Source of Truth). QML does
// no sorting of its own; it renders the order this returns.
//
// The sort is STABLE and descending by usage count, with the tie-break falling
// back to the given stored order. Consequence: with fresh stats (all counts
// zero) the result equals the stored order, so turning the toggle on changes
// nothing until real usage accumulates — nothing jumps around for no reason.

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace schnelle_umlaute {

// Reorder `stored` by descending usage. `counts` maps a variant value to its
// count for this one base char (a missing value counts as zero). Stable, so
// equal-count variants keep their relative order from `stored`.
inline std::vector<std::string>
sortVariantsByUsage(const std::vector<std::string> &stored,
                    const std::unordered_map<std::string, long long> &counts) {
    std::vector<std::string> result(stored);
    std::stable_sort(
        result.begin(), result.end(),
        [&counts](const std::string &a, const std::string &b) {
            const auto ia = counts.find(a);
            const auto ib = counts.find(b);
            const long long ca = ia != counts.end() ? ia->second : 0;
            const long long cb = ib != counts.end() ? ib->second : 0;
            return ca > cb;
        });
    return result;
}

} // namespace schnelle_umlaute

#endif
