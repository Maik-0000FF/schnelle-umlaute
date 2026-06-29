// Cross-component round-trip for profiles.conf: the QML editor's
// ProfileListModel writes the file by hand (it does not link fcitx-config),
// and the engine reads it with fcitx's readAsIni into ProfilesConfig. The two
// must agree on the on-disk INI format, so this test exercises BOTH directions:
//
//   1. ProfileListModel writes  -> engine readAsIni parses (runtime path:
//      editor saves, engine ReloadAddonConfig re-reads).
//   2. engine safeSaveAsIni writes -> ProfileListModel reads (the Phase-2
//      switchProfile path, where the engine persists the active profile).
//
// Links ProfileListModel.cpp (Qt) and Fcitx5::Core (readAsIni / ProfilesConfig
// from config.h) in one TU; QT_NO_KEYWORDS keeps the fcitx headers safe under
// the Qt macro set. XDG_CONFIG_HOME is redirected so both sides hit the same
// scratch file.

#include "ProfileListModel.h"
#include "config.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <fcitx-config/iniparser.h>

#include <QCoreApplication>
#include <QString>

#include <string>

using fcitx::ProfileEntryConfig;
using fcitx::ProfilesConfig;
using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

const ProfileEntryConfig *findEntry(const ProfilesConfig &cfg,
                                    const std::string &name) {
    for (const auto &e : *cfg.profiles) {
        if (*e.name == name)
            return &e;
    }
    return nullptr;
}

constexpr const char *kProfilesRel = "schnelle-umlaute/profiles.conf";

} // namespace

// Function-try-block: ProfilesConfig is constructed inline here (its
// FCITX_CONFIGURATION Option members are header-defined), so clang-tidy's
// bugprone-exception-escape sees a throwing path into main. Catch it.
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    TempXdgConfigHome tempdir("testprofilesconf");

    // -- Direction 1: editor writes -> engine reads --------------------------
    // "Mein Profil" has a space, so it exercises the quote/escape path on both
    // sides (the editor escapes it, fcitx readAsIni unescapes it).
    {
        ProfileListModel m; // seeds + writes Standard
        EXPECT(m.createProfile(QStringLiteral("Mathematik")));
        EXPECT(m.createProfile(QStringLiteral("Mein Profil")));
        EXPECT(m.setActiveRow(1));
        EXPECT(m.setSelectKey(1, QStringLiteral("Control+Alt+1")));
        EXPECT(m.setFavorite(1, true));
        m.setCycleNext(QStringLiteral("Control+Alt+Period"));
        m.setCyclePrev(QStringLiteral("Control+Alt+Comma"));
    }

    ProfilesConfig cfg;
    fcitx::readAsIni(cfg, kProfilesRel);

    EXPECT(cfg.profiles->size() == 3);
    EXPECT(*cfg.active == std::string("Mathematik"));
    EXPECT(*cfg.cycleNext == std::string("Control+Alt+Period"));
    EXPECT(*cfg.cyclePrev == std::string("Control+Alt+Comma"));

    const auto *standard = findEntry(cfg, "Standard");
    EXPECT(standard != nullptr);
    EXPECT(*standard->file == std::string("mappings.txt"));

    const auto *math = findEntry(cfg, "Mathematik");
    EXPECT(math != nullptr);
    EXPECT(*math->file == std::string("profiles/mathematik.txt"));
    EXPECT(*math->selectKey == std::string("Control+Alt+1"));
    EXPECT(*math->favorite == true);

    // The spaced name must survive the editor's escaping unchanged.
    const auto *spaced = findEntry(cfg, "Mein Profil");
    EXPECT(spaced != nullptr);
    EXPECT(*spaced->file == std::string("profiles/mein-profil.txt"));
    std::fprintf(stderr, "ok direction1_editor_write_engine_read\n");

    // -- Direction 2: engine writes (canonical fcitx INI) -> editor reads ----
    // safeSaveAsIni quotes "Mein Profil"; the editor must unescape it back.
    fcitx::safeSaveAsIni(cfg, kProfilesRel);

    ProfileListModel m2;
    EXPECT(m2.rowCount() == 3);
    EXPECT(m2.profileNames().value(0) == QStringLiteral("Standard"));
    EXPECT(m2.profileNames().value(1) == QStringLiteral("Mathematik"));
    EXPECT(m2.profileNames().value(2) == QStringLiteral("Mein Profil"));
    EXPECT(m2.fileForRow(1) == QStringLiteral("profiles/mathematik.txt"));
    EXPECT(m2.fileForRow(2) == QStringLiteral("profiles/mein-profil.txt"));
    EXPECT(m2.active() == QStringLiteral("Mathematik"));
    EXPECT(m2.cycleNext() == QStringLiteral("Control+Alt+Period"));
    EXPECT(m2.cyclePrev() == QStringLiteral("Control+Alt+Comma"));
    EXPECT(m2.data(m2.index(1), ProfileListModel::SelectKeyRole).toString()
           == QStringLiteral("Control+Alt+1"));
    EXPECT(m2.data(m2.index(1), ProfileListModel::FavoriteRole).toBool());
    std::fprintf(stderr, "ok direction2_engine_write_editor_read\n");

    return 0;
} catch (...) {
    std::fprintf(stderr, "FAIL: unexpected exception in main\n");
    return 1;
}
