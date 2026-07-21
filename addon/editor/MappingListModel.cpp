#include "MappingListModel.h"
#include "FcitxReload.h"
#include "editor_paths.h"
#include "mappings-io.h"
#include "merge_override_io.h"
#include "profile_compose.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace {

// Resolve a profile-relative file ("mappings.txt" / "profiles/<slug>.txt") to
// an absolute path under ~/.config/fcitx5/<config subdir>/.
QString resolveProfilePath(const QString &relFile) {
    return schnelle_umlaute::configDirPath() + relFile;
}

// Load an overlay profile's file as an ordered list of (base, variants).
// The order matters for the composed display: overlay-only bases keep the
// source profile's order. Entries that split into zero variants are skipped,
// matching the engine loader.
std::vector<std::pair<std::string, std::vector<std::string>>>
loadOrderedProfile(const QString &relFile) {
    std::vector<std::pair<std::string, std::vector<std::string>>> out;
    const QString path = resolveProfilePath(relFile);
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        for (const auto &m : schnelle_umlaute::parseMappings(fp)) {
            auto vars = schnelle_umlaute::splitOutputs(m.output);
            if (!vars.empty())
                out.emplace_back(m.input, std::move(vars));
        }
        std::fclose(fp);
    }
    return out;
}

} // namespace

MappingListModel::MappingListModel(QObject *parent)
    : QAbstractListModel(parent) {
    load();
}

int MappingListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    // While composing, the view shows the composed rows; otherwise the own
    // entries directly (the unchanged, granular-edit path).
    return composing() ? static_cast<int>(display_.size())
                       : static_cast<int>(entries_.size());
}

QVariant MappingListModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();
    if (composing()) {
        if (row < 0 || row >= static_cast<int>(display_.size()))
            return {};
        const auto &d = display_[row];
        switch (role) {
        case InputRole:
            return d.input;
        case OutputRole:
            return d.output;
        case SourceRole:
            return d.source;
        default:
            return {};
        }
    }
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return {};
    const auto &e = entries_[row];
    switch (role) {
    case InputRole:
        return e.input;
    case OutputRole:
        return e.output;
    case SourceRole:
        return QStringLiteral("own");
    default:
        return {};
    }
}

QHash<int, QByteArray> MappingListModel::roleNames() const {
    return {
        {InputRole, "input"},
        {OutputRole, "output"},
        {SourceRole, "source"},
    };
}

bool MappingListModel::isValidInputChar(const QString &input) {
    if (input.isEmpty())
        return false;
    auto ucs4 = input.toUcs4();
    if (ucs4.size() != 1)
        return false;
    uint cp = ucs4[0];
    // '#' (comment marker) and '\' (escape character) are written escaped by
    // save(), so both round-trip as real input keys and need no rejection here.
    return QChar::isPrint(cp) && !QChar::isSpace(cp);
}

bool MappingListModel::isValidOutputChar(const QString &output) {
    return !output.contains(QChar('\n')) && !output.contains(QChar('\r'));
}

bool MappingListModel::hasInput(const QString &input, int excludeRow) const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (i == excludeRow)
            continue;
        if (entries_[i].input == input)
            return true;
    }
    return false;
}

bool MappingListModel::validateInput(const QString &input,
                                     int excludeRow) const {
    return isValidInputChar(input) && !hasInput(input, excludeRow);
}

bool MappingListModel::validateOutput(const QString &output) const {
    if (output.isEmpty() || !isValidOutputChar(output))
        return false;
    // Reject an output that splits into zero cycling variants (e.g. a lone
    // ","), which the engine would otherwise drop as "no valid outputs",
    // losing the mapping silently. A space is a valid output on purpose (e.g.
    // mapping a key to " " so terminal commands skip shell history), so it
    // survives the split as a one-character variant and stays allowed;
    // whitespace is never trimmed, here or in the engine.
    return !schnelle_umlaute::splitOutputs(output.toStdString()).empty();
}

QString MappingListModel::inputErrorFor(const QString &input,
                                        int excludeRow) const {
    if (input.isEmpty())
        return {};
    if (!isValidInputChar(input)) {
        return tr("Must be a single printable character");
    }
    if (hasInput(input, excludeRow)) {
        return tr("This key is already mapped");
    }
    return {};
}

// Mirror of inputErrorFor for the output field: empty is not an error (the
// Add/Apply button just stays disabled), otherwise explain each way an output
// is rejected so the reason shows live in the editor instead of a generic toast.
QString MappingListModel::outputErrorFor(const QString &output) const {
    if (output.isEmpty())
        return {};
    if (!isValidOutputChar(output)) {
        return tr("Output must not contain line breaks");
    }
    if (schnelle_umlaute::splitOutputs(output.toStdString()).empty()) {
        return tr("Output must have at least one variant (a lone \",\" is empty)");
    }
    return {};
}

