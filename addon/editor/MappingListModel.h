#ifndef SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QChar>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "merge_manifest_io.h"
#include "profile_compose.h"
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
    // True when the edit target is the merge base, so the list shows composed
    // rows (per-chip provenance) instead of the plain own mappings. The merge
    // manifest is owned by MergeManifestModel; this model only READS merge.conf
    // to display the composed result (writes still go through that owner).
    Q_PROPERTY(bool composing READ composing NOTIFY composingChanged)

public:
    enum Roles {
        InputRole = Qt::UserRole + 1,
        OutputRole,
        // While composing: the row's variants as a list of {value, order, name}
        // maps, one per instance, carrying provenance for the coloured chips.
        ComposedVariantsRole,
    };

    explicit MappingListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString saveStatus() const { return saveStatus_; }

    QString profileFile() const { return profileFile_; }
    void setProfileFile(const QString &file);

    bool composing() const { return composing_; }
    // Re-read merge.conf and rebuild the composed view. Called from QML when the
    // merge manifest changes (its single owner is MergeManifestModel; this model
    // only reads the file to display the composed result).
    Q_INVOKABLE void reloadComposed();

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
    // Remove a single cycling variant from a mapping's output. Removing the sole
    // variant is refused (a mapping keeps at least one output; delete the whole
    // mapping with the trash button). Comma-escaping is resolved via
    // splitOutputs/joinOutputs, so a variant with a literal comma round-trips.
    Q_INVOKABLE bool removeVariant(const QString &input, const QString &variant);
    // Rewrite a mapping's variants in the given order (drag-reorder). The order
    // must be a permutation of the current variants; a stale drag is rejected.
    Q_INVOKABLE bool setVariantOrder(const QString &input,
                                     const QStringList &order);
    // Move a single variant from one mapping's output to another's (cross-row
    // drag). It is removed from fromInput (dropping its last variant removes the
    // whole row) and appended to toInput unless that mapping already has it.
    Q_INVOKABLE bool moveVariant(const QString &fromInput, const QString &variant,
                                 const QString &toInput);

Q_SIGNALS:
    void countChanged();
    void saveStatusChanged();
    void profileFileChanged();
    void errorOccurred(const QString &message);
    // A non-blocking hint (not an error): the action was applied but is worth
    // flagging, e.g. a chip dropped onto a row that already has that variant,
    // which the engine cycles through twice (a dead slot). The row also shows a
    // warning border via MappingRow's duplicate check.
    void variantWarning(const QString &message);
    void composingChanged();

private:
    // Re-read merge.conf into manifest_, recompute composing_ (edit target is
    // the base), and refresh displayRows_ without emitting a reset (the caller
    // wraps this in begin/endResetModel). setProfileFile and reloadComposed both
    // funnel through here.
    void refreshComposedState();
    // Build displayRows_ by composing the base's own mappings (entries_) with
    // the appended source profiles, tagging each variant with its provenance
    // (value + 1-based position + source File; the name is resolved in QML).
    void rebuildComposed();
    // Load a profile file's mappings into a flat base -> variants map, for use
    // as a compose source. Unsafe or missing files yield an empty map.
    schnelle_umlaute::VariantMap loadProfileMap(const QString &relFile) const;
    static bool isValidInputChar(const QString &input);
    static bool isValidOutputChar(const QString &output);
    bool hasInput(const QString &input, int excludeRow) const;
    void load();
    bool save();
    void setSaveStatus(const QString &status);

    struct Entry {
        QString input;
        QString output;
    };
    std::vector<Entry> entries_;

    // One composed row: the base char plus its ordered variant instances, each
    // a {value, order, name} map (provenance for the coloured chips). Populated
    // only while composing_.
    struct DisplayRow {
        QString input;
        QVariantList variants;
    };
    std::vector<DisplayRow> displayRows_;
    bool composing_ = false;
    // The current merge.conf, re-read on every composed rebuild. This model is a
    // READER only; MergeManifestModel remains the single writer.
    schnelle_umlaute::MergeManifest manifest_;

    QString saveStatus_;
    // Relative to ~/.config/fcitx5/<config subdir>/. Default is the Standard
    // profile's legacy file (the editor overrides this to the active profile
    // on startup).
    QString profileFile_ =
        QString::fromLatin1(schnelle_umlaute::kMappingsFile);
};

#endif
