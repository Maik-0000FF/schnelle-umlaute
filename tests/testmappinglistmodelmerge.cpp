// Unit tests for MappingListModel's composed (merge) view — the part the
// plain validation/IO tests do not reach: composing gating on the active base,
// deterministic row order, per-chip provenance, the cascade delete into the
// origin profile, the cross-row move within a source, the manifest order
// override, model-wide duplicate detection, and the usage-frequency preview.
//
// Links MappingListModel.cpp directly (AUTOMOC handles Q_OBJECT) and redirects
// XDG_CONFIG_HOME to a scratch dir, like testmappinglistmodel. Each test seeds
// its own profile files + merge.conf (+ usage.conf) in that dir, then drives a
// model whose edit target is the base so composing() turns on. The base is a
// profiles/ file (not the default mappings.txt) so setProfileFile() actually
// switches target and recomputes composing() instead of early-returning.
//
// Pure header logic (compose, manifest IO, usage IO, the sort comparator) stays
// covered by testprofilecompose / testmergemanifestio / testusageio /
// testusagesort; this exercises the model that wires them to the files.

#include "MappingListModel.h"
#include "editor_paths.h"
#include "mappings-io.h"
#include "merge_manifest_io.h"
#include "profile_paths.h"
#include "test_expect.h"
#include "test_tempdir.h"
#include "usage_io.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>

using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

// Profile refs used across the tests. Functions (not namespace-scope QString
// constants) so no throwing constructor runs during static initialization.
QString kBase() { return QStringLiteral("profiles/base.txt"); }
QString kSrc() { return QStringLiteral("profiles/src.txt"); }

// Write a profile file through a throwaway model, so the on-disk format is
// exactly what the editor writes (addMapping() persists via save()). A
// profiles/ target loads empty (no Standard-profile defaults), so the file ends
// up holding precisely the given entries.
void writeProfile(
    const QString &relFile,
    std::initializer_list<std::pair<const char *, const char *>> entries) {
    MappingListModel w;
    w.setProfileFile(relFile);
    for (const auto &e : entries)
        EXPECT(
            w.addMapping(QString::fromUtf8(e.first), QString::fromUtf8(e.second)));
}

void ensureConfigDir() { QDir().mkpath(schnelle_umlaute::configDirPath()); }

// Write merge.conf via the shared serializer (same bytes the manifest owner
// writes), so the model reads a real manifest.
void writeMergeConf(const schnelle_umlaute::MergeManifest &mf) {
    ensureConfigDir();
    const std::string path =
        (schnelle_umlaute::configDirPath() +
         QString::fromLatin1(schnelle_umlaute::kMergeConf))
            .toStdString();
    std::ofstream(path) << schnelle_umlaute::serializeMergeManifest(mf);
}

// Write usage.conf via the shared serializer (the engine-written file).
void writeUsageConf(const schnelle_umlaute::UsageCounts &counts) {
    ensureConfigDir();
    const std::string path =
        (schnelle_umlaute::configDirPath() +
         QString::fromLatin1(schnelle_umlaute::kUsageFile))
            .toStdString();
    std::ofstream(path) << schnelle_umlaute::serializeUsage(counts);
}

// A merge with base + one appended source; no order override.
schnelle_umlaute::MergeManifest baseAndSource() {
    schnelle_umlaute::MergeManifest mf;
    mf.base = kBase().toStdString();
    mf.sources = {kSrc().toStdString()};
    return mf;
}

// Open the base as the edit target so composing() turns on.
void openBase(MappingListModel &m) {
    m.setProfileFile(kBase());
    EXPECT(m.composing());
}

QString rowInput(MappingListModel &m, int r) {
    return m.data(m.index(r), MappingListModel::InputRole).toString();
}
QString rowOutput(MappingListModel &m, int r) {
    return m.data(m.index(r), MappingListModel::OutputRole).toString();
}
int rowOf(MappingListModel &m, const char *input) {
    for (int i = 0; i < m.rowCount(); ++i)
        if (rowInput(m, i) == QString::fromUtf8(input))
            return i;
    return -1;
}
QVariantList variantsOf(MappingListModel &m, const char *input) {
    const int r = rowOf(m, input);
    return r < 0
               ? QVariantList{}
               : m.data(m.index(r), MappingListModel::ComposedVariantsRole)
                     .toList();
}

