#include "MappingListModel.h"
#include "FcitxReload.h"
#include "mappings-io.h"
#include "profile_paths.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

// Resolve a profile-relative file ("mappings.txt" / "profiles/<slug>.txt") to
// an absolute path under ~/.config/fcitx5/<config subdir>/.
QString resolveProfilePath(const QString &relFile) {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/fcitx5/") +
           QLatin1String(schnelle_umlaute::kConfigSubdir) + QStringLiteral("/") +
           relFile;
}

} // namespace

MappingListModel::MappingListModel(QObject *parent)
    : QAbstractListModel(parent) {
    load();
}

int MappingListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return static_cast<int>(entries_.size());
}

QVariant MappingListModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= static_cast<int>(entries_.size())) {
        return {};
    }
    const auto &e = entries_[index.row()];
    switch (role) {
    case InputRole:
        return e.input;
    case OutputRole:
        return e.output;
    default:
        return {};
    }
}

QHash<int, QByteArray> MappingListModel::roleNames() const {
    return {
        {InputRole, "input"},
        {OutputRole, "output"},
    };
}

bool MappingListModel::isValidInputChar(const QString &input) {
    if (input.isEmpty())
        return false;
    auto ucs4 = input.toUcs4();
    if (ucs4.size() != 1)
        return false;
    uint cp = ucs4[0];
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
    return !output.isEmpty() && isValidOutputChar(output);
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
    if (!isValidOutputChar(output) || output.isEmpty()) {
        Q_EMIT errorOccurred(tr("Output must not contain line breaks"));
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

void MappingListModel::setProfileFile(const QString &file) {
    QString f = file.isEmpty() ? QLatin1String(schnelle_umlaute::kMappingsFile)
                               : file;
    if (f == profileFile_)
        return;
    profileFile_ = f;
    Q_EMIT profileFileChanged();
    // Reload the model from the newly selected edit target. Wrapped in
    // begin/endResetModel so the QML view rebinds to the new rows.
    beginResetModel();
    load();
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
    if (entries_.empty()) {
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
