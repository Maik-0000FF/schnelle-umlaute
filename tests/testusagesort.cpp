// Unit tests for the single frequency-sort comparator (usage_sort.h).
// Standalone — header-only, just libc. The three properties the whole feature
// leans on: descending by count, stable tie-break on the stored order, and a
// no-op when all counts are zero (fresh stats look exactly like toggle-off).

#include "usage_sort.h"

#include "test_expect.h"

#include <string>
#include <unordered_map>
#include <vector>

using schnelle_umlaute::sortVariantsByUsage;

namespace {

void testDescendingByCount() {
    std::vector<std::string> stored{"\xc3\xa4", "\xc3\xa2", "\xc3\xa0"}; // ä â à
    std::unordered_map<std::string, long long> counts{
        {"\xc3\xa0", 40}, {"\xc3\xa4", 12}, {"\xc3\xa2", 3}};
    auto out = sortVariantsByUsage(stored, counts);
    EXPECT(out.size() == 3);
    EXPECT(out[0] == "\xc3\xa0"); // à, 40
    EXPECT(out[1] == "\xc3\xa4"); // ä, 12
    EXPECT(out[2] == "\xc3\xa2"); // â, 3
}

void testAllZeroIsStoredOrder() {
    std::vector<std::string> stored{"\xc3\xa4", "\xc3\xa2", "\xc3\xa0"};
    std::unordered_map<std::string, long long> counts; // empty → all zero
    auto out = sortVariantsByUsage(stored, counts);
    EXPECT(out == stored); // nothing jumps around
}

void testTiesKeepStoredOrder() {
    std::vector<std::string> stored{"\xc3\xa4", "\xc3\xa2", "\xc3\xa0"};
    // ä and à tie at 5, â is 0.
    std::unordered_map<std::string, long long> counts{{"\xc3\xa4", 5},
                                                      {"\xc3\xa0", 5}};
    auto out = sortVariantsByUsage(stored, counts);
    EXPECT(out[0] == "\xc3\xa4"); // ä before à — stored relative order kept
    EXPECT(out[1] == "\xc3\xa0"); // à
    EXPECT(out[2] == "\xc3\xa2"); // â, 0, last
}

} // namespace

int main() {
    testDescendingByCount();
    testAllZeroIsStoredOrder();
    testTiesKeepStoredOrder();
    std::printf("testusagesort: all passed\n");
    return 0;
}
