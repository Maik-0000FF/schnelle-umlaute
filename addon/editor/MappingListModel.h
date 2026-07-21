#ifndef SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QChar>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "profile_compose.h" // OverrideLayer (composed-view override storage)
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

    // Composed-view mutations (issue #112), valid only while composing(). They
    // tune the merged result without touching any source profile: an own
    // variant is edited in this profile's own .txt, an inherited one via the
    // active profile's merge-override sidecar.
    // Remove one variant from a base's composed output.
    Q_INVOKABLE bool removeComposedVariant(const QString &input,
                                           const QString &variant);
    // Arrange a base's composed variants in the given order (stored as an
    // order override; new upstream variants still append).
    Q_INVOKABLE bool setComposedOrder(const QString &input,
                                      const QStringList &order);
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
    // The active profile's merge-override sidecar: same path as its .txt with
    // ".txt" swapped for ".merge". Load/serialize the per-base remove/order ops.
    QString sidecarPath() const;
    schnelle_umlaute::OverrideLayer loadSidecar() const;
    // Write the sidecar (or delete it when the layer is empty) and reload the
    // addon so the change takes effect at runtime.
    void saveSidecar(const schnelle_umlaute::OverrideLayer &layer);
    // Whether any overlay source provides `variant` for `base` (i.e. it is an
    // inherited variant, removed via the sidecar rather than the own .txt).
    bool inheritedHasVariant(const std::string &base,
                             const std::string &variant) const;

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
