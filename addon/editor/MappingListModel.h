#ifndef SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QChar>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "merge_manifest_io.h"
#include "profile_compose.h"
#include "profile_paths.h"
#include "usage_io.h"
#include "usage_sort.h"

class QFileSystemWatcher;

class MappingListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // The status line's text, translated for display. Anything that has to
    // DECIDE on the status reads saveState instead — see SaveState.
    Q_PROPERTY(QString saveStatus READ saveStatus NOTIFY saveStatusChanged)
    Q_PROPERTY(SaveState saveState READ saveState NOTIFY saveStatusChanged)
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
    // Mirror of the config toggle (set from QML). When on, the chip display is
    // sorted by usage frequency via the shared comparator, so the editor preview
    // matches the runtime cycle order. Non-destructive: the stored order is
    // untouched, and manual reorder is locked while this is on (see QML).
    Q_PROPERTY(bool sortByFrequency READ sortByFrequency WRITE setSortByFrequency
                   NOTIFY sortByFrequencyChanged)
    // Bumped whenever the set of duplicated output values changes, so the chip
    // warning borders re-evaluate. A value counts as duplicated when it occurs
    // more than once across ALL rows (twice in one row = a dead cycle slot, or
    // under two keys = redundancy); the border only informs, never removes.
    Q_PROPERTY(int duplicateRevision READ duplicateRevision NOTIFY
                   duplicatesChanged)
    // Bumped whenever usage.conf is re-read (engine writes it on focus-out), so
    // the frequency preview re-sorts live while the editor stays open instead of
    // only on a manual toggle. The chip display binding depends on this.
    Q_PROPERTY(int usageRevision READ usageRevision NOTIFY usageChanged)
    // True when usage.conf exists and holds counts, so the editor can disable
    // the reset control when there is nothing to reset. Re-evaluated whenever
    // the usage file appears, changes or is removed (usageDataChanged).
    Q_PROPERTY(bool hasUsageData READ hasUsageData NOTIFY usageDataChanged)

public:
    enum Roles {
        InputRole = Qt::UserRole + 1,
        OutputRole,
        // While composing: the row's variants as a list of {value, order, name}
        // maps, one per instance, carrying provenance for the coloured chips.
        ComposedVariantsRole,
    };

    // What last happened to the mappings file. The untranslated fact behind the
    // status line: the displayed text goes through tr(), so the footer used to
    // compare it against the English literals "Saved"/"Loaded" and would have
    // painted its dot as an error under every other locale. It switches on this
    // instead, and saveStatus() derives the text from it, so they cannot drift.
    enum SaveState {
        NoState, // nothing read or written yet
        Loaded,
        Saved,
        OpenFailed,
        WriteFailed,
    };
    Q_ENUM(SaveState)

    explicit MappingListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString saveStatus() const;
    SaveState saveState() const { return saveState_; }

    QString profileFile() const { return profileFile_; }
    void setProfileFile(const QString &file);

    bool composing() const { return composing_; }
    int duplicateRevision() const { return duplicateRevision_; }
    int usageRevision() const { return usageRevision_; }
    bool hasUsageData() const;
    // Request a usage-counter reset: write the one-shot sidecar marker and
    // reload the addon, so the engine (sole owner of the counts) clears them and
    // deletes usage.conf. The preview updates via the usage watcher when the
    // file disappears.
    Q_INVOKABLE void resetUsageCounts();
    // True if this output value occurs more than once across all rows (same row
    // twice, or under two different keys). Chips carry a warning border either
    // way, as an informational cue the user can keep or clean up.
    Q_INVOKABLE bool isDuplicateValue(const QString &value) const {
        return duplicateValues_.contains(value);
    }
    bool sortByFrequency() const { return sortByFrequency_; }
    void setSortByFrequency(bool v);
    // Sort a row's variants by usage frequency (most-used first) for display,
    // using the one shared comparator so the preview matches the runtime cycle.
    // Non-destructive: the input list and the stored order are unchanged.
    Q_INVOKABLE QStringList sortByUsage(const QString &base,
                                        const QStringList &variants) const;
    // Re-read merge.conf and rebuild the composed view. Called from QML when the
    // merge manifest changes (its single owner is MergeManifestModel; this model
    // only reads the file to display the composed result).
    Q_INVOKABLE void reloadComposed();
    // Delete one variant instance from the composed view. An own variant (from
    // the base) is removed from the base's own file; a variant from an appended
    // source cascades into THAT profile's file (its origin). Gated behind a
    // confirm dialog in QML, since it edits a profile you are not looking at.
    Q_INVOKABLE bool removeComposedVariant(const QString &input,
                                           const QString &value,
                                           const QString &file);
    // Move a variant to a different base char WITHIN its source profile (the
    // base's own file or an appended one): removed from `fromInput`'s mapping
    // and added to `toInput`'s, creating that mapping if absent, so the source
    // stays self-contained and profiles never intermix. No-op if fromInput ==
    // toInput. Snackbar-noted in QML (it edits a source profile).
    Q_INVOKABLE bool moveComposedVariant(const QString &fromInput,
                                         const QString &value,
                                         const QString &file,
                                         const QString &toInput);

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
    // whole row) and always appended to toInput, even when that mapping already
    // carries the value: a dragged variant is never silently dropped, and the
    // resulting duplicate is flagged (variantWarning plus the row's warning
    // border) rather than prevented.
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
    void sortByFrequencyChanged();
    void duplicatesChanged();
    void usageChanged();
    void usageDataChanged();

