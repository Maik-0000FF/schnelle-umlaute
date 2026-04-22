#ifndef SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H

#include <QAbstractListModel>
#include <QChar>
#include <QQmlEngine>
#include <QString>
#include <vector>

class MappingListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString saveStatus READ saveStatus NOTIFY saveStatusChanged)

public:
    enum Roles {
        InputRole = Qt::UserRole + 1,
        OutputRole,
    };

    explicit MappingListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString saveStatus() const { return saveStatus_; }

    Q_INVOKABLE bool addMapping(const QString &input, const QString &output);
    Q_INVOKABLE void removeMapping(int row);
    Q_INVOKABLE bool updateMapping(int row, const QString &input,
                                   const QString &output);
    Q_INVOKABLE void moveMapping(int from, int to);
    Q_INVOKABLE bool validateInput(const QString &input,
                                   int excludeRow = -1) const;
    Q_INVOKABLE bool validateOutput(const QString &output) const;
    Q_INVOKABLE QString inputErrorFor(const QString &input,
                                      int excludeRow = -1) const;
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void countChanged();
    void saveStatusChanged();
    void errorOccurred(const QString &message);

private:
    static bool isValidInputChar(const QString &input);
    static bool isValidOutputChar(const QString &output);
    bool hasInput(const QString &input, int excludeRow) const;
    void load();
    bool save();
    void setSaveStatus(const QString &status);

    struct Entry {
        QString input;
        QString output;
    };
    std::vector<Entry> entries_;
    QString saveStatus_;
};

#endif
