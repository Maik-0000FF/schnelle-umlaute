// Integration tests for MappingModel: load/save round-trip, load-fallback
// semantics, and stateful operations (addItem/deleteItem/moveUp/moveDown/
// hasInput/needSave). Complements testmappingmodel.cpp, which exercises the
// inline validators standalone.
//
// The test redirects fcitx5's user config to a tempdir via XDG_CONFIG_HOME,
// so save()/load() never touch ~/.config. Set before any fcitx5 singleton
// access: StandardPaths::global() caches the environment at first call.

#include "mappings-io.h"
#include "model.h"

#include <QCoreApplication>
#include <QString>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using fcitx::MappingModel;
using schnelle_umlaute::defaultMappings;
using schnelle_umlaute::parseMappings;
using schnelle_umlaute::RawMapping;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

namespace {

std::string g_tempdir;

std::string mappingsPath() {
    return g_tempdir + "/fcitx5/schnelle-umlaute/mappings.txt";
}

// Ensure the parent directory exists so we can pre-seed files for tests.
// save() creates it itself, but load-fallback tests write the file directly.
void ensureMappingsDir() {
    std::filesystem::create_directories(g_tempdir + "/fcitx5/schnelle-umlaute");
}

void removeMappingsFile() {
    std::error_code ec;
    std::filesystem::remove(mappingsPath(), ec);
}

void writeMappingsFile(const std::string &content) {
    ensureMappingsDir();
    FILE *fp = std::fopen(mappingsPath().c_str(), "w");
    EXPECT(fp != nullptr);
    if (!content.empty()) {
        std::fwrite(content.data(), 1, content.size(), fp);
    }
    std::fclose(fp);
}

std::vector<RawMapping> reparseMappingsFile() {
    FILE *fp = std::fopen(mappingsPath().c_str(), "r");
    if (!fp)
        return {};
    auto r = parseMappings(fp);
    std::fclose(fp);
    return r;
}

bool mappingsFileExists() {
    struct stat st;
    return ::stat(mappingsPath().c_str(), &st) == 0;
}

// --- Group 1: save() → parseMappings round-trip ----------------------------

// Defaults-only round-trip: load() with no file populates defaults,
// save() writes them, re-parsing the file yields byte-identical entries.
// Guards against format drift between the writer (model.cpp: fprintf "%s=%s\n")
// and the reader (mappings-io.h: parseMappings).
void testRoundTripDefaults() {
    removeMappingsFile();
    MappingModel m;
    m.load();
    m.save();

    auto parsed = reparseMappingsFile();
    auto defaults = defaultMappings();
    EXPECT(parsed.size() == defaults.size());
    for (size_t i = 0; i < defaults.size(); ++i) {
        EXPECT(parsed[i].input == defaults[i].input);
        EXPECT(parsed[i].output == defaults[i].output);
    }
}

// Round-trip with user-added entries covering the format's awkward cases:
// multi-byte UTF-8 input, cycling commas in output, leading/trailing space
// in output (intentional — see editor.cpp:107), emoji input (4-byte UTF-8),
// '=' as input (valid since the delimiter is the '=' AFTER the first char).
void testRoundTripWithCustomEntries() {
    removeMappingsFile();
    MappingModel m;
    m.load();
    // Reset to empty so we control the full entry set. The public API has
    // no clear(); delete rows from the back until empty.
    while (m.rowCount() > 0) {
        m.deleteItem(m.rowCount() - 1);
    }
    EXPECT(m.rowCount() == 0);

    struct Entry {
        const char *input;
        const char *output;
    };
    Entry entries[] = {
        {"a", "\xc3\xa4"},             // single-byte input, 2-byte output
        {"\xc3\xa4", "ae"},            // 2-byte input, ASCII output
        {"\xe2\x82\xac", "EUR"},       // 3-byte input (€)
        {"\xf0\x9f\x98\x80", "smile"}, // 4-byte input (😀)
        {"o", "\xc3\xb6,oe,O"},        // cycling variants with commas
        {"s", " ls"},                  // leading space (intentional)
        {"t", "trail "},               // trailing space (intentional)
        {"=", "bang"},                 // '=' as input key
    };
    for (const auto &e : entries) {
        m.addItem(QString::fromUtf8(e.input), QString::fromUtf8(e.output));
    }
    m.save();

    auto parsed = reparseMappingsFile();
    EXPECT(parsed.size() == sizeof(entries) / sizeof(entries[0]));
    for (size_t i = 0; i < parsed.size(); ++i) {
        EXPECT(parsed[i].input == entries[i].input);
        EXPECT(parsed[i].output == entries[i].output);
    }

    // Re-load into a fresh model and re-save. A second round-trip must
    // produce the exact same bytes — catches any asymmetric normalization
    // (e.g. if load silently dropped entries save would not put back).
    MappingModel m2;
    m2.load();
    EXPECT(m2.rowCount() ==
           static_cast<int>(sizeof(entries) / sizeof(entries[0])));
    m2.save();
    auto parsed2 = reparseMappingsFile();
    EXPECT(parsed2.size() == parsed.size());
    for (size_t i = 0; i < parsed.size(); ++i) {
        EXPECT(parsed2[i].input == parsed[i].input);
        EXPECT(parsed2[i].output == parsed[i].output);
    }
}

// --- Group 2: load() fallback semantics ------------------------------------

// No file exists → defaults are loaded and needSave stays false (a fresh
// install must not prompt the user to save something they never changed).
void testLoadFallbackMissingFile() {
    removeMappingsFile();
    EXPECT(!mappingsFileExists());

    MappingModel m;
    m.load();

    auto defaults = defaultMappings();
    EXPECT(m.rowCount() == static_cast<int>(defaults.size()));
    EXPECT(m.needSave() == false);
    // Sanity: first default is ("a", "ä"). If this drifts, the user-visible
    // seed set changed — catch it here rather than in manual testing.
    auto first = m.data(m.index(0, 0)).toString();
    auto firstOut = m.data(m.index(0, 1)).toString();
    EXPECT(first == QString::fromUtf8(defaults[0].input.c_str()));
    EXPECT(firstOut == QString::fromUtf8(defaults[0].output.c_str()));
}

// Empty file → defaults (load() checks entries_.empty() after parseMappings
// and falls back). A user who deleted all entries would otherwise be left
// with nothing the next time fcitx5 starts.
void testLoadFallbackEmptyFile() {
    writeMappingsFile("");
    MappingModel m;
    m.load();
    EXPECT(m.rowCount() == static_cast<int>(defaultMappings().size()));
    EXPECT(m.needSave() == false);
}

// File present but every line is invalid (bad UTF-8, missing '=', empty
// output). parseMappings returns empty, fallback kicks in. Same reasoning
// as the empty-file case: never leave the user with zero mappings.
void testLoadFallbackAllInvalid() {
    writeMappingsFile("\xc3\x41=bad\n" // invalid UTF-8 continuation
                      "noequals\n"     // missing '='
                      "a=\n"           // empty output
                      "# just a comment\n"
                      "\n");
    MappingModel m;
    m.load();
    EXPECT(m.rowCount() == static_cast<int>(defaultMappings().size()));
    EXPECT(m.needSave() == false);
}

// Valid file → entries loaded as-is, defaults NOT overlaid. needSave stays
// false (load() is idempotent from the user's perspective).
void testLoadValidFile() {
    writeMappingsFile("x=eins\ny=zwei\n");
    MappingModel m;
    m.load();
    EXPECT(m.rowCount() == 2);
    EXPECT(m.needSave() == false);
    EXPECT(m.data(m.index(0, 0)).toString() == QStringLiteral("x"));
    EXPECT(m.data(m.index(0, 1)).toString() == QStringLiteral("eins"));
    EXPECT(m.data(m.index(1, 0)).toString() == QStringLiteral("y"));
    EXPECT(m.data(m.index(1, 1)).toString() == QStringLiteral("zwei"));
}

// Mixed valid + invalid file → only valid entries loaded, no fallback
// (parseMappings returned a non-empty vector, so defaults are NOT overlaid).
// Covers the case where a user hand-edited mappings.txt and introduced one
// syntax error — they lose the bad line, not their whole config.
void testLoadValidWithSomeInvalidLines() {
    writeMappingsFile("x=eins\n"
                      "\xc3\x41=bad\n"
                      "y=zwei\n");
    MappingModel m;
    m.load();
    EXPECT(m.rowCount() == 2);
    EXPECT(m.needSave() == false);
    EXPECT(m.data(m.index(0, 0)).toString() == QStringLiteral("x"));
    EXPECT(m.data(m.index(1, 0)).toString() == QStringLiteral("y"));
}

// --- Group 3: addItem / deleteItem / moveUp / moveDown / hasInput ---------

// Start from a clean state on every test. Helper: load defaults, clear all.
MappingModel *freshEmptyModel() {
    removeMappingsFile();
    auto *m = new MappingModel();
    m->load();
    while (m->rowCount() > 0) {
        m->deleteItem(m->rowCount() - 1);
    }
    return m;
}

// addItem appends at the end, returns an index pointing at the new row,
// and marks the model dirty (Save button in the UI relies on needSave()).
void testAddItem() {
    auto *m = freshEmptyModel();
    EXPECT(m->rowCount() == 0);

    auto idx = m->addItem(QStringLiteral("z"), QStringLiteral("zzz"));
    EXPECT(idx.row() == 0);
    EXPECT(idx.column() == 0);
    EXPECT(m->rowCount() == 1);
    EXPECT(m->needSave() == true);

    m->addItem(QStringLiteral("y"), QStringLiteral("yy"));
    EXPECT(m->rowCount() == 2);
    // Entries are kept in insertion order.
    EXPECT(m->data(m->index(0, 0)).toString() == QStringLiteral("z"));
    EXPECT(m->data(m->index(1, 0)).toString() == QStringLiteral("y"));
    delete m;
}

// hasInput finds existing entries; excludeRow lets setData check "any OTHER
// row has this input" so editing a row to its own current value is allowed.
void testHasInput() {
    auto *m = freshEmptyModel();
    m->addItem(QStringLiteral("a"), QStringLiteral("AA"));
    m->addItem(QStringLiteral("b"), QStringLiteral("BB"));
    m->addItem(QStringLiteral("c"), QStringLiteral("CC"));

    EXPECT(m->hasInput(QStringLiteral("a")) == true);
    EXPECT(m->hasInput(QStringLiteral("b")) == true);
    EXPECT(m->hasInput(QStringLiteral("z")) == false);

    // Excluding the row that holds "a" → reports "a" as NOT present.
    EXPECT(m->hasInput(QStringLiteral("a"), 0) == false);
    // Excluding a different row → "a" is still found at row 0.
    EXPECT(m->hasInput(QStringLiteral("a"), 1) == true);
    // Out-of-range excludeRow behaves as "exclude nothing" (the loop's
    // `if (i == excludeRow)` never matches). Document the current behavior.
    EXPECT(m->hasInput(QStringLiteral("a"), 999) == true);
    EXPECT(m->hasInput(QStringLiteral("a"), -1) == true);

    // Multi-byte inputs compare as whole QString (not byte-wise), which is
    // what the editor's duplicate check needs for UTF-8 keys.
    m->addItem(QString::fromUtf8("ä"), QStringLiteral("ae"));
    EXPECT(m->hasInput(QString::fromUtf8("ä")) == true);
    EXPECT(m->hasInput(QString::fromUtf8("ö")) == false);
    delete m;
}

// deleteItem removes the requested row; out-of-range indices are no-ops
// (the editor's Delete button relies on this for stale selections).
void testDeleteItem() {
    auto *m = freshEmptyModel();
    m->addItem(QStringLiteral("a"), QStringLiteral("1"));
    m->addItem(QStringLiteral("b"), QStringLiteral("2"));
    m->addItem(QStringLiteral("c"), QStringLiteral("3"));
    EXPECT(m->rowCount() == 3);

    m->deleteItem(1); // remove "b"
    EXPECT(m->rowCount() == 2);
    EXPECT(m->data(m->index(0, 0)).toString() == QStringLiteral("a"));
    EXPECT(m->data(m->index(1, 0)).toString() == QStringLiteral("c"));
    EXPECT(m->needSave() == true);

    // Out-of-range: no-op, no crash, no row change.
    m->deleteItem(-1);
    m->deleteItem(999);
    EXPECT(m->rowCount() == 2);
    delete m;
}

// moveUp swaps row with row-1; moveDown swaps row with row+1. Boundary
// calls (first row up, last row down) are no-ops — the UI wires the Up/Down
// buttons unconditionally and relies on this guard.
void testMoveUpAndDown() {
    auto *m = freshEmptyModel();
    m->addItem(QStringLiteral("a"), QStringLiteral("1"));
    m->addItem(QStringLiteral("b"), QStringLiteral("2"));
    m->addItem(QStringLiteral("c"), QStringLiteral("3"));

    // moveUp(0): no-op (already at top).
    m->moveUp(0);
    EXPECT(m->data(m->index(0, 0)).toString() == QStringLiteral("a"));
    EXPECT(m->data(m->index(1, 0)).toString() == QStringLiteral("b"));
    EXPECT(m->data(m->index(2, 0)).toString() == QStringLiteral("c"));

    // moveUp(2): "c" moves up → [a, c, b].
    m->moveUp(2);
    EXPECT(m->data(m->index(0, 0)).toString() == QStringLiteral("a"));
    EXPECT(m->data(m->index(1, 0)).toString() == QStringLiteral("c"));
    EXPECT(m->data(m->index(2, 0)).toString() == QStringLiteral("b"));
    EXPECT(m->needSave() == true);

    // moveDown(last=2): no-op (already at bottom).
    m->moveDown(2);
    EXPECT(m->data(m->index(2, 0)).toString() == QStringLiteral("b"));

    // moveDown(0): "a" moves down → [c, a, b].
    m->moveDown(0);
    EXPECT(m->data(m->index(0, 0)).toString() == QStringLiteral("c"));
    EXPECT(m->data(m->index(1, 0)).toString() == QStringLiteral("a"));
    EXPECT(m->data(m->index(2, 0)).toString() == QStringLiteral("b"));

    // Out-of-range: no-op.
    m->moveUp(-1);
    m->moveUp(999);
    m->moveDown(-1);
    m->moveDown(999);
    EXPECT(m->rowCount() == 3);
    EXPECT(m->data(m->index(0, 0)).toString() == QStringLiteral("c"));
    delete m;
}

// save() clears the dirty flag; a subsequent mutation sets it again.
// This is what drives the editor's "unsaved changes" indicator.
void testNeedSaveLifecycle() {
    auto *m = freshEmptyModel();
    m->addItem(QStringLiteral("x"), QStringLiteral("X"));
    EXPECT(m->needSave() == true);

    m->save();
    EXPECT(m->needSave() == false);

    m->addItem(QStringLiteral("y"), QStringLiteral("Y"));
    EXPECT(m->needSave() == true);
    m->save();
    EXPECT(m->needSave() == false);

    m->deleteItem(0);
    EXPECT(m->needSave() == true);
    m->save();
    EXPECT(m->needSave() == false);

    m->moveUp(m->rowCount() > 1 ? 1 : 0);
    // If moveUp was a no-op (only 1 row left), trigger a change another way.
    if (!m->needSave()) {
        m->addItem(QStringLiteral("z"), QStringLiteral("Z"));
    }
    EXPECT(m->needSave() == true);
    delete m;
}

} // namespace

