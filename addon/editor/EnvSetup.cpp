#include "EnvSetup.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {

// systemd-user-environment-generators reads every *.conf in this
// directory at session start and adds the key=value pairs to the
// user's environment. Identical path on Arch, Fedora, openSUSE,
// Debian/Ubuntu/Mint/Kali — all run systemd-managed user sessions
// through their default login managers (SDDM, GDM, LightDM-with-
// systemd). Non-systemd login flows (startx without a DM) are a
// documented edge case.
QString envDirPath() {
    return QDir::homePath() + QStringLiteral("/.config/environment.d");
}

QString envFilePath() {
    return envDirPath() + QStringLiteral("/fcitx5.conf");
}

constexpr auto kFcitxValue = "fcitx";
constexpr auto kXmodifiersValue = "@im=fcitx";

} // namespace

EnvSetup::EnvSetup(QObject *parent) : QObject(parent) {}

bool EnvSetup::isConfigured() const {
    const QByteArray gtk = qgetenv("GTK_IM_MODULE");
    const QByteArray qt = qgetenv("QT_IM_MODULE");
    const QByteArray xmod = qgetenv("XMODIFIERS");
    return gtk == kFcitxValue && qt == kFcitxValue && xmod == kXmodifiersValue;
}

bool EnvSetup::writeConfig() {
    QDir dir;
    if (!dir.mkpath(envDirPath())) {
        return false;
    }
    QFile file(envFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                   QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << "GTK_IM_MODULE=fcitx\n";
    out << "QT_IM_MODULE=fcitx\n";
    out << "XMODIFIERS=@im=fcitx\n";
    out << "GLFW_IM_MODULE=ibus\n";
    return true;
}

QString EnvSetup::configPath() const { return envFilePath(); }
