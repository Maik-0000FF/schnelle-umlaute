// Unit tests for the pure profile-cycle logic (profile_cycle.h): the favorites
// vs all selection and the wrapping next/previous step. No fcitx/Qt needed.

#include "profile_cycle.h"
#include "test_expect.h"

#include <cstdio>
#include <string>
#include <vector>

using schnelle_umlaute::CycleEntry;
using schnelle_umlaute::cycleNames;
using schnelle_umlaute::cycleTarget;

int main() {
    // No favorites: cycle through all profiles, in order, wrapping.
    {
        std::vector<CycleEntry> p = {{"A", false}, {"B", false}, {"C", false}};
        auto names = cycleNames(p);
        EXPECT(names.size() == 3);
        EXPECT(cycleTarget(names, "A", +1) == "B");
        EXPECT(cycleTarget(names, "C", +1) == "A"); // wrap forward
        EXPECT(cycleTarget(names, "A", -1) == "C"); // wrap backward
        EXPECT(cycleTarget(names, "B", -1) == "A");
    }

    // Some favorites: cycle through favorites only, preserving order.
    {
        std::vector<CycleEntry> p = {
            {"A", false}, {"B", true}, {"C", false}, {"D", true}};
        auto names = cycleNames(p);
        EXPECT(names.size() == 2);
        EXPECT(names[0] == "B");
        EXPECT(names[1] == "D");
        EXPECT(cycleTarget(names, "B", +1) == "D");
        EXPECT(cycleTarget(names, "D", +1) == "B"); // wrap
        // Active is not a favorite: step to first (next) / last (previous).
        EXPECT(cycleTarget(names, "A", +1) == "B");
        EXPECT(cycleTarget(names, "A", -1) == "D");
    }

    // Empty profile list: nothing to cycle to.
    {
        std::vector<CycleEntry> p = {};
        EXPECT(cycleTarget(cycleNames(p), "X", +1).empty());
    }

    std::fprintf(stderr, "ok testprofilecycle\n");
    return 0;
}
