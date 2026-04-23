// Unit tests for MappingListModel validation:
//   isValidInputChar, isValidOutputChar (private but exercised via the
//   Q_INVOKABLE wrappers validateInput/validateOutput/inputErrorFor),
//   and the excludeRow semantics of validateInput/inputErrorFor that drive
//   the QML editor's "already mapped" error for every row except the one
//   currently being edited.
//
// Replaces testmappingmodel.cpp from the deleted Qt-plugin MappingModel.
// Links MappingListModel.cpp directly — AUTOMOC handles Q_OBJECT. Redirects
// XDG_CONFIG_HOME so the constructor's load() never touches ~/.config (and
// save() done by addMapping() lands in a scratch dir). Parse/format of
// mappings.txt itself stays covered by testmappingsio.

#include "MappingListModel.h"

#include <QCoreApplication>
#include <QString>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#define EXPECT(cond) do {                                                   \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

namespace {

std::string g_tempdir;

void setupTempdir() {
    char tmpl[] = "/tmp/testmappinglistmodel.XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) {
        std::fprintf(stderr, "mkdtemp failed\n");
        std::abort();
    }
    g_tempdir = dir;
    setenv("XDG_CONFIG_HOME", dir, 1);
}

void cleanupTempdir() {
    if (!g_tempdir.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(g_tempdir, ec);
    }
}

// The constructor's load() falls back to schnelle_umlaute::defaultMappings()
// when mappings.txt is missing or empty. That shape is intentional for the
// first-run UX, but in validation tests it collides with inputs like "a"
// that we want to use freely. Drain the model so every test starts from a
// clean slate without assuming a specific default set.
void clearModel(MappingListModel &m) {
    while (m.rowCount() > 0) m.removeMapping(0);
}

} // namespace

// -- validateOutput: hard rejections (file-format hazards) -------------------

// Embedded '\n' is the mappings.txt entry separator. parseMappings would split
// one mapping into two lines on reload, silently dropping the tail.
void testOutputRejectsNewline(MappingListModel &m) {
    EXPECT(!m.validateOutput(QStringLiteral("hello\nworld")));
    EXPECT(!m.validateOutput(QStringLiteral("\nhello")));
    EXPECT(!m.validateOutput(QStringLiteral("hello\n")));
}

// '\r' inside an output would reach commitString and render erratically.
void testOutputRejectsCarriageReturn(MappingListModel &m) {
    EXPECT(!m.validateOutput(QStringLiteral("hello\rworld")));
    EXPECT(!m.validateOutput(QStringLiteral("line1\r\nline2")));
    EXPECT(!m.validateOutput(QStringLiteral("\r")));
}

// Empty output is rejected — every mapping must produce something.
void testOutputRejectsEmpty(MappingListModel &m) {
    EXPECT(!m.validateOutput(QString()));
    EXPECT(!m.validateOutput(QStringLiteral("")));
}

// -- validateOutput: values that must still pass ----------------------------

void testOutputAcceptsPlainAscii(MappingListModel &m) {
    EXPECT(m.validateOutput(QStringLiteral("hello")));
}
void testOutputAcceptsMultiByteUtf8(MappingListModel &m) {
    EXPECT(m.validateOutput(QString::fromUtf8("ä")));
    EXPECT(m.validateOutput(QString::fromUtf8("€")));
}
// Leading/trailing spaces are intentional — preserved so a user can map e.g.
// " ls" to skip terminal history.
void testOutputAcceptsLeadingAndTrailingSpace(MappingListModel &m) {
    EXPECT(m.validateOutput(QStringLiteral(" ls")));
    EXPECT(m.validateOutput(QStringLiteral("hello ")));
}
void testOutputAcceptsTabs(MappingListModel &m) {
    EXPECT(m.validateOutput(QStringLiteral("\tindent")));
    EXPECT(m.validateOutput(QStringLiteral("col1\tcol2")));
}
// Cycling variant separator — splitOutputs handles it later; not a format hazard.
void testOutputAcceptsCommas(MappingListModel &m) {
    EXPECT(m.validateOutput(QStringLiteral("ä,à,á,â")));
}
void testOutputAcceptsEmoji(MappingListModel &m) {
    EXPECT(m.validateOutput(QString::fromUtf8("😀")));
    EXPECT(m.validateOutput(QString::fromUtf8("hi 😀 bye")));
}

// -- validateInput: single printable character --------------------------------

void testInputRejectsEmpty(MappingListModel &m) {
    EXPECT(!m.validateInput(QString()));
    EXPECT(!m.validateInput(QStringLiteral("")));
}
void testInputRejectsMultipleChars(MappingListModel &m) {
    EXPECT(!m.validateInput(QStringLiteral("ab")));
    EXPECT(!m.validateInput(QStringLiteral("abc")));
}
void testInputRejectsWhitespace(MappingListModel &m) {
    EXPECT(!m.validateInput(QStringLiteral(" ")));
    EXPECT(!m.validateInput(QStringLiteral("\t")));
}
void testInputRejectsControlChars(MappingListModel &m) {
    // '\n' is not printable.
    EXPECT(!m.validateInput(QStringLiteral("\n")));
}
void testInputAcceptsSinglePrintable(MappingListModel &m) {
    EXPECT(m.validateInput(QStringLiteral("a")));
    EXPECT(m.validateInput(QStringLiteral(";")));
    EXPECT(m.validateInput(QStringLiteral("#")));
}
// One Unicode codepoint = one character from the user's perspective, even if
// stored as a surrogate pair in UTF-16.
void testInputAcceptsSingleUtf8Codepoint(MappingListModel &m) {
    EXPECT(m.validateInput(QString::fromUtf8("ä")));
    EXPECT(m.validateInput(QString::fromUtf8("€")));
    EXPECT(m.validateInput(QString::fromUtf8("😀")));
}

