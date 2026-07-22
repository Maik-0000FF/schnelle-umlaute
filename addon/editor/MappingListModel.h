#ifndef SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_MAPPING_LIST_MODEL_H

#include <vector>
#include <QAbstractListModel>
#include <QChar>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "profile_paths.h"

class MappingListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString saveStatus READ saveStatus NOTIFY saveStatusChanged)
    // Which profile's mappings file this model edits, relative to
    // ~/.config/fcitx5/schnelle-umlaute/ ("mappings.txt" for the Standard
    // profile, "profiles/<slug>.txt" otherwise). This is the EDIT target and
    // is independent of the runtime-active profile: editing a non-active
    // profile writes its file but does not change what the engine is using.
    Q_PROPERTY(QString profileFile READ profileFile WRITE setProfileFile NOTIFY
                   profileFileChanged)

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

    QString profileFile() const { return profileFile_; }
    void setProfileFile(const QString &file);

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
    Q_INVOKABLE QString outputErrorFor(const QString &output) const;
    // Remove a single cycling variant from a mapping's output; when the last
    // variant goes, the whole mapping is removed. Comma-escaping is resolved via
    // splitOutputs/joinOutputs, so a variant with a literal comma round-trips.
    Q_INVOKABLE bool removeVariant(const QString &input, const QString &variant);
    // Rewrite a mapping's variants in the given order (drag-reorder). The order
    // must be a permutation of the current variants; a stale drag is rejected.
    Q_INVOKABLE bool setVariantOrder(const QString &input,
                                     const QStringList &order);
    // Move a single variant from one mapping's output to another's (cross-row
    // drag). It is removed from fromInput (dropping its last variant removes the
    // whole row) and appended to toInput unless that mapping already has it.
    Q_INVOKABLE bool moveVariant(const QString &fromInput, const QString &variant,
                                 const QString &toInput);

Q_SIGNALS:
    void countChanged();
    void saveStatusChanged();
    void profileFileChanged();
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
    // Relative to ~/.config/fcitx5/<config subdir>/. Default is the Standard
    // profile's legacy file (the editor overrides this to the active profile
    // on startup).
    QString profileFile_ =
        QString::fromLatin1(schnelle_umlaute::kMappingsFile);
};

#endif
