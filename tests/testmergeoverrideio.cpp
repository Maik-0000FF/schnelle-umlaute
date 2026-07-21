// Unit tests for the merge-override sidecar parser (merge_override_io.h).
// Pure header-only logic. Verifies the directive parsing (-/!/~), the comma /
// double-comma escaping of value lists, comment/unknown-line skipping, and a
// serialize -> parse round-trip. ASCII variant tokens keep it encoding-clean.

#include "merge_override_io.h"
#include "test_expect.h"

#include <cstdio>
#include <string>
#include <vector>

using schnelle_umlaute::OverrideLayer;
using schnelle_umlaute::parseMergeOverride;
using schnelle_umlaute::serializeMergeOverride;
using Vec = std::vector<std::string>;

// Parse from an in-memory string via a temporary FILE (portable, no I/O deps).
static OverrideLayer parseStr(const std::string &s) {
    FILE *fp = std::tmpfile();
    EXPECT(fp != nullptr);
    std::fwrite(s.data(), 1, s.size(), fp);
    std::rewind(fp);
    OverrideLayer l = parseMergeOverride(fp);
    std::fclose(fp);
    return l;
}

// The three directives parse into the right fields.
void testDirectives() {
    OverrideLayer l = parseStr("-x\n!a=B,C\n~a=B,A,C\n# a comment\n\n");
    EXPECT(l.removedBases.count("x") == 1);
    EXPECT(l.removedBases.size() == 1);
    EXPECT(l.perBase.at("a").remove == Vec({"B", "C"}));
    EXPECT(l.perBase.at("a").order == Vec({"B", "A", "C"}));
    EXPECT(l.perBase.size() == 1);
    // `add` is never stored in the sidecar.
    EXPECT(l.perBase.at("a").add.empty());
}

// A literal comma inside a variant survives via double-comma escaping.
void testCommaEscaping() {
    OverrideLayer l = parseStr("!a=B,,C\n"); // one variant "B,C"
    EXPECT(l.perBase.at("a").remove == Vec({"B,C"}));
}

// Comments, blank lines, and unrecognized directives are ignored.
void testSkips() {
    OverrideLayer l = parseStr("random line\n=nope\n# c\n\n?bad\n");
    EXPECT(l.removedBases.empty());
    EXPECT(l.perBase.empty());
}

// A '!' / '~' without an '=' is not a valid op and is skipped.
void testMalformedOpSkipped() {
    OverrideLayer l = parseStr("!a\n~b\n");
    EXPECT(l.perBase.empty());
}

// serialize -> parse reconstructs the same layer.
void testRoundTrip() {
    OverrideLayer in;
    in.removedBases.insert("x");
    in.perBase["a"].remove = {"B"};
    in.perBase["a"].order = {"B", "A", "C"};
    in.perBase["e"].order = {"E1", "E2"};

    OverrideLayer out = parseStr(serializeMergeOverride(in));
    EXPECT(out.removedBases == in.removedBases);
    EXPECT(out.perBase.at("a").remove == Vec({"B"}));
    EXPECT(out.perBase.at("a").order == Vec({"B", "A", "C"}));
    EXPECT(out.perBase.at("e").order == Vec({"E1", "E2"}));
    // A base with only an order op has no remove entries and vice versa.
    EXPECT(out.perBase.at("e").remove.empty());
}

int main() {
    testDirectives();
    testCommaEscaping();
    testSkips();
    testMalformedOpSkipped();
    testRoundTrip();
    std::fprintf(stderr, "testmergeoverrideio: all passed\n");
    return 0;
}
