// RAII helper for tests that need a scratch XDG_CONFIG_HOME.
//
// Every editor-side test (MappingListModel, SettingsModel) triggers a save()
// on construction or on the first setter, which lands under
// $XDG_CONFIG_HOME/fcitx5. Pointing that at a mkdtemp() dir keeps the tests
// hermetic — no ~/.config writes, no cross-test bleed, and the destructor
// cleans up so /tmp doesn't accumulate leftovers across runs.
//
// setenv/unsetenv is process-wide, so reusing a single TempXdgConfigHome per
// test run (and reset() between cases) is the intended pattern. Reset is
// strictly "wipe dir contents" — XDG_CONFIG_HOME stays pointed at the same
// path, so Qt's QStandardPaths cache (which samples the env once) keeps
// returning the right directory.

#ifndef SCHNELLE_UMLAUTE_TESTS_TEST_TEMPDIR_H
#define SCHNELLE_UMLAUTE_TESTS_TEST_TEMPDIR_H

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace schnelle_umlaute_tests {

class TempXdgConfigHome {
public:
    explicit TempXdgConfigHome(const char *prefix) {
        std::string tmpl = std::string("/tmp/") + prefix + ".XXXXXX";
        std::string buf(tmpl);
        char *dir = mkdtemp(buf.data());
        if (!dir) {
            std::fprintf(stderr, "mkdtemp failed for %s\n", prefix);
            std::abort();
        }
        path_ = dir;
        setenv("XDG_CONFIG_HOME", path_.c_str(), 1);
    }

    ~TempXdgConfigHome() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempXdgConfigHome(const TempXdgConfigHome &) = delete;
    TempXdgConfigHome &operator=(const TempXdgConfigHome &) = delete;

    const std::string &path() const { return path_; }

    // Wipe every file in the tempdir but keep the path and env var intact —
    // cheaper than tearing the whole thing down and lets successive tests
    // start from a blank state without re-calling setenv().
    void reset() {
        std::error_code ec;
        for (auto &entry : std::filesystem::directory_iterator(path_, ec)) {
            std::filesystem::remove_all(entry.path(), ec);
        }
    }

private:
    std::string path_;
};

} // namespace schnelle_umlaute_tests

#endif
