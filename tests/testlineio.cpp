// Unit tests for readLine (line_io.h), the truncation-guarded line reader all
// three config parsers share.
//
// The rule under test: a line that does not fit the read buffer is dropped
// whole, never handed back in pieces. Getting that wrong turns one corrupt line
// into two bogus entries, so the boundary cases below (line ends exactly on the
// buffer edge, with and without a trailing newline) matter as much as the plain
// overlong case.
//
// Standalone — no Qt, no fcitx5 runtime, just the header and libc.

#include "line_io.h"

#include "test_expect.h"

#include <cstdio>
#include <string>
#include <vector>

using schnelle_umlaute::kLineBufferSize;
using schnelle_umlaute::readLine;

namespace {

// fgets reads at most kLineBufferSize - 1 bytes, so that is the longest line
// that still fits a single read. Every size below is derived from it rather
// than restated, so the cases stay meaningful if the buffer is resized.
constexpr size_t kMaxLine = kLineBufferSize - 1;

// Drain `content` through readLine by writing it to a tmpfile() and rewinding,
// which keeps the production FILE* signature exercised end-to-end.
std::vector<std::string> readAll(const std::string &content) {
    FILE *fp = std::tmpfile();
    if (!fp) {
        std::fprintf(stderr, "tmpfile() failed\n");
        std::abort();
    }
    if (!content.empty())
        std::fwrite(content.data(), 1, content.size(), fp);
    if (std::fseek(fp, 0, SEEK_SET) != 0) {
        std::fprintf(stderr, "fseek to start of tmpfile failed\n");
        std::abort();
    }
    std::vector<std::string> lines;
    std::string line;
    while (readLine(fp, line))
        lines.push_back(line);
    std::fclose(fp);
    return lines;
}

// -- Ordinary reading --------------------------------------------------------

void testEmptyStream() { EXPECT(readAll("").empty()); }

void testPlainLines() {
    auto r = readAll("eins\nzwei\ndrei\n");
    EXPECT(r.size() == 3);
    EXPECT(r[0] == "eins");
    EXPECT(r[1] == "zwei");
    EXPECT(r[2] == "drei");
}

// A final line with no trailing newline is still a line.
void testLastLineWithoutNewline() {
    auto r = readAll("eins\nzwei");
    EXPECT(r.size() == 2);
    EXPECT(r[1] == "zwei");
}

// CRLF files (a hand-edited config from another platform) must not leave a
// stray '\r' at the end of every field.
void testCarriageReturnStripped() {
    auto r = readAll("eins\r\nzwei\r\n");
    EXPECT(r.size() == 2);
    EXPECT(r[0] == "eins");
    EXPECT(r[1] == "zwei");
}

// Blank lines are handed back as empty strings; skipping them is each parser's
// own decision, not the reader's.
void testEmptyLinesPreserved() {
    auto r = readAll("eins\n\ndrei\n");
    EXPECT(r.size() == 3);
    EXPECT(r[0] == "eins");
    EXPECT(r[1].empty());
    EXPECT(r[2] == "drei");
}

// -- Truncation guard --------------------------------------------------------

// The core rule: an overlong line is dropped, and the lines around it survive
// intact. Without the guard the middle line would yield two entries here.
void testOverlongLineDropped() {
    const std::string big(kMaxLine + 100, 'x');
    auto r = readAll("eins\n" + big + "\ndrei\n");
    EXPECT(r.size() == 2);
    EXPECT(r[0] == "eins");
    EXPECT(r[1] == "drei");
}

// Back-to-back overlong lines must both be dropped without leaving the reader
// mid-line for the valid one that follows.
void testConsecutiveOverlongLinesDropped() {
    const std::string big1(kMaxLine * 2, 'x');
    const std::string big2(kMaxLine * 3, 'y');
    auto r = readAll(big1 + "\n" + big2 + "\ndrei\n");
    EXPECT(r.size() == 1);
    EXPECT(r[0] == "drei");
}

// An overlong line that runs straight into EOF (no trailing newline) is dropped
// too, and ends the stream.
void testOverlongLineAtEofDropped() {
    const std::string big(kMaxLine + 100, 'x');
    auto r = readAll("eins\n" + big);
    EXPECT(r.size() == 1);
    EXPECT(r[0] == "eins");
}

// -- Buffer boundary ---------------------------------------------------------

// content + '\n' == kMaxLine bytes read: the buffer is full but the back
// character is '\n', so the line is plainly complete.
void testLineEndsWithNewlineAtBoundary() {
    const size_t bigLen = kMaxLine - 1; // + '\n' == kMaxLine
    const std::string big(bigLen, 'x');
    auto r = readAll(big + "\nzwei\n");
    EXPECT(r.size() == 2);
    EXPECT(r[0].size() == bigLen);
    EXPECT(r[1] == "zwei");
}

// kMaxLine content bytes and the newline immediately after: the buffer fills
// with no '\n' in it, so the guard triggers, but the very next byte is the
// newline — the line is complete and must be kept, not dropped.
void testLineFillsBufferThenNewline() {
    const std::string big(kMaxLine, 'x');
    auto r = readAll(big + "\nzwei\n");
    EXPECT(r.size() == 2);
    EXPECT(r[0].size() == kMaxLine);
    EXPECT(r[1] == "zwei");
}

// kMaxLine content bytes followed by EOF: same ambiguity, resolved the other
// way. The line is complete and must be kept.
void testLineFillsBufferThenEof() {
    const std::string big(kMaxLine, 'x');
    auto r = readAll(big);
    EXPECT(r.size() == 1);
    EXPECT(r[0].size() == kMaxLine);
}

} // namespace

int main() {
    testEmptyStream();
    testPlainLines();
    testLastLineWithoutNewline();
    testCarriageReturnStripped();
    testEmptyLinesPreserved();
    testOverlongLineDropped();
    testConsecutiveOverlongLinesDropped();
    testOverlongLineAtEofDropped();
    testLineEndsWithNewlineAtBoundary();
    testLineFillsBufferThenNewline();
    testLineFillsBufferThenEof();
    std::printf("testlineio: all tests passed\n");
    return 0;
}
