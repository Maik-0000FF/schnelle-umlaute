#include "EnvSetup.h"

#include "../src/session_env.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

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
    QSaveFile file(envFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    static constexpr QByteArrayView payload =
        "GTK_IM_MODULE=fcitx\n"
        "QT_IM_MODULE=fcitx\n"
        "XMODIFIERS=@im=fcitx\n"
        "GLFW_IM_MODULE=ibus\n";
    if (file.write(payload.data(), payload.size()) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

QString EnvSetup::configPath() const { return envFilePath(); }

bool EnvSetup::hasValidConfigFile() const {
    QFile file(envFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    // Line-strict match: require each line to appear uncommented and
    // not embedded in another token. A commented-out line like
    // "# GTK_IM_MODULE=fcitx" must not satisfy the check, otherwise
    // a user-disabled config would look "set up" and never trigger
    // the setup dialog again.
    //
    // GLFW_IM_MODULE is intentionally NOT validated here even though
    // writeConfig() writes it. It is non-canonical for fcitx5 (used
    // only by GLFW-based clients like Alacritty), and users who
    // hand-edit the file to remove it should still see the "logout
    // pending" hint rather than be sent back through setup.
    bool gtk = false;
    bool qt = false;
    bool xmod = false;
    const QByteArray content = file.readAll();
    for (const QByteArray &raw : content.split('\n')) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line == "GTK_IM_MODULE=fcitx") {
            gtk = true;
        } else if (line == "QT_IM_MODULE=fcitx") {
            qt = true;
        } else if (line == "XMODIFIERS=@im=fcitx") {
            xmod = true;
        }
    }
    return gtk && qt && xmod;
}

bool EnvSetup::honorsEnvironmentD() const {
    return fcitx::detectSessionEnv().mechanism ==
           fcitx::EnvMechanism::EnvironmentD;
}

QString EnvSetup::sessionName() const {
    return QString::fromStdString(fcitx::detectSessionEnv().session);
}

QString EnvSetup::compositorEnvSnippet() const {
    return QString::fromStdString(fcitx::detectSessionEnv().snippet);
}

QString EnvSetup::compositorConfigPath() const {
    return QString::fromStdString(fcitx::detectSessionEnv().configPath);
}

bool EnvSetup::writeCompositorConfig() {
    const fcitx::SessionEnvInfo info = fcitx::detectSessionEnv();
    if (info.configPath.empty() || info.snippet.empty()) {
        // No single known config file for this compositor (sway/river/...):
        // the dialog shows the snippet for manual placement instead.
        return false;
    }

    QString path = QString::fromStdString(info.configPath);
    if (path.startsWith(QStringLiteral("~/"))) {
        path = QDir::homePath() + path.mid(1);
    }

    // Read the user's current config; absence is fine — we create it.
    QByteArray existing;
    {
        QFile in(path);
        if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
            existing = in.readAll();
        }
    }

    const fcitx::CompositorEnvMerge merge =
        fcitx::mergeCompositorEnv(existing.toStdString(), info.snippet);
    if (!merge.changed) {
        return true; // lines already present — nothing to write
    }

    QDir dir;
    if (!dir.mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray payload = QByteArray::fromStdString(merge.content);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
