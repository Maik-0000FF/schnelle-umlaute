// Unit tests for the global merge-overlay composition (profile_compose.h).
// Pure header-only logic — no fcitx5/Qt runtime. Verifies the compose contract
// (issue #112): ordered union of sources, per-variant override ops (add /
// remove / order), whole-base removal, and dedup. Variant tokens are plain
// ASCII ("A", "B", ...) since the logic is string-agnostic and tokens keep the
// expectations readable and encoding-independent.

#include "profile_compose.h"
#include "test_expect.h"

#include <string>
#include <vector>

using schnelle_umlaute::BaseOverride;
using schnelle_umlaute::compose;
using schnelle_umlaute::composeBase;
using schnelle_umlaute::OverrideLayer;
using schnelle_umlaute::VariantMap;

using Vec = std::vector<std::string>;

// composeBase over source maps passed by value, for terse tests.
static Vec baseOf(const std::string &b, const std::vector<VariantMap> &srcMaps,
                  const BaseOverride &ops = {}) {
    std::vector<const VariantMap *> ptrs;
    ptrs.reserve(srcMaps.size());
    for (const auto &m : srcMaps)
        ptrs.push_back(&m);
    return composeBase(b, ptrs, ops);
}

// A single source passes variants through untouched; an unknown base is empty.
void testSingleSourceIdentity() {
    VariantMap de{{"a", {"A1", "A2"}}, {"o", {"O1"}}};
    EXPECT(baseOf("a", {de}) == Vec({"A1", "A2"}));
    EXPECT(baseOf("o", {de}) == Vec({"O1"}));
    EXPECT(baseOf("x", {de}).empty());
}

// Two sources union in source order with dedup: active leads, the overlay
// appends only what is new.
void testOrderedUnionDedup() {
    VariantMap active{{"a", {"A", "B"}}};
    VariantMap overlay{{"a", {"B", "C"}}};
    EXPECT(baseOf("a", {active, overlay}) == Vec({"A", "B", "C"}));
}

// A base only in a later source still appears.
void testBaseOnlyInOverlay() {
    VariantMap active{{"a", {"A"}}};
    VariantMap overlay{{"n", {"N"}}};
    EXPECT(baseOf("n", {active, overlay}) == Vec({"N"}));
}

// Three sources compose in sequence: active first, then overlay 1, then 2.
void testThreeSourceOrder() {
    VariantMap active{{"a", {"A"}}};
    VariantMap ov1{{"a", {"S"}}};
    VariantMap ov2{{"a", {"N"}}};
    EXPECT(baseOf("a", {active, ov1, ov2}) == Vec({"A", "S", "N"}));
}

// `remove` drops an inherited variant and stays live: even though a later
// source also provides it, it does not come back.
void testRemoveIsLive() {
    VariantMap active{{"a", {"A", "B"}}};
    VariantMap overlay{{"a", {"B", "C"}}}; // B re-provided by the overlay
    BaseOverride ops;
    ops.remove = {"B"};
    EXPECT(baseOf("a", {active, overlay}, ops) == Vec({"A", "C"}));
}

// `add` appends own variants; an add already present is not duplicated.
void testAddOwnAndNoDup() {
    VariantMap active{{"a", {"A"}}};
    BaseOverride ops;
    ops.add = {"X", "A"}; // A already inherited
    EXPECT(baseOf("a", {active}, ops) == Vec({"A", "X"}));
}

// `order` lists chosen variants first; an unlisted (new upstream) variant
// appends after; a listed-but-absent variant is ignored.
void testOrderReorderLiveFriendly() {
    VariantMap active{{"a", {"A", "B", "C"}}};
    BaseOverride ops;
    ops.order = {"C", "A", "Z"}; // Z is not in the pool -> ignored
    // C, A first (in that order), then the remaining pool (B) appended.
    EXPECT(baseOf("a", {active}, ops) == Vec({"C", "A", "B"}));
}

// Removing every variant (or providing an all-removing ops with no add) yields
// an empty list, so compose() drops the base.
void testEmptyPoolDropsBase() {
    VariantMap active{{"a", {"A", "B"}}};
    BaseOverride ops;
    ops.remove = {"A", "B"};
    EXPECT(baseOf("a", {active}, ops).empty());

    std::vector<const VariantMap *> ptrs{&active};
    OverrideLayer layer;
    layer.perBase["a"] = ops;
    VariantMap eff = compose(ptrs, layer);
    EXPECT(eff.find("a") == eff.end());
}

// removedBases drops a whole inherited base regardless of its variants.
void testRemovedBases() {
    VariantMap active{{"a", {"A"}}, {"o", {"O"}}};
    std::vector<const VariantMap *> ptrs{&active};
    OverrideLayer layer;
    layer.removedBases.insert("o");
    VariantMap eff = compose(ptrs, layer);
    EXPECT(eff.find("o") == eff.end());
    EXPECT(eff.at("a") == Vec({"A"}));
}

// removedBases wins over an add: a whole-base removal is absolute, so even a
// variant the override would add cannot resurrect the base.
void testRemovedBasesBeatsAdd() {
    VariantMap active{{"a", {"A"}}};
    std::vector<const VariantMap *> ptrs{&active};
    OverrideLayer layer;
    layer.perBase["a"].add = {"X"}; // would add X to base 'a'
    layer.removedBases.insert("a"); // but the base is removed outright
    VariantMap eff = compose(ptrs, layer);
    EXPECT(eff.find("a") == eff.end());
}

// compose() gathers bases from every source and applies per-base ops.
void testComposeFullMap() {
    VariantMap active{{"a", {"A"}}};
    VariantMap overlay{{"a", {"B"}}, {"n", {"N"}}};
    std::vector<const VariantMap *> ptrs{&active, &overlay};
    OverrideLayer layer;
    BaseOverride aOps;
    aOps.add = {"X"};
    layer.perBase["a"] = aOps;
    VariantMap eff = compose(ptrs, layer);
    EXPECT(eff.at("a") == Vec({"A", "B", "X"}));
    EXPECT(eff.at("n") == Vec({"N"}));
    EXPECT(eff.size() == 2);
}

// A profile that is both the active base and in the overlay contributes once.
void testSelfMergeDedup() {
    VariantMap p{{"a", {"A", "B"}}};
    // active == overlay entry (same map used twice in the sequence)
    EXPECT(baseOf("a", {p, p}) == Vec({"A", "B"}));
}

int main() {
    testSingleSourceIdentity();
    testOrderedUnionDedup();
    testBaseOnlyInOverlay();
    testThreeSourceOrder();
    testRemoveIsLive();
    testAddOwnAndNoDup();
    testOrderReorderLiveFriendly();
    testEmptyPoolDropsBase();
    testRemovedBases();
    testRemovedBasesBeatsAdd();
    testComposeFullMap();
    testSelfMergeDedup();
    std::fprintf(stderr, "testprofilecompose: all passed\n");
    return 0;
}
