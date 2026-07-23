#include "MappingListModel.h"
#include "FcitxReload.h"
#include "editor_paths.h"
#include "mappings-io.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QVariantMap>

#include <algorithm>
#include <cstdio>
#include <set>

namespace {

// Resolve a profile-relative file ("mappings.txt" / "profiles/<slug>.txt") to
// an absolute path under ~/.config/fcitx5/<config subdir>/.
QString resolveProfilePath(const QString &relFile) {
    return schnelle_umlaute::configDirPath() + relFile;
}

// Read merge.conf via the shared parser (same format the engine and the
// manifest owner use). Absent file yields an empty manifest (no base). This
// model is a reader only; MergeManifestModel remains the single writer.
schnelle_umlaute::MergeManifest readMergeConf() {
    schnelle_umlaute::MergeManifest m;
    const QString path = schnelle_umlaute::configDirPath() +
                         QLatin1String(schnelle_umlaute::kMergeConf);
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        m = schnelle_umlaute::parseMergeManifest(fp);
        std::fclose(fp);
    }
    return m;
}

// Read usage.conf (engine-written per-(base, variant) counters). Absent file
// yields an empty table, so the preview sort is a no-op until usage accrues.
schnelle_umlaute::UsageCounts readUsageConf() {
    schnelle_umlaute::UsageCounts c;
    const QString path = schnelle_umlaute::configDirPath() +
                         QLatin1String(schnelle_umlaute::kUsageFile);
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        c = schnelle_umlaute::parseUsage(fp);
        std::fclose(fp);
    }
    return c;
}

} // namespace

MappingListModel::MappingListModel(QObject *parent)
    : QAbstractListModel(parent) {
    load();
}

int MappingListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    if (composing_)
        return static_cast<int>(displayRows_.size());
    return static_cast<int>(entries_.size());
}

QVariant MappingListModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();
    if (composing_) {
        if (row < 0 || row >= static_cast<int>(displayRows_.size()))
            return {};
        const auto &r = displayRows_[row];
        switch (role) {
        case InputRole:
            return r.input;
        case OutputRole: {
            // A joined fallback for any consumer not using ComposedVariantsRole;
            // the composed chips read the provenance list instead.
            std::vector<std::string> vals;
            vals.reserve(r.variants.size());
            for (const auto &v : r.variants)
                vals.push_back(v.toMap()
                                   .value(QStringLiteral("value"))
                                   .toString()
                                   .toStdString());
            return QString::fromStdString(schnelle_umlaute::joinOutputs(vals));
        }
        case ComposedVariantsRole:
            return r.variants;
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
    case ComposedVariantsRole:
        return QVariantList{};
    default:
        return {};
    }
}

