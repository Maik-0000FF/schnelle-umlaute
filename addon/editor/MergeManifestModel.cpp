#include "MergeManifestModel.h"
#include "FcitxReload.h"
#include "editor_paths.h"
#include "profile_paths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

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
    if (!manifest_.base.empty() && f == manifest_.base)
        return 0;
    for (size_t i = 0; i < manifest_.sources.size(); ++i)
        if (manifest_.sources[i] == f)
            return static_cast<int>(i) + 1;
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

void MergeManifestModel::toggleMerge(const QString &file) {
    if (file.isEmpty() || !isSafe(file))
        return;
    const std::string f = file.toStdString();
    if (manifest_.base.empty()) {
        manifest_.base = f; // first pick becomes the base
    } else if (f == manifest_.base) {
        manifest_ = schnelle_umlaute::MergeManifest{}; // base clicked: dissolve
    } else {
        auto &src = manifest_.sources;
        auto it = std::find(src.begin(), src.end(), f);
        if (it != src.end())
            src.erase(it); // toggle an appended source off
        else
            src.push_back(f); // append in click order
    }
    pruneOrder();
    save();
}

void MergeManifestModel::onProfileRemoved(const QString &file) {
    const std::string f = file.toStdString();
    bool touched = false;
    if (!manifest_.base.empty() && f == manifest_.base) {
        manifest_ = schnelle_umlaute::MergeManifest{}; // base gone: dissolve
        touched = true;
    } else {
        auto &src = manifest_.sources;
        auto it = std::find(src.begin(), src.end(), f);
        if (it != src.end()) {
            src.erase(it);
            touched = true;
        }
    }
    if (touched) {
        pruneOrder();
        save();
    }
}

void MergeManifestModel::pruneToExisting(const QStringList &existingFiles) {
    std::set<std::string> exist;
    for (const QString &f : existingFiles)
        exist.insert(f.toStdString());
    bool touched = false;
    if (!manifest_.base.empty() && exist.find(manifest_.base) == exist.end()) {
        manifest_ = schnelle_umlaute::MergeManifest{}; // base gone: dissolve
        touched = true;
    } else {
        auto &src = manifest_.sources;
        const auto before = src.size();
        src.erase(std::remove_if(src.begin(), src.end(),
                                 [&exist](const std::string &s) {
                                     return exist.find(s) == exist.end();
                                 }),
                  src.end());
        touched = src.size() != before;
    }
    if (touched) {
        pruneOrder();
        save();
    }
}

void MergeManifestModel::setOrderOverride(
    const std::string &base, std::vector<schnelle_umlaute::Variant> sequence) {
    if (sequence.empty())
        manifest_.order.erase(base);
    else
        manifest_.order[base] = std::move(sequence);
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
