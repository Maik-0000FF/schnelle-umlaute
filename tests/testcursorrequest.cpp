// Unit tests for the KWin cursor source's query-id rules.
// Pure functions, no DBus/Qt/KWin runtime — they decide which SendCursor reply
// the daemon is allowed to act on, the one thing that keeps a reply from a
// gesture that is already over out of the overlay that is open now.

#include "overlay/cursor_request.h"

#include "test_expect.h"

#include <cstdio>
#include <limits>

using schnelle_umlaute::isReplyForActiveQuery;
using schnelle_umlaute::kFirstRequestId;
using schnelle_umlaute::kNoRequest;
using schnelle_umlaute::nextRequestId;

// The reply the live query is waiting for: the only one that may be applied.
void testReplyOfActiveQueryIsAccepted() {
    EXPECT(isReplyForActiveQuery(7, 7));
    EXPECT(isReplyForActiveQuery(kFirstRequestId, kFirstRequestId));
}

// The race this whole mechanism exists for: a query was superseded (a new
// overlay opened) or timed out, and its KWin script answers late. Its pointer
// belongs to a gesture that is over, so it must not resolve the query that is
// running now.
void testLateReplyOfSupersededQueryIsDropped() {
    EXPECT(!isReplyForActiveQuery(7, 8));
    EXPECT(!isReplyForActiveQuery(8, 7));
}

// Nothing in flight: no reply matches, so a stray or spurious SendCursor (the
// method is unauthenticated) cannot move an overlay that was placed by other
// means, e.g. on the grid fallback.
void testIdleSourceMatchesNothing() {
    EXPECT(!isReplyForActiveQuery(kFirstRequestId, kNoRequest));
    EXPECT(!isReplyForActiveQuery(kNoRequest, kNoRequest));
    EXPECT(!isReplyForActiveQuery(0, kNoRequest));
}

// kNoRequest is the "idle" marker, so it must never be handed out as a real id;
// a query carrying it could be matched by a source that is not even waiting.
void testNoRequestIsNeverAValidId() {
    EXPECT(kFirstRequestId != kNoRequest);
    EXPECT(!isReplyForActiveQuery(kNoRequest, kFirstRequestId));
}

// Plain increment for the common case.
void testCounterAdvances() {
    EXPECT(nextRequestId(kFirstRequestId) == kFirstRequestId + 1);
    EXPECT(nextRequestId(41) == 42);
}

// The counter wraps rather than overflowing (signed overflow is UB), and the
// wrap lands on a usable id, never on kNoRequest.
void testCounterWrapsInsteadOfOverflowing() {
    const int wrapped = nextRequestId(std::numeric_limits<int>::max());
    EXPECT(wrapped == kFirstRequestId);
    EXPECT(wrapped != kNoRequest);
}

int main() {
    testReplyOfActiveQueryIsAccepted();
    testLateReplyOfSupersededQueryIsDropped();
    testIdleSourceMatchesNothing();
    testNoRequestIsNeverAValidId();
    testCounterAdvances();
    testCounterWrapsInsteadOfOverflowing();
    std::printf("testcursorrequest: all passed\n");
    return 0;
}