bool MappingListModel::addMapping(const QString &input, const QString &output) {
    if (!validateInput(input) || !validateOutput(output)) {
        Q_EMIT errorOccurred(tr("Invalid entry"));
        return false;
    }
    // A new own entry may override an inherited base; validateInput checks
    // uniqueness only against own entries, which is what we want.
    if (composing()) {
        entries_.push_back({input, output});
        Q_EMIT countChanged();
        save();
        rebuildDisplay();
        return true;
    }
    int row = static_cast<int>(entries_.size());
    beginInsertRows(QModelIndex(), row, row);
    entries_.push_back({input, output});
    endInsertRows();
    Q_EMIT countChanged();
    save();
    return true;
}

void MappingListModel::removeMapping(int row) {
    if (composing()) {
        if (row < 0 || row >= static_cast<int>(display_.size()))
            return;
        const int idx = display_[row].ownIndex;
        if (idx < 0) {
            // An inherited (overlay-only) row: removing it is a per-variant
            // override (a later increment); for now, direct it to the source.
            Q_EMIT errorOccurred(
                tr("Inherited from the merge overlay; edit its source profile"));
            return;
        }
        entries_.erase(entries_.begin() + idx);
        Q_EMIT countChanged();
        save();
        rebuildDisplay();
        return;
    }
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return;
    beginRemoveRows(QModelIndex(), row, row);
    entries_.erase(entries_.begin() + row);
    endRemoveRows();
    Q_EMIT countChanged();
    save();
}

bool MappingListModel::updateMapping(int row, const QString &input,
                                     const QString &output) {
    if (composing()) {
        if (row < 0 || row >= static_cast<int>(display_.size()))
            return false;
        const int idx = display_[row].ownIndex;
        if (idx < 0) {
            Q_EMIT errorOccurred(
                tr("Inherited from the merge overlay; edit its source profile"));
            return false;
        }
        if (!isValidInputChar(input) || hasInput(input, idx)) {
            Q_EMIT errorOccurred(inputErrorFor(input, idx));
            return false;
        }
        if (!validateOutput(output)) {
            Q_EMIT errorOccurred(outputErrorFor(output));
            return false;
        }
        entries_[idx].input = input;
        entries_[idx].output = output;
        save();
        rebuildDisplay();
        return true;
    }
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    if (!isValidInputChar(input) || hasInput(input, row)) {
        Q_EMIT errorOccurred(inputErrorFor(input, row));
        return false;
    }
    if (!validateOutput(output)) {
        Q_EMIT errorOccurred(outputErrorFor(output));
        return false;
    }
    entries_[row].input = input;
    entries_[row].output = output;
    auto idx = index(row);
    Q_EMIT dataChanged(idx, idx, {InputRole, OutputRole});
    save();
    return true;
}

void MappingListModel::moveMapping(int from, int to) {
    if (composing()) {
        // In the composed view only own rows reorder (they form the leading
        // block); moving an inherited/merged row is a per-base override (a
        // later step). Map the display rows to own entries and reorder there.
        const int nd = static_cast<int>(display_.size());
        if (from < 0 || from >= nd || to < 0 || to >= nd || from == to)
            return;
        const int fromOwn = display_[from].ownIndex;
        const int toOwn = display_[to].ownIndex;
        if (fromOwn < 0 || toOwn < 0)
            return; // an inherited row is involved
        auto entry = std::move(entries_[fromOwn]);
        entries_.erase(entries_.begin() + fromOwn);
        entries_.insert(entries_.begin() + toOwn, std::move(entry));
        save();
        rebuildDisplay();
        return;
    }
    int n = static_cast<int>(entries_.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    // Qt wants the insertion position in the *original* index space, so an
    // in-place move down needs +1.
    int destRow = (to > from) ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow)) {
        return;
    }
    auto entry = std::move(entries_[from]);
    entries_.erase(entries_.begin() + from);
    entries_.insert(entries_.begin() + to, std::move(entry));
    endMoveRows();
    save();
}

void MappingListModel::setProfileFile(const QString &file) {
    QString f = file.isEmpty() ? QLatin1String(schnelle_umlaute::kMappingsFile)
                               : file;
    // Defense in depth: profileFile is a writable property and resolveProfilePath
    // just concatenates it onto the config dir. Every other profile path goes
    // through the shared isSafeProfileFile rule; apply it here too so a relative
    // or traversing value can never read or write outside the config dir. An
    // unsafe value falls back to the Standard mappings file.
    if (!schnelle_umlaute::isSafeProfileFile(f.toStdString()))
        f = QLatin1String(schnelle_umlaute::kMappingsFile);
    if (f == profileFile_)
        return;
    profileFile_ = f;
    Q_EMIT profileFileChanged();
    // Reload from the new edit target, then rebuild the (possibly composed)
    // display. rebuildDisplay does the model reset and the count signal.
    load();
    rebuildDisplay();
}

