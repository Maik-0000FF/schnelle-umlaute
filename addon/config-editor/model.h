#ifndef SCHNELLE_UMLAUTE_CONFIG_EDITOR_MODEL_H
#define SCHNELLE_UMLAUTE_CONFIG_EDITOR_MODEL_H

#include <QAbstractTableModel>
#include <QString>
#include <vector>

namespace fcitx {

class MappingModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit MappingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    void load();
    void save();
    QModelIndex addItem(const QString &input, const QString &output);
    void deleteItem(int row);
    void moveUp(int row);
    void moveDown(int row);
    bool needSave() const;
    bool hasInput(const QString &input, int excludeRow = -1) const;
    static bool isValidInput(const QString &input);

Q_SIGNALS:
    void needSaveChanged(bool);

private:
    void setNeedSave(bool needSave);
    void loadDefaults();

    struct Entry {
        QString input;
        QString output;
    };
    std::vector<Entry> entries_;
    bool needSave_ = false;
};

} // namespace fcitx

#endif
