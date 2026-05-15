// Integration tests for MappingListModel: add/remove/update/move state ops
// and save/load round-trip against mappings.txt.
//
// Replaces testmappingmodelio.cpp from the deleted Qt-plugin MappingModel.
// Redirects XDG_CONFIG_HOME to a tempdir so save() (called implicitly by
// every mutation) lands there. save() also triggers a DBus reload — the
// session-bus send is async and silently skipped when no bus is available,
// so the test stays hermetic.
//
// Parse/format of mappings.txt itself is covered by testmappingsio; here we
// only care that the model writes something the parser can read back and
// that the in-memory state matches what the user expects after each op.

#include "MappingListModel.h"
#include "mappings-io.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <QCoreApplication>
#include <QModelIndex>
#include <QString>
#include <QVariant>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

// Holder for the scratch XDG_CONFIG_HOME. main() owns it and the per-test
// helpers below read its path; putting it here (rather than plumbing it
// through every helper signature) keeps the test bodies focused on
// behavior rather than wiring.
TempXdgConfigHome *g_tempdir = nullptr;

std::string mappingsPath() {
    return g_tempdir->path() + "/fcitx5/schnelle-umlaute/mappings.txt";
}

void ensureDirs() {
    std::filesystem::create_directories(g_tempdir->path() +
                                        "/fcitx5/schnelle-umlaute");
}

void seedEmptyMappings() {
    ensureDirs();
    std::FILE *fp = std::fopen(mappingsPath().c_str(), "w");
    if (fp)
        std::fclose(fp);
}

// The constructor loads defaultMappings() as a fallback whenever the file
// is empty or missing. Most tests want a truly blank model so they can add
// the keys they care about without tripping over the defaults. Drain the
// entries but keep the instance; removeMapping() triggers save() so the
// file on disk ends up empty, matching the in-memory state.
void drainDefaults(MappingListModel &m) {
    while (m.rowCount() > 0)
        m.removeMapping(0);
}

void writeMappingsFile(const std::string &contents) {
    ensureDirs();
    std::FILE *fp = std::fopen(mappingsPath().c_str(), "w");
    if (!fp) {
        std::fprintf(stderr, "cannot open %s\n", mappingsPath().c_str());
        std::abort();
    }
    std::fwrite(contents.data(), 1, contents.size(), fp);
    std::fclose(fp);
}

std::string readMappingsFile() {
    std::FILE *fp = std::fopen(mappingsPath().c_str(), "r");
    if (!fp)
        return {};
    std::string out;
    char buf[4096];
    // Read until a short fread tells us the stream is exhausted, then
    // exit the loop. Re-entering fread on an EOF-stream is what the
    // unix.Stream analyzer flags as undefined behaviour.
    bool streamEnded = false;
    while (!streamEnded) {
        size_t n = std::fread(buf, 1, sizeof(buf), fp);
        if (n > 0)
            out.append(buf, n);
        if (n < sizeof(buf))
            streamEnded = true;
    }
    std::fclose(fp);
    return out;
}

// Per-test isolation — wipe the tempdir contents but keep the path intact
// so Qt's QStandardPaths cache (sampled once at startup) still resolves
// GenericConfigLocation correctly for the next model's load/save cycle.
void resetTempdir() { g_tempdir->reset(); }

QString inputAt(const MappingListModel &m, int row) {
    return m.data(m.index(row, 0), MappingListModel::InputRole).toString();
}
QString outputAt(const MappingListModel &m, int row) {
    return m.data(m.index(row, 0), MappingListModel::OutputRole).toString();
}

} // namespace

// -- constructor: empty file falls back to defaults --------------------------

// A freshly installed system has no mappings.txt. The model must populate
// itself with the built-in defaults so the user sees something to work with.
void testEmptyFileLoadsDefaults() {
    resetTempdir();
    // No mappings.txt exists at all.
    MappingListModel m;
    EXPECT(m.rowCount() > 0);
    const auto defaults = schnelle_umlaute::defaultMappings();
    EXPECT(m.rowCount() == static_cast<int>(defaults.size()));
}

// An existing but empty file (e.g. the user deleted every entry in the UI)
// also triggers the defaults fallback — otherwise a deleted-last-row state
// would persist as "no mappings ever" forever.
void testExplicitEmptyFileLoadsDefaults() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    EXPECT(m.rowCount() > 0);
}

// A non-empty file is preferred over defaults — otherwise the user's own
// mappings would never be read back.
void testExistingFileIsPreferredOverDefaults() {
    resetTempdir();
    writeMappingsFile("x=ø\ny=ñ\n");
    MappingListModel m;
    EXPECT(m.rowCount() == 2);
    EXPECT(inputAt(m, 0) == QStringLiteral("x"));
    EXPECT(outputAt(m, 0) == QString::fromUtf8("ø"));
    EXPECT(inputAt(m, 1) == QStringLiteral("y"));
    EXPECT(outputAt(m, 1) == QString::fromUtf8("ñ"));
}

