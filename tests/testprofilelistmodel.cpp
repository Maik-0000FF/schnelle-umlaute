// Unit tests for ProfileListModel: CRUD, name + slug uniqueness, Standard /
// last-profile protection, active reassignment on delete, first-run seeding,
// and the editor's own profiles.conf write/read round-trip.
//
// Links ProfileListModel.cpp directly (AUTOMOC handles Q_OBJECT). Redirects
// XDG_CONFIG_HOME so the constructor's load()/seed and every save() land in a
// scratch dir instead of ~/.config. Mutators call reloadSchnelleUmlauteAddon()
// over DBus, which is a no-op without a session bus, keeping the test hermetic.

#include "ProfileListModel.h"
#include "editor_paths.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QModelIndex>
#include <QString>
#include <QStringList>

using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

QString rowName(const ProfileListModel &m, int row) {
    return m.profileNames().value(row);
}

// Where ProfileListModel reads/writes profiles.conf, built from the same
// shared constants the model uses.
QString profilesConfPathForTest() {
    return schnelle_umlaute::configDirPath() +
           QLatin1String(schnelle_umlaute::kProfilesConf);
}

QString rowSelectKey(ProfileListModel &m, int row) {
    return m.data(m.index(row), ProfileListModel::SelectKeyRole).toString();
}

} // namespace

// -- seeding -----------------------------------------------------------------

// A fresh config dir yields exactly one profile: the protected Standard,
// pointing at the legacy mappings.txt, active.
void testSeedsStandard(ProfileListModel &m) {
    EXPECT(m.rowCount() == 1);
    EXPECT(rowName(m, 0) == QStringLiteral("Standard"));
    EXPECT(m.fileForRow(0) == QStringLiteral("mappings.txt"));
    EXPECT(m.active() == QStringLiteral("Standard"));
    EXPECT(m.isProtected(0)); // Standard and the last profile are protected
}

// -- create ------------------------------------------------------------------

void testCreateAddsProfileWithSlugFile(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.rowCount() == 2);
    EXPECT(rowName(m, 1) == QStringLiteral("Mathematik"));
    EXPECT(m.fileForRow(1) == QStringLiteral("profiles/mathematik.txt"));
    EXPECT(!m.isProtected(1)); // not Standard, not the last one
    EXPECT(m.isProtected(0));  // Standard stays protected
}

// -- name uniqueness ---------------------------------------------------------

void testRejectsDuplicateName(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(!m.createProfile(QStringLiteral("Mathematik")));
    // Case-insensitive + trimmed: these are the same name.
    EXPECT(!m.createProfile(QStringLiteral("mathematik")));
    EXPECT(!m.createProfile(QStringLiteral("  MATHEMATIK  ")));
    EXPECT(m.rowCount() == 2);
    EXPECT(!m.nameErrorFor(QStringLiteral("Mathematik"), -1).isEmpty());
    EXPECT(!m.nameErrorFor(QStringLiteral(" mathematik "), -1).isEmpty());
}

// Two distinct names whose slugs collide must still get distinct files.
void testDistinctNamesSlugCollisionGetsUniqueFile(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.fileForRow(1) == QStringLiteral("profiles/mathematik.txt"));
    // "Mathematik!" is a different display name, but slugifies to "mathematik".
    EXPECT(m.createProfile(QStringLiteral("Mathematik!")));
    EXPECT(m.rowCount() == 3);
    EXPECT(m.fileForRow(2) == QStringLiteral("profiles/mathematik-2.txt"));
}

// -- invalid names -----------------------------------------------------------

void testRejectsEmptyAndInvalidNames(ProfileListModel &m) {
    EXPECT(!m.createProfile(QString()));
    EXPECT(!m.createProfile(QStringLiteral("   ")));
    EXPECT(!m.createProfile(QStringLiteral("a/b")));
    EXPECT(!m.createProfile(QStringLiteral("a\\b")));
    EXPECT(m.rowCount() == 1); // nothing added
    EXPECT(!m.nameErrorFor(QString(), -1).isEmpty());
    EXPECT(!m.nameErrorFor(QStringLiteral("a/b"), -1).isEmpty());
}

// -- rename ------------------------------------------------------------------

void testRename(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.renameProfile(1, QStringLiteral("Physik")));
    EXPECT(rowName(m, 1) == QStringLiteral("Physik"));
    // Renaming to an existing name fails.
    EXPECT(!m.renameProfile(1, QStringLiteral("Standard")));
    // Keeping the same name (excludeRow = own row) is allowed.
    EXPECT(m.renameProfile(1, QStringLiteral("Physik")));
}

