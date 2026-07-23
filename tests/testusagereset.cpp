// Engine-side test for the usage-reset sidecar helpers (mappings_loader):
// takeUsageResetMarker consumes the one-shot marker (present -> true + removed,
// absent -> false, idempotent), and deleteUsage removes usage.conf (no-op when
// absent). This guards the core of the reset handshake, which is otherwise only
// exercised by a live test.
//
// Links mappings_loader.cpp + Fcitx5::Core (fcitx StandardPaths). Redirects
// XDG_CONFIG_HOME (and clears XDG_CONFIG_DIRS) so the helpers only touch the
// scratch dir, the same harness as testprofilesloader.

#include "mappings_loader.h"
#include "profile_paths.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using schnelle_umlaute::deleteUsage;
using schnelle_umlaute::kConfigSubdir;
using schnelle_umlaute::kUsageFile;
using schnelle_umlaute::kUsageResetMarker;
using schnelle_umlaute::takeUsageResetMarker;
using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

void touch(const std::string &abs) {
    std::filesystem::create_directories(
        std::filesystem::path(abs).parent_path());
    std::FILE *fp = std::fopen(abs.c_str(), "w");
    if (!fp) {
        std::fprintf(stderr, "cannot write %s\n", abs.c_str());
        std::abort();
    }
    std::fputs("x", fp);
    std::fclose(fp);
}

bool exists(const std::string &abs) { return std::filesystem::exists(abs); }

} // namespace

int main() {
    TempXdgConfigHome tempdir("testusagereset");
    setenv("XDG_CONFIG_DIRS", "", 1); // ignore system config dirs
    const std::string sub = tempdir.path() + "/fcitx5/" + kConfigSubdir;
    const std::string marker = sub + "/" + kUsageResetMarker;
    const std::string usage = sub + "/" + kUsageFile;

    // No marker: nothing to consume.
    EXPECT(!takeUsageResetMarker());

    // Marker present: consumed exactly once (returns true and removes the
    // file), then absent again.
    touch(marker);
    EXPECT(exists(marker));
    EXPECT(takeUsageResetMarker());
    EXPECT(!exists(marker));
    EXPECT(!takeUsageResetMarker());

    // deleteUsage removes usage.conf, and is a no-op (no crash) when absent.
    touch(usage);
    EXPECT(exists(usage));
    deleteUsage();
    EXPECT(!exists(usage));
    deleteUsage();
    EXPECT(!exists(usage));

    std::fprintf(stderr, "ok testusagereset\n");
    return 0;
}
