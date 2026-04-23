// Shared EXPECT macro for the addon's standalone test binaries.
//
// Each testX.cpp is its own executable, built without a framework like
// GoogleTest to keep the link graph small (Qt6::Core is enough, often not
// even that). They all share the same "assert-or-abort-with-location"
// pattern, so the macro lives in one place.
//
// std::abort terminates the whole test binary on first failure — that
// lines up with how ctest reads the exit code, and the file:line message
// on stderr is what --output-on-failure surfaces.

#ifndef SCHNELLE_UMLAUTE_TESTS_TEST_EXPECT_H
#define SCHNELLE_UMLAUTE_TESTS_TEST_EXPECT_H

#include <cstdio>
#include <cstdlib>

#define EXPECT(cond) do {                                                    \
    if (!(cond)) {                                                           \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        std::abort();                                                        \
    }                                                                        \
} while (0)

#endif