// -- addMapping --------------------------------------------------------------

void testAddMappingAppends() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    EXPECT(m.rowCount() == 1);
    EXPECT(inputAt(m, 0) == QStringLiteral("a"));
    EXPECT(outputAt(m, 0) == QString::fromUtf8("ä"));
}

void testAddMappingRejectsInvalidInput() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(!m.addMapping(QStringLiteral("ab"), QString::fromUtf8("x")));
    EXPECT(!m.addMapping(QStringLiteral(""), QString::fromUtf8("x")));
    EXPECT(m.rowCount() == 0);
}

void testAddMappingRejectsInvalidOutput() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(!m.addMapping(QStringLiteral("a"), QStringLiteral("line1\nline2")));
    EXPECT(!m.addMapping(QStringLiteral("a"), QString()));
    EXPECT(m.rowCount() == 0);
}

void testAddMappingRejectsDuplicateInput() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    EXPECT(!m.addMapping(QStringLiteral("a"), QString::fromUtf8("à")));
    EXPECT(m.rowCount() == 1);
}

// -- removeMapping -----------------------------------------------------------

void testRemoveMappingByRow() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    EXPECT(m.addMapping(QStringLiteral("o"), QString::fromUtf8("ö")));
    m.removeMapping(0);
    EXPECT(m.rowCount() == 1);
    EXPECT(inputAt(m, 0) == QStringLiteral("o"));
}

void testRemoveMappingOutOfRangeIsNoOp() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    m.removeMapping(-1);
    m.removeMapping(99);
    EXPECT(m.rowCount() == 1);
}

// -- updateMapping -----------------------------------------------------------

void testUpdateMappingChangesValues() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    EXPECT(m.updateMapping(0, QStringLiteral("a"), QString::fromUtf8("â")));
    EXPECT(outputAt(m, 0) == QString::fromUtf8("â"));
    // Renaming the input is also allowed.
    EXPECT(m.updateMapping(0, QStringLiteral("b"), QString::fromUtf8("â")));
    EXPECT(inputAt(m, 0) == QStringLiteral("b"));
}

// The excludeRow semantics of hasInput let a user keep the row's own input
// unchanged without triggering a "duplicate" error against itself.
void testUpdateMappingKeepsSelfInput() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    // Same input, different output — must succeed.
    EXPECT(m.updateMapping(0, QStringLiteral("a"), QString::fromUtf8("à")));
    EXPECT(outputAt(m, 0) == QString::fromUtf8("à"));
}

void testUpdateMappingRejectsDuplicateFromOtherRow() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
    EXPECT(m.addMapping(QStringLiteral("o"), QString::fromUtf8("ö")));
    // Row 1 tries to adopt row 0's input — must be rejected.
    EXPECT(!m.updateMapping(1, QStringLiteral("a"), QString::fromUtf8("ö")));
    EXPECT(inputAt(m, 1) == QStringLiteral("o"));
}

void testUpdateMappingOutOfRangeIsRejected() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(!m.updateMapping(0, QStringLiteral("a"), QString::fromUtf8("ä")));
    EXPECT(!m.updateMapping(-1, QStringLiteral("a"), QString::fromUtf8("ä")));
}

// -- moveMapping -------------------------------------------------------------

void testMoveMappingDown() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("A")));
    EXPECT(m.addMapping(QStringLiteral("b"), QStringLiteral("B")));
    EXPECT(m.addMapping(QStringLiteral("c"), QStringLiteral("C")));
    m.moveMapping(0, 2);
    EXPECT(inputAt(m, 0) == QStringLiteral("b"));
    EXPECT(inputAt(m, 1) == QStringLiteral("c"));
    EXPECT(inputAt(m, 2) == QStringLiteral("a"));
}

void testMoveMappingUp() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("A")));
    EXPECT(m.addMapping(QStringLiteral("b"), QStringLiteral("B")));
    EXPECT(m.addMapping(QStringLiteral("c"), QStringLiteral("C")));
    m.moveMapping(2, 0);
    EXPECT(inputAt(m, 0) == QStringLiteral("c"));
    EXPECT(inputAt(m, 1) == QStringLiteral("a"));
    EXPECT(inputAt(m, 2) == QStringLiteral("b"));
}

void testMoveMappingSameIndexIsNoOp() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("A")));
    EXPECT(m.addMapping(QStringLiteral("b"), QStringLiteral("B")));
    m.moveMapping(0, 0);
    EXPECT(inputAt(m, 0) == QStringLiteral("a"));
    EXPECT(inputAt(m, 1) == QStringLiteral("b"));
}

