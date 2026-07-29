#ifndef SCHNELLE_UMLAUTE_EDITOR_FCITX_RELOAD_H
#define SCHNELLE_UMLAUTE_EDITOR_FCITX_RELOAD_H

#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringLiteral>

inline void reloadAddonConfig(const QString &addon) {
    auto msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.fcitx.Fcitx5"), QStringLiteral("/controller"),
        QStringLiteral("org.fcitx.Fcitx.Controller1"),
        QStringLiteral("ReloadAddonConfig"));
    msg << addon;
    QDBusConnection::sessionBus().send(msg);
}

inline void reloadSchnelleUmlauteAddon() {
    reloadAddonConfig(QStringLiteral("schnelle-umlaute"));
}

#endif
