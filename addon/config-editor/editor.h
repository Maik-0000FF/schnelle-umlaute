#ifndef SCHNELLE_UMLAUTE_CONFIG_EDITOR_EDITOR_H
#define SCHNELLE_UMLAUTE_CONFIG_EDITOR_EDITOR_H

#include <QLabel>
#include <QStringList>
#include <fcitxqtconfiguiwidget.h>
#include "ui_editor.h"

namespace fcitx {

class MappingModel;

class MappingEditor : public FcitxQtConfigUIWidget, public Ui::MappingEditor {
    Q_OBJECT
public:
    explicit MappingEditor(QWidget *parent = nullptr);

    void load() override;
    void save() override;
    QString title() override;
    QString icon() override;
    QSize sizeHint() const override;

protected:
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void addMapping();
    void deleteMapping();
    void itemFocusChanged();

private:
    void showInputError(const QString &msg);
    void showInputWarning(const QString &msg);
    void showOutputError(const QString &msg);
    void clearInputError();
    // Live-revalidate both fields; called from inputEdit and outputEdit
    // textChanged signals so the UI feedback stays in sync with the text.
    // Priority mirrors addMapping(): input errors > output errors > warnings.
    void revalidate();
    void loadLeaderKeys();
    bool isLeaderKeyConflict(const QString &input) const;

    MappingModel *model_;
    QLabel *statusLabel_;
    QStringList leaderKeys_;
};

} // namespace fcitx

#endif
