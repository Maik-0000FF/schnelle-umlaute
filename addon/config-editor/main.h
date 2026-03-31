#ifndef SCHNELLE_UMLAUTE_CONFIG_EDITOR_MAIN_H
#define SCHNELLE_UMLAUTE_CONFIG_EDITOR_MAIN_H

#include <fcitxqtconfiguiplugin.h>

namespace fcitx {

class MappingEditorPlugin : public FcitxQtConfigUIPlugin {
    Q_OBJECT
public:
    Q_PLUGIN_METADATA(IID FcitxQtConfigUIFactoryInterface_iid FILE
                      "mapping-editor.json")
    explicit MappingEditorPlugin(QObject *parent = nullptr);
    FcitxQtConfigUIWidget *create(const QString &key) override;
};

} // namespace fcitx

#endif
