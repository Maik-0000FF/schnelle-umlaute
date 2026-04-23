#include "SettingsModel.h"
#include "FcitxReload.h"
#include "../src/layer_shell_capability.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

namespace {

QString configFilePath() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/fcitx5/conf/schnelle-umlaute.conf");
}

QString toBool(bool v) { return v ? QStringLiteral("True") : QStringLiteral("False"); }
bool fromBool(const QString &s) { return s.compare("True", Qt::CaseInsensitive) == 0; }

} // namespace

SettingsModel::SettingsModel(QObject *parent) : QObject(parent) {
    const auto cap = fcitx::detectLayerShellCapability();
    layerShellAvailable_ = cap.supported;
    layerShellSession_ = QString::fromStdString(cap.session);
    layerShellReason_ = QString::fromStdString(cap.reason);
    load();
}

void SettingsModel::setDelayLowercase(int v) {
    if (delayLowercase_ == v) return;
    delayLowercase_ = v;
    Q_EMIT delayLowercaseChanged();
    save();
}
void SettingsModel::setDelayUppercase(int v) {
    if (delayUppercase_ == v) return;
    delayUppercase_ = v;
    Q_EMIT delayUppercaseChanged();
    save();
}
void SettingsModel::setLeaderSpace(bool v) {
    if (leaderSpace_ == v) return;
    leaderSpace_ = v;
    Q_EMIT leaderSpaceChanged();
    save();
}
void SettingsModel::setLeaderLeft(bool v) {
    if (leaderLeft_ == v) return;
    leaderLeft_ = v;
    Q_EMIT leaderLeftChanged();
    save();
}
void SettingsModel::setLeaderRight(bool v) {
    if (leaderRight_ == v) return;
    leaderRight_ = v;
    Q_EMIT leaderRightChanged();
    save();
}
void SettingsModel::setLeaderUp(bool v) {
    if (leaderUp_ == v) return;
    leaderUp_ = v;
    Q_EMIT leaderUpChanged();
    save();
}
void SettingsModel::setLeaderDown(bool v) {
    if (leaderDown_ == v) return;
    leaderDown_ = v;
    Q_EMIT leaderDownChanged();
    save();
}
void SettingsModel::setLeaderAlt(bool v) {
    if (leaderAlt_ == v) return;
    leaderAlt_ = v;
    Q_EMIT leaderAltChanged();
    save();
}
void SettingsModel::setCustomKey1Enabled(bool v) {
    if (customKey1Enabled_ == v) return;
    customKey1Enabled_ = v;
    Q_EMIT customKey1EnabledChanged();
    save();
}
void SettingsModel::setCustomKey1(const QString &v) {
    if (customKey1_ == v) return;
    customKey1_ = v;
    Q_EMIT customKey1Changed();
    if (isValidLeaderKey(v)) save();
}
void SettingsModel::setCustomKey2Enabled(bool v) {
    if (customKey2Enabled_ == v) return;
    customKey2Enabled_ = v;
    Q_EMIT customKey2EnabledChanged();
    save();
}
void SettingsModel::setCustomKey2(const QString &v) {
    if (customKey2_ == v) return;
    customKey2_ = v;
    Q_EMIT customKey2Changed();
    if (isValidLeaderKey(v)) save();
}
void SettingsModel::setAppFilterMode(const QString &v) {
    if (appFilterMode_ == v) return;
    appFilterMode_ = v;
    Q_EMIT appFilterModeChanged();
    save();
}
void SettingsModel::setOverlayEnabled(bool v) {
    if (overlayEnabled_ == v) return;
    overlayEnabled_ = v;
    Q_EMIT overlayEnabledChanged();
    save();
}
void SettingsModel::setOverlayPosition(const QString &v) {
    if (overlayPosition_ == v) return;
    overlayPosition_ = v;
    Q_EMIT overlayPositionChanged();
    save();
}

void SettingsModel::addBlacklistEntry(const QString &entry) {
    auto trimmed = entry.trimmed();
    if (trimmed.isEmpty() || blacklist_.contains(trimmed)) return;
    blacklist_ << trimmed;
    Q_EMIT blacklistChanged();
    save();
}
void SettingsModel::removeBlacklistEntry(int index) {
    if (index < 0 || index >= blacklist_.size()) return;
    blacklist_.removeAt(index);
    Q_EMIT blacklistChanged();
    save();
}
void SettingsModel::addWhitelistEntry(const QString &entry) {
    auto trimmed = entry.trimmed();
    if (trimmed.isEmpty() || whitelist_.contains(trimmed)) return;
    whitelist_ << trimmed;
    Q_EMIT whitelistChanged();
    save();
}
void SettingsModel::removeWhitelistEntry(int index) {
    if (index < 0 || index >= whitelist_.size()) return;
    whitelist_.removeAt(index);
    Q_EMIT whitelistChanged();
    save();
}

bool SettingsModel::isActiveLeaderKey(const QString &key) const {
    if (key.isEmpty()) return false;
    if (customKey1Enabled_ && customKey1_ == key) return true;
    if (customKey2Enabled_ && customKey2_ == key) return true;
    return false;
}

bool SettingsModel::isValidLeaderKey(const QString &s) {
    if (s.isEmpty()) return true; // empty means "not set yet" — not an error
    auto ucs4 = s.toUcs4();
    if (ucs4.size() != 1) return false;
    return !QChar::isSpace(ucs4[0]);
}