// Read a profile file's raw stored output for one input, straight off disk
// (bypassing any merge), so a cascade delete / cross-row move can be verified
// as an actual write to the origin file. Empty string = input not present.
std::string fileVariants(const QString &relFile, const std::string &input) {
    const std::string path =
        (schnelle_umlaute::configDirPath() + relFile).toStdString();
    std::string out;
    if (FILE *fp = std::fopen(path.c_str(), "r")) {
        for (const auto &m : schnelle_umlaute::parseMappings(fp))
            if (m.input == input)
                out = m.output;
        std::fclose(fp);
    }
    return out;
}

// -- composing gating -------------------------------------------------------

// No manifest → no base → the model stays in the plain view even on a profile.
void testComposingFalseWithoutManifest() {
    writeProfile(kBase(), {{"a", "ä"}});
    MappingListModel m;
    m.setProfileFile(kBase());
    EXPECT(!m.composing());
}

// Composing turns on only when the edit target IS the manifest base; any other
// profile (here the appended source) stays plain, so the merge never wanders.
void testComposingGatedOnBase() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"o", "ö"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    m.setProfileFile(kBase());
    EXPECT(m.composing());
    m.setProfileFile(kSrc());
    EXPECT(!m.composing());
}

// -- composed rows ----------------------------------------------------------

// The composed view lists the base's own rows plus the base chars that only the
// appended source carries.
void testComposedShowsBaseAndAppendedRows() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"o", "ö"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.rowCount() == 2);
    EXPECT(rowOutput(m, rowOf(m, "a")) == QString::fromUtf8("ä"));
    EXPECT(rowOutput(m, rowOf(m, "o")) == QString::fromUtf8("ö"));
}

// Appended-only rows follow the SOURCE file order, not the arbitrary iteration
// order of a map — the base's own rows first, then the source's in file order.
void testComposedRowOrderFollowsFileOrder() {
    writeProfile(kBase(), {{"a", "ä"}});
    // File order z, m, b — deliberately not sorted, so a map-iteration order
    // would almost certainly differ.
    writeProfile(kSrc(), {{"z", "Z"}, {"m", "M"}, {"b", "B"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.rowCount() == 4);
    EXPECT(rowInput(m, 0) == QStringLiteral("a"));
    EXPECT(rowInput(m, 1) == QStringLiteral("z"));
    EXPECT(rowInput(m, 2) == QStringLiteral("m"));
    EXPECT(rowInput(m, 3) == QStringLiteral("b"));
}

// Each chip carries its provenance: the value, the 1-based source position
// (base = 1, first appended source = 2), and the origin file.
void testComposedVariantProvenance() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"a", "á"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    const QVariantList vs = variantsOf(m, "a");
    EXPECT(vs.size() == 2);
    const QVariantMap v0 = vs[0].toMap();
    EXPECT(v0.value(QStringLiteral("value")).toString() ==
           QString::fromUtf8("ä"));
    EXPECT(v0.value(QStringLiteral("order")).toInt() == 1);
    EXPECT(v0.value(QStringLiteral("file")).toString() == kBase());
    const QVariantMap v1 = vs[1].toMap();
    EXPECT(v1.value(QStringLiteral("value")).toString() ==
           QString::fromUtf8("á"));
    EXPECT(v1.value(QStringLiteral("order")).toInt() == 2);
    EXPECT(v1.value(QStringLiteral("file")).toString() == kSrc());
}

// -- cascade delete ---------------------------------------------------------

// Deleting a chip that came from an appended source cascades into THAT profile's
// file, not the base.
void testRemoveComposedVariantCascadesToSource() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"o", "ö,ó"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.removeComposedVariant(QStringLiteral("o"), QString::fromUtf8("ó"),
                                   kSrc()));
    EXPECT(fileVariants(kSrc(), "o") == "ö");
    EXPECT(rowOutput(m, rowOf(m, "o")) == QString::fromUtf8("ö"));
}

// Deleting an own chip edits the base's own file.
void testRemoveComposedVariantFromBase() {
    writeProfile(kBase(), {{"a", "ä,á"}});
    writeProfile(kSrc(), {{"o", "ö"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.removeComposedVariant(QStringLiteral("a"), QString::fromUtf8("á"),
                                   kBase()));
    EXPECT(fileVariants(kBase(), "a") == "ä");
    EXPECT(rowOutput(m, rowOf(m, "a")) == QString::fromUtf8("ä"));
}

// Deleting the last chip of a source row drops the whole mapping in the origin
// file (the composed view has no separate row trash; the chip ✕ is the path).
void testRemoveComposedLastVariantDropsMapping() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"x", "X"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.rowCount() == 2);
    EXPECT(m.removeComposedVariant(QStringLiteral("x"), QStringLiteral("X"),
                                   kSrc()));
    EXPECT(fileVariants(kSrc(), "x").empty());
    EXPECT(m.rowCount() == 1);
    EXPECT(rowOf(m, "x") == -1);
}

