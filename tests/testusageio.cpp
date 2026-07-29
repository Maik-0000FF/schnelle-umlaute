// Unit tests for the usage-counter parser/serializer (usage_io.h). Standalone —
// header-only, just libc. The file is engine-written / editor-read, so the two
// sides must agree byte-for-byte; the round-trip and the malformed-line
// tolerance are what guard that.

#include "usage_io.h"

#include "test_expect.h"

#include <cstdio>
#include <string>

using schnelle_umlaute::parseUsage;
using schnelle_umlaute::serializeUsage;
using schnelle_umlaute::UsageCounts;

namespace {

UsageCounts parseString(const std::string &content) {
    FILE *fp = std::tmpfile();
    EXPECT(fp != nullptr);
    if (!content.empty())
        std::fwrite(content.data(), 1, content.size(), fp);
    EXPECT(std::fseek(fp, 0, SEEK_SET) == 0);
    auto c = parseUsage(fp);
    std::fclose(fp);
    return c;
}

void testRoundTrip() {
    UsageCounts c;
    c["a"]["\xc3\xa4"] = 12;    // ä
    c["a"]["\xc3\xa0"] = 40;    // à
    c["\xc3\x9f"]["ss"] = 3;    // ß -> ss

    const std::string text = serializeUsage(c);
    auto back = parseString(text);
    EXPECT(back.at("a").at("\xc3\xa4") == 12);
    EXPECT(back.at("a").at("\xc3\xa0") == 40);
    EXPECT(back.at("\xc3\x9f").at("ss") == 3);

    // Sorted, deterministic: re-serialize is byte-identical.
    EXPECT(serializeUsage(back) == text);
}

void testMalformedAndNegativeSkipped() {
    auto c = parseString("a\t\xc3\xa4\t7\n"
                         "# comment\n"
                         "missingtabs\n"
                         "a\t\xc3\xa0\tnotanumber\n"
                         "a\t\xc3\xb6\t-5\n"    // negative → skipped
                         // Out of range for long long: must be skipped as
                         // malformed, not clamped to LLONG_MAX and stored as a
                         // real counter (which would outrank every honest count
                         // in the frequency order for good).
                         "a\t\xc3\xbc\t99999999999999999999\n"
                         // Must stay AFTER the overflow line: strtoll does not
                         // clear errno on success, so this valid count is what
                         // pins the per-iteration errno reset. Reorder the two
                         // and that half of the coverage silently disappears.
                         "o\t\xc3\xb6\t9\n");
    EXPECT(c.at("a").at("\xc3\xa4") == 7);
    EXPECT(c.at("a").count("\xc3\xa0") == 0);
    EXPECT(c.at("a").count("\xc3\xb6") == 0);
    EXPECT(c.at("a").count("\xc3\xbc") == 0);
    EXPECT(c.at("o").at("\xc3\xb6") == 9);
}

// An overlong line must be dropped whole, not split. Split in two, its prefix
// would land as a counter under a truncated variant and its tail would be read
// as a further line, so a single corrupt entry would poison the frequency order
// with two bogus ones.
void testOverlongLineDropped() {
    const std::string huge(schnelle_umlaute::kLineBufferSize + 100, 'x');
    auto c = parseString("a\t\xc3\xa4\t12\n"
                         "a\t" +
                         huge +
                         "\t7\n"
                         "o\t\xc3\xb6\t5\n");
    EXPECT(c.at("a").size() == 1);
    EXPECT(c.at("a").at("\xc3\xa4") == 12);
    EXPECT(c.at("o").at("\xc3\xb6") == 5);
}

// The bug the escaping exists for: a tab is a legal mapped output (the editor
// rejects only \n and \r), and unescaped it put a fourth field on the line, so
// the counter was dropped on every load and could never accumulate.
void testTabVariantRoundTrips() {
    UsageCounts c;
    c["a"]["\t"] = 5;
    c["a"]["\xc3\xa4"] = 2;

    const std::string text = serializeUsage(c);
    // On the wire the tab is the two characters backslash + t, so the line
    // still has exactly two separators.
    EXPECT(text.find("a\t\\t\t5\n") != std::string::npos);

    auto back = parseString(text);
    EXPECT(back.at("a").at("\t") == 5);
    EXPECT(back.at("a").at("\xc3\xa4") == 2);
    EXPECT(serializeUsage(back) == text);
}

// The escape character itself has to survive, or a variant carrying a
// backslash would come back as something else.
void testBackslashVariantRoundTrips() {
    UsageCounts c;
    c["\\"]["\\t"] = 4; // base a lone backslash, variant backslash + t
    c["a"]["x\\"] = 1;  // trailing backslash

    auto back = parseString(serializeUsage(c));
    EXPECT(back.at("\\").at("\\t") == 4);
    EXPECT(back.at("\\").count("\t") == 0); // NOT read as a tab
    EXPECT(back.at("a").at("x\\") == 1);
}

// Without the marker the file predates escaping, where a backslash stood for
// itself. Reading it raw is what keeps such a variant intact.
void testLegacyFileParsedRaw() {
    auto c = parseString("a\t\\t\t5\n");
    EXPECT(c.at("a").at("\\t") == 5); // two characters, not a tab
    EXPECT(c.at("a").count("\t") == 0);

    // And the next save carries it over into the escaped format unchanged.
    const std::string text = serializeUsage(c);
    EXPECT(text.rfind(schnelle_umlaute::kUsageFormatMarker, 0) == 0);
    auto back = parseString(text);
    EXPECT(back.at("a").at("\\t") == 5);
    EXPECT(back.at("a").count("\t") == 0);
}

// A file hand-edited into an escape the format does not define keeps both
// characters instead of silently losing the backslash.
void testUnknownEscapeKeptLiteral() {
    auto c = parseString(std::string(schnelle_umlaute::kUsageFormatMarker) +
                         "\n"
                         "a\t\\q\t3\n"
                         "o\tx\\\t2\n"); // trailing lone backslash
    EXPECT(c.at("a").at("\\q") == 3);
    EXPECT(c.at("o").at("x\\") == 2);
}

// An empty table must serialize to an empty file, marker and all: the editor
// reads "no counters" off a zero-byte usage.conf to disable its reset control,
// so a lone header line would light that control up with nothing behind it.
void testEmptyTableSerializesEmpty() {
    EXPECT(serializeUsage(UsageCounts{}).empty());
}

} // namespace

int main() {
    testRoundTrip();
    testMalformedAndNegativeSkipped();
    testOverlongLineDropped();
    testTabVariantRoundTrips();
    testBackslashVariantRoundTrips();
    testLegacyFileParsedRaw();
    testUnknownEscapeKeptLiteral();
    testEmptyTableSerializesEmpty();
    std::printf("testusageio: all passed\n");
    return 0;
}
