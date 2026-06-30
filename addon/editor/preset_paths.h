#ifndef SCHNELLE_UMLAUTE_EDITOR_PRESET_PATHS_H
#define SCHNELLE_UMLAUTE_EDITOR_PRESET_PATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

#include "profile_paths.h"

// Locates the read-only directory of bundled preset profiles. This must work
// across every install layout the project targets (Nix store, /usr, /usr/local,
// AUR, source build), so it tries several strategies rather than assuming XDG
// alone. The presets live at "<datadir>/schnelle-umlaute/presets" (installed
// next to the editor's other share files), so the binary-relative path resolves
// on any prefix-based install; XDG covers system installs; an env override is
// for running the un-installed editor against the repo; and a compiled-in
// absolute path is the last-resort fallback.

namespace schnelle_umlaute {

namespace detail {
inline QString withSlash(const QString &dir) {
    return dir.endsWith(QChar('/')) ? dir : dir + QChar('/');
}
} // namespace detail

// Returns the presets dir (with a trailing slash) of the first strategy that
// resolves to an existing directory, or an empty string if none is found.
inline QString presetsDir() {
    const QString sub =
        QString::fromLatin1(kConfigSubdir) + QStringLiteral("/presets");

    // 1. Explicit override (dev/test: point at the repo's presets dir).
    const QByteArray env = qgetenv("SCHNELLE_UMLAUTE_PRESETS_DIR");
    if (!env.isEmpty()) {
        const QString p = QString::fromLocal8Bit(env);
        if (QDir(p).exists())
            return detail::withSlash(p);
    }

    // 2. Relative to the editor binary: <prefix>/bin -> <prefix>/share/...
    //    Works for any prefix-based install, including the Nix store.
    const QString rel = QCoreApplication::applicationDirPath() +
                        QStringLiteral("/../share/") + sub;
    if (QDir(rel).exists()) {
        // canonicalFilePath() can still return empty even after exists() (a
        // permission change or broken symlink between the two calls). Guard it,
        // so a "/" never leaks through withSlash() and makes availablePresets()
        // scan the filesystem root; fall through to the next strategy instead.
        const QString canon = QFileInfo(rel).canonicalFilePath();
        if (!canon.isEmpty())
            return detail::withSlash(canon);
    }

    // 3. XDG data dirs (system installs under /usr/share, /usr/local/share).
    const QString xdg = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, sub, QStandardPaths::LocateDirectory);
    if (!xdg.isEmpty())
        return detail::withSlash(xdg);

    // 4. Compiled-in absolute install path (CMAKE_INSTALL_FULL_DATADIR/...).
#ifdef SCHNELLE_UMLAUTE_PRESETS_DIR
    {
        const QString baked = QStringLiteral(SCHNELLE_UMLAUTE_PRESETS_DIR);
        if (QDir(baked).exists())
            return detail::withSlash(baked);
    }
#endif

    return {};
}

} // namespace schnelle_umlaute

#endif
