#include "MergeManifestModel.h"
#include "FcitxReload.h"
#include "editor_paths.h"
#include "profile_paths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QVariantMap>

#include <algorithm>
#include <cstdio>
#include <set>

namespace {

QString mergeConfPath() {
    return schnelle_umlaute::configDirPath() +
           QLatin1String(schnelle_umlaute::kMergeConf);
}

bool isSafe(const QString &file) {
    return schnelle_umlaute::isSafeProfileFile(file.toStdString());
}

} // namespace

MergeManifestModel::MergeManifestModel(QObject *parent) : QObject(parent) {
    load();
}

void MergeManifestModel::load() {
    manifest_ = schnelle_umlaute::MergeManifest{};
    FILE *fp = std::fopen(mergeConfPath().toUtf8().constData(), "r");
    if (fp) {
        manifest_ = schnelle_umlaute::parseMergeManifest(fp);
        std::fclose(fp);
    }
}

QString MergeManifestModel::mergeBase() const {
    return QString::fromStdString(manifest_.base);
}

bool MergeManifestModel::isMergeBase(const QString &file) const {
    return !manifest_.base.empty() && file.toStdString() == manifest_.base;
}

int MergeManifestModel::orderIndex(const QString &file) const {
    const std::string f = file.toStdString();
    const auto refs = composeRefs(); // base first, then appended sources
    for (size_t i = 0; i < refs.size(); ++i)
        if (refs[i] == f)
            return static_cast<int>(i) + 1; // 1-based: position 1 is the base
    return -1;
}

std::vector<std::string> MergeManifestModel::composeRefs() const {
    std::vector<std::string> refs;
    if (manifest_.base.empty())
        return refs;
    refs.push_back(manifest_.base);
    for (const auto &s : manifest_.sources)
        if (s != manifest_.base)
            refs.push_back(s);
    return refs;
}

void MergeManifestModel::setCombinedRefs(const std::vector<std::string> &refs) {
    if (refs.empty()) {
        manifest_ = schnelle_umlaute::MergeManifest{};
    } else {
        manifest_.base = refs.front(); // position 1 is always the base
        manifest_.sources.assign(refs.begin() + 1, refs.end());
    }
    pruneOrder();
    save();
}

void MergeManifestModel::toggleMerge(const QString &file) {
    if (file.isEmpty() || !isSafe(file))
        return;
    const std::string f = file.toStdString();
    auto refs = composeRefs();
    auto it = std::find(refs.begin(), refs.end(), f);
    if (it != refs.end())
        refs.erase(it); // remove; if it was position 1, the next is promoted
    else
        refs.push_back(f); // append in click order
    setCombinedRefs(refs);
}

void MergeManifestModel::onProfileRemoved(const QString &file) {
    const std::string f = file.toStdString();
    auto refs = composeRefs();
    auto it = std::find(refs.begin(), refs.end(), f);
    if (it == refs.end())
        return;
    refs.erase(it);
    setCombinedRefs(refs);
}

void MergeManifestModel::pruneToExisting(const QStringList &existingFiles) {
    std::set<std::string> exist;
    for (const QString &f : existingFiles)
        exist.insert(f.toStdString());
    const auto refs = composeRefs();
    std::vector<std::string> kept;
    kept.reserve(refs.size());
    for (const auto &r : refs)
        if (exist.count(r))
            kept.push_back(r);
    if (kept.size() != refs.size())
        setCombinedRefs(kept);
}

void MergeManifestModel::setOrderOverride(const QString &base,
                                          const QVariantList &sequence) {
    std::vector<schnelle_umlaute::Variant> seq;
    seq.reserve(sequence.size());
    for (const auto &item : sequence) {
        const QVariantMap m = item.toMap();
        seq.push_back(
            {m.value(QStringLiteral("value")).toString().toStdString(),
             m.value(QStringLiteral("file")).toString().toStdString()});
    }
    const std::string b = base.toStdString();
    if (seq.empty())
        manifest_.order.erase(b);
    else
        manifest_.order[b] = std::move(seq);
    save();
}

void MergeManifestModel::pruneOrder() {
    std::set<std::string> valid;
    if (!manifest_.base.empty())
        valid.insert(manifest_.base);
    for (const auto &s : manifest_.sources)
        valid.insert(s);
    for (auto it = manifest_.order.begin(); it != manifest_.order.end();) {
        auto &insts = it->second;
        insts.erase(std::remove_if(insts.begin(), insts.end(),
                                   [&valid](const schnelle_umlaute::Variant &v) {
                                       return valid.find(v.sourceRef) ==
                                              valid.end();
                                   }),
                    insts.end());
        if (insts.empty())
            it = manifest_.order.erase(it);
        else
            ++it;
    }
}

bool MergeManifestModel::save() {
    const QString path = mergeConfPath();
    bool ok = true;
    // Fully dissolved (no base): remove merge.conf so the engine reads
    // "no merge", mirroring how the profile sidecars are deleted when empty.
    if (manifest_.base.empty()) {
        if (QFile::exists(path))
            ok = QFile::remove(path);
    } else {
        const std::string data =
            schnelle_umlaute::serializeMergeManifest(manifest_);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            Q_EMIT errorOccurred(file.errorString());
            return false;
        }
        const QByteArray buf = QByteArray::fromStdString(data);
        if (file.write(buf) != buf.size() || !file.commit()) {
            Q_EMIT errorOccurred(file.errorString());
            return false;
        }
    }
    Q_EMIT manifestChanged();
    // A merge on the active base recomposes only after the engine reloads.
    reloadSchnelleUmlauteAddon();
    return ok;
}