QHash<int, QByteArray> MappingListModel::roleNames() const {
    return {
        {InputRole, "input"},
        {OutputRole, "output"},
        {ComposedVariantsRole, "composedVariants"},
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
    int row = static_cast<int>(entries_.size());
    beginInsertRows(QModelIndex(), row, row);
    entries_.push_back({input, output});
    endInsertRows();
    Q_EMIT countChanged();
    save();
    return true;
}

void MappingListModel::removeMapping(int row) {
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

bool MappingListModel::removeVariant(const QString &input,
                                     const QString &variant) {
    for (int row = 0; row < static_cast<int>(entries_.size()); ++row) {
        if (entries_[row].input != input)
            continue;
        auto vars =
            schnelle_umlaute::splitOutputs(entries_[row].output.toStdString());
        auto it = std::find(vars.begin(), vars.end(), variant.toStdString());
        if (it == vars.end())
            return false;
        if (vars.size() == 1) {
            // Refuse to remove the sole variant: a chip action must never delete
            // the whole mapping and its input. The ✕ is hidden on a single-chip
            // row, but guard here too and say why if it is ever reached.
            Q_EMIT errorOccurred(
                tr("A mapping keeps at least one output; delete the whole "
                   "mapping with the trash button."));
            return false;
        }
        vars.erase(it);
        entries_[row].output =
            QString::fromStdString(schnelle_umlaute::joinOutputs(vars));
        auto idx = index(row);
        Q_EMIT dataChanged(idx, idx, {OutputRole});
        save();
        return true;
    }
    return false;
}

bool MappingListModel::setVariantOrder(const QString &input,
                                       const QStringList &order) {
    for (int row = 0; row < static_cast<int>(entries_.size()); ++row) {
        if (entries_[row].input != input)
            continue;
        std::vector<std::string> next;
        next.reserve(order.size());
        for (const auto &v : order)
            next.push_back(v.toStdString());
        // The new order must be a permutation of the current variants, so a
        // stale drag can never add, drop or alter a variant.
        auto current =
            schnelle_umlaute::splitOutputs(entries_[row].output.toStdString());
        auto a = next;
        auto b = current;
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        if (a != b)
            return false;
        entries_[row].output =
            QString::fromStdString(schnelle_umlaute::joinOutputs(next));
        auto idx = index(row);
        Q_EMIT dataChanged(idx, idx, {OutputRole});
        save();
        return true;
    }
    return false;
}

bool MappingListModel::moveVariant(const QString &fromInput,
                                   const QString &variant,
                                   const QString &toInput) {
    if (fromInput == toInput)
        return false;
    int fromRow = -1;
    int toRow = -1;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].input == fromInput)
            fromRow = i;
        if (entries_[i].input == toInput)
            toRow = i;
    }
    if (fromRow < 0 || toRow < 0)
        return false;
    const std::string var = variant.toStdString();
    auto fromVars =
        schnelle_umlaute::splitOutputs(entries_[fromRow].output.toStdString());
    auto it = std::find(fromVars.begin(), fromVars.end(), var);
    if (it == fromVars.end())
        return false;
    auto toVars =
        schnelle_umlaute::splitOutputs(entries_[toRow].output.toStdString());
    if (std::find(toVars.begin(), toVars.end(), var) != toVars.end()) {
        // The target already carries this variant. This is allowed (the user
        // may deliberately want it), but at runtime it is a dead cycle slot,
        // stepped through twice, so warn instead of silently accepting. The
        // move still proceeds below, adding the duplicate; the row shows a
        // warning border via MappingRow's duplicate check.
        Q_EMIT variantWarning(
            tr("“%1” is now a duplicate in this mapping (a dead cycle slot)")
                .arg(variant));
    }
    if (fromVars.size() == 1) {
        // Refuse to move the sole variant out: it would leave an empty, invalid
        // mapping. The chip snaps back; deleting a mapping is the trash button.
        Q_EMIT errorOccurred(
            tr("A mapping keeps at least one output; delete the whole mapping "
               "with the trash button."));
        return false;
    }
    fromVars.erase(it);
    toVars.push_back(var);
    entries_[toRow].output =
        QString::fromStdString(schnelle_umlaute::joinOutputs(toVars));
    {
        auto idx = index(toRow);
        Q_EMIT dataChanged(idx, idx, {OutputRole});
    }
    entries_[fromRow].output =
        QString::fromStdString(schnelle_umlaute::joinOutputs(fromVars));
    {
        auto idx = index(fromRow);
        Q_EMIT dataChanged(idx, idx, {OutputRole});
    }
    save();
    return true;
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
    // Reload the model from the newly selected edit target. Wrapped in
    // begin/endResetModel so the QML view rebinds to the new rows. Whether the
    // new target is the merge base (composed view) is recomputed here too.
    beginResetModel();
    load();
    refreshComposedState();
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT composingChanged();
}

void MappingListModel::reloadComposed() {
    beginResetModel();
    refreshComposedState();
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT composingChanged();
}

void MappingListModel::reloadUsage() { usageCounts_ = readUsageConf(); }

void MappingListModel::setSortByFrequency(bool v) {
    if (sortByFrequency_ == v)
        return;
    sortByFrequency_ = v;
    reloadUsage(); // fresh counts so the preview reflects current usage
    Q_EMIT sortByFrequencyChanged();
    if (composing_)
        reloadComposed(); // composed rows re-sort inside rebuildComposed
    // Normal rows re-sort via the QML chip binding on sortByFrequency.
}

