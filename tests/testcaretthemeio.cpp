// Unit tests for the on-disk side of the caret theme (addon/caret_theme_io.h),
// restricted to restore(): the branch that decides what happens to the user's
// own classicui theme when the feature is switched off. The files it touches
// all hang off XDG_CONFIG_HOME, so pointing that at a temporary directory
// keeps the test off the real fcitx5 config. The DBus reload it fires at the
// end is a no-op without a session bus, which is exactly the case here.

#include "caret_theme_io.h"

#include "test_expect.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>

using schnelle_umlaute::caret::backupPath;
using schnelle_umlaute::caret::classicUiConfPath;
using schnelle_umlaute::caret::readFlatIni;
using schnelle_umlaute::caret::restore;

namespace {

// The two files restore() reads and writes. Written straight, not through the
// header's own writer, so a test failure points at restore() and not at the
// setup.
void writeFile(const QString &path, const QString &content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    EXPECT(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream ts(&f);
    ts << content;
}

QString themeKeyOf(const QString &path) {
    return readFlatIni(path).value(QStringLiteral("Theme"));
}

} // namespace

int main(int argc, char **argv) {
    // QDBusConnection wants an application instance to attach to; without one
    // the reload at the end of restore() warns instead of quietly doing
    // nothing.
    QCoreApplication app(argc, argv);

    QTemporaryDir configHome;
    EXPECT(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());

    // ── No backup recorded ───────────────────────────────────────────────
    // Nothing to put back, so the fcitx5 defaults are the best guess and the
    // restore counts as done.
    {
        writeFile(classicUiConfPath(),
                  QStringLiteral("Theme=schnelle-umlaute\n"));
        EXPECT(!QFile::exists(backupPath()));

        EXPECT(restore());
        EXPECT(themeKeyOf(classicUiConfPath()) == QStringLiteral("default"));
    }

    // ── Backup present and readable ──────────────────────────────────────
    // The recorded theme goes back and the record is dropped, so a second
    // switch-on can record the theme that is current then.
    {
        writeFile(classicUiConfPath(),
                  QStringLiteral("Theme=schnelle-umlaute\n"));
        writeFile(backupPath(), QStringLiteral("Theme=mine\n"
                                               "UseDarkTheme=True\n"
                                               "UseAccentColor=False\n"));

        EXPECT(restore());
        EXPECT(themeKeyOf(classicUiConfPath()) == QStringLiteral("mine"));
        EXPECT(!QFile::exists(backupPath()));
    }

    // ── Backup present but yields nothing ────────────────────────────────
    // An empty or otherwise unreadable backup is not the same as no backup:
    // taking the defaults here would overwrite the user's theme and then
    // delete the only record of it. The restore has to fail, leave
    // classicui.conf alone, and keep the file for a later attempt.
    {
        writeFile(classicUiConfPath(),
                  QStringLiteral("Theme=schnelle-umlaute\n"));
        writeFile(backupPath(), QStringLiteral("# nothing but a comment\n"));

        EXPECT(!restore());
        EXPECT(themeKeyOf(classicUiConfPath()) ==
               QStringLiteral("schnelle-umlaute"));
        EXPECT(QFile::exists(backupPath()));
    }

    return 0;
}
