#ifndef SCHNELLE_UMLAUTE_USAGE_IO_H
#define SCHNELLE_UMLAUTE_USAGE_IO_H

// Parse and serialize the per-(base char, committed variant) usage counters
// (usage.conf). Written by the engine, read by the editor. Kept in one shared
// header so both sides agree on the format (Single Source of Truth), the same
// rationale as mappings-io.h.
//
// Format: one counter per line, tab-separated:
//   <base>\t<variant>\t<count>
// base and variant are opaque UTF-8 fields (tab-separated so no comma
// escaping is needed); count is a non-negative decimal integer. Lines
// starting with '#' are comments; empty and malformed lines are skipped.

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace schnelle_umlaute {

// base char -> (variant value -> commit count).
using UsageCounts =
    std::unordered_map<std::string,
                       std::unordered_map<std::string, long long>>;

inline UsageCounts parseUsage(FILE *fp) {
    UsageCounts counts;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        // Split into exactly three fields: base, variant, count. Variant is
        // the middle field; both base and variant are single-tab-free tokens
        // in practice, so a plain two-tab split is unambiguous.
        const size_t t1 = line.find('\t');
        if (t1 == std::string::npos)
            continue;
        const size_t t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos)
            continue;
        const std::string base = line.substr(0, t1);
        const std::string variant = line.substr(t1 + 1, t2 - t1 - 1);
        const std::string countStr = line.substr(t2 + 1);
        if (base.empty() || variant.empty() || countStr.empty())
            continue;
        char *end = nullptr;
        // errno is reset first so ERANGE can be told apart from a leftover
        // value: without it an out-of-range count clamps to LLONG_MAX and is
        // stored as a real counter (~9.2e18), skewing the frequency order for
        // good instead of being skipped as malformed.
        errno = 0;
        const long long n = std::strtoll(countStr.c_str(), &end, 10);
        if (end == countStr.c_str() || *end != '\0' || n < 0 || errno == ERANGE)
            continue;
        counts[base][variant] = n;
    }
    return counts;
}

inline std::string serializeUsage(const UsageCounts &counts) {
    // Sort bases and variants so an unchanged table re-serializes
    // byte-identically instead of churning line order.
    std::vector<std::string> bases;
    bases.reserve(counts.size());
    for (const auto &kv : counts)
        bases.push_back(kv.first);
    std::sort(bases.begin(), bases.end());

    std::string out;
    for (const auto &base : bases) {
        const auto &variants = counts.at(base);
        std::vector<std::string> keys;
        keys.reserve(variants.size());
        for (const auto &kv : variants)
            keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const auto &variant : keys) {
            out += base;
            out += '\t';
            out += variant;
            out += '\t';
            out += std::to_string(variants.at(variant));
            out += '\n';
        }
    }
    return out;
}

} // namespace schnelle_umlaute

#endif