private:
    // Recompute the set of output values that occur more than once across all
    // rows (entries_ normally, displayRows_ while composing) and bump the
    // revision if it changed, so the chip warning borders refresh.
    void recomputeDuplicates();
    // Re-read usage.conf into usageCounts_ (engine-written, editor-read), so the
    // preview sort reflects current usage, and bump usageRevision_ so the chip
    // display re-evaluates. Called on toggle, on composed rebuild, and from the
    // file watcher.
    void reloadUsage();
    // React to usage.conf changing on disk (engine flush): re-read and re-sort
    // the preview live, then re-arm the watch (the engine replaces the file
    // atomically, which drops the old watch). No-op unless the sort is on.
    void onUsageFileChanged();
    // Arm the watch on usage.conf if the file exists and is not already watched.
    void ensureUsageWatch();
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
    // as a compose source. Unsafe or missing files yield an empty map. When
    // inputOrder is given it also receives the base chars in file order, so the
    // composed row order stays deterministic (a map alone would iterate
    // arbitrarily and reshuffle the rows on every rebuild).
    schnelle_umlaute::VariantMap
    loadProfileMap(const QString &relFile,
                   std::vector<std::string> *inputOrder = nullptr) const;
    // Remove one occurrence of value from `input`'s output in the given profile
    // file (dropping the whole mapping if it becomes empty), write it back
    // atomically, and reload the engine. Backs the composed-view cascade delete.
    bool removeVariantFromProfileFile(const QString &relFile,
                                      const QString &input,
                                      const QString &value);
    // Move one occurrence of value from fromInput to toInput in the given
    // profile file (creating toInput's mapping if absent), write it back
    // atomically, and reload the engine. Backs the composed-view cross-row move.
    bool moveVariantInProfileFile(const QString &relFile, const QString &value,
                                  const QString &fromInput,
                                  const QString &toInput);
    static bool isValidInputChar(const QString &input);
    static bool isValidOutputChar(const QString &output);
    bool hasInput(const QString &input, int excludeRow) const;
    void load();
    bool save();
    void setSaveState(SaveState state);

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
    // Preview-sort state. usageCounts_ is re-read from usage.conf (engine-written)
    // so the editor preview matches what the runtime cycle would use.
    bool sortByFrequency_ = false;
    schnelle_umlaute::UsageCounts usageCounts_;
    // Output values occurring more than once across all rows; drives the chip
    // warning borders. Recomputed on every data change (recomputeDuplicates).
    QSet<QString> duplicateValues_;
    int duplicateRevision_ = 0;
    // Watches usage.conf so the frequency preview updates live (see reloadUsage).
    QFileSystemWatcher *usageWatcher_ = nullptr;
    int usageRevision_ = 0;
    // Set once the file watch is first armed, so the first-appearance refresh in
    // ensureUsageWatch runs only for a genuinely new file, not for the re-arm
    // after the engine's atomic rename (which onUsageFileChanged already handled).
    bool usageWatchArmed_ = false;

    SaveState saveState_ = NoState;
    // Relative to ~/.config/fcitx5/<config subdir>/. Default is the Standard
    // profile's legacy file (the editor overrides this to the active profile
    // on startup).
    QString profileFile_ =
        QString::fromLatin1(schnelle_umlaute::kMappingsFile);
};

#endif
