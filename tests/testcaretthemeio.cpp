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
using schnelle_umlaute::caret::brokenBackupPath;
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

// Raw content, to show that a file moved aside kept what was in it rather than
// just existing under the new name.
QString readFile(const QString &path) {
    QFile f(path);
    EXPECT(f.open(QIODevice::ReadOnly | QIODevice::Text));
    return QTextStream(&f).readAll();
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
    // deleting it would drop the only record of the user's theme. It is moved
    // aside with its content intact, and the restore then proceeds on the
    // defaults so the switch does not get stuck on a file nobody can fix from
    // inside the editor.
    {
        const QString content = QStringLiteral("# nothing but a comment\n");
        writeFile(classicUiConfPath(),
                  QStringLiteral("Theme=schnelle-umlaute\n"));
        writeFile(backupPath(), content);

        EXPECT(restore());
        EXPECT(themeKeyOf(classicUiConfPath()) == QStringLiteral("default"));
        EXPECT(!QFile::exists(backupPath()));
        EXPECT(readFile(brokenBackupPath()) == content);
    }

    // ── A second unreadable backup ───────────────────────────────────────
    // The file moved aside earlier must not block the next move, otherwise the
    // switch is stuck again from the second occurrence on.
    {
        const QString content = QStringLiteral("# the newer broken one\n");
        writeFile(classicUiConfPath(),
                  QStringLiteral("Theme=schnelle-umlaute\n"));
        writeFile(backupPath(), content);
        EXPECT(QFile::exists(brokenBackupPath())); // left by the case above

        EXPECT(restore());
        EXPECT(!QFile::exists(backupPath()));
        EXPECT(readFile(brokenBackupPath()) == content);
    }

    // ── The failed path is reported ──────────────────────────────────────
    // A restore that cannot write classicui.conf names that file, not the
    // backup, so the message points at what actually needs attention.
    {
        QFile::remove(backupPath());
        QFile::remove(classicUiConfPath());
        // Occupying the path with a directory is what makes the write fail;
        // the tests run as root in CI, where read-only bits do not.
        EXPECT(QDir().mkpath(classicUiConfPath()));

        QString failedPath;
        EXPECT(!restore(&failedPath));
        EXPECT(failedPath == classicUiConfPath());

        EXPECT(QDir().rmdir(classicUiConfPath()));
    }

    return 0;
}