// -- validateInput + excludeRow: duplicate detection ------------------------

// The QML editor calls validateInput(candidate, currentRow) when a user is
// editing row N, so that keeping the existing input unchanged doesn't trigger
// a "duplicate" error against the row itself.
void testExcludeRowAllowsSelfInput(MappingListModel &m) {
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("ä")));
    // Without excludeRow, "a" is now a duplicate of row 0.
    EXPECT(!m.validateInput(QStringLiteral("a")));
    // With excludeRow=0, the existing entry is skipped → valid again.
    EXPECT(m.validateInput(QStringLiteral("a"), 0));
}

void testExcludeRowRejectsOtherRowsInput(MappingListModel &m) {
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("ä")));
    EXPECT(m.addMapping(QStringLiteral("o"), QStringLiteral("ö")));
    // Editing row 1 ("o" → "ö"): "a" still collides with row 0.
    EXPECT(!m.validateInput(QStringLiteral("a"), 1));
    // Editing row 0: "o" still collides with row 1.
    EXPECT(!m.validateInput(QStringLiteral("o"), 0));
}

// excludeRow outside the current range is a no-op (nothing to skip) — the
// QML editor uses -1 as "no exclusion" in the add-new-entry case.
void testExcludeRowOutOfRangeDoesNothing(MappingListModel &m) {
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("ä")));
    EXPECT(!m.validateInput(QStringLiteral("a"), -1));
    EXPECT(!m.validateInput(QStringLiteral("a"), 999));
}

// -- inputErrorFor: the localized string shown under the input field --------

// Empty input is not an error per se — the "Add" button just stays disabled.
void testInputErrorEmptyInputReturnsEmpty(MappingListModel &m) {
    EXPECT(m.inputErrorFor(QString()).isEmpty());
}

void testInputErrorReportsMultiChar(MappingListModel &m) {
    QString err = m.inputErrorFor(QStringLiteral("ab"));
    EXPECT(!err.isEmpty());
    EXPECT(err.contains(QStringLiteral("single")) ||
           err.contains(QStringLiteral("printable")));
}

void testInputErrorReportsDuplicate(MappingListModel &m) {
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("ä")));
    QString err = m.inputErrorFor(QStringLiteral("a"));
    EXPECT(!err.isEmpty());
    EXPECT(err.contains(QStringLiteral("already")));
}

// The "already mapped" path must also respect excludeRow so the editor
// doesn't flag a user for keeping their own input unchanged.
void testInputErrorExcludeRowSuppressesSelfDuplicate(MappingListModel &m) {
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("ä")));
    EXPECT(m.inputErrorFor(QStringLiteral("a"), 0).isEmpty());
}

// -- test runner ------------------------------------------------------------

using TestFn = void (*)(MappingListModel &);

struct TestCase {
    const char *name;
    TestFn fn;
};

const TestCase kTests[] = {
    {"testOutputRejectsNewline",                   testOutputRejectsNewline},
    {"testOutputRejectsCarriageReturn",            testOutputRejectsCarriageReturn},
    {"testOutputRejectsEmpty",                     testOutputRejectsEmpty},
    {"testOutputAcceptsPlainAscii",                testOutputAcceptsPlainAscii},
    {"testOutputAcceptsMultiByteUtf8",             testOutputAcceptsMultiByteUtf8},
    {"testOutputAcceptsLeadingAndTrailingSpace",   testOutputAcceptsLeadingAndTrailingSpace},
    {"testOutputAcceptsTabs",                      testOutputAcceptsTabs},
    {"testOutputAcceptsCommas",                    testOutputAcceptsCommas},
    {"testOutputAcceptsEmoji",                     testOutputAcceptsEmoji},
    {"testInputRejectsEmpty",                      testInputRejectsEmpty},
    {"testInputRejectsMultipleChars",              testInputRejectsMultipleChars},
    {"testInputRejectsWhitespace",                 testInputRejectsWhitespace},
    {"testInputRejectsControlChars",               testInputRejectsControlChars},
    {"testInputAcceptsSinglePrintable",            testInputAcceptsSinglePrintable},
    {"testInputAcceptsSingleUtf8Codepoint",        testInputAcceptsSingleUtf8Codepoint},
    {"testExcludeRowAllowsSelfInput",              testExcludeRowAllowsSelfInput},
    {"testExcludeRowRejectsOtherRowsInput",        testExcludeRowRejectsOtherRowsInput},
    {"testExcludeRowOutOfRangeDoesNothing",        testExcludeRowOutOfRangeDoesNothing},
    {"testInputErrorEmptyInputReturnsEmpty",       testInputErrorEmptyInputReturnsEmpty},
    {"testInputErrorReportsMultiChar",             testInputErrorReportsMultiChar},
    {"testInputErrorReportsDuplicate",             testInputErrorReportsDuplicate},
    {"testInputErrorExcludeRowSuppressesSelfDuplicate",
        testInputErrorExcludeRowSuppressesSelfDuplicate},
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    setupTempdir();

    for (const auto &tc : kTests) {
        // Fresh state per test so duplicate-detection tests don't bleed into
        // each other through the shared mappings.txt that save() writes.
        std::filesystem::remove_all(g_tempdir + "/fcitx5");
        MappingListModel model;
        clearModel(model);
        tc.fn(model);
        std::fprintf(stderr, "ok %s\n", tc.name);
    }

    cleanupTempdir();
    return 0;
}
