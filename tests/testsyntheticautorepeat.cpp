// Unit tests for isSyntheticAutoRepeatRelease().
// Pure header-only logic. Verifies the predicate that recognises a held
// key's synthetic auto-repeat release from its frozen frontend timestamp
// plus a minimum wall-clock gap since the gesture's press (issue #73). The
// integration side (test-frontend events carry time()==0, so the predicate
// never fires and the historic immediate-commit path runs) is covered
// implicitly by the main testschnelleumlaute suite staying green.

#include "src/synthetic_autorepeat.h"

#include "test_expect.h"

#include <cstdint>
#include <cstdio>

using fcitx::isSyntheticAutoRepeatRelease;
using fcitx::kSyntheticReleaseMinElapsedUsec;

// Realistic wall-clock gaps between a gesture's press and a release:
// one auto-repeat period at the common 25 Hz rate, and the initial repeat
// delay before the first repeat of a burst. Both far exceed the plausibility
// threshold.
constexpr uint64_t kRepeatPeriodUsec = 40'000;
constexpr uint64_t kInitialRepeatDelayUsec = 600'000;

// Frozen timestamp: KWin stamps every event of a held key's auto-repeat burst
// with the original press time, so release time == press time (arriving a
// repeat period or more after the press) → synthetic.
void testFrozenTimestampIsSynthetic() {
    EXPECT(isSyntheticAutoRepeatRelease(18354071, 18354071,
                                        kInitialRepeatDelayUsec));
    EXPECT(isSyntheticAutoRepeatRelease(42, 42, kRepeatPeriodUsec));
}

// A genuine release carries an advanced timestamp (the real hold duration),
// so it must not be treated as synthetic regardless of the elapsed time.
void testAdvancedTimestampIsGenuine() {
    EXPECT(!isSyntheticAutoRepeatRelease(18358119, 18358038,
                                         kRepeatPeriodUsec)); // +81ms tap
    EXPECT(!isSyntheticAutoRepeatRelease(18354614, 18354071,
                                         kInitialRepeatDelayUsec)); // burst end
    EXPECT(!isSyntheticAutoRepeatRelease(100, 0, kRepeatPeriodUsec));
}

// time()==0 (frontend that never stamps events, e.g. the test frontend) must
// never match, even against a zero press time, otherwise every release would
// be swallowed. This is the essential safety guard.
void testZeroTimeIsNeverSynthetic() {
    EXPECT(!isSyntheticAutoRepeatRelease(0, 0, kRepeatPeriodUsec));
    EXPECT(!isSyntheticAutoRepeatRelease(0, 18354071, kRepeatPeriodUsec));
}

// Equality, not ordering: robust even if time() has wrapped past INT_MAX into
// negative values on a long uptime, where two frozen negative stamps still
// match and a differing pair still does not.
void testNegativeWrappedTimestamps() {
    EXPECT(isSyntheticAutoRepeatRelease(-5, -5, kRepeatPeriodUsec));
    EXPECT(!isSyntheticAutoRepeatRelease(-5, -6, kRepeatPeriodUsec));
}

// Injected input (ydotool/wtype, autotype) can produce a GENUINE press+release
// pair sharing one millisecond timestamp, delivered back to back. The elapsed
// guard must keep such pairs on the historic immediate-commit path: equal
// timestamps alone are not enough below the plausibility threshold.
void testEqualTimestampFastPairIsGenuine() {
    EXPECT(!isSyntheticAutoRepeatRelease(42, 42, 0));
    EXPECT(!isSyntheticAutoRepeatRelease(42, 42, 500));
    EXPECT(!isSyntheticAutoRepeatRelease(
        18354071, 18354071, kSyntheticReleaseMinElapsedUsec - 1));
    // Boundary: exactly the threshold qualifies as synthetic.
    EXPECT(isSyntheticAutoRepeatRelease(18354071, 18354071,
                                        kSyntheticReleaseMinElapsedUsec));
}

int main() {
    testFrozenTimestampIsSynthetic();
    testAdvancedTimestampIsGenuine();
    testZeroTimeIsNeverSynthetic();
    testNegativeWrappedTimestamps();
    testEqualTimestampFastPairIsGenuine();
    std::fprintf(stderr, "testsyntheticautorepeat: all tests passed\n");
    return 0;
}
