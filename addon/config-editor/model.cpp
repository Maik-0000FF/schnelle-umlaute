#include "model.h"
#include "mappings-io.h"
#include <QTextStream>
#include <fcitx-utils/i18n.h>
#if __has_include(<fcitx-utils/standardpaths.h>)
#include <fcitx-utils/standardpaths.h>
#define SU_HAS_NEW_STDPATHS 1
#else
#include <fcitx-utils/standardpath.h>
#define SU_HAS_NEW_STDPATHS 0
#endif
#include <fcitx-utils/unixfd.h>
#include <fcitx-utils/fs.h>
#include <fcntl.h>

namespace fcitx {

MappingModel::MappingModel(QObject *parent) : QAbstractTableModel(parent) {}

int MappingModel::rowCount(const QModelIndex &) const {
    return static_cast<int>(entries_.size());
}

int MappingModel::columnCount(const QModelIndex &) const { return 2; }

QVariant MappingModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 ||
        index.row() >= static_cast<int>(entries_.size())) {
        return {};
    }
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        const auto &e = entries_[index.row()];
        return index.column() == 0 ? e.input : e.output;
    }
    return {};
}

QVariant MappingModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return section == 0 ? _("Input") : _("Output");
    }
    return {};
}

Qt::ItemFlags MappingModel::flags(const QModelIndex &index) const {
    auto f = QAbstractTableModel::flags(index);
    if (index.isValid()) {
        f |= Qt::ItemIsEditable;
    }
    return f;
}

bool MappingModel::setData(const QModelIndex &index, const QVariant &value,
                           int role) {
    if (role != Qt::EditRole || !index.isValid()) {
        return false;
    }
    // Guard against out-of-range indices (e.g. stale edit delegate after row removal).
    if (index.row() < 0 || index.row() >= static_cast<int>(entries_.size())) {
        return false;
    }
    auto &e = entries_[index.row()];
    if (index.column() == 0) {
        QString input = value.toString().trimmed();
        if (!isValidInput(input)) {
            return false;
        }
        if (hasInput(input, index.row())) {
            return false;
        }
        e.input = input;
    } else {
        e.output = value.toString();
    }
    Q_EMIT dataChanged(index, index, {role});
    setNeedSave(true);
    return true;
}

QModelIndex MappingModel::addItem(const QString &input,
                                  const QString &output) {
    int row = static_cast<int>(entries_.size());
    beginInsertRows(QModelIndex(), row, row);
    entries_.push_back({input, output});
    endInsertRows();
    setNeedSave(true);
    return index(row, 0);
}

void MappingModel::deleteItem(int row) {
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    entries_.erase(entries_.begin() + row);
    endRemoveRows();
    setNeedSave(true);
}

void MappingModel::moveUp(int row) {
    if (row <= 0 || row >= static_cast<int>(entries_.size())) {
        return;
    }
    if (!beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1)) {
        return;
    }
    std::swap(entries_[row - 1], entries_[row]);
    endMoveRows();
    setNeedSave(true);
}

void MappingModel::moveDown(int row) {
    if (row < 0 || row + 1 >= static_cast<int>(entries_.size())) {
        return;
    }
    if (!beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2)) {
        return;
    }
    std::swap(entries_[row], entries_[row + 1]);
    endMoveRows();
    setNeedSave(true);
}

void MappingModel::load() {
    beginResetModel();
    entries_.clear();
#if SU_HAS_NEW_STDPATHS
    auto file = StandardPaths::global().open(StandardPathsType::PkgConfig,
                                             "schnelle-umlaute/mappings.txt");
    if (file.isValid()) {
        auto fp = fs::openFD(file, "r");
#else
    auto file = StandardPath::global().open(
        StandardPath::Type::PkgConfig, "schnelle-umlaute/mappings.txt", O_RDONLY);
    if (file.fd() >= 0) {
        auto fp = fs::openFD(file, "r");
#endif
        if (fp) {
            for (const auto &m : schnelle_umlaute::parseMappings(fp.get())) {
                entries_.push_back(
                    {QString::fromStdString(m.input),
                     QString::fromStdString(m.output)});
            }
        }
    }
    if (entries_.empty()) {
        loadDefaults();
    }
    endResetModel();
    setNeedSave(false);
}

void MappingModel::save() {
#if SU_HAS_NEW_STDPATHS
    bool ok = StandardPaths::global().safeSave(
        StandardPathsType::PkgConfig, "schnelle-umlaute/mappings.txt",
#else
    bool ok = StandardPath::global().safeSave(
        StandardPath::Type::PkgConfig, "schnelle-umlaute/mappings.txt",
#endif
        [this](int fd) {
            UnixFD ufd(fd);
            auto fp = fs::openFD(ufd, "wb");
            if (!fp) {
                return false;
            }
            for (const auto &e : entries_) {
                fprintf(fp.get(), "%s=%s\n",
                        e.input.toUtf8().constData(),
                        e.output.toUtf8().constData());
            }
            // Flush buffered writes so ferror() catches late errors
            // (e.g. disk full on the final buffer).
            fflush(fp.get());
            return ferror(fp.get()) == 0;
        });
    if (ok) {
        setNeedSave(false);
    }
}

bool MappingModel::needSave() const { return needSave_; }

void MappingModel::setNeedSave(bool needSave) {
    if (needSave_ != needSave) {
        needSave_ = needSave;
        Q_EMIT needSaveChanged(needSave_);
    }
}

void MappingModel::loadDefaults() {
    entries_.clear();
    for (const auto &m : schnelle_umlaute::defaultMappings()) {
        entries_.push_back(
            {QString::fromStdString(m.input),
             QString::fromStdString(m.output)});
    }
}

bool MappingModel::isValidInput(const QString &input) {
    if (input.isEmpty()) return false;
    // Exactly 1 Unicode codepoint
    auto ucs4 = input.toUcs4();
    if (ucs4.size() != 1) return false;
    // Use UCS-4 codepoint for property checks — QChar only covers BMP,
    // characters above U+FFFF would appear as surrogates and fail isPrint().
    uint cp = ucs4[0];
    return QChar::isPrint(cp) && !QChar::isSpace(cp);
}

bool MappingModel::hasInput(const QString &input, int excludeRow) const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (i == excludeRow) continue;
        if (entries_[i].input == input) return true;
    }
    return false;
}

} // namespace fcitx
