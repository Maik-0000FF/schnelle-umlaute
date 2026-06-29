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

std::vector<std::string> splitOutputs(const std::string &output) {
    std::vector<std::string> outputs;
    if (output.empty())
        return outputs;

    std::string current;
    for (size_t i = 0; i < output.length(); ++i) {
        if (output[i] == ',') {
            if (i + 1 < output.length() && output[i + 1] == ',') {
                current += ',';
                ++i;
            } else {
                if (!current.empty()) {
                    outputs.push_back(std::move(current));
                    current.clear();
                }
            }
        } else {
            current += output[i];
        }
    }
    // Trailing comma produces an empty segment which is intentionally
    // skipped — an empty cycling variant would be useless.
    if (!current.empty()) {
        outputs.push_back(std::move(current));
    }
    return outputs;
}

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
    if (map.empty()) {
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
