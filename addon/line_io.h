#ifndef SCHNELLE_UMLAUTE_LINE_IO_H
#define SCHNELLE_UMLAUTE_LINE_IO_H

// Line reader shared by every config-file parser in this project
// (mappings-io.h, usage_io.h, merge_manifest_io.h).
//
// All three read a plain line-oriented text file with fgets, and all three need
// the same non-trivial guard against a line longer than the read buffer. Doing
// that guard once here keeps the rule in a single place instead of restating it
// per parser, where the copies would inevitably drift.

#include <cstddef>
#include <cstdio>
#include <string>

namespace schnelle_umlaute {

// Read buffer for one line of a config file. Sized well above any realistic
// mapping, usage counter or manifest line; anything longer is treated as
// corrupt input (see readLine).
inline constexpr std::size_t kLineBufferSize = 4096;

// Read one complete logical line from `fp` into `line`, stripped of its
// trailing newline / carriage return. Returns false once the stream has no
// further usable line.
//
// A line longer than kLineBufferSize - 1 bytes is dropped whole rather than
// handed back in pieces: fgets would otherwise return a prefix that parses as a
// truncated entry and a tail that parses as a bogus extra entry, so one
// overlong line would corrupt two. Dropping it silently keeps the damage to the
// broken line itself and the reader continues with the next one.
//
// `line` is left in an unspecified state when the function returns false.
inline bool readLine(FILE *fp, std::string &line) {
    char buf[kLineBufferSize];
    while (std::fgets(buf, sizeof(buf), fp)) {
        line.assign(buf);
        // fgets filled the whole buffer AND did not reach a newline →
        // candidate for truncation. Still ambiguous: the line could end exactly
        // at the buffer boundary (the next char is '\n' or EOF), in which case
        // it is actually complete.
        const bool mightBeTruncated =
            (line.size() == sizeof(buf) - 1) && line.back() != '\n';
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (!mightBeTruncated)
            return true;
        int c = std::fgetc(fp);
        // The line just happened to end on the buffer boundary (or the stream
        // ended there): it is complete after all. An EOF here also sets the
        // stream's EOF indicator, so the next call's fgets returns null and the
        // loop terminates without re-reading anything.
        if (c == EOF || c == '\n')
            return true;
        // Truly truncated — drain the rest of the physical line and drop it.
        while ((c = std::fgetc(fp)) != EOF && c != '\n') {
        }
        if (c == EOF)
            return false;
    }
    return false;
}

} // namespace schnelle_umlaute

#endif
