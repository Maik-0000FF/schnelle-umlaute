#ifndef SCHNELLE_UMLAUTE_EDITOR_PROFILE_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_PROFILE_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QFileSystemWatcher;

// Manages the mapping profiles stored in
// ~/.config/fcitx5/schnelle-umlaute/profiles.conf. A profile maps a display
// Name to a relative File ("mappings.txt" for the protected Standard profile,
// "profiles/<slug>.txt" otherwise) plus an optional SelectKey shortcut. The
// "active" profile is the one the engine loads at runtime; it is independent
// of which profile the MappingListModel currently edits.
//
// This model owns profiles.conf exclusively (kept out of schnelle-umlaute.conf,
// which SettingsModel fully rewrites). The file format is the fcitx INI the
// engine's ProfilesConfig reads: top-level Active/CycleNext/CyclePrev plus
// [Profiles/<i>] sections with Name/File/SelectKey/Favorite. The editor does
// not link fcitx-config, so it reads/writes that format by hand; the key names
// and True/False bool spelling must stay in sync with the engine's
// FCITX_CONFIGURATION in config.h (see the CONTRACT note there).
class ProfileListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString cycleNext READ cycleNext WRITE setCycleNext NOTIFY
                   cycleNextChanged)
    Q_PROPERTY(QString cyclePrev READ cyclePrev WRITE setCyclePrev NOTIFY
                   cyclePrevChanged)
    // Bumped on every persisted change; lets QML combos that build a plain
    // name list rebind when profiles are added/renamed/removed.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        FileRole,
        SelectKeyRole,
        IsActiveRole,
        IsProtectedRole,
        FavoriteRole,
    };

    explicit ProfileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString active() const { return active_; }
    int revision() const { return revision_; }
    // Profile display names in row order, for plain QML combo models.
    Q_INVOKABLE QStringList profileNames() const;
    QString cycleNext() const { return cycleNext_; }
    QString cyclePrev() const { return cyclePrev_; }
    void setCycleNext(const QString &combo);
    void setCyclePrev(const QString &combo);

    // Relative File of a row, for binding the MappingListModel edit target.
    Q_INVOKABLE QString fileForRow(int row) const;
    Q_INVOKABLE int activeRow() const;

    Q_INVOKABLE bool createProfile(const QString &name);
    // Bundled-preset library: lists the read-only presets shipped with the app
    // (each {file, name, description, count}) for the editor's "add from library"
    // picker, and copies a chosen one into the user's profiles/<slug>.txt then
    // registers it. The copy decouples the user's profile from the bundled
    // template, so later customizations survive app updates.
    Q_INVOKABLE QVariantList availablePresets() const;
    Q_INVOKABLE bool addProfileFromPreset(const QString &presetFile);
    Q_INVOKABLE bool renameProfile(int row, const QString &name);
    Q_INVOKABLE bool removeProfile(int row);
    Q_INVOKABLE bool setActiveRow(int row);
    Q_INVOKABLE bool setSelectKey(int row, const QString &combo);
    Q_INVOKABLE bool setFavorite(int row, bool favorite);

    // Validation helpers for the QML UI (mirrors MappingListModel::inputErrorFor
    // / hasInput). Empty return == valid. excludeRow skips a row (for rename).
    Q_INVOKABLE QString nameErrorFor(const QString &name,
                                     int excludeRow = -1) const;
    // Standard and the last remaining profile are protected from deletion.
    Q_INVOKABLE bool isProtected(int row) const;

Q_SIGNALS:
    void countChanged();
    void activeChanged();
    void cycleNextChanged();
    void cyclePrevChanged();
    void changed();
    // Emitted after a profile is deleted, carrying its (now removed) relative
    // file. Lets the Mappings edit target reset if it was pointing at it.
    void profileRemoved(const QString &file);
    void errorOccurred(const QString &message);

private:
    struct Entry {
        QString name;
        QString file;
        QString selectKey;
        bool favorite = false;
    };

    static QString normalizedName(const QString &name);
    static QString slugify(const QString &name);
    bool nameExists(const QString &name, int excludeRow) const;
    bool fileExists(const QString &file, int excludeRow) const;
    // Which cycle slot to skip in the duplicate check, so re-setting a slot
    // does not collide with itself.
    enum class CycleSlot { None, Next, Prev };
    // True if combo is unused by any other shortcut. excludeRow skips one
    // profile's SelectKey; excludeCycle skips CycleNext/CyclePrev. Empty == free.
    bool isComboFree(const QString &combo, int excludeRow,
                     CycleSlot excludeCycle) const;
    // Canonical form for comparing combos the way the engine matches them:
    // modifiers in a fixed order, a single letter upper-cased. Lets the
    // duplicate check treat "Alt+Control+j" and "Control+Alt+J" as the same.
    static QString canonicalCombo(const QString &combo);
    QString uniqueSlugFile(const QString &name) const;
    // Like uniqueSlugFile but takes an already-slugified base (e.g. a preset's
    // filename), suffixing -2, -3 … on collision. uniqueSlugFile slugifies a
    // display name and delegates here.
    QString uniqueFileForBase(const QString &base) const;
    // A display name not yet taken, suffixing " 2", " 3" … on collision. Used
    // when auto-registering a loose file whose derived name clashes.
    QString uniqueName(const QString &base) const;
    // Scans the user's profiles/ dir and appends an entry for every loose,
    // not-yet-registered *.txt (drop-in profiles), deriving its name from the
    // file. Returns how many were added; the caller persists if > 0. Mutates
    // entries_ directly (called only from load(), before any view binds).
    int registerLooseProfiles();

    static QString escapeValue(const QString &s);
    static QString unescapeValue(const QString &s);
    static bool isSafeProfileFile(const QString &file);
    void load();
    bool save();
    // Re-read just the Active name from disk before a mutating save, so a
    // profile switched at runtime by the engine (shortcut) is not clobbered by
    // the editor's stale in-memory active_.
    void reloadActiveFromDisk();
    void seedStandardIfEmpty(bool persist);
    // Re-read the active profile when profiles.conf changes on disk, so the
    // active marker follows a runtime profile switch the engine made via a
    // shortcut (not just the editor's own dropdown actions).
    void onProfilesConfChanged();

    std::vector<Entry> entries_;
    QString active_;
    QString cycleNext_;
    QString cyclePrev_;
    int revision_ = 0;
    // Watches profiles.conf for external writes (the engine persists Active=
    // on every shortcut switch). Owned via the QObject parent.
    QFileSystemWatcher *confWatcher_ = nullptr;
};

#endif