// Renaming the active profile keeps the active pointer in sync.
void testRenameActiveUpdatesActive(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.setActiveRow(1));
    EXPECT(m.active() == QStringLiteral("Mathematik"));
    EXPECT(m.renameProfile(1, QStringLiteral("Physik")));
    EXPECT(m.active() == QStringLiteral("Physik"));
}

// -- delete + protection -----------------------------------------------------

void testStandardAndLastAreProtected(ProfileListModel &m) {
    // Last remaining (also Standard) cannot be deleted.
    EXPECT(m.isProtected(0));
    EXPECT(!m.removeProfile(0));
    EXPECT(m.rowCount() == 1);
    // Even with a second profile, Standard stays protected.
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.isProtected(0));
    EXPECT(!m.removeProfile(0));
    EXPECT(m.rowCount() == 2);
}

void testRemoveNonStandard(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.removeProfile(1));
    EXPECT(m.rowCount() == 1);
    EXPECT(rowName(m, 0) == QStringLiteral("Standard"));
}

// Deleting the active profile falls back to the first remaining one.
void testRemoveActiveReassigns(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.setActiveRow(1));
    EXPECT(m.active() == QStringLiteral("Mathematik"));
    EXPECT(m.removeProfile(1));
    EXPECT(m.active() == QStringLiteral("Standard"));
}

// -- set active + select key -------------------------------------------------

void testSetActive(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.setActiveRow(1));
    EXPECT(m.active() == QStringLiteral("Mathematik"));
    EXPECT(m.activeRow() == 1);
}

void testSetSelectKey(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.setSelectKey(1, QStringLiteral("Control+Alt+1")));
    EXPECT(rowSelectKey(m, 1) == QStringLiteral("Control+Alt+1"));
}

// -- persistence round-trip (editor write → editor read) ---------------------

void testPersistenceRoundTrip(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.setActiveRow(1));
    EXPECT(m.setSelectKey(1, QStringLiteral("Control+Alt+1")));
    m.setCycleNext(QStringLiteral("Control+Alt+Period"));
    m.setCyclePrev(QStringLiteral("Control+Alt+Comma"));

    // A second model over the same config dir must observe the persisted state.
    ProfileListModel m2;
    EXPECT(m2.rowCount() == 2);
    EXPECT(rowName(m2, 0) == QStringLiteral("Standard"));
    EXPECT(rowName(m2, 1) == QStringLiteral("Mathematik"));
    EXPECT(m2.fileForRow(1) == QStringLiteral("profiles/mathematik.txt"));
    EXPECT(m2.active() == QStringLiteral("Mathematik"));
    EXPECT(rowSelectKey(m2, 1) == QStringLiteral("Control+Alt+1"));
    EXPECT(m2.cycleNext() == QStringLiteral("Control+Alt+Period"));
    EXPECT(m2.cyclePrev() == QStringLiteral("Control+Alt+Comma"));
}

// A profile name with a space is quote-escaped on write and unescaped on read;
// it must survive the editor's own round-trip unchanged.
void testSpacedNameRoundTrip(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mein Profil")));
    EXPECT(m.fileForRow(1) == QStringLiteral("profiles/mein-profil.txt"));
    ProfileListModel m2;
    EXPECT(m2.rowCount() == 2);
    EXPECT(rowName(m2, 1) == QStringLiteral("Mein Profil"));
}

// A profile File pointing outside profiles/ (path traversal from a hand-edited
// profiles.conf) is dropped on load.
void testRejectsUnsafeFileOnLoad(ProfileListModel &m) {
    Q_UNUSED(m);
    QFile f(profilesConfPathForTest());
    QDir().mkpath(QFileInfo(f.fileName()).absolutePath());
    EXPECT(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("Active=Standard\n\n[Profiles/0]\nName=Standard\nFile=mappings.txt\n"
            "\n[Profiles/1]\nName=Evil\nFile=../mappings.txt\n");
    f.close();
    ProfileListModel m2;
    // The unsafe "../mappings.txt" entry is dropped; only Standard survives.
    EXPECT(m2.rowCount() == 1);
    EXPECT(rowName(m2, 0) == QStringLiteral("Standard"));
}

// A profile switched at runtime by the engine (which writes Active to
// profiles.conf) must not be reverted when the editor later persists an
// unrelated change. The editor re-reads Active from disk before each mutating
// save, so a stale in-memory active_ never clobbers the runtime switch.
void testEditorPreservesRuntimeActive(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.active() == QStringLiteral("Standard"));

    // Stand in for the engine: a second model writes Active=Mathematik to disk.
    {
        ProfileListModel engineSide;
        EXPECT(engineSide.setActiveRow(1));
    }
    EXPECT(m.active() == QStringLiteral("Standard")); // m's copy is now stale

    // A non-active edit must preserve the on-disk (runtime) Active.
    EXPECT(m.setFavorite(1, true));
    EXPECT(m.active() == QStringLiteral("Mathematik")); // adopted from disk

    ProfileListModel after;
    EXPECT(after.active() == QStringLiteral("Mathematik")); // not reverted
    EXPECT(after.data(after.index(1), ProfileListModel::FavoriteRole).toBool());
}

