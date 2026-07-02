#ifndef SCHNELLE_UMLAUTE_EDITOR_PATHS_H
#define SCHNELLE_UMLAUTE_EDITOR_PATHS_H

#include <QLatin1String>
#include <QStandardPaths>
#include <QString>

#include "profile_paths.h"

// One place for the editor's absolute config dir, so MappingListModel and
// ProfileListModel don't each spell out the "/fcitx5/<subdir>/" assembly.
// The meaning-bearing subdir name itself lives in profile_paths.h (shared with
// the engine); this only adds the Qt-side base path.
namespace schnelle_umlaute {

inline QString configDirPath() {
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/fcitx5/") + QLatin1String(kConfigSubdir) +
           QStringLiteral("/");
}

} // namespace schnelle_umlaute

#endif
