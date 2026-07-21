// Unit tests for the merge-override sidecar parser (merge_override_io.h).
// Pure header-only logic. Verifies the directive parsing (-/!/~), the comma /
// double-comma escaping of value lists, comment/unknown-line skipping, and a
// serialize -> parse round-trip. Variant tokens are plain ASCII (the logic is
// string-agnostic), but the bases include real multi-byte UTF-8 umlauts since
// that is the app's actual domain and the base is read as one UTF-8 character.

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
    EXPECT(std::fseek(fp, 0, SEEK_SET) == 0);
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

// A real multi-byte UTF-8 base (this is an umlaut app): the base is the first
// whole UTF-8 character of the directive, so its full byte sequence is taken,
// not just the leading byte. ä = U+00E4, ö = U+00F6, ß = U+00DF.
void testUtf8Base() {
    OverrideLayer l =
        parseStr("-\xC3\xA4\n!\xC3\xB6=x,y\n~\xC3\x9F=a,b\n");
    EXPECT(l.removedBases.count("\xC3\xA4") == 1); // ä removed entirely
    EXPECT(l.removedBases.size() == 1);
    EXPECT(l.perBase.at("\xC3\xB6").remove == Vec({"x", "y"})); // ö
    EXPECT(l.perBase.at("\xC3\x9F").order == Vec({"a", "b"}));  // ß
}

// A literal comma in a value round-trips: serialize emits the double-comma
// escape (joinOutputs), and parse reads it back as one variant. This exercises
// the serialize side of the escaping, which the plain round-trip below does not.
void testSerializeCommaEscaping() {
    OverrideLayer in;
    in.perBase["a"].remove = {"B,C"}; // a single variant containing a comma
    const std::string text = serializeMergeOverride(in);
    EXPECT(text.find("B,,C") != std::string::npos); // comma doubled on the way out
    OverrideLayer out = parseStr(text);
    EXPECT(out.perBase.at("a").remove == Vec({"B,C"}));
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
    testUtf8Base();
    testSerializeCommaEscaping();
    testSkips();
    testMalformedOpSkipped();
    testRoundTrip();
    std::fprintf(stderr, "testmergeoverrideio: all passed\n");
    return 0;
}