// A shortcut combo may be bound to only one action: re-using it for another
// profile's SelectKey or for a cycle key is rejected, so a keypress never has
// an ambiguous meaning.
void testRejectsDuplicateShortcut(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.createProfile(QStringLiteral("Physik")));
    EXPECT(m.setSelectKey(1, QStringLiteral("Control+Alt+1")));

    // Same combo on another profile -> rejected, that row stays unset.
    EXPECT(!m.setSelectKey(2, QStringLiteral("Control+Alt+1")));
    EXPECT(rowSelectKey(m, 2).isEmpty());
    // A different combo is accepted.
    EXPECT(m.setSelectKey(2, QStringLiteral("Control+Alt+2")));
    // Re-setting a row to its own existing combo is fine (excludeRow).
    EXPECT(m.setSelectKey(1, QStringLiteral("Control+Alt+1")));

    // Cycle keys collide with select keys too.
    m.setCycleNext(QStringLiteral("Control+Alt+1")); // taken -> ignored
    EXPECT(m.cycleNext().isEmpty());
    m.setCycleNext(QStringLiteral("Control+Alt+J")); // free -> set
    EXPECT(m.cycleNext() == QStringLiteral("Control+Alt+J"));
    // CyclePrev cannot reuse CycleNext.
    m.setCyclePrev(QStringLiteral("Control+Alt+J"));
    EXPECT(m.cyclePrev().isEmpty());
}

// The duplicate check matches the engine's combo equivalence: modifier order
// and letter case don't matter, so a hand-edited "Alt+Control+j" still
// collides with "Control+Alt+J".
void testDuplicateShortcutIgnoresOrderAndCase(ProfileListModel &m) {
    EXPECT(m.createProfile(QStringLiteral("Mathematik")));
    EXPECT(m.createProfile(QStringLiteral("Physik")));
    EXPECT(m.setSelectKey(1, QStringLiteral("Control+Alt+J")));
    EXPECT(!m.setSelectKey(2, QStringLiteral("Alt+Control+j")));
    EXPECT(rowSelectKey(m, 2).isEmpty());
}

// -- runner ------------------------------------------------------------------

using TestFn = void (*)(ProfileListModel &);
struct TestCase {
    const char *name;
    TestFn fn;
};

const TestCase kTests[] = {
    {"testSeedsStandard", testSeedsStandard},
    {"testCreateAddsProfileWithSlugFile", testCreateAddsProfileWithSlugFile},
    {"testRejectsDuplicateName", testRejectsDuplicateName},
    {"testDistinctNamesSlugCollisionGetsUniqueFile",
     testDistinctNamesSlugCollisionGetsUniqueFile},
    {"testRejectsEmptyAndInvalidNames", testRejectsEmptyAndInvalidNames},
    {"testRename", testRename},
    {"testRenameActiveUpdatesActive", testRenameActiveUpdatesActive},
    {"testStandardAndLastAreProtected", testStandardAndLastAreProtected},
    {"testRemoveNonStandard", testRemoveNonStandard},
    {"testRemoveActiveReassigns", testRemoveActiveReassigns},
    {"testSetActive", testSetActive},
    {"testSetSelectKey", testSetSelectKey},
    {"testPersistenceRoundTrip", testPersistenceRoundTrip},
    {"testSpacedNameRoundTrip", testSpacedNameRoundTrip},
    {"testRejectsUnsafeFileOnLoad", testRejectsUnsafeFileOnLoad},
    {"testEditorPreservesRuntimeActive", testEditorPreservesRuntimeActive},
    {"testRejectsDuplicateShortcut", testRejectsDuplicateShortcut},
    {"testDuplicateShortcutIgnoresOrderAndCase",
     testDuplicateShortcutIgnoresOrderAndCase},
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    TempXdgConfigHome tempdir("testprofilelistmodel");

    for (const auto &tc : kTests) {
        tempdir.reset();
        ProfileListModel model;
        tc.fn(model);
        std::fprintf(stderr, "ok %s\n", tc.name);
    }
    return 0;
}
