#ifndef SCHNELLE_UMLAUTE_APP_FILTER_H
#define SCHNELLE_UMLAUTE_APP_FILTER_H

// Per-application enable/disable filter. Matches the focused IC's program
// name against a configured blacklist or whitelist; returns whether the
// addon should skip processing for that app.

#include "config.h"

#include <string>
#include <vector>

namespace fcitx {

class InputContext;

class AppFilter {
public:
    // Replace the active configuration. Values are moved in; no copies
    // are retained by the caller. Safe to call at any time; subsequent
    // isFiltered() calls see the new state.
    void configure(AppFilterMode mode, std::vector<std::string> blacklist,
                   std::vector<std::string> whitelist);

    // Whether processing should be skipped for this IC's program.
    // Disabled mode always returns false. Empty program name is treated
    // as "unknown" — blacklisted nothing (returns false) or not
    // whitelisted (returns true) depending on mode.
    bool isFiltered(InputContext *ic) const;

private:
    AppFilterMode mode_ = AppFilterMode::Disabled;
    std::vector<std::string> blacklist_;
    std::vector<std::string> whitelist_;
};

} // namespace fcitx

#endif