QStringList MappingListModel::sortByUsage(const QString &base,
                                          const QStringList &variants) const {
    std::vector<std::string> vals;
    vals.reserve(variants.size());
    for (const QString &v : variants)
        vals.push_back(v.toStdString());
    static const std::unordered_map<std::string, long long> kEmpty;
    const auto it = usageCounts_.find(base.toStdString());
    const auto sorted = schnelle_umlaute::sortVariantsByUsage(
        vals, it != usageCounts_.end() ? it->second : kEmpty);
    QStringList out;
    out.reserve(static_cast<int>(sorted.size()));
    for (const auto &s : sorted)
        out << QString::fromStdString(s);
    return out;
}

void MappingListModel::refreshComposedState() {
    manifest_ = readMergeConf();
    composing_ = !manifest_.base.empty() &&
                 profileFile_.toStdString() == manifest_.base;
    if (sortByFrequency_)
        reloadUsage(); // fresh counts for the composed preview sort
    rebuildComposed();
    recomputeDuplicates();
}

void MappingListModel::recomputeDuplicates() {
    std::unordered_map<std::string, int> counts;
    if (composing_) {
        for (const auto &row : displayRows_)
            for (const auto &v : row.variants)
                ++counts[v.toMap()
                             .value(QStringLiteral("value"))
                             .toString()
                             .toStdString()];
    } else {
        for (const auto &e : entries_)
            for (const auto &val :
                 schnelle_umlaute::splitOutputs(e.output.toStdString()))
                ++counts[val];
    }
    QSet<QString> dups;
    for (const auto &kv : counts)
        if (kv.second > 1)
            dups.insert(QString::fromStdString(kv.first));
    if (dups != duplicateValues_) {
        duplicateValues_ = std::move(dups);
        ++duplicateRevision_;
        Q_EMIT duplicatesChanged();
    }
}

schnelle_umlaute::VariantMap
MappingListModel::loadProfileMap(const QString &relFile) const {
    schnelle_umlaute::VariantMap map;
    if (!schnelle_umlaute::isSafeProfileFile(relFile.toStdString()))
        return map;
    const QString path = resolveProfilePath(relFile);
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        for (const auto &m : schnelle_umlaute::parseMappings(fp)) {
            auto outs = schnelle_umlaute::splitOutputs(m.output);
            if (!outs.empty())
                map[m.input] = std::move(outs);
        }
        std::fclose(fp);
    }
    return map;
}

void MappingListModel::rebuildComposed() {
    displayRows_.clear();
    if (!composing_)
        return;

    // The base's own map comes from entries_ (already loaded for the base file).
    schnelle_umlaute::VariantMap ownMap;
    for (const auto &e : entries_)
        ownMap[e.input.toStdString()] =
            schnelle_umlaute::splitOutputs(e.output.toStdString());

    // Combined refs: base first, then appended sources. The 1-based position is
    // both the badge number and the provenance colour index.
    std::vector<std::string> refs;
    refs.push_back(manifest_.base);
    for (const auto &s : manifest_.sources)
        if (s != manifest_.base)
            refs.push_back(s);

    // Load the appended sources once, keep them alive for compose().
    std::vector<schnelle_umlaute::VariantMap> extra;
    extra.reserve(refs.size());
    for (size_t i = 1; i < refs.size(); ++i)
        extra.push_back(loadProfileMap(QString::fromStdString(refs[i])));

    std::vector<schnelle_umlaute::ComposeSource> sources;
    sources.reserve(refs.size());
    sources.push_back({refs[0], &ownMap});
    for (size_t i = 1; i < refs.size(); ++i)
        sources.push_back({refs[i], &extra[i - 1]});

    const auto composed = schnelle_umlaute::compose(sources, manifest_.order);

    auto positionOf = [&refs](const std::string &ref) -> int {
        for (size_t i = 0; i < refs.size(); ++i)
            if (refs[i] == ref)
                return static_cast<int>(i) + 1; // 1-based
        return -1;
    };

    // Row order: the base's own inputs first (in entries_ order), then inputs
    // that appear only in appended sources, in source order.
    std::set<std::string> emitted;
    auto emitRow = [&](const std::string &input) {
        if (!emitted.insert(input).second)
            return;
        const auto it = composed.find(input);
        if (it == composed.end() || it->second.empty())
            return;
        std::vector<schnelle_umlaute::Variant> insts = it->second;
        // Preview sort: reorder the instances by the value's usage via the one
        // shared comparator, so the composed preview matches the runtime cycle.
        // Provenance rides along (occurrence-by-occurrence), so a duplicate from
        // two profiles keeps its colours. The manifest order stays the truth.
        if (sortByFrequency_) {
            std::vector<std::string> vals;
            vals.reserve(insts.size());
            for (const auto &v : insts)
                vals.push_back(v.value);
            static const std::unordered_map<std::string, long long> kEmpty;
            const auto cIt = usageCounts_.find(input);
            const auto sortedVals = schnelle_umlaute::sortVariantsByUsage(
                vals, cIt != usageCounts_.end() ? cIt->second : kEmpty);
            std::vector<schnelle_umlaute::Variant> sorted;
            std::vector<bool> used(insts.size(), false);
            for (const auto &sv : sortedVals)
                for (size_t i = 0; i < insts.size(); ++i)
                    if (!used[i] && insts[i].value == sv) {
                        used[i] = true;
                        sorted.push_back(insts[i]);
                        break;
                    }
            insts = std::move(sorted);
        }
        DisplayRow row;
        row.input = QString::fromStdString(input);
        for (const auto &v : insts) {
            QVariantMap m;
            m.insert(QStringLiteral("value"), QString::fromStdString(v.value));
            m.insert(QStringLiteral("order"), positionOf(v.sourceRef));
            m.insert(QStringLiteral("file"),
                     QString::fromStdString(v.sourceRef));
            row.variants.append(m);
        }
        displayRows_.push_back(std::move(row));
    };
    for (const auto &e : entries_)
        emitRow(e.input.toStdString());
    for (size_t i = 1; i < refs.size(); ++i)
        for (const auto &kv : extra[i - 1])
            emitRow(kv.first);
}

