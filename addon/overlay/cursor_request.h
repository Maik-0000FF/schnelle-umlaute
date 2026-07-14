#ifndef SCHNELLE_UMLAUTE_CURSOR_REQUEST_H
#define SCHNELLE_UMLAUTE_CURSOR_REQUEST_H

// Query-id bookkeeping for the KWin cursor source: which reply belongs to the
// query in flight, and how ids are handed out. Pure and free of Qt/DBus so the
// rule is unit-tested (tests/testcursorrequest.cpp) without a compositor.

#include <limits>

namespace schnelle_umlaute {

// kNoRequest means "no query in flight". It is never handed to a script, so an
// idle source cannot be matched by any reply. Ids travel over D-Bus as a plain
// int: KWin's callDBus() marshals a script number as int32 without knowing the
// declared signature, so a wider type would fail to match SendCursor at all.
constexpr int kNoRequest = 0;
constexpr int kFirstRequestId = 1;

// True when a reply belongs to the query that is actually in flight. A reply
// from a superseded or timed-out query carries the pointer of a gesture that is
// already over, so applying it would move the overlay that is open NOW to a
// stale point. An idle source (activeId == kNoRequest) matches nothing at all.
//
// This is a correlation check, not an authorisation one: the id is written in
// plain text into the script file, so it keeps replies apart, it does not keep
// callers out. SendCursor stays unauthenticated by design (see the trust note
// on OverlayDBusAdaptor::SendCursor).
inline bool isReplyForActiveQuery(int replyId, int activeId) {
    return activeId != kNoRequest && replyId == activeId;
}

// The counter value that follows `counter`, wrapping back to the first id
// instead of overflowing (signed overflow is undefined behaviour). An id only
// has to differ from the queries it could race, never to be unique for all
// time, and the wrap never yields kNoRequest.
inline int nextRequestId(int counter) {
    return counter == std::numeric_limits<int>::max() ? kFirstRequestId
                                                      : counter + 1;
}

} // namespace schnelle_umlaute

#endif
