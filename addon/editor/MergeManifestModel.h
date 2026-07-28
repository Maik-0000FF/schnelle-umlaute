#ifndef SCHNELLE_UMLAUTE_EDITOR_MERGE_MANIFEST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MERGE_MANIFEST_MODEL_H

#include <string>
#include <vector>

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "merge_manifest_io.h"

// Sole authority over merge.conf. Both UI models talk to this one object so the
// manifest has a single in-memory copy and a single writer. Two independent
// read-modify-write owners would lose updates: each would hold a stale copy of
// the other's slice and clobber it on write, even without real concurrency
// (Qt is single-threaded). The shared serializer only unifies the format, not
// the coherence of two in-memory states, so the manifest lives here once.
//
// Slices: ProfileListModel / ProfileSelector drive the STRUCTURE (the chosen
// base plus the appended sources in click order); MappingListModel drives the
// CONTENT (the per-base order overrides) and the composed view. The engine only
// reads the file (composes at runtime), so it is not a writer.
class MergeManifestModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The base profile's File ("" when no merge is configured). QML binds the
    // base badge and the composed view to manifestChanged.
    Q_PROPERTY(QString mergeBase READ mergeBase NOTIFY manifestChanged)

public:
    explicit MergeManifestModel(QObject *parent = nullptr);

    QString mergeBase() const;

    // --- Structure slice (ProfileSelector) ---------------------------------
    Q_INVOKABLE bool isMergeBase(const QString &file) const;
    // Position in the merge, 1-based: 1 = base (applied first), 2..N = the rest
    // in click order, -1 = not in the merge. There is no special base to manage:
    // whichever profile sits at position 1 is the base, and removing it promotes
    // the next. Also the provenance colour index (position 1 -> first colour).
    Q_INVOKABLE int orderIndex(const QString &file) const;
    // Click a profile's merge control: a profile not in the merge is appended as
    // a source in click order (the first clicked becomes the base); a profile
    // already in the merge is removed. Removing the base (position 1) promotes
    // the next source to base; the merge dissolves only when the removed profile
    // was the last remaining ref.
    Q_INVOKABLE void toggleMerge(const QString &file);

    // --- Lifecycle (driven from the profile list) --------------------------
    // A deleted base dissolves the merge; a deleted appended source is pruned.
    // Keyed on File, which is stable across rename, so rename needs nothing.
    Q_INVOKABLE void onProfileRemoved(const QString &file);
    // Drop merge refs (base or sources) that name no existing profile, so a
    // profile deleted while the editor was closed can't leave a dangling merge.
    Q_INVOKABLE void pruneToExisting(const QStringList &existingFiles);

    // The ordered source refs for composing: base first, then appended sources.
    // Empty when no merge is configured.
    std::vector<std::string> composeRefs() const;
    // Replace one base char's order override with the given instance sequence
    // (from a composed-view reorder) and persist. Each element is a {value,
    // file} map; an empty sequence clears the override.
    Q_INVOKABLE void setOrderOverride(const QString &base,
                                      const QVariantList &sequence);

Q_SIGNALS:
    void manifestChanged();
    void errorOccurred(const QString &message);

private:
    void load();
    bool save();
    // save(), plus the rollback every mutation needs: the setters change
    // manifest_ first and persist after, so a failed write would otherwise
    // leave the editor showing a merge that is not on disk and that the engine
    // never sees. Re-reading the file puts the two back in step (save() has
    // already reported the failure to the user by then).
    void persist();
    // The single structural mutation route: rebuild base + sources from one
    // ordered ref list (element 0 is the base), prune stale order entries, and
    // persist. Every add/remove/prune funnels through here, so "position 1 is
    // the base" always holds and removing the base just promotes the next
    // profile rather than dissolving the merge.
    void setCombinedRefs(const std::vector<std::string> &refs);
    // Drop order-override instances whose source ref is no longer part of the
    // merge (base + sources), and empty base entries. Cheap tidy-up on any
    // structure change; the compose is self-healing regardless.
    void pruneOrder();

    schnelle_umlaute::MergeManifest manifest_;
};

#endif