// -- cross-row move ---------------------------------------------------------

// Moving a chip to another base char re-maps it WITHIN the same source profile:
// removed from the source's fromInput, appended to its toInput.
void testMoveComposedVariantWithinSource() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"e", "é,è"}, {"o", "ö"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.moveComposedVariant(QStringLiteral("e"), QString::fromUtf8("é"),
                                 kSrc(), QStringLiteral("o")));
    EXPECT(fileVariants(kSrc(), "e") == "è");
    EXPECT(fileVariants(kSrc(), "o") == "ö,é");
    EXPECT(rowOutput(m, rowOf(m, "o")) == QString::fromUtf8("ö,é"));
}

// Moving out a row's only chip is refused (it would silently drop the mapping);
// the source file is left untouched.
void testMoveComposedVariantRefusesLastChip() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"e", "é"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(!m.moveComposedVariant(QStringLiteral("e"), QString::fromUtf8("é"),
                                  kSrc(), QStringLiteral("o")));
    EXPECT(fileVariants(kSrc(), "e") == "é");
    EXPECT(fileVariants(kSrc(), "o").empty());
}

// A dropped chip whose value carries a literal comma must be stored ESCAPED
// when the move has to CREATE the target row in the source file. Written raw,
// "x,y" would come back as two variants on the next parse and the chip would
// silently split in half.
void testMoveComposedVariantEscapesCreatedRowInSource() {
    // "o" exists as a composed row (from the base) but not in the source file,
    // so the move takes the create-the-row branch.
    writeProfile(kBase(), {{"a", "ä"}, {"o", "ö"}});
    writeProfile(kSrc(), {{"e", "x,,y,z"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.moveComposedVariant(QStringLiteral("e"), QStringLiteral("x,y"),
                                 kSrc(), QStringLiteral("o")));
    EXPECT(fileVariants(kSrc(), "o") == "x,,y");
    const auto vars = schnelle_umlaute::splitOutputs(fileVariants(kSrc(), "o"));
    EXPECT(vars.size() == 1 && vars[0] == "x,y");
}

// Same duty on the base's own side, which writes through entries_ instead of
// the profile-file rewriter: the created row must carry the escaped form too.
void testMoveComposedVariantEscapesCreatedRowInBase() {
    // The chip is owned by the base; "o" exists only in the appended source, so
    // the base has no row for it yet.
    writeProfile(kBase(), {{"e", "x,,y,z"}});
    writeProfile(kSrc(), {{"o", "ö"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.moveComposedVariant(QStringLiteral("e"), QStringLiteral("x,y"),
                                 kBase(), QStringLiteral("o")));
    EXPECT(fileVariants(kBase(), "o") == "x,,y");
    const auto vars =
        schnelle_umlaute::splitOutputs(fileVariants(kBase(), "o"));
    EXPECT(vars.size() == 1 && vars[0] == "x,y");
}

// -- manifest order override ------------------------------------------------

// A per-base order override in merge.conf rearranges the composed chips
// (anchored on value + source), so a reversed override flips the natural order.
void testOrderOverrideAppliedInComposed() {
    writeProfile(kBase(), {{"a", "ä"}});
    writeProfile(kSrc(), {{"a", "á"}});
    schnelle_umlaute::MergeManifest mf = baseAndSource();
    // Natural order is ä(base), á(source); the override reverses it.
    mf.order["a"] = {{"á", kSrc().toStdString()}, {"ä", kBase().toStdString()}};
    writeMergeConf(mf);

    MappingListModel m;
    openBase(m);
    EXPECT(rowOutput(m, rowOf(m, "a")) == QString::fromUtf8("á,ä"));
}

// -- duplicate detection ----------------------------------------------------

// The warning set flags a value that occurs more than once across all composed
// rows (here the same value under two different keys), and only that value.
void testDuplicateDetectionAcrossComposedRows() {
    writeProfile(kBase(), {{"a", "ä,á"}});
    writeProfile(kSrc(), {{"o", "ä"}});
    writeMergeConf(baseAndSource());

    MappingListModel m;
    openBase(m);
    EXPECT(m.isDuplicateValue(QString::fromUtf8("ä")));
    EXPECT(!m.isDuplicateValue(QString::fromUtf8("á")));
}

// -- usage-frequency preview ------------------------------------------------

// sortByUsage orders by stored counts (most-used first) and is a no-op for a
// base with no recorded usage (keeps the stored order).
void testSortByUsagePreview() {
    writeProfile(kBase(), {{"a", "ä,á"}});
    schnelle_umlaute::UsageCounts uc;
    uc["a"]["ä"] = 5;
    uc["a"]["á"] = 1;
    writeUsageConf(uc);

    MappingListModel m;
    m.setSortByFrequency(true); // loads usage.conf
    EXPECT(m.sortByUsage(QStringLiteral("a"),
                         QStringList{QString::fromUtf8("á"),
                                     QString::fromUtf8("ä")}) ==
           (QStringList{QString::fromUtf8("ä"), QString::fromUtf8("á")}));
    // No counts for "o" → stored order is preserved.
    EXPECT(m.sortByUsage(QStringLiteral("o"),
                         QStringList{QString::fromUtf8("ö"),
                                     QString::fromUtf8("ó")}) ==
           (QStringList{QString::fromUtf8("ö"), QString::fromUtf8("ó")}));
}

// With the toggle on, the composed row itself is reordered by usage, so the
// preview matches the runtime cycle.
void testFrequencySortReordersComposedRow() {
    writeProfile(kBase(), {{"a", "ä,á"}});
    schnelle_umlaute::MergeManifest mf;
    // base with no appended sources still composes
    mf.base = kBase().toStdString();
    writeMergeConf(mf);
    schnelle_umlaute::UsageCounts uc;
    uc["a"]["á"] = 5;
    uc["a"]["ä"] = 1;
    writeUsageConf(uc);

    MappingListModel m;
    openBase(m);
    EXPECT(rowOutput(m, rowOf(m, "a")) == QString::fromUtf8("ä,á")); // stored
    m.setSortByFrequency(true);
    EXPECT(rowOutput(m, rowOf(m, "a")) ==
           QString::fromUtf8("á,ä")); // most-used first
}

// -- usage-data presence (drives the reset control) -------------------------

// hasUsageData is true only when usage.conf holds counts, so the reset control
// is disabled when there is nothing to reset: absent or empty file -> false.
void testHasUsageData() {
    MappingListModel m;
    EXPECT(!m.hasUsageData());
    schnelle_umlaute::UsageCounts uc;
    uc["a"]["ä"] = 3;
    writeUsageConf(uc);
    EXPECT(m.hasUsageData());
    writeUsageConf(schnelle_umlaute::UsageCounts{});
    EXPECT(!m.hasUsageData());
}

// -- test runner ------------------------------------------------------------

struct TestCase {
    const char *name;
    void (*fn)();
};

const TestCase kTests[] = {
    {"testComposingFalseWithoutManifest", testComposingFalseWithoutManifest},
    {"testComposingGatedOnBase", testComposingGatedOnBase},
    {"testComposedShowsBaseAndAppendedRows", testComposedShowsBaseAndAppendedRows},
    {"testComposedRowOrderFollowsFileOrder", testComposedRowOrderFollowsFileOrder},
    {"testComposedVariantProvenance", testComposedVariantProvenance},
    {"testRemoveComposedVariantCascadesToSource",
     testRemoveComposedVariantCascadesToSource},
    {"testRemoveComposedVariantFromBase", testRemoveComposedVariantFromBase},
    {"testRemoveComposedLastVariantDropsMapping",
     testRemoveComposedLastVariantDropsMapping},
    {"testMoveComposedVariantWithinSource", testMoveComposedVariantWithinSource},
    {"testMoveComposedVariantRefusesLastChip",
     testMoveComposedVariantRefusesLastChip},
    {"testMoveComposedVariantEscapesCreatedRowInSource",
     testMoveComposedVariantEscapesCreatedRowInSource},
    {"testMoveComposedVariantEscapesCreatedRowInBase",
     testMoveComposedVariantEscapesCreatedRowInBase},
    {"testOrderOverrideAppliedInComposed", testOrderOverrideAppliedInComposed},
    {"testDuplicateDetectionAcrossComposedRows",
     testDuplicateDetectionAcrossComposedRows},
    {"testSortByUsagePreview", testSortByUsagePreview},
    {"testFrequencySortReordersComposedRow", testFrequencySortReordersComposedRow},
    {"testHasUsageData", testHasUsageData},
};

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    TempXdgConfigHome tempdir("testmappinglistmodelmerge");

    for (const auto &tc : kTests) {
        tempdir.reset();
        tc.fn();
        std::fprintf(stderr, "ok %s\n", tc.name);
    }
    return 0;
}
