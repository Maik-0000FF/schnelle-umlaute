#include "mappings_loader.h"

#include "mappings-io.h"
#include "profile_paths.h"

#include <fcitx-utils/log.h>
#if __has_include(<fcitx-utils/standardpaths.h>)
#include <fcitx-utils/standardpaths.h>
#define SU_HAS_NEW_STDPATHS 1
#else
#include <fcntl.h>
#include <fcitx-utils/standardpath.h>
#define SU_HAS_NEW_STDPATHS 0
#endif
#include <fcitx-utils/fs.h>

#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <utility>

namespace schnelle_umlaute {

namespace {

// Open a config-dir file (relative to the addon's PkgConfig dir) and run
// `parse` on it, returning a default-constructed result when the file is
// absent. Keeps the two StandardPaths API variants in one place, shared by the
// mappings, manifest, and usage loaders.
template <typename Parse>
auto openAndParse(const std::string &relPath, Parse parse)
    -> decltype(parse(std::declval<FILE *>())) {
    using namespace fcitx;
    using Result = decltype(parse(std::declval<FILE *>()));
#if SU_HAS_NEW_STDPATHS
    auto file =
        StandardPaths::global().open(StandardPathsType::PkgConfig, relPath);
    if (file.isValid()) {
        auto fp = fs::openFD(file, "r");
#else
    auto file = StandardPath::global().open(StandardPath::Type::PkgConfig,
                                            relPath, O_RDONLY);
    if (file.fd() >= 0) {
        auto fp = fs::openFD(file, "r");
#endif
        if (fp)
            return parse(fp.get());
    }
    return Result{};
}

} // namespace

UmlautMap loadMappingsFromFile(const std::string &relPath) {
    using namespace fcitx;
    UmlautMap map;
#if SU_HAS_NEW_STDPATHS
    auto file =
        StandardPaths::global().open(StandardPathsType::PkgConfig, relPath);
    if (file.isValid()) {
        auto fp = fs::openFD(file, "r");
#else
    auto file = StandardPath::global().open(StandardPath::Type::PkgConfig,
                                            relPath, O_RDONLY);
    if (file.fd() >= 0) {
        auto fp = fs::openFD(file, "r");
#endif
        if (fp) {
            for (const auto &m : parseMappings(fp.get())) {
                auto outputs = splitOutputs(m.output);
                if (outputs.empty()) {
                    FCITX_WARN() << "Schnelle: Mapping '" << m.input
                                 << "' has no valid outputs"
                                 << " — skipped";
                    continue;
                }
                map[m.input] = std::move(outputs);
            }
        }
    }
    // The built-in German defaults are a first-install convenience for the
    // Standard profile only. Other profiles stay genuinely empty when their
    // file is missing/empty, so a freshly created profile starts blank instead
    // of inheriting the umlaut set.
    if (map.empty() && isStandardProfile(relPath)) {
        for (const auto &m : defaultMappings()) {
            map[m.input] = splitOutputs(m.output);
        }
    }
    return map;
}

UmlautMap loadMappingsFromFile() {
    return loadMappingsFromFile(std::string(kConfigSubdir) + "/" +
                               kMappingsFile);
}

MergeManifest loadMergeManifest() {
    return openAndParse(std::string(kConfigSubdir) + "/" + kMergeConf,
                        parseMergeManifest);
}

UsageCounts loadUsage() {
    return openAndParse(std::string(kConfigSubdir) + "/" + kUsageFile,
                        parseUsage);
}

bool saveUsage(const UsageCounts &counts) {
    using namespace fcitx;
    const std::string data = serializeUsage(counts);
    const std::string relPath = std::string(kConfigSubdir) + "/" + kUsageFile;
    // Write the whole serialized table to the temp fd StandardPaths hands us;
    // it atomically renames into place on success (temp file + rename), so a
    // concurrent editor read never sees a half-written file.
    auto writeAll = [&data](int fd) -> bool {
        size_t off = 0;
        while (off < data.size()) {
            const ssize_t n = ::write(fd, data.data() + off, data.size() - off);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                return false;
            }
            off += static_cast<size_t>(n);
        }
        return true;
    };
#if SU_HAS_NEW_STDPATHS
    return StandardPaths::global().safeSave(StandardPathsType::PkgConfig,
                                            relPath, writeAll);
#else
    return StandardPath::global().safeSave(StandardPath::Type::PkgConfig,
                                           relPath, writeAll);
#endif
}

} // namespace schnelle_umlaute