bool MappingListModel::removeVariantFromProfileFile(const QString &relFile,
                                                    const QString &input,
                                                    const QString &value) {
    if (!schnelle_umlaute::isSafeProfileFile(relFile.toStdString()))
        return false;
    const QString path = resolveProfilePath(relFile);
    std::vector<schnelle_umlaute::RawMapping> rows;
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        rows = schnelle_umlaute::parseMappings(fp);
        std::fclose(fp);
    } else {
        return false;
    }
    const std::string in = input.toStdString();
    const std::string val = value.toStdString();
    bool changed = false;
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        if (it->input != in)
            continue;
        auto vars = schnelle_umlaute::splitOutputs(it->output);
        auto vit = std::find(vars.begin(), vars.end(), val);
        if (vit == vars.end())
            return false; // not present; nothing to remove
        vars.erase(vit);
        if (vars.empty())
            rows.erase(it); // last variant gone: drop the whole mapping
        else
            it->output = schnelle_umlaute::joinOutputs(vars);
        changed = true;
        break;
    }
    if (!changed)
        return false;
    // Write back atomically, in the same escaped format as save().
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    QByteArray buf;
    for (const auto &r : rows) {
        if (r.input == "#" || r.input == "\\")
            buf += '\\';
        buf += QByteArray::fromStdString(r.input);
        buf += '=';
        buf += QByteArray::fromStdString(r.output);
        buf += '\n';
    }
    if (file.write(buf) != buf.size() || !file.commit()) {
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    reloadSchnelleUmlauteAddon();
    return true;
}

bool MappingListModel::removeComposedVariant(const QString &input,
                                             const QString &value,
                                             const QString &file) {
    if (!composing_)
        return false;
    if (file.toStdString() == manifest_.base) {
        // Own variant: edit the base's own entries_ directly.
        const std::string val = value.toStdString();
        for (int row = 0; row < static_cast<int>(entries_.size()); ++row) {
            if (entries_[row].input != input)
                continue;
            auto vars = schnelle_umlaute::splitOutputs(
                entries_[row].output.toStdString());
            auto it = std::find(vars.begin(), vars.end(), val);
            if (it == vars.end())
                return false;
            vars.erase(it);
            if (vars.empty())
                entries_.erase(entries_.begin() + row); // whole mapping gone
            else
                entries_[row].output =
                    QString::fromStdString(schnelle_umlaute::joinOutputs(vars));
            save();           // writes the base file + engine reload
            reloadComposed(); // rebuild the composed view from the new entries_
            return true;
        }
        return false;
    }
    // Appended source: cascade into the origin profile's file.
    if (!removeVariantFromProfileFile(file, input, value))
        return false;
    reloadComposed();
    return true;
}

