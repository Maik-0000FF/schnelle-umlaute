#ifndef SCHNELLE_UMLAUTE_CARET_THEME_IO_H
#define SCHNELLE_UMLAUTE_CARET_THEME_IO_H

// The on-disk side of the caret theme: generate an fcitx5 classicui theme
// matching the chosen palette, point classicui at it, and put the user's own
// theme back when the feature is switched off. The pure text transforms live
// next door in caret_theme.h; this header owns the files and the reload that
// makes fcitx5 pick them up, because writing the files without the reload
// leaves the caret window on the old colours and looks like nothing happened.
//
// Shared because two processes apply it: the editor when the user picks a
// theme, and the overlay daemon when the system switches between light and
// dark while the editor is closed. It used to sit in the editor's SettingsModel
// alone, which meant the caret silently stopped following on an automatic
// switch. Qt is fine here (both consumers are Qt processes); the fcitx5 engine
// does not include it.
//
// All compositor-agnostic: classicui is fcitx5's own renderer, and the only
// "stop following the desktop" switch is UseAccentColor=False, there is no
// Plasma-specific key.

#include "caret_theme.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace schnelle_umlaute {
namespace caret {

// The generated theme's name. It is the directory under the user's fcitx5
// themes, the value written into classicui.conf, and the marker that tells a
// later run that classicui is already pointing at this theme rather than the
// user's own. Three uses, one spelling.
inline constexpr const char *kThemeName = "schnelle-umlaute";

inline QString classicUiConfPath() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/fcitx5/conf/classicui.conf");
}

// User-dir fcitx5 theme; a unique name so it never shadows a system theme.
inline QString themeDir() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base + QStringLiteral("/fcitx5/themes/") + QLatin1String(kThemeName);
}

// Kept in the config dir, not the generated theme dir, so removing the
// throwaway theme never loses the record of the user's previous classicui
// theme (which restore() needs to put it back).
inline QString backupPath() {
    const auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const auto rel =
        QStringLiteral("/fcitx5/conf/schnelle-umlaute-classicui-backup.conf");
    return base + rel;
}

// Returns false if the file could not be written. Every caller sits behind the
// caret-theme toggle, whose whole effect is these files, so a failure has to
// travel back up rather than leave the toggle looking applied.
inline bool writeTextFileAtomic(const QString &path, const QString &content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    ts << content;
    ts.flush();
    return f.commit();
}

// File wrapper around the pure parser in caret_theme.h.
inline QMap<QString, QString> readFlatIni(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return parseFlatIni(QString::fromUtf8(f.readAll()));
}

// Read classicui.conf, patch the given keys (the pure line transform lives in
// caret_theme.h, preserving every other line), write it back atomically.
// Returns false if the write failed.
inline bool setClassicUiKeys(const QMap<QString, QString> &kv) {
    const QString path = classicUiConfPath();
    QStringList lines;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd())
            lines << in.readLine();
        f.close();
    }
    lines = applyIniKeys(lines, kv);
    const QString text = lines.join(QLatin1Char('\n')) +
                         (lines.isEmpty() ? QString() : QStringLiteral("\n"));
    return writeTextFileAtomic(path, text);
}

// Re-reads classicui.conf and re-parses the active theme, so freshly written
// files take effect without restarting fcitx5.
inline void reloadClassicUi() {
    auto msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.fcitx.Fcitx5"), QStringLiteral("/controller"),
        QStringLiteral("org.fcitx.Fcitx.Controller1"),
        QStringLiteral("ReloadAddonConfig"));
    msg << QStringLiteral("classicui");
    QDBusConnection::sessionBus().send(msg);
}

inline bool writeFiles(const QString &background, const QString &text,
                       const QString &highlight, const QString &highlightText,
                       const QString &border) {
    const QString themeConf = generateCaretThemeConf(
        background, text, highlight, highlightText, border);
    // Bail before touching classicui.conf: pointing it at a theme whose
    // theme.conf could not be written would leave fcitx5 on a broken theme.
    if (!writeTextFileAtomic(themeDir() + QStringLiteral("/theme.conf"),
                             themeConf))
        return false;

    // Back up the user's current classicui theme once (before it is
    // overridden), so turning the toggle off can put it back exactly.
    //
    // Skip it when classicui already points at the generated theme: that is not
    // the user's theme, it is this one from an earlier run. Recording it would
    // make the restore a no-op and lose the real previous theme for good. The
    // case is reachable since two processes write here, and either may find the
    // backup file missing while the toggle is on.
    const QMap<QString, QString> cur = readFlatIni(classicUiConfPath());
    const bool alreadyOurs =
        cur.value(QStringLiteral("Theme")) == QLatin1String(kThemeName);
    if (!QFile::exists(backupPath()) && !alreadyOurs) {
        QStringList backup;
        backup << QStringLiteral("Theme=") +
                      cur.value(QStringLiteral("Theme"),
                                QStringLiteral("default"));
        backup << QStringLiteral("UseDarkTheme=") +
                      cur.value(QStringLiteral("UseDarkTheme"),
                                QStringLiteral("False"));
        backup << QStringLiteral("UseAccentColor=") +
                      cur.value(QStringLiteral("UseAccentColor"),
                                QStringLiteral("True"));
        // A missing backup is not fatal: restore() falls back to the fcitx5
        // defaults, so the toggle still works, it just cannot put an exotic
        // previous theme back. Not worth aborting the apply over.
        writeTextFileAtomic(backupPath(), backup.join(QLatin1Char('\n')) +
                                              QStringLiteral("\n"));
    }
    // Point classicui at our theme and stop it from following the desktop
    // accent/dark scheme (UseAccentColor=False is the real override switch;
    // there is no Plasma-specific key).
    return setClassicUiKeys(
        {{QStringLiteral("Theme"), QString::fromLatin1(kThemeName)},
         {QStringLiteral("UseDarkTheme"), QStringLiteral("False")},
         {QStringLiteral("UseAccentColor"), QStringLiteral("False")}});
}

// Write the theme for these colours and make fcitx5 pick it up. False means
// nothing was applied and the caller should report it.
inline bool apply(const QString &background, const QString &text,
                  const QString &highlight, const QString &highlightText,
                  const QString &border) {
    if (!writeFiles(background, text, highlight, highlightText, border))
        return false;
    reloadClassicUi();
    return true;
}

// Put the user's own classicui theme back and make fcitx5 pick that up.
inline bool restore() {
    QMap<QString, QString> backup = readFlatIni(backupPath());
    if (backup.isEmpty()) {
        // No backup recorded: fall back to fcitx5 defaults.
        backup = {{QStringLiteral("Theme"), QStringLiteral("default")},
                  {QStringLiteral("UseDarkTheme"), QStringLiteral("False")},
                  {QStringLiteral("UseAccentColor"), QStringLiteral("True")}};
    }
    if (!setClassicUiKeys(backup))
        return false;
    // Only drop the backup once it has actually been put back, so a failed
    // restore can be retried instead of losing the record for good.
    QFile::remove(backupPath());
    reloadClassicUi();
    return true;
}

} // namespace caret
} // namespace schnelle_umlaute

#endif
