// Unit tests for the mappings-io parser.
// Standalone — no fcitx5 frontend/runtime dependency, just the header
// and libc. Intended to run fast and catch parser-level regressions,
// especially around UTF-8 validation (F1) where utf8CharLen alone would
// previously accept invalid continuation bytes as input keys.

#include "mappings-io.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using schnelle_umlaute::parseMappings;
using schnelle_umlaute::RawMapping;

namespace {

// Parse `content` by writing it to a tmpfile() and rewinding — keeps the
// production parseMappings signature (FILE *) exercised end-to-end.
std::vector<RawMapping> parseString(const std::string &content) {
    FILE *fp = std::tmpfile();
    if (!fp) {
        std::fprintf(stderr, "tmpfile() failed\n");
        std::abort();
    }
    if (!content.empty()) {
        std::fwrite(content.data(), 1, content.size(), fp);
    }
    if (std::fseek(fp, 0, SEEK_SET) != 0) {
        std::fprintf(stderr, "fseek to start of tmpfile failed\n");
        std::abort();
    }
    auto result = parseMappings(fp);
    std::fclose(fp);
    return result;
}

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// -- Valid input shapes ------------------------------------------------------

void testValidAscii() {
    auto r = parseString("a=apfel\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "apfel");
}

void testValidMultiByte() {
    // 'ä' (U+00E4) = C3 A4                — 2-byte
    // '€' (U+20AC) = E2 82 AC              — 3-byte
    // '😀' (U+1F600) = F0 9F 98 80         — 4-byte
    auto r = parseString("\xc3\xa4=ae\n"
                         "\xe2\x82\xac=EUR\n"
                         "\xf0\x9f\x98\x80=smile\n");
    EXPECT(r.size() == 3);
    EXPECT(r[0].input == "\xc3\xa4");
    EXPECT(r[0].output == "ae");
    EXPECT(r[1].input == "\xe2\x82\xac");
    EXPECT(r[1].output == "EUR");
    EXPECT(r[2].input == "\xf0\x9f\x98\x80");
    EXPECT(r[2].output == "smile");
}

// -- F1: invalid UTF-8 must be rejected --------------------------------------

// Lead 0xC3 promises a 2-byte sequence, but the second byte 0x41 ('A')
// is NOT a valid continuation byte (needs 0x80-0xBF).
// Pre-F1 behavior: parser stored "\xC3\x41" as input key (invalid UTF-8).
// Post-F1 behavior: line is skipped.
void testTwoByteBadContinuationSkipped() {
    auto r = parseString("\xc3\x41=bogus\n");
    EXPECT(r.empty());
}

// 3-byte sequence where the third byte is not a continuation.
// Lead 0xE2 + valid 0x82 + invalid 0x41.
void testThreeByteBadContinuationSkipped() {
    auto r = parseString("\xe2\x82\x41=bogus\n");
    EXPECT(r.empty());
}

// 4-byte sequence where the fourth byte is not a continuation.
// Lead 0xF0 + 0x9F + 0x98 + invalid 0x41.
void testFourByteBadContinuationSkipped() {
    auto r = parseString("\xf0\x9f\x98\x41=bogus\n");
    EXPECT(r.empty());
}

// A bare continuation byte (0x80) as lead must be rejected — it's never
// a valid start of a UTF-8 character.
void testContinuationByteAsLeadSkipped() {
    auto r = parseString("\x80=x\n");
    EXPECT(r.empty());
}

// 0xFF is never a valid UTF-8 byte.
void testFfByteAsLeadSkipped() {
    auto r = parseString("\xff=x\n");
    EXPECT(r.empty());
}

// Mixed file: valid entries must still be parsed when invalid lines are
// interleaved — one bad line must not poison the rest of the file.
void testMixedValidAndInvalid() {
    auto r = parseString("a=eins\n"
                         "\xc3\x41=bogus\n" // invalid 2-byte continuation
                         "o=zwei\n"
                         "\xe2\x82\x41=x\n" // invalid 3-byte continuation
                         "u=drei\n");
    EXPECT(r.size() == 3);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "eins");
    EXPECT(r[1].input == "o");
    EXPECT(r[1].output == "zwei");
    EXPECT(r[2].input == "u");
    EXPECT(r[2].output == "drei");
}

// -- Format edge cases (not F1 specific, but guard against regressions) -----

void testCommentAndEmptyLinesSkipped() {
    auto r = parseString("# comment\n"
                         "\n"
                         "a=eins\n"
                         "# another comment\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "eins");
}

// '=' itself is a valid input key since the format uses the '=' AFTER the
// first UTF-8 character as the delimiter. Keep this property under test.
void testEqualsAsInputKey() {
    auto r = parseString("==bang\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "=");
    EXPECT(r[0].output == "bang");
}

// A leading backslash escapes an input key: "\#=..." maps '#' (which would
// otherwise start a comment) and "\\=..." maps '\' itself.
void testEscapedHashInputKey() {
    auto r = parseString("\\#=hash\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "#");
    EXPECT(r[0].output == "hash");
}
void testEscapedBackslashInputKey() {
    auto r = parseString("\\\\=slash\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "\\");
    EXPECT(r[0].output == "slash");
}

// A bare "#=..." stays a comment (only the escaped form maps '#'), and an
// escaped line with no output is skipped like any other empty-output line.
void testBareHashStillComment() {
    auto r = parseString("#=notamapping\n"
                         "a=eins\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
}
void testEscapedInputEmptyOutputSkipped() {
    auto r = parseString("\\#=\n");
    EXPECT(r.empty());
}

// Backward compatibility: a bare "\=..." (a '\' key written before the escape
// existed) still parses as '\', since only "\#"/"\\" are treated as escapes.
void testLegacyBareBackslashInputKey() {
    auto r = parseString("\\=legacy\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "\\");
    EXPECT(r[0].output == "legacy");
}

// '=' embedded in the output must round-trip verbatim: the delimiter is the
// FIRST '=' after the leading UTF-8 char, every later '=' is data. Pinning
// this guards against a "split on '='" refactor that would silently truncate
// the output at the first inner '='.
void testEqualsInOutput() {
    auto r = parseString("a=hello=world\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "hello=world");
}

void testMissingEqualsSkipped() {
    auto r = parseString("noequals\n");
    EXPECT(r.empty());
}

void testEmptyOutputSkipped() {
    auto r = parseString("a=\n");
    EXPECT(r.empty());
}

// CRLF line endings must be trimmed (file edited on Windows should still
// parse).
void testCrlfTrimmed() {
    auto r = parseString("a=eins\r\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "eins");
}

// Trailing line without newline is still parsed (last line of a file saved
// without a final newline).
void testNoTrailingNewline() {
    auto r = parseString("a=eins");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "eins");
}

// -- F2: overlong lines must be dropped, not split ---------------------------

// The parser's internal fgets buffer is 4096 bytes. A single line longer
// than that would otherwise be split into two fgets reads, producing a
// truncated prefix (parsed as a bogus mapping) and a tail (misparsed as
// a new line). All three entries below must parse in full; the overlong
// line in the middle must be dropped.
void testOverlongLineSkipped() {
    std::string big(5000, 'x');
    auto r = parseString("a=eins\no=" + big + "\nu=drei\n");
    EXPECT(r.size() == 2);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "eins");
    EXPECT(r[1].input == "u");
    EXPECT(r[1].output == "drei");
}

// A line whose byte layout lands exactly on the buffer boundary (4094
// content bytes + '\n' = 4095 bytes read) must parse normally — back
// character is '\n', truncation check does not trigger.
void testLineExactlyAtBufferBoundary() {
    // "o=" + 4092 x's + "\n" = 4095 bytes → fits, back is '\n'
    std::string big(4092, 'x');
    auto r = parseString("a=eins\no=" + big + "\nu=drei\n");
    EXPECT(r.size() == 3);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output == "eins");
    EXPECT(r[1].input == "o");
    EXPECT(r[1].output.size() == 4092);
    EXPECT(r[2].input == "u");
    EXPECT(r[2].output == "drei");
}

// A line of 4095 content bytes with no trailing newline, followed by EOF.
// fgets fills the buffer (size == 4095, back != '\n'), but the next read
// returns EOF — the line is actually complete and must be accepted.
void testLineFillsBufferEofNoNewline() {
    // "a=" + 4093 x's = 4095 bytes, no '\n', EOF follows
    std::string big(4093, 'x');
    auto r = parseString("a=" + big);
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "a");
    EXPECT(r[0].output.size() == 4093);
}

// Two overlong lines back-to-back must both be skipped without corrupting
// parser state for the trailing valid line.
void testConsecutiveOverlongLinesSkipped() {
    std::string big1(6000, 'x');
    std::string big2(7000, 'y');
    auto r = parseString("a=" + big1 +
                         "\n"
                         "o=" +
                         big2 +
                         "\n"
                         "u=drei\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0].input == "u");
    EXPECT(r[0].output == "drei");
}

// -- joinOutputs: the escaping inverse of splitOutputs -----------------------

void testJoinOutputsRoundTrip() {
    using schnelle_umlaute::joinOutputs;
    using schnelle_umlaute::splitOutputs;
    // splitOutputs(joinOutputs(v)) == v across a range of variant lists,
    // including literal commas (escaped as ",,") and a lone comma.
    const std::vector<std::vector<std::string>> cases = {
        {},
        {"ae"},
        {"ae", "oe", "ue"},
        {"a,b"},       // one variant carrying a literal comma
        {"a,b", "c"},  // a literal comma right next to a separator
        {","},         // a lone comma variant
        {",", "."},    // a comma variant first, then another
        {"a,,b"},      // two literal commas inside one variant
    };
    for (const auto &v : cases) {
        EXPECT(splitOutputs(joinOutputs(v)) == v);
    }
    // Empty variants are dropped, matching splitOutputs.
    EXPECT(joinOutputs({"a", "", "b"}) == "a,b");
    // A literal comma is written as a doubled comma.
    EXPECT(joinOutputs({"a,b"}) == "a,,b");
    EXPECT(joinOutputs({","}) == ",,");
}

} // namespace

int main() {
    testValidAscii();
    testValidMultiByte();

    testTwoByteBadContinuationSkipped();
    testThreeByteBadContinuationSkipped();
    testFourByteBadContinuationSkipped();
    testContinuationByteAsLeadSkipped();
    testFfByteAsLeadSkipped();
    testMixedValidAndInvalid();

    testCommentAndEmptyLinesSkipped();
    testEqualsAsInputKey();
    testEscapedHashInputKey();
    testEscapedBackslashInputKey();
    testBareHashStillComment();
    testEscapedInputEmptyOutputSkipped();
    testLegacyBareBackslashInputKey();
    testEqualsInOutput();
    testMissingEqualsSkipped();
    testEmptyOutputSkipped();
    testCrlfTrimmed();
    testNoTrailingNewline();

    testOverlongLineSkipped();
    testLineExactlyAtBufferBoundary();
    testLineFillsBufferEofNoNewline();
    testConsecutiveOverlongLinesSkipped();

    testJoinOutputsRoundTrip();

    std::printf("All mappings-io parser tests passed.\n");
    return 0;
}