bool MappingListModel::moveVariantInProfileFile(const QString &relFile,
                                                const QString &value,
                                                const QString &fromInput,
                                                const QString &toInput) {
    if (!schnelle_umlaute::isSafeProfileFile(relFile.toStdString()))
        return false;
    const QString path = resolveProfilePath(relFile);
    std::vector<schnelle_umlaute::RawMapping> rows;
    if (FILE *fp = std::fopen(path.toUtf8().constData(), "r")) {
        rows = schnelle_umlaute::parseMappings(fp);
        std::fclose(fp);
    } else {
        return false;
    }
    const std::string from = fromInput.toStdString();
    const std::string to = toInput.toStdString();
    const std::string val = value.toStdString();
    // Remove from the source's fromInput mapping (drop it if it empties).
    bool removed = false;
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        if (it->input != from)
            continue;
        auto vars = schnelle_umlaute::splitOutputs(it->output);
        auto vit = std::find(vars.begin(), vars.end(), val);
        if (vit == vars.end())
            return false;
        vars.erase(vit);
        if (vars.empty())
            rows.erase(it);
        else
            it->output = schnelle_umlaute::joinOutputs(vars);
        removed = true;
        break;
    }
    if (!removed)
        return false;
    // Add to the source's toInput mapping (create if absent). Always added, even
    // if the target already has it: the dragged variant must never be silently
    // dropped, and duplicates are allowed (a warning border flags them).
    bool found = false;
    for (auto &r : rows) {
        if (r.input != to)
            continue;
        auto vars = schnelle_umlaute::splitOutputs(r.output);
        vars.push_back(val);
        r.output = schnelle_umlaute::joinOutputs(vars);
        found = true;
        break;
    }
    if (!found)
        rows.push_back({to, val});
    // Write back atomically, in the same escaped format as save().
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    QByteArray buf;
    for (const auto &r : rows) {
        if (r.input == "#" || r.input == "\\")
            buf += '\\';
        buf += QByteArray::fromStdString(r.input);
        buf += '=';
        buf += QByteArray::fromStdString(r.output);
        buf += '\n';
    }
    if (file.write(buf) != buf.size() || !file.commit()) {
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    reloadSchnelleUmlauteAddon();
    return true;
}

bool MappingListModel::moveComposedVariant(const QString &fromInput,
                                           const QString &value,
                                           const QString &file,
                                           const QString &toInput) {
    if (!composing_ || fromInput == toInput)
        return false;
    if (file.toStdString() == manifest_.base) {
        // The base's own mappings live in entries_; move within them.
        const std::string val = value.toStdString();
        bool removed = false;
        for (int r = 0; r < static_cast<int>(entries_.size()); ++r) {
            if (entries_[r].input != fromInput)
                continue;
            auto vars = schnelle_umlaute::splitOutputs(
                entries_[r].output.toStdString());
            auto it = std::find(vars.begin(), vars.end(), val);
            if (it == vars.end())
                return false;
            vars.erase(it);
            if (vars.empty())
                entries_.erase(entries_.begin() + r);
            else
                entries_[r].output =
                    QString::fromStdString(schnelle_umlaute::joinOutputs(vars));
            removed = true;
            break;
        }
        if (!removed)
            return false;
        bool found = false;
        for (auto &e : entries_) {
            if (e.input != toInput)
                continue;
            auto vars = schnelle_umlaute::splitOutputs(e.output.toStdString());
            vars.push_back(val); // always add; never silently drop the variant
            e.output =
                QString::fromStdString(schnelle_umlaute::joinOutputs(vars));
            found = true;
            break;
        }
        if (!found)
            entries_.push_back({toInput, value});
        save();
        reloadComposed();
        return true;
    }
    if (!moveVariantInProfileFile(file, value, fromInput, toInput))
        return false;
    reloadComposed();
    return true;
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
    recomputeDuplicates();
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
    recomputeDuplicates();
    reloadSchnelleUmlauteAddon();
    return true;
}

void MappingListModel::setSaveStatus(const QString &status) {
    if (saveStatus_ != status) {
        saveStatus_ = status;
        Q_EMIT saveStatusChanged();
    }
}
