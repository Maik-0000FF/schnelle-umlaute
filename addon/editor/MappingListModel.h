#ifndef SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QChar>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "profile_paths.h"

class MappingListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString saveStatus READ saveStatus NOTIFY saveStatusChanged)
    // Which profile's mappings file this model edits, relative to
    // ~/.config/fcitx5/schnelle-umlaute/ ("mappings.txt" for the Standard
    // profile, "profiles/<slug>.txt" otherwise). This is the EDIT target and
    // is independent of the runtime-active profile: editing a non-active
    // profile writes its file but does not change what the engine is using.
    Q_PROPERTY(QString profileFile READ profileFile WRITE setProfileFile NOTIFY
                   profileFileChanged)
    // Composition inputs (issue #112), set from QML. When the edit target is the
    // active profile and the overlay is non-empty, the displayed rows are the
    // composed effective mapping (own + overlay), not just the own entries.
    // mergeOverlay is the ordered list of profile refs ("profile:<file>"),
    // activeProfileFile the active profile's relative file.
    Q_PROPERTY(QStringList mergeOverlay READ mergeOverlay WRITE setMergeOverlay
                   NOTIFY mergeOverlayChanged)
    Q_PROPERTY(QString activeProfileFile READ activeProfileFile WRITE
                   setActiveProfileFile NOTIFY activeProfileFileChanged)

public:
    enum Roles {
        InputRole = Qt::UserRole + 1,
        OutputRole,
        // "own" (only this profile), "inherited" (only from the overlay), or
        // "merged" (this profile plus overlay contributions).
        SourceRole,
    };

    explicit MappingListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString saveStatus() const { return saveStatus_; }

    QString profileFile() const { return profileFile_; }
    void setProfileFile(const QString &file);

    QStringList mergeOverlay() const { return mergeOverlay_; }
    void setMergeOverlay(const QStringList &refs);
    QString activeProfileFile() const { return activeProfileFile_; }
    void setActiveProfileFile(const QString &file);

    Q_INVOKABLE bool addMapping(const QString &input, const QString &output);
    Q_INVOKABLE void removeMapping(int row);
    Q_INVOKABLE bool updateMapping(int row, const QString &input,
                                   const QString &output);
    Q_INVOKABLE void moveMapping(int from, int to);
    Q_INVOKABLE bool validateInput(const QString &input,
                                   int excludeRow = -1) const;
    Q_INVOKABLE bool validateOutput(const QString &output) const;
    Q_INVOKABLE QString inputErrorFor(const QString &input,
                                      int excludeRow = -1) const;
    Q_INVOKABLE QString outputErrorFor(const QString &output) const;

Q_SIGNALS:
    void countChanged();
    void saveStatusChanged();
    void profileFileChanged();
    void mergeOverlayChanged();
    void activeProfileFileChanged();
    void errorOccurred(const QString &message);

private:
    static bool isValidInputChar(const QString &input);
    static bool isValidOutputChar(const QString &output);
    bool hasInput(const QString &input, int excludeRow) const;
    void load();
    bool save();
    void setSaveStatus(const QString &status);
    // True when the displayed rows should be the composed effective mapping:
    // the edit target is the active profile and the overlay is non-empty.
    bool composing() const;
    // Recompute the displayed rows (display_) from the own entries and, when
    // composing(), the overlay profiles. Own edits and save() always act on
    // entries_ (the own mappings); this only rebuilds what the view shows.
    void rebuildDisplay();

    struct Entry {
        QString input;
        QString output;
    };
    // A displayed row. output is the composed output field; source is "own",
    // "inherited", or "merged"; ownIndex is the row's index into entries_, or
    // -1 for a purely inherited (overlay-only) base that has no own entry.
    struct DisplayRow {
        QString input;
        QString output;
        QString source;
        int ownIndex;
    };
    std::vector<Entry> entries_;
    std::vector<DisplayRow> display_;
    QStringList mergeOverlay_;
    QString activeProfileFile_;
    QString saveStatus_;
    // Relative to ~/.config/fcitx5/<config subdir>/. Default is the Standard
    // profile's legacy file (the editor overrides this to the active profile
    // on startup).
    QString profileFile_ =
        QString::fromLatin1(schnelle_umlaute::kMappingsFile);
};

#endif