void MappingListModel::setMergeOverlay(const QStringList &refs) {
    if (refs == mergeOverlay_)
        return;
    mergeOverlay_ = refs;
    Q_EMIT mergeOverlayChanged();
    rebuildDisplay();
}

void MappingListModel::setActiveProfileFile(const QString &file) {
    if (file == activeProfileFile_)
        return;
    activeProfileFile_ = file;
    Q_EMIT activeProfileFileChanged();
    rebuildDisplay();
}

bool MappingListModel::composing() const {
    return !mergeOverlay_.isEmpty() && !activeProfileFile_.isEmpty() &&
           profileFile_ == activeProfileFile_;
}

void MappingListModel::rebuildDisplay() {
    beginResetModel();
    display_.clear();
    if (composing()) {
        // The edit target's own mappings as a base -> variants map.
        schnelle_umlaute::VariantMap own;
        for (const auto &e : entries_) {
            own[e.input.toStdString()] =
                schnelle_umlaute::splitOutputs(e.output.toStdString());
        }
        // Load each overlay profile (ordered), and build its map for compose.
        std::vector<std::vector<
            std::pair<std::string, std::vector<std::string>>>>
            overlayOrdered;
        std::vector<schnelle_umlaute::VariantMap> overlayMaps;
        for (const QString &ref : mergeOverlay_) {
            static const QString kPrefix = QStringLiteral("profile:");
            if (!ref.startsWith(kPrefix))
                continue;
            const QString relFile = ref.mid(kPrefix.size());
            if (relFile.isEmpty() ||
                !schnelle_umlaute::isSafeProfileFile(relFile.toStdString()))
                continue;
            auto ordered = loadOrderedProfile(relFile);
            schnelle_umlaute::VariantMap m;
            for (const auto &kv : ordered)
                m[kv.first] = kv.second;
            overlayOrdered.push_back(std::move(ordered));
            overlayMaps.push_back(std::move(m));
        }
        std::vector<const schnelle_umlaute::VariantMap *> sources;
        sources.push_back(&own);
        for (const auto &m : overlayMaps)
            sources.push_back(&m);
        // Apply the active profile's merge-override sidecar (remove/order/
        // removed ops) so the composed view matches what the engine produces.
        schnelle_umlaute::VariantMap effective =
            schnelle_umlaute::compose(sources, loadSidecar());

        auto overlayHas = [&](const std::string &base) {
            for (const auto &m : overlayMaps) {
                if (m.count(base))
                    return true;
            }
            return false;
        };
        std::unordered_set<std::string> emitted;
        // Own bases first, in the edit target's own order.
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            const std::string base = entries_[i].input.toStdString();
            auto it = effective.find(base);
            if (it == effective.end())
                continue;
            const QString out =
                QString::fromStdString(schnelle_umlaute::joinOutputs(it->second));
            const QString src = overlayHas(base) ? QStringLiteral("merged")
                                                 : QStringLiteral("own");
            display_.push_back({entries_[i].input, out, src, i});
            emitted.insert(base);
        }
        // Then overlay-only bases, in overlay order.
        for (const auto &ordered : overlayOrdered) {
            for (const auto &kv : ordered) {
                if (emitted.count(kv.first))
                    continue;
                auto it = effective.find(kv.first);
                if (it == effective.end())
                    continue;
                const QString out = QString::fromStdString(
                    schnelle_umlaute::joinOutputs(it->second));
                display_.push_back({QString::fromStdString(kv.first), out,
                                    QStringLiteral("inherited"), -1});
                emitted.insert(kv.first);
            }
        }
    }
    endResetModel();
    Q_EMIT countChanged();
}

void MappingListModel::load() {
    entries_.clear();
    QString path = resolveProfilePath(profileFile_);
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        for (const auto &m : schnelle_umlaute::parseMappings(fp)) {
            entries_.push_back({QString::fromStdString(m.input),
                                QString::fromStdString(m.output)});
        }
        std::fclose(fp);
    }
    // The German defaults are seeded only for the Standard profile (the
    // first-install convenience). A freshly created profile loads empty, so the
    // user fills it from scratch instead of inheriting the umlaut set. Mirrors
    // the engine loader's fallback rule.
    if (entries_.empty() &&
        schnelle_umlaute::isStandardProfile(profileFile_.toStdString())) {
        for (const auto &m : schnelle_umlaute::defaultMappings()) {
            entries_.push_back({QString::fromStdString(m.input),
                                QString::fromStdString(m.output)});
        }
    }
    setSaveStatus(tr("Loaded"));
}

