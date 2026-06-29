#ifndef SCHNELLE_UMLAUTE_EDITOR_PROFILE_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_PROFILE_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

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
// [Profiles/<i>] sections with Name/File/SelectKey. The editor does not link
// fcitx-config, so it reads/writes that format by hand.
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
    Q_INVOKABLE bool renameProfile(int row, const QString &name);
    Q_INVOKABLE bool removeProfile(int row);
    Q_INVOKABLE bool setActiveRow(int row);
    Q_INVOKABLE bool setSelectKey(int row, const QString &combo);

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
    };

    static QString normalizedName(const QString &name);
    static QString slugify(const QString &name);
    bool nameExists(const QString &name, int excludeRow) const;
    bool fileExists(const QString &file, int excludeRow) const;
    QString uniqueSlugFile(const QString &name) const;

    static QString escapeValue(const QString &s);
    static QString unescapeValue(const QString &s);
    static bool isSafeProfileFile(const QString &file);
    void load();
    bool save();
    void seedStandardIfEmpty(bool persist);

    std::vector<Entry> entries_;
    QString active_;
    QString cycleNext_;
    QString cyclePrev_;
    int revision_ = 0;
};

#endif
