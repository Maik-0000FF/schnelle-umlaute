// Unit tests for isSyntheticAutoRepeatRelease().
// Pure header-only logic. Verifies the predicate that recognises a held
// key's synthetic auto-repeat release from its frozen frontend timestamp
// (issue #73). The integration side (test-frontend events carry time()==0, so
// the predicate never fires and the historic immediate-commit path runs) is
// covered implicitly by the main testschnelleumlaute suite staying green.

#include "src/synthetic_autorepeat.h"

#include "test_expect.h"

#include <cstdio>

using fcitx::isSyntheticAutoRepeatRelease;

// Frozen timestamp: KWin stamps every event of a held key's auto-repeat burst
// with the original press time, so release time == press time → synthetic.
void testFrozenTimestampIsSynthetic() {
    EXPECT(isSyntheticAutoRepeatRelease(18354071, 18354071));
    EXPECT(isSyntheticAutoRepeatRelease(42, 42));
}

// A genuine release carries an advanced timestamp (the real hold duration),
// so it must not be treated as synthetic.
void testAdvancedTimestampIsGenuine() {
    EXPECT(!isSyntheticAutoRepeatRelease(18358119, 18358038)); // +81ms tap
    EXPECT(!isSyntheticAutoRepeatRelease(18354614, 18354071)); // burst end
    EXPECT(!isSyntheticAutoRepeatRelease(100, 0));
}

// time()==0 (frontend that never stamps events, e.g. the test frontend) must
// never match, even against a zero press time, otherwise every release would
// be swallowed. This is the essential safety guard.
void testZeroTimeIsNeverSynthetic() {
    EXPECT(!isSyntheticAutoRepeatRelease(0, 0));
    EXPECT(!isSyntheticAutoRepeatRelease(0, 18354071));
}

// Equality, not ordering: robust even if time() has wrapped past INT_MAX into
// negative values on a long uptime, where two frozen negative stamps still
// match and a differing pair still does not.
void testNegativeWrappedTimestamps() {
    EXPECT(isSyntheticAutoRepeatRelease(-5, -5));
    EXPECT(!isSyntheticAutoRepeatRelease(-5, -6));
}

int main() {
    testFrozenTimestampIsSynthetic();
    testAdvancedTimestampIsGenuine();
    testZeroTimeIsNeverSynthetic();
    testNegativeWrappedTimestamps();
    std::fprintf(stderr, "testsyntheticautorepeat: all tests passed\n");
    return 0;
}
