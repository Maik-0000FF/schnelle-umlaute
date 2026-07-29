#ifndef SCHNELLE_UMLAUTE_USAGE_IO_H
#define SCHNELLE_UMLAUTE_USAGE_IO_H

// Parse and serialize the per-(base char, committed variant) usage counters
// (usage.conf). Written by the engine, read by the editor. Kept in one shared
// header so both sides agree on the format (Single Source of Truth), the same
// rationale as mappings-io.h.
//
// Format: a marker line, then one counter per line, tab-separated:
//   #!format=2
//   <base>\t<variant>\t<count>
// base and variant are UTF-8 fields; count is a non-negative decimal integer.
// Lines starting with '#' are comments; empty and malformed lines are skipped.
//
// base and variant are escaped: a backslash becomes "\\" and a tab "\t". Both
// are mapped OUTPUTS, and a mapping may legitimately carry a tab (the editor
// rejects only \n and \r), which without escaping put a fourth field on the
// line and made the parser drop that counter on every single load, so the
// variant could never accumulate a count. Newlines need no escape: a mapping
// cannot hold one, and the file is line-based.
//
// The marker exists because escaping is not backwards compatible: in a file
// written before it, a backslash stood for itself, so a variant of "\t" (two
// characters) would now be read as a tab. A file without the marker is
// therefore parsed raw, and the next save rewrites it escaped, with the
// marker. Counters are the engine's own derived data, so that upgrade is
// lossless and needs no separate migration pass.

// readLine: the truncation-guarded line reader every parser here shares.
#include "line_io.h"

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

// First line of an escaped file. Its absence is what marks a legacy one.
inline constexpr const char *kUsageFormatMarker = "#!format=2";

// Escape/unescape a single base or variant field. Only the two characters that
// would otherwise be ambiguous are touched: the field separator and the escape
// character itself.
inline std::string escapeUsageField(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        if (c == '\\')
            out += "\\\\";
        else if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    return out;
}

// A backslash before anything else is kept as-is, both characters, so a file
// hand-edited into an unknown escape loses nothing. Same for a trailing lone
// backslash.
inline std::string unescapeUsageField(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '\\' || i + 1 == in.size()) {
            out += in[i];
            continue;
        }
        const char next = in[i + 1];
        if (next == '\\') {
            out += '\\';
            ++i;
        } else if (next == 't') {
            out += '\t';
            ++i;
        } else {
            out += in[i];
        }
    }
    return out;
}

inline UsageCounts parseUsage(FILE *fp) {
    UsageCounts counts;
    std::string line;
    // Set by the marker, which is accepted anywhere BEFORE the first counter
    // line, not just on line one: the writer always puts it first, but a file
    // that picked up a hand-written comment above it would otherwise fall back
    // to raw parsing without a word, and every escaped tab in it would come
    // back as a backslash and a t. After the first counter line the format is
    // settled, so a marker there is just another comment. A legacy file has no
    // marker at all and its fields are taken literally.
    bool escaped = false;
    bool sawCounter = false;
    while (readLine(fp, line)) {
        if (line.empty())
            continue;
        if (!sawCounter && line == kUsageFormatMarker) {
            escaped = true;
            continue;
        }
        if (line[0] == '#')
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
        std::string base = line.substr(0, t1);
        std::string variant = line.substr(t1 + 1, t2 - t1 - 1);
        const std::string countStr = line.substr(t2 + 1);
        if (base.empty() || variant.empty() || countStr.empty())
            continue;
        if (escaped) {
            base = unescapeUsageField(base);
            variant = unescapeUsageField(variant);
        }
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
        sawCounter = true;
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

    std::string body;
    for (const auto &base : bases) {
        const auto &variants = counts.at(base);
        std::vector<std::string> keys;
        keys.reserve(variants.size());
        for (const auto &kv : variants)
            keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const auto &variant : keys) {
            body += escapeUsageField(base);
            body += '\t';
            body += escapeUsageField(variant);
            body += '\t';
            body += std::to_string(variants.at(variant));
            body += '\n';
        }
    }

    // No counter lines means an EMPTY file, marker included: the editor's
    // hasUsageData() (and with it the reset control) treats a zero-byte
    // usage.conf as "nothing to reset", and a lone header would turn that into
    // a reset button with no counters behind it. A file with no fields has
    // nothing to escape either, so the marker buys nothing there.
    //
    // The test is on the lines actually produced rather than on the bases: a
    // base mapped to an empty variant table contributes no line, so an
    // outer-map check would emit the header anyway and re-create exactly the
    // case this guards against.
    if (body.empty())
        return {};

    std::string out = kUsageFormatMarker;
    out += '\n';
    out += body;
    return out;
}

} // namespace schnelle_umlaute

#endif
