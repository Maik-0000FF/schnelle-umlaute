#include "main.h"
#include "editor.h"

namespace fcitx {

MappingEditorPlugin::MappingEditorPlugin(QObject *parent)
    : FcitxQtConfigUIPlugin(parent) {}

FcitxQtConfigUIWidget *MappingEditorPlugin::create(const QString &key) {
    Q_UNUSED(key);
    return new MappingEditor;
}

} // namespace fcitx
