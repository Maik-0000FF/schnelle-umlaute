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

#include <cstdio>
#include <utility>

namespace schnelle_umlaute {

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

} // namespace schnelle_umlaute
