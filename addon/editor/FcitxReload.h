#ifndef SCHNELLE_UMLAUTE_EDITOR_FCITX_RELOAD_H
#define SCHNELLE_UMLAUTE_EDITOR_FCITX_RELOAD_H

#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringLiteral>

inline void reloadSchnelleUmlauteAddon() {
    auto msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.fcitx.Fcitx5"), QStringLiteral("/controller"),
        QStringLiteral("org.fcitx.Fcitx.Controller1"),
        QStringLiteral("ReloadAddonConfig"));
    msg << QStringLiteral("schnelle-umlaute");
    QDBusConnection::sessionBus().send(msg);
}

#endif
