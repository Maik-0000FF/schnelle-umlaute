#ifndef SCHNELLE_UMLAUTE_MAPPINGS_IO_H
#define SCHNELLE_UMLAUTE_MAPPINGS_IO_H

// Shared mapping file parser and default mappings.
// Used by the addon engine (std::string) and the config editor (QString).
// Keeping the format definition in one place prevents the two from diverging.

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace schnelle_umlaute {

// A single input→output mapping as raw strings.
// "output" may contain comma-separated cycling variants (parsed by the engine).
struct RawMapping {
    std::string input;
    std::string output;
};

// Default mappings: German umlauts (ä ö ü) and Eszett (ß), both cases.
inline std::vector<RawMapping> defaultMappings() {
    return {
        {"a", "\xc3\xa4"}, {"o", "\xc3\xb6"}, {"u", "\xc3\xbc"},
        {"s", "\xc3\x9f"}, {"A", "\xc3\x84"}, {"O", "\xc3\x96"},
        {"U", "\xc3\x9c"},
    };
}

// Parse mappings from an open FILE*.
// Format: single ASCII byte + '=' + output, one mapping per line.
// Lines starting with '#' are comments, empty lines are skipped.
// Multi-byte UTF-8 inputs (e.g. ñ) are not supported since input
// keys correspond to physical keyboard keys (single ASCII byte).
inline std::vector<RawMapping> parseMappings(FILE *fp) {
    std::vector<RawMapping> entries;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        // Trim trailing newline / carriage return
        while (!line.empty() &&
               (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') continue;
        if (line.size() >= 3 && line[1] == '=') {
            auto input = line.substr(0, 1);
            auto output = line.substr(2);
            if (!output.empty()) {
                entries.push_back({std::move(input), std::move(output)});
            }
        }
    }
    return entries;
}

} // namespace schnelle_umlaute

#endif
