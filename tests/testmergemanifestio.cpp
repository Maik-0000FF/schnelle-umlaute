// Unit tests for the merge manifest parser/serializer (merge_manifest_io.h).
// Standalone — header-only, just libc. Proves the tab-separated format
// round-trips base + ordered sources + per-base order overrides, including a
// variant value containing a comma (which the tab fields must NOT need to
// escape) and a multi-byte base char.

#include "merge_manifest_io.h"

#include "test_expect.h"

#include <cstdio>
#include <string>

using schnelle_umlaute::MergeManifest;
using schnelle_umlaute::parseMergeManifest;
using schnelle_umlaute::serializeMergeManifest;
using schnelle_umlaute::Variant;

namespace {

MergeManifest parseString(const std::string &content) {
    FILE *fp = std::tmpfile();
    EXPECT(fp != nullptr);
    if (!content.empty())
        std::fwrite(content.data(), 1, content.size(), fp);
    EXPECT(std::fseek(fp, 0, SEEK_SET) == 0);
    auto m = parseMergeManifest(fp);
    std::fclose(fp);
    return m;
}

void testEmptyIsNoMerge() {
    auto m = parseString("");
    EXPECT(m.base.empty());
    EXPECT(m.sources.empty());
    EXPECT(m.order.empty());
}

void testBaseAndSourceOrder() {
    auto m = parseString("base=profiles/de.txt\n"
                         "source=profiles/fr.txt\n"
                         "source=profiles/es.txt\n");
    EXPECT(m.base == "profiles/de.txt");
    EXPECT(m.sources.size() == 2);
    EXPECT(m.sources[0] == "profiles/fr.txt");
    EXPECT(m.sources[1] == "profiles/es.txt");
}

void testRoundTripWithOverrideAndComma() {
    MergeManifest m;
    m.base = "mappings.txt";
    m.sources = {"profiles/fr.txt"};
    // A variant value containing a comma must survive the tab format untouched.
    m.order["a"] = {{"\xc3\xa0", "profiles/fr.txt"},
                    {"x,y", "mappings.txt"}};
    // Multi-byte base char key.
    m.order["\xc3\x9f"] = {{"ss", "mappings.txt"}};

    const std::string text = serializeMergeManifest(m);
    auto back = parseString(text);
    EXPECT(back.base == "mappings.txt");
    EXPECT(back.sources.size() == 1 && back.sources[0] == "profiles/fr.txt");
    EXPECT(back.order.size() == 2);
    EXPECT(back.order.at("a").size() == 2);
    EXPECT((back.order.at("a")[0] == Variant{"\xc3\xa0", "profiles/fr.txt"}));
    EXPECT((back.order.at("a")[1] == Variant{"x,y", "mappings.txt"}));
    EXPECT((back.order.at("\xc3\x9f")[0] == Variant{"ss", "mappings.txt"}));

    // Serializing the parsed result again is byte-identical (deterministic).
    EXPECT(serializeMergeManifest(back) == text);
}

void testMalformedLinesSkipped() {
    auto m = parseString("base=mappings.txt\n"
                         "# a comment\n"
                         "garbage line without equals\n"
                         "~\tonly\ttwo\n"       // too few tab fields → skipped
                         "source=\n"             // empty source → skipped
                         "source=profiles/x.txt\n");
    EXPECT(m.base == "mappings.txt");
    EXPECT(m.sources.size() == 1 && m.sources[0] == "profiles/x.txt");
    EXPECT(m.order.empty());
}

// An overlong line must be dropped whole, not split. Split in two, its tail
// would be read as a further directive line — a truncated "source=" prefix plus
// a bogus second entry from the same corrupt line.
void testOverlongLineDropped() {
    const std::string huge(schnelle_umlaute::kLineBufferSize + 100, 'x');
    auto m = parseString("base=mappings.txt\n"
                         "source=" +
                         huge +
                         "\n"
                         "source=profiles/x.txt\n");
    EXPECT(m.base == "mappings.txt");
    EXPECT(m.sources.size() == 1 && m.sources[0] == "profiles/x.txt");
}

} // namespace

int main() {
    testEmptyIsNoMerge();
    testBaseAndSourceOrder();
    testRoundTripWithOverrideAndComma();
    testMalformedLinesSkipped();
    testOverlongLineDropped();
    std::printf("testmergemanifestio: all passed\n");
    return 0;
}