int main(int argc, char *argv[]) {
    // Redirect fcitx5's user-config lookup into a tempdir BEFORE constructing
    // anything that might access StandardPaths::global() (including Qt's init,
    // which is safe, but play it safe and set env first).
    char tmpl[] = "/tmp/schnelle-umlaute-test-XXXXXX";
    const char *dir = mkdtemp(tmpl);
    if (!dir) {
        std::fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }
    g_tempdir = dir;
    setenv("XDG_CONFIG_HOME", g_tempdir.c_str(), 1);
    // Also pin XDG_DATA_HOME/XDG_CACHE_HOME so no stray read escapes the
    // sandbox on systems where fcitx5 consults them during StandardPaths init.
    setenv("XDG_DATA_HOME", (g_tempdir + "/data").c_str(), 1);
    setenv("XDG_CACHE_HOME", (g_tempdir + "/cache").c_str(), 1);

    QCoreApplication app(argc, argv);

    testRoundTripDefaults();
    testRoundTripWithCustomEntries();

    testLoadFallbackMissingFile();
    testLoadFallbackEmptyFile();
    testLoadFallbackAllInvalid();
    testLoadValidFile();
    testLoadValidWithSomeInvalidLines();

    testAddItem();
    testHasInput();
    testDeleteItem();
    testMoveUpAndDown();
    testNeedSaveLifecycle();

    // Best-effort cleanup — leave the tempdir on failure for inspection.
    std::error_code ec;
    std::filesystem::remove_all(g_tempdir, ec);

    std::printf("All mapping-model I/O tests passed.\n");
    return 0;
}
