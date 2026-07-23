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
                         "o\t\xc3\xb6\t9\n");
    EXPECT(c.at("a").at("\xc3\xa4") == 7);
    EXPECT(c.at("a").count("\xc3\xa0") == 0);
    EXPECT(c.at("a").count("\xc3\xb6") == 0);
    EXPECT(c.at("o").at("\xc3\xb6") == 9);
}

} // namespace

int main() {
    testRoundTrip();
    testMalformedAndNegativeSkipped();
    std::printf("testusageio: all passed\n");
    return 0;
}
