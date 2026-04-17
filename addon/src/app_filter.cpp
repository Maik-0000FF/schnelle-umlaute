#include "app_filter.h"

#include <fcitx/inputcontext.h>

#include <string>
#include <utility>

namespace fcitx {

void AppFilter::configure(AppFilterMode mode,
                          std::vector<std::string> blacklist,
                          std::vector<std::string> whitelist) {
    mode_ = mode;
    blacklist_ = std::move(blacklist);
    whitelist_ = std::move(whitelist);
}

bool AppFilter::isFiltered(InputContext *ic) const {
    if (mode_ == AppFilterMode::Disabled) return false;

    const std::string &program = ic->program();
    if (program.empty())
        return mode_ == AppFilterMode::Whitelist;

    // Empty list entries are skipped: find("") returns 0 (matches
    // anything), which would make a stray blank line in the blacklist
    // disable the addon entirely, or a blank line in the whitelist
    // bypass the filter entirely.
    if (mode_ == AppFilterMode::Blacklist) {
        for (const auto &app : blacklist_) {
            if (app.empty()) continue;
            if (program.find(app) != std::string::npos) return true;
        }
        return false;
    }
    // Whitelist: only active in listed apps
    for (const auto &app : whitelist_) {
        if (app.empty()) continue;
        if (program.find(app) != std::string::npos) return false;
    }
    return true;
}

} // namespace fcitx
