// Unit tests for the base-anchored, duplicate-preserving profile compose
// (profile_compose.h). Standalone — header-only logic, just libc.
//
// The two properties that matter most and that the old dedup-based compose
// could not provide: duplicates are preserved with per-instance provenance,
// and the runtime projection collapses them by value (no dead cycle slots).

#include "profile_compose.h"

#include "test_expect.h"

#include <string>
#include <vector>

using schnelle_umlaute::compose;
using schnelle_umlaute::composeBase;
using schnelle_umlaute::ComposeSource;
using schnelle_umlaute::OrderOverride;
using schnelle_umlaute::projectValues;
using schnelle_umlaute::Variant;
using schnelle_umlaute::VariantMap;

namespace {

// A duplicate variant coming from two sources yields two instances, each
// tagged with its own source, in source order (base first, then appended).
void testDuplicatesPreservedWithProvenance() {
    VariantMap de{{"a", {"\xc3\xa4"}}};             // ä
    VariantMap fr{{"a", {"\xc3\xa0", "\xc3\xa4"}}}; // à, ä
    std::vector<ComposeSource> sources{{"mappings.txt", &de},
                                       {"profiles/fr.txt", &fr}};

    auto out = composeBase("a", sources, nullptr);
    EXPECT(out.size() == 3);
    EXPECT((out[0] == Variant{"\xc3\xa4", "mappings.txt"}));
    EXPECT((out[1] == Variant{"\xc3\xa0", "profiles/fr.txt"}));
    EXPECT((out[2] == Variant{"\xc3\xa4", "profiles/fr.txt"}));
}

// A missing source map or a base absent from a source contributes nothing.
void testMissingSourcesSkipped() {
    VariantMap de{{"a", {"\xc3\xa4"}}};
    std::vector<ComposeSource> sources{{"mappings.txt", &de},
                                       {"profiles/empty.txt", nullptr}};
    auto out = composeBase("a", sources, nullptr);
    EXPECT(out.size() == 1);
    EXPECT(out[0].sourceRef == "mappings.txt");
    EXPECT(composeBase("x", sources, nullptr).empty());
}

// The order override arranges instances by value+source; listed-and-present
// instances come first in override order, the rest append in natural order.
void testOrderOverrideArranges() {
    VariantMap de{{"a", {"\xc3\xa4"}}};
    VariantMap fr{{"a", {"\xc3\xa0", "\xc3\xa4"}}};
    std::vector<ComposeSource> sources{{"mappings.txt", &de},
                                       {"profiles/fr.txt", &fr}};
    std::vector<Variant> order{{"\xc3\xa0", "profiles/fr.txt"},
                               {"\xc3\xa4", "mappings.txt"}};
    auto out = composeBase("a", sources, &order);
    EXPECT(out.size() == 3);
    EXPECT((out[0] == Variant{"\xc3\xa0", "profiles/fr.txt"}));
    EXPECT((out[1] == Variant{"\xc3\xa4", "mappings.txt"}));
    EXPECT((out[2] == Variant{"\xc3\xa4", "profiles/fr.txt"})); // remainder
}

// A stored override that no longer matches (source edited/removed) self-heals:
// unmatched entries are dropped, the surviving instances keep natural order.
void testOrderOverrideSelfHeals() {
    VariantMap de{{"a", {"\xc3\xa4"}}};
    VariantMap fr{{"a", {"\xc3\xa0"}}};
    std::vector<ComposeSource> sources{{"mappings.txt", &de},
                                       {"profiles/fr.txt", &fr}};
    // Override references ö@removed (gone) and ä@mappings (present).
    std::vector<Variant> order{{"\xc3\xb6", "profiles/removed.txt"},
                               {"\xc3\xa4", "mappings.txt"}};
    auto out = composeBase("a", sources, &order);
    EXPECT(out.size() == 2);
    EXPECT((out[0] == Variant{"\xc3\xa4", "mappings.txt"})); // matched first
    EXPECT((out[1] == Variant{"\xc3\xa0", "profiles/fr.txt"})); // natural rest
}

// Runtime projection keeps duplicate values (order preserved), so the cycle
// matches the composed editor view: the second ä is a dead slot, not removed.
void testProjectValuesKeepsDuplicates() {
    std::vector<Variant> instances{{"\xc3\xa4", "mappings.txt"},
                                   {"\xc3\xa0", "profiles/fr.txt"},
                                   {"\xc3\xa4", "profiles/fr.txt"}};
    auto values = projectValues(instances);
    EXPECT(values.size() == 3);
    EXPECT(values[0] == "\xc3\xa4");
    EXPECT(values[1] == "\xc3\xa0");
    EXPECT(values[2] == "\xc3\xa4");
}

// compose() covers every base across the sources; projectValues(map) drops a
// base only if it has no instances (never happens here).
void testComposeAndProjectMap() {
    VariantMap de{{"a", {"\xc3\xa4"}}, {"o", {"\xc3\xb6"}}};
    VariantMap fr{{"a", {"\xc3\xa0"}}, {"e", {"\xc3\xa9"}}};
    std::vector<ComposeSource> sources{{"mappings.txt", &de},
                                       {"profiles/fr.txt", &fr}};
    auto composed = compose(sources, OrderOverride{});
    EXPECT(composed.size() == 3); // a, o, e
    EXPECT(composed.at("a").size() == 2);

    auto runtime = projectValues(composed);
    EXPECT(runtime.size() == 3);
    EXPECT(runtime.at("a").size() == 2); // ä, à (no dup here)
    EXPECT(runtime.at("o").size() == 1);
    EXPECT(runtime.at("e").size() == 1);
}

} // namespace

int main() {
    testDuplicatesPreservedWithProvenance();
    testMissingSourcesSkipped();
    testOrderOverrideArranges();
    testOrderOverrideSelfHeals();
    testProjectValuesKeepsDuplicates();
    testComposeAndProjectMap();
    std::printf("testprofilecompose: all passed\n");
    return 0;
}
