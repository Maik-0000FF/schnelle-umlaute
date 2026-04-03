#ifndef SCHNELLE_UMLAUTE_CONFIG_EDITOR_EDITOR_H
#define SCHNELLE_UMLAUTE_CONFIG_EDITOR_EDITOR_H

#include "ui_editor.h"
#include <fcitxqtconfiguiwidget.h>
#include <QLabel>

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
    void clearInputError();

    MappingModel *model_;
    QLabel *inputStatus_;
};

} // namespace fcitx

#endif
