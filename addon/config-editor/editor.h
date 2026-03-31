#ifndef SCHNELLE_UMLAUTE_CONFIG_EDITOR_EDITOR_H
#define SCHNELLE_UMLAUTE_CONFIG_EDITOR_EDITOR_H

#include "ui_editor.h"
#include <fcitxqtconfiguiwidget.h>

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

private Q_SLOTS:
    void addMapping();
    void deleteMapping();
    void itemFocusChanged();

private:
    MappingModel *model_;
};

} // namespace fcitx

#endif
