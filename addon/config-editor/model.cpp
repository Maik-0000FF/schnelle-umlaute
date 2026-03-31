#include "model.h"
#include <QTextStream>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/standardpaths.h>
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
    auto &e = entries_[index.row()];
    if (index.column() == 0) {
        e.input = value.toString();
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
    auto file = StandardPaths::global().open(StandardPathsType::PkgConfig,
                                             "schnelle-umlaute/mappings.txt");
    if (file.isValid()) {
        auto fp = fs::openFD(file, "r");
        if (fp) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), fp.get())) {
                QString line = QString::fromUtf8(buf).trimmed();
                if (line.isEmpty() || line.startsWith('#')) {
                    continue;
                }
                int tab = line.indexOf('\t');
                if (tab > 0) {
                    entries_.push_back(
                        {line.left(tab), line.mid(tab + 1)});
                }
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
    StandardPaths::global().safeSave(
        StandardPathsType::PkgConfig, "schnelle-umlaute/mappings.txt",
        [this](int fd) {
            UnixFD ufd(fd);
            auto fp = fs::openFD(ufd, "wb");
            if (!fp) {
                return false;
            }
            for (const auto &e : entries_) {
                fprintf(fp.get(), "%s\t%s\n",
                        e.input.toUtf8().constData(),
                        e.output.toUtf8().constData());
            }
            return true;
        });
    setNeedSave(false);
}

bool MappingModel::needSave() const { return needSave_; }

void MappingModel::setNeedSave(bool needSave) {
    if (needSave_ != needSave) {
        needSave_ = needSave;
        Q_EMIT needSaveChanged(needSave_);
    }
}

void MappingModel::loadDefaults() {
    entries_ = {
        {"a", "\xc3\xa4"}, {"o", "\xc3\xb6"}, {"u", "\xc3\xbc"},
        {"s", "\xc3\x9f"}, {"A", "\xc3\x84"}, {"O", "\xc3\x96"},
        {"U", "\xc3\x9c"},
    };
}

} // namespace fcitx