bool MappingListModel::save() {
    QString path = resolveProfilePath(profileFile_);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setSaveStatus(tr("Open failed"));
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    QByteArray buf;
    for (const auto &e : entries_) {
        // Escape an input the plain parser would otherwise misread: '#' starts a
        // comment line, '\' is the escape character itself. parseMappings reads
        // "\#=..." / "\\=..." back to the literal key.
        if (e.input == QStringLiteral("#") || e.input == QStringLiteral("\\"))
            buf += '\\';
        buf += e.input.toUtf8();
        buf += '=';
        buf += e.output.toUtf8();
        buf += '\n';
    }
    if (file.write(buf) != buf.size() || !file.commit()) {
        setSaveStatus(tr("Write failed"));
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    setSaveStatus(tr("Saved"));
    reloadSchnelleUmlauteAddon();
    return true;
}

void MappingListModel::setSaveStatus(const QString &status) {
    if (saveStatus_ != status) {
        saveStatus_ = status;
        Q_EMIT saveStatusChanged();
    }
}

QString MappingListModel::sidecarPath() const {
    QString rel = profileFile_;
    if (rel.endsWith(QStringLiteral(".txt")))
        rel = rel.left(rel.size() - 4) + QStringLiteral(".merge");
    else
        rel += QStringLiteral(".merge");
    return resolveProfilePath(rel);
}

schnelle_umlaute::OverrideLayer MappingListModel::loadSidecar() const {
    schnelle_umlaute::OverrideLayer layer;
    if (FILE *fp = std::fopen(sidecarPath().toUtf8().constData(), "r")) {
        layer = schnelle_umlaute::parseMergeOverride(fp);
        std::fclose(fp);
    }
    return layer;
}

void MappingListModel::saveSidecar(
    const schnelle_umlaute::OverrideLayer &layer) {
    const QString path = sidecarPath();
    const std::string text = schnelle_umlaute::serializeMergeOverride(layer);
    if (text.empty()) {
        // No overrides left: drop the sidecar so the config stays clean.
        QFile::remove(path);
        reloadSchnelleUmlauteAddon();
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT errorOccurred(file.errorString());
        return;
    }
    const QByteArray buf = QByteArray::fromStdString(text);
    if (file.write(buf) != buf.size() || !file.commit()) {
        Q_EMIT errorOccurred(file.errorString());
        return;
    }
    reloadSchnelleUmlauteAddon();
}

bool MappingListModel::inheritedHasVariant(const std::string &base,
                                           const std::string &variant) const {
    for (const QString &ref : mergeOverlay_) {
        static const QString kPrefix = QStringLiteral("profile:");
        if (!ref.startsWith(kPrefix))
            continue;
        const QString file = ref.mid(kPrefix.size());
        if (file.isEmpty() ||
            !schnelle_umlaute::isSafeProfileFile(file.toStdString()))
            continue;
        for (const auto &kv : loadOrderedProfile(file)) {
            if (kv.first == base) {
                for (const auto &v : kv.second) {
                    if (v == variant)
                        return true;
                }
            }
        }
    }
    return false;
}

bool MappingListModel::removeComposedVariant(const QString &input,
                                             const QString &variant) {
    if (!composing())
        return false;
    const std::string base = input.toStdString();
    const std::string var = variant.toStdString();
    bool changed = false;
    // If it is an own variant, drop it from this profile's own .txt entry.
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].input != input)
            continue;
        auto vars =
            schnelle_umlaute::splitOutputs(entries_[i].output.toStdString());
        auto it = std::find(vars.begin(), vars.end(), var);
        if (it != vars.end()) {
            vars.erase(it);
            if (vars.empty())
                entries_.erase(entries_.begin() + i); // no own variants left
            else
                entries_[i].output = QString::fromStdString(
                    schnelle_umlaute::joinOutputs(vars));
            save(); // rewrites the own .txt
            changed = true;
        }
        break;
    }
    // If it is (also) inherited, record a remove op so the overlay copy drops.
    if (inheritedHasVariant(base, var)) {
        auto layer = loadSidecar();
        auto &rem = layer.perBase[base].remove;
        if (std::find(rem.begin(), rem.end(), var) == rem.end()) {
            rem.push_back(var);
            saveSidecar(layer);
            changed = true;
        }
    }
    if (changed)
        rebuildDisplay();
    return changed;
}

bool MappingListModel::setComposedOrder(const QString &input,
                                        const QStringList &order) {
    if (!composing())
        return false;
    auto layer = loadSidecar();
    std::vector<std::string> ord;
    ord.reserve(order.size());
    for (const QString &v : order)
        ord.push_back(v.toStdString());
    layer.perBase[input.toStdString()].order = std::move(ord);
    saveSidecar(layer);
    rebuildDisplay();
    return true;
}