void SettingsModel::load() {
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    QString section;
    QStringList blacklist, whitelist;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        QString key = line.left(eq);
        QString val = line.mid(eq + 1);

        if (section == QLatin1String("Delay")) {
            if (key == "Lowercase") delayLowercase_ = val.toInt();
            else if (key == "Uppercase") delayUppercase_ = val.toInt();
        } else if (section == QLatin1String("Leader")) {
            if (key == "Space") leaderSpace_ = fromBool(val);
            else if (key == "Left") leaderLeft_ = fromBool(val);
            else if (key == "Right") leaderRight_ = fromBool(val);
            else if (key == "Up") leaderUp_ = fromBool(val);
            else if (key == "Down") leaderDown_ = fromBool(val);
            else if (key == "Alt") leaderAlt_ = fromBool(val);
        } else if (section == QLatin1String("Leader/Custom")) {
            if (key == "CustomKeyEnabled") customKey1Enabled_ = fromBool(val);
            else if (key == "CustomKey") customKey1_ = val;
            else if (key == "CustomKey2Enabled") customKey2Enabled_ = fromBool(val);
            else if (key == "CustomKey2") customKey2_ = val;
        } else if (section == QLatin1String("AppFilter")) {
            if (key == "Mode") appFilterMode_ = val;
        } else if (section == QLatin1String("AppFilter/Blacklist")) {
            bool ok = false;
            int idx = key.toInt(&ok);
            if (ok) {
                while (blacklist.size() <= idx) blacklist << QString();
                blacklist[idx] = val;
            }
        } else if (section == QLatin1String("AppFilter/Whitelist")) {
            bool ok = false;
            int idx = key.toInt(&ok);
            if (ok) {
                while (whitelist.size() <= idx) whitelist << QString();
                whitelist[idx] = val;
            }
        } else if (section == QLatin1String("Overlay")) {
            if (key == "Enabled") overlayEnabled_ = fromBool(val);
            else if (key == "Position") overlayPosition_ = val;
        }
    }
    blacklist_ = blacklist;
    whitelist_ = whitelist;

    Q_EMIT delayLowercaseChanged();
    Q_EMIT delayUppercaseChanged();
    Q_EMIT leaderSpaceChanged();
    Q_EMIT leaderLeftChanged();
    Q_EMIT leaderRightChanged();
    Q_EMIT leaderUpChanged();
    Q_EMIT leaderDownChanged();
    Q_EMIT leaderAltChanged();
    Q_EMIT customKey1EnabledChanged();
    Q_EMIT customKey1Changed();
    Q_EMIT customKey2EnabledChanged();
    Q_EMIT customKey2Changed();
    Q_EMIT appFilterModeChanged();
    Q_EMIT blacklistChanged();
    Q_EMIT whitelistChanged();
    Q_EMIT overlayEnabledChanged();
    Q_EMIT overlayPositionChanged();
}

void SettingsModel::save() {
    QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QTextStream out(&f);

    // Matches the comment style fcitx5-configtool writes so both tools
    // produce identical files and round-trips stay clean.
    out << "# Mappings\n";
    out << "Mappings=\n";
    out << "\n";
    out << "[Delay]\n";
    out << "# Lowercase (ms)\n";
    out << "Lowercase=" << delayLowercase_ << "\n";
    out << "# Uppercase (ms)\n";
    out << "Uppercase=" << delayUppercase_ << "\n";
    out << "\n";
    out << "[Leader]\n";
    out << "# Space\n"          << "Space=" << toBool(leaderSpace_) << "\n";
    out << "# Left Arrow\n"     << "Left="  << toBool(leaderLeft_)  << "\n";
    out << "# Right Arrow\n"    << "Right=" << toBool(leaderRight_) << "\n";
    out << "# Up Arrow\n"       << "Up="    << toBool(leaderUp_)    << "\n";
    out << "# Down Arrow\n"     << "Down="  << toBool(leaderDown_)  << "\n";
    out << "# Alt/AltGr\n"      << "Alt="   << toBool(leaderAlt_)   << "\n";
    out << "\n";
    out << "[Leader/Custom]\n";
    out << "# Custom Leader 1\n"
        << "CustomKeyEnabled=" << toBool(customKey1Enabled_) << "\n";
    out << "#   \xe2\x86\xb3 Key\n"
        << "CustomKey=" << customKey1_ << "\n";
    out << "# Custom Leader 2 (hand-split)\n"
        << "CustomKey2Enabled=" << toBool(customKey2Enabled_) << "\n";
    out << "#   \xe2\x86\xb3 Key\n"
        << "CustomKey2=" << customKey2_ << "\n";
    out << "\n";
    out << "[AppFilter]\n";
    out << "# Mode\n" << "Mode=" << appFilterMode_ << "\n";
    if (blacklist_.isEmpty()) {
        out << "# Blacklist\n" << "Blacklist=\n";
    }
    if (whitelist_.isEmpty()) {
        out << "# Whitelist\n" << "Whitelist=\n";
    }
    if (!blacklist_.isEmpty()) {
        out << "\n[AppFilter/Blacklist]\n";
        for (int i = 0; i < blacklist_.size(); ++i) {
            out << i << "=" << blacklist_[i] << "\n";
        }
    }
    if (!whitelist_.isEmpty()) {
        out << "\n[AppFilter/Whitelist]\n";
        for (int i = 0; i < whitelist_.size(); ++i) {
            out << i << "=" << whitelist_[i] << "\n";
        }
    }
    out << "\n[Overlay]\n";
    out << "# Show overlay while cycling\n"
        << "Enabled=" << toBool(overlayEnabled_) << "\n";
    out << "# Position on screen\n"
        << "Position=" << overlayPosition_ << "\n";
    out.flush();
    f.commit();
    reloadFcitx();
    Q_EMIT saveFinished();
}

void SettingsModel::reloadFcitx() { reloadSchnelleUmlauteAddon(); }
