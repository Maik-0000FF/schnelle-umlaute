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

// Byte length of the first UTF-8 character based on its leading byte.
// Returns 0 for invalid lead bytes (continuation bytes or 0xFE/0xFF).
inline size_t utf8CharLen(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

// Parse mappings from an open FILE*.
// Format: one UTF-8 character + '=' + output, one mapping per line.
// The input character may be ASCII (1 byte) or multi-byte UTF-8
// (e.g. é, ñ on native keyboard layouts). '=' itself is a valid input
// since the delimiter is always the '=' after the first UTF-8 character.
// Lines starting with '#' are comments, empty lines are skipped.
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
        size_t inputLen = utf8CharLen(static_cast<unsigned char>(line[0]));
        if (inputLen == 0 || line.size() <= inputLen || line[inputLen] != '=') {
            continue;
        }
        auto input = line.substr(0, inputLen);
        auto output = line.substr(inputLen + 1);
        if (!output.empty()) {
            entries.push_back({std::move(input), std::move(output)});
        }
    }
    return entries;
}

} // namespace schnelle_umlaute

#endif