void testMoveMappingOutOfRangeIsNoOp() {
    resetTempdir();
    seedEmptyMappings();
    MappingListModel m;
    drainDefaults(m);
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("A")));
    EXPECT(m.addMapping(QStringLiteral("b"), QStringLiteral("B")));
    m.moveMapping(-1, 0);
    m.moveMapping(0, 99);
    m.moveMapping(99, 0);
    EXPECT(inputAt(m, 0) == QStringLiteral("a"));
    EXPECT(inputAt(m, 1) == QStringLiteral("b"));
}

// -- save/load round-trip ----------------------------------------------------

// Every mutation calls save() implicitly. The file on disk must contain
// exactly what the model holds, and a fresh MappingListModel reading the
// same file must reconstruct the same state.
void testMutationsPersistToDisk() {
    resetTempdir();
    seedEmptyMappings();
    {
        MappingListModel m;
        drainDefaults(m);
        EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
        EXPECT(m.addMapping(QStringLiteral("o"), QString::fromUtf8("ö")));
        EXPECT(m.addMapping(QStringLiteral("u"), QString::fromUtf8("ü")));
        m.moveMapping(2, 0);
        m.removeMapping(1);
        EXPECT(m.updateMapping(0, QStringLiteral("U"), QString::fromUtf8("Ü")));
    }
    // Fresh model reads from the same file — state must match what the
    // previous model left behind.
    MappingListModel m2;
    EXPECT(m2.rowCount() == 2);
    EXPECT(inputAt(m2, 0) == QStringLiteral("U"));
    EXPECT(outputAt(m2, 0) == QString::fromUtf8("Ü"));
    EXPECT(inputAt(m2, 1) == QStringLiteral("o"));
    EXPECT(outputAt(m2, 1) == QString::fromUtf8("ö"));
}

// On-disk format: one "input=output\n" per entry, UTF-8 encoded, preserved
// in user-specified order. testmappingsio covers the parser corner cases; we
// only spot-check the shape to catch catastrophic format changes.
void testOnDiskFormatIsUtf8KeyValue() {
    resetTempdir();
    seedEmptyMappings();
    {
        MappingListModel m;
        drainDefaults(m);
        EXPECT(m.addMapping(QStringLiteral("a"), QString::fromUtf8("ä")));
        EXPECT(m.addMapping(QStringLiteral(";"), QStringLiteral("--")));
    }
    const auto raw = readMappingsFile();
    EXPECT(raw.find("a=\xc3\xa4\n") != std::string::npos);
    EXPECT(raw.find(";=--\n") != std::string::npos);
}

// -- test runner -------------------------------------------------------------

using TestFn = void (*)();
struct TestCase {
    const char *name;
    TestFn fn;
};

const TestCase kTests[] = {
    {"testEmptyFileLoadsDefaults", testEmptyFileLoadsDefaults},
    {"testExplicitEmptyFileLoadsDefaults", testExplicitEmptyFileLoadsDefaults},
    {"testExistingFileIsPreferredOverDefaults",
     testExistingFileIsPreferredOverDefaults},
    {"testAddMappingAppends", testAddMappingAppends},
    {"testAddMappingRejectsInvalidInput", testAddMappingRejectsInvalidInput},
    {"testAddMappingRejectsInvalidOutput", testAddMappingRejectsInvalidOutput},
    {"testAddMappingRejectsDuplicateInput",
     testAddMappingRejectsDuplicateInput},
    {"testRemoveMappingByRow", testRemoveMappingByRow},
    {"testRemoveMappingOutOfRangeIsNoOp", testRemoveMappingOutOfRangeIsNoOp},
    {"testUpdateMappingChangesValues", testUpdateMappingChangesValues},
    {"testUpdateMappingKeepsSelfInput", testUpdateMappingKeepsSelfInput},
    {"testUpdateMappingRejectsDuplicateFromOtherRow",
     testUpdateMappingRejectsDuplicateFromOtherRow},
    {"testUpdateMappingOutOfRangeIsRejected",
     testUpdateMappingOutOfRangeIsRejected},
    {"testMoveMappingDown", testMoveMappingDown},
    {"testMoveMappingUp", testMoveMappingUp},
    {"testMoveMappingSameIndexIsNoOp", testMoveMappingSameIndexIsNoOp},
    {"testMoveMappingOutOfRangeIsNoOp", testMoveMappingOutOfRangeIsNoOp},
    {"testMutationsPersistToDisk", testMutationsPersistToDisk},
    {"testOnDiskFormatIsUtf8KeyValue", testOnDiskFormatIsUtf8KeyValue},
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    TempXdgConfigHome tempdir("testmappinglistmodelio");
    g_tempdir = &tempdir;
    for (const auto &tc : kTests) {
        tc.fn();
        std::fprintf(stderr, "ok %s\n", tc.name);
    }
    return 0;
}
