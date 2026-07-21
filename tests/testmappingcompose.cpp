// Unit tests for MappingListModel's composed merge view (issue #112).
// When the edit target is the active profile and the merge overlay is
// non-empty, the model shows the composed effective mapping (own + overlay)
// with a source role, while save() still writes only the own entries.
// Non-ASCII is avoided: variant tokens are plain letters, the logic is
// string-agnostic. XDG_CONFIG_HOME is redirected to a scratch dir.

#include "MappingListModel.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <cstdio>

using schnelle_umlaute_tests::TempXdgConfigHome;

static QString roleAt(MappingListModel &m, int row,
                      MappingListModel::Roles role) {
    return m.data(m.index(row), role).toString();
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    TempXdgConfigHome tempdir("testmappingcompose");

    // Overlay source profile: a -> [B, C], n -> [N].
    {
        MappingListModel ov;
        ov.setProfileFile(QStringLiteral("profiles/spanish.txt"));
        EXPECT(ov.rowCount() == 0); // a non-standard profile loads empty
        EXPECT(ov.addMapping(QStringLiteral("a"), QStringLiteral("B,C")));
        EXPECT(ov.addMapping(QStringLiteral("n"), QStringLiteral("N")));
    }

    // Base (active) profile: a -> [A], o -> [O].
    MappingListModel m;
    m.setProfileFile(QStringLiteral("profiles/base.txt"));
    EXPECT(m.rowCount() == 0);
    EXPECT(m.addMapping(QStringLiteral("a"), QStringLiteral("A")));
    EXPECT(m.addMapping(QStringLiteral("o"), QStringLiteral("O")));
    EXPECT(m.rowCount() == 2); // not composing yet: own rows only

    // Turn composition on: edit target == active, overlay = spanish.
    m.setActiveProfileFile(QStringLiteral("profiles/base.txt"));
    m.setMergeOverlay(
        QStringList{QStringLiteral("profile:profiles/spanish.txt")});

    // Composed rows: own bases first (a, o), then overlay-only base (n).
    EXPECT(m.rowCount() == 3);
    // a: own [A] + overlay [B, C] -> union [A, B, C], source "merged".
    EXPECT(roleAt(m, 0, MappingListModel::InputRole) == QStringLiteral("a"));
    EXPECT(roleAt(m, 0, MappingListModel::OutputRole) == QStringLiteral("A,B,C"));
    EXPECT(roleAt(m, 0, MappingListModel::SourceRole) == QStringLiteral("merged"));
    // o: only own.
    EXPECT(roleAt(m, 1, MappingListModel::InputRole) == QStringLiteral("o"));
    EXPECT(roleAt(m, 1, MappingListModel::OutputRole) == QStringLiteral("O"));
    EXPECT(roleAt(m, 1, MappingListModel::SourceRole) == QStringLiteral("own"));
    // n: only from the overlay.
    EXPECT(roleAt(m, 2, MappingListModel::InputRole) == QStringLiteral("n"));
    EXPECT(roleAt(m, 2, MappingListModel::OutputRole) == QStringLiteral("N"));
    EXPECT(roleAt(m, 2, MappingListModel::SourceRole) ==
           QStringLiteral("inherited"));

    // An inherited row is read-only for now: removing it is refused.
    m.removeMapping(2);
    EXPECT(m.rowCount() == 3);

    // Removing an own row works and only touches the own file.
    m.removeMapping(1); // remove 'o'
    EXPECT(m.rowCount() == 2); // a (merged), n (inherited)
    EXPECT(roleAt(m, 0, MappingListModel::InputRole) == QStringLiteral("a"));
    EXPECT(roleAt(m, 1, MappingListModel::InputRole) == QStringLiteral("n"));

    // The own file now holds only 'a' -> [A]: the overlay was never written in.
    {
        MappingListModel check;
        check.setProfileFile(QStringLiteral("profiles/base.txt"));
        EXPECT(check.rowCount() == 1);
        EXPECT(roleAt(check, 0, MappingListModel::InputRole) ==
               QStringLiteral("a"));
        EXPECT(roleAt(check, 0, MappingListModel::OutputRole) ==
               QStringLiteral("A"));
    }

    std::fprintf(stderr, "testmappingcompose: all passed\n");
    return 0;
}
