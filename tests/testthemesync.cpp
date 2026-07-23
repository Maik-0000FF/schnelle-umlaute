// Guards against theme-name drift across the C++/QML boundary. The valid theme
// names live in two files that cannot share code (different languages):
// addon/themes.h (isValidTheme, the editor setTheme and overlay SetTheme
// guards) and addon/palette/Palettes.qml (the palettes `all`, the picker order
// `ids`, and the display names `labels`). If a name exists in one but not the
// other, a theme silently falls back to the default or is rejected, with no
// build error. This test reads both sources as text and asserts all four name
// sets are identical, so adding a theme to only one place fails CI.
//
// Pure text parsing (std only) so it needs no Qt/QML runtime; the two source
// paths are injected as compile definitions from CMake.

#include "test_expect.h"

#include <cstdio>
#include <exception>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace {

std::string readFile(const char *path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::abort();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The substring of `s` after `startMarker` up to the next `endChar`.
std::string between(const std::string &s, const std::string &startMarker,
                    char endChar) {
    const auto p = s.find(startMarker);
    EXPECT(p != std::string::npos);
    const auto start = p + startMarker.size();
    const auto e = s.find(endChar, start);
    EXPECT(e != std::string::npos);
    return s.substr(start, e - start);
}

// Every capture-group-1 match of `pattern` in `s`, as a set.
std::set<std::string> matches(const std::string &s,
                              const std::string &pattern) {
    const std::regex re(pattern);
    std::set<std::string> out;
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re);
         it != std::sregex_iterator(); ++it)
        out.insert((*it)[1].str());
    return out;
}

void run() {
    const std::string themesH = readFile(THEMES_H_PATH);
    const std::string palettes = readFile(PALETTES_QML_PATH);

    // themes.h: every QStringLiteral("id") in the isValidTheme list.
    const auto validator =
        matches(themesH, R"RE(QStringLiteral\("([a-z-]+)"\))RE");
    // Palettes `all`: each theme block opens with a quoted key + brace,
    // `"id": {` (inner keys like `overlay:` are unquoted, so are not matched).
    const auto all = matches(palettes, R"RE("([a-z-]+)":\s*\{)RE");
    // Palettes `ids`: the quoted entries inside the ids array.
    const auto ids = matches(between(palettes, "property var ids: [", ']'),
                             R"RE("([a-z-]+)")RE");
    // Palettes `labels`: the quoted keys inside the labels map (values carry
    // spaces/capitals, so `"[a-z-]+":` matches only the id keys).
    const auto labels =
        matches(between(palettes, "property var labels: ({", '}'),
                R"RE("([a-z-]+)":)RE");

    // Sanity: the parse found something (a broken regex would pass vacuously).
    EXPECT(validator.size() >= 4);

    EXPECT(validator == all);
    EXPECT(validator == ids);
    EXPECT(validator == labels);

    std::fprintf(stderr, "ok testthemesync (%zu themes in sync)\n",
                 validator.size());
}

} // namespace

int main() {
    // std::regex construction can throw regex_error; keep it from escaping
    // main.
    try {
        run();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL testthemesync: %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "FAIL testthemesync: unknown exception\n");
        return 1;
    }
    return 0;
}
