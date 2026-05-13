#ifndef SCHNELLE_UMLAUTE_CONFIG_EDITOR_MODEL_H
#define SCHNELLE_UMLAUTE_CONFIG_EDITOR_MODEL_H

#include <vector>
#include <QAbstractTableModel>
#include <QChar>
#include <QString>

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

    // Accept exactly one printable, non-whitespace Unicode codepoint.
    // Kept inline so standalone unit tests can call it without linking
    // against model.cpp (which depends on fcitx-utils for load/save).
    static bool isValidInput(const QString &input) {
        if (input.isEmpty())
            return false;
        auto ucs4 = input.toUcs4();
        if (ucs4.size() != 1)
            return false;
        // Use UCS-4 codepoint for property checks — QChar only covers BMP,
        // characters above U+FFFF would appear as surrogates and fail
        // isPrint().
        uint cp = ucs4[0];
        return QChar::isPrint(cp) && !QChar::isSpace(cp);
    }

    // Reject embedded line breaks: '\n' is the mappings.txt entry separator
    // (fgets splits on it), so an embedded newline would break one mapping
    // across two parsed lines on reload and silently drop the tail. '\r'
    // would be trimmed at end-of-line and pass through in the middle, also
    // producing confusing results. Tabs, spaces (including leading/trailing),
    // commas and any other printable character remain allowed.
    static bool isValidOutput(const QString &output) {
        return !output.contains(QChar('\n')) && !output.contains(QChar('\r'));
    }

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
