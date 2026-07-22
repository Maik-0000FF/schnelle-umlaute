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
    if (lead < 0x80)
        return 1;
    if ((lead & 0xE0) == 0xC0)
        return 2;
    if ((lead & 0xF0) == 0xE0)
        return 3;
    if ((lead & 0xF8) == 0xF0)
        return 4;
    return 0;
}

// Byte length of a valid UTF-8 character at the start of [s, s+len).
// Returns 0 if the lead byte is invalid, if the buffer is too short for
// the indicated length, or if any continuation byte is not in 0x80-0xBF.
// A hand-edited mappings.txt with a wrong encoding could otherwise slip
// invalid UTF-8 through the parser, since utf8CharLen inspects only the
// lead byte. No overlong-encoding check: realistic editors don't produce
// them, and the cost outweighs the benefit for a user-owned config file.
inline size_t utf8FirstCharBytes(const char *s, size_t len) {
    if (len == 0)
        return 0;
    size_t n = utf8CharLen(static_cast<unsigned char>(s[0]));
    if (n == 0 || n > len)
        return 0;
    for (size_t i = 1; i < n; ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
            return 0;
    }
    return n;
}

// Parse mappings from an open FILE*.
// Format: one UTF-8 character + '=' + output, one mapping per line.
// The input character may be ASCII (1 byte) or multi-byte UTF-8
// (e.g. é, ñ on native keyboard layouts). '=' itself is a valid input
// since the delimiter is always the '=' after the first UTF-8 character.
// Lines starting with '#' are comments, empty lines are skipped.
//
// Lines longer than sizeof(buf)-1 bytes are dropped entirely: fgets would
// otherwise split them into two chunks, causing the prefix to be parsed as
// a truncated mapping and the tail as a garbled second line.
inline std::vector<RawMapping> parseMappings(FILE *fp) {
    std::vector<RawMapping> entries;
    char buf[4096];
    bool streamEnded = false;
    while (!streamEnded && fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        // fgets filled the whole buffer AND did not reach a newline →
        // candidate for truncation. Still ambiguous: the line could end
        // exactly at the buffer boundary (next char is '\n' or EOF),
        // in which case it is actually complete.
        bool mightBeTruncated =
            (line.size() == sizeof(buf) - 1) && line.back() != '\n';
        // Trim trailing newline / carriage return
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (mightBeTruncated) {
            int c = std::fgetc(fp);
            if (c == EOF) {
                // Stream ended on the buffer boundary. Parse this line
                // as the final one; setting streamEnded ensures we don't
                // re-enter fgets on a stream already in EOF state.
                streamEnded = true;
            } else if (c != '\n') {
                // Truly truncated — drain the rest of the physical line
                // and drop this entry. Parsing the prefix would store a
                // corrupt mapping and misinterpret the tail as new lines.
                while ((c = std::fgetc(fp)) != EOF && c != '\n') {
                }
                if (c == EOF)
                    break;
                continue;
            }
            // c == '\n' → the line just happened to end on the buffer
            // boundary. It is complete; proceed with normal parsing.
        }
        if (line.empty())
            continue;
        // A leading backslash escapes an input key that the plain parse would
        // otherwise misread: "\#=x" maps '#' (which starts a comment line when
        // unescaped) and "\\=x" maps '\' itself. The escaped byte is always
        // ASCII ('#' or '\') followed by '='. Any other leading backslash falls
        // through to the plain parse, so an old "\=x" (a bare '\' key written
        // before this escape existed) still reads as '\', and comments and
        // normal lines stay untouched.
        if (line.size() >= 3 && line[0] == '\\' &&
            (line[1] == '#' || line[1] == '\\') && line[2] == '=') {
            std::string input(1, line[1]);
            std::string output = line.substr(3);
            if (!output.empty()) {
                entries.push_back({std::move(input), std::move(output)});
            }
            continue;
        }
        if (line[0] == '#') // comment
            continue;
        size_t inputLen = utf8FirstCharBytes(line.data(), line.size());
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

// Split a raw output string into cycling variants.
// Comma separates variants: "a,b" → ["a", "b"].
// Double comma escapes a literal comma: "a,,b" → ["a,b"].
// Empty segments are skipped: "a,,,b" → ["a,", "b"] (greedy from left).
// A lone "," (or any all-separator string) yields an empty list, which callers
// treat as "no valid outputs". Lives here, next to the parser, so the engine
// and the editor's validation agree on what the output field means.
inline std::vector<std::string> splitOutputs(const std::string &output) {
    std::vector<std::string> outputs;
    if (output.empty())
        return outputs;

    std::string current;
    for (size_t i = 0; i < output.length(); ++i) {
        if (output[i] == ',') {
            if (i + 1 < output.length() && output[i + 1] == ',') {
                current += ',';
                ++i;
            } else {
                if (!current.empty()) {
                    outputs.push_back(std::move(current));
                    current.clear();
                }
            }
        } else {
            current += output[i];
        }
    }
    // Trailing comma produces an empty segment which is intentionally
    // skipped (an empty cycling variant would be useless).
    if (!current.empty()) {
        outputs.push_back(std::move(current));
    }
    return outputs;
}

// Rejoin cycling variants into the stored comma form, the inverse of
// splitOutputs: a literal comma inside a variant is escaped as ",," and the
// variants are joined with single commas. Empty variants are skipped so the
// result round-trips through splitOutputs unchanged.
inline std::string joinOutputs(const std::vector<std::string> &outputs) {
    std::string result;
    bool first = true;
    for (const auto &out : outputs) {
        if (out.empty())
            continue;
        if (!first)
            result += ',';
        first = false;
        for (char c : out) {
            if (c == ',')
                result += ",,";
            else
                result += c;
        }
    }
    return result;
}

} // namespace schnelle_umlaute

#endif
