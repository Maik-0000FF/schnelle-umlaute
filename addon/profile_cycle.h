#ifndef SCHNELLE_UMLAUTE_PROFILE_CYCLE_H
#define SCHNELLE_UMLAUTE_PROFILE_CYCLE_H

#include <string>
#include <vector>

// Pure logic for the profile cycle shortcut, kept free of fcitx/Qt so it can be
// unit-tested directly. The engine builds the input list from its loaded
// profiles and feeds the result to switchProfile.

namespace schnelle_umlaute {

struct CycleEntry {
    std::string name;
    bool favorite;
};

// The names the cycle shortcut steps through: the favorites if any profile is
// marked favorite, otherwise all profiles. Order is preserved.
inline std::vector<std::string>
cycleNames(const std::vector<CycleEntry> &profiles) {
    std::vector<std::string> favorites;
    std::vector<std::string> all;
    for (const auto &p : profiles) {
        all.push_back(p.name);
        if (p.favorite) {
            favorites.push_back(p.name);
        }
    }
    return favorites.empty() ? all : favorites;
}

// Target name for one cycle step (delta +1 next, -1 previous), wrapping around.
// If the active profile is not in the list (e.g. cycling favorites while a
// non-favorite is active), steps to the first (delta > 0) or last (delta < 0)
// entry. Returns "" when the list is empty.
inline std::string cycleTarget(const std::vector<std::string> &list,
                               const std::string &active, int delta) {
    const int n = static_cast<int>(list.size());
    if (n == 0) {
        return "";
    }
    int idx = -1;
    for (int i = 0; i < n; ++i) {
        if (list[i] == active) {
            idx = i;
            break;
        }
    }
    int next;
    if (idx < 0) {
        next = (delta > 0) ? 0 : n - 1;
    } else {
        next = (((idx + delta) % n) + n) % n;
    }
    return list[next];
}

} // namespace schnelle_umlaute

#endif
