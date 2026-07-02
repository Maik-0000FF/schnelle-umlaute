// Engine-side loader test for the per-profile defaults rule: the built-in
// German umlaut defaults are seeded ONLY for the Standard profile
// (mappings.txt). A freshly created, non-Standard profile must load empty in
// the actual IME, not inherit the umlaut set. The editor-model side of the
// same rule is covered by testmappinglistmodelio; this guards the engine path
// (loadMappingsFromFile + isStandardProfile) so a future refactor can't
// silently re-seed defaults into every profile.
//
// Links mappings_loader.cpp and Fcitx5::Core (fcitx StandardPaths). Redirects
// XDG_CONFIG_HOME (and clears XDG_CONFIG_DIRS) so the loader only sees the
// scratch dir, never a system config.

#include "mappings_loader.h"
#include "profile_paths.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using schnelle_umlaute::kConfigSubdir;
using schnelle_umlaute::kMappingsFile;
using schnelle_umlaute::kProfilesSubdir;
using schnelle_umlaute::loadMappingsFromFile;
using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

// Config-dir-relative path the loader expects (e.g. "schnelle-umlaute/x").
std::string rel(const std::string &file) {
    return std::string(kConfigSubdir) + "/" + file;
}

void writeFile(const std::string &abs, const std::string &content) {
    std::filesystem::create_directories(
        std::filesystem::path(abs).parent_path());
    std::FILE *fp = std::fopen(abs.c_str(), "w");
    if (!fp) {
        std::fprintf(stderr, "cannot write %s\n", abs.c_str());
        std::abort();
    }
    std::fwrite(content.data(), 1, content.size(), fp);
    std::fclose(fp);
}

} // namespace

int main() {
    TempXdgConfigHome tempdir("testprofilesloader");
    setenv("XDG_CONFIG_DIRS", "", 1); // ignore system config dirs
    const std::string sub = tempdir.path() + "/fcitx5/" + kConfigSubdir;

    // Standard profile, file missing -> German defaults seeded.
    {
        auto m = loadMappingsFromFile(rel(kMappingsFile));
        EXPECT(!m.empty());
    }

    // Non-Standard profile, file missing -> stays empty (no defaults).
    {
        auto m =
            loadMappingsFromFile(rel(std::string(kProfilesSubdir) + "/leer.txt"));
        EXPECT(m.empty());
    }

    // Non-Standard profile with content -> exactly that content, no defaults.
    {
        writeFile(sub + "/" + kProfilesSubdir + "/math.txt", "x=ø\n");
        auto m =
            loadMappingsFromFile(rel(std::string(kProfilesSubdir) + "/math.txt"));
        EXPECT(m.size() == 1);
        EXPECT(m.count("x") == 1);
    }

    // Standard profile with content -> that content (its own file is read).
    {
        writeFile(sub + "/" + kMappingsFile, "a=ä\n");
        auto m = loadMappingsFromFile(rel(kMappingsFile));
        EXPECT(m.size() == 1);
        EXPECT(m.count("a") == 1);
    }

    std::fprintf(stderr, "ok testprofilesloader\n");
    return 0;
}
