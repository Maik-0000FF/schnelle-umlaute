#include "SettingsModel.h"
#include "FcitxReload.h"
#include "caret_theme.h"
#include "../src/layer_shell_capability.h"
#include "../themes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

QString configFilePath() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/fcitx5/conf/schnelle-umlaute.conf");
}

QString toBool(bool v) {
    return v ? QStringLiteral("True") : QStringLiteral("False");
}
bool fromBool(const QString &s) {
    return s.compare("True", Qt::CaseInsensitive) == 0;
}

// A leader's captured physical key. Anything that cannot name a pressable key,
// including an absent or unparseable value, reads back as kNoKeyCode and leaves
// the leader unassigned. Never fabricate a keycode from a malformed value: a
// wrong physical key would trigger the wrong leader and mis-classify its
// keyboard half.
int toKeyCode(const QString &s) {
    bool ok = false;
    const int code = s.trimmed().toInt(&ok);
    return (ok && fcitx::isUsableKeyCode(code)) ? code : kNoKeyCode;
}

// --- Caret theme: generate an fcitx5 classicui theme matching the editor
// palette and point classicui at it. All compositor-agnostic (classicui is
// fcitx5's own renderer); the only "stop following the desktop" switch is
// UseAccentColor=False, there is no Plasma-specific key.

QString classicUiConfPath() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/fcitx5/conf/classicui.conf");
}
// User-dir fcitx5 theme; a unique name so it never shadows a system theme.
QString caretThemeDir() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base + QStringLiteral("/fcitx5/themes/schnelle-umlaute");
}
// Kept in the config dir, not the generated theme dir, so removing the
// throwaway theme never loses the record of the user's previous classicui
// theme (which restoreClassicUiTheme needs to put it back).
QString caretThemeBackupPath() {
    const auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const auto rel =
        QStringLiteral("/fcitx5/conf/schnelle-umlaute-classicui-backup.conf");
    return base + rel;
}

void writeTextFileAtomic(const QString &path, const QString &content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);
    ts << content;
    ts.flush();
    f.commit();
}

// File wrapper around the pure parser in caret_theme.h.
QMap<QString, QString> readFlatIni(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return schnelle_umlaute::parseFlatIni(QString::fromUtf8(f.readAll()));
}

// Read classicui.conf, patch the given keys (the pure line transform lives in
// caret_theme.h, preserving every other line), write it back atomically.
void setClassicUiKeys(const QMap<QString, QString> &kv) {
    const QString path = classicUiConfPath();
    QStringList lines;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd())
            lines << in.readLine();
        f.close();
    }
    lines = schnelle_umlaute::applyIniKeys(lines, kv);
    writeTextFileAtomic(path, lines.join(QLatin1Char('\n')) +
                                  (lines.isEmpty() ? QString()
                                                   : QStringLiteral("\n")));
}

// Pre-1.2 used a 3×3 grid; 1.2 moved to 7×3. Map the old names onto the
// equivalent column so existing configs survive an upgrade without
// resetting the user's chosen position.
QString migrateLegacyPosition(const QString &v) {
    if (v == QLatin1String("TopLeft"))
        return QStringLiteral("TopCol1");
    if (v == QLatin1String("TopCenter"))
        return QStringLiteral("TopCol4");
    if (v == QLatin1String("TopRight"))
        return QStringLiteral("TopCol7");
    if (v == QLatin1String("CenterLeft"))
        return QStringLiteral("CenterCol1");
    if (v == QLatin1String("Center"))
        return QStringLiteral("CenterCol4");
    if (v == QLatin1String("CenterRight"))
        return QStringLiteral("CenterCol7");
    if (v == QLatin1String("BottomLeft"))
        return QStringLiteral("BottomCol1");
    if (v == QLatin1String("BottomCenter"))
        return QStringLiteral("BottomCol4");
    if (v == QLatin1String("BottomRight"))
        return QStringLiteral("BottomCol7");
    return v;
}

} // namespace

SettingsModel::SettingsModel(QObject *parent) : QObject(parent) {
    const auto cap = fcitx::detectLayerShellCapability();
    layerShellAvailable_ = cap.supported;
    layerShellSession_ = QString::fromStdString(cap.session);
    layerShellReason_ = QString::fromStdString(cap.reason);

    // Forward all leader-related changes to a single umbrella signal so QML
    // can refresh isActiveLeaderKey-based bindings with one Connections hook.
    connect(this, &SettingsModel::leaderSpaceChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderLeftChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderRightChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderUpChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderDownChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderAltChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey1EnabledChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey1Changed, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey2EnabledChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey2Changed, this,
            &SettingsModel::leadersChanged);

    load();
}

void SettingsModel::setDelayLowercase(int v) {
    if (delayLowercase_ == v)
        return;
    delayLowercase_ = v;
    Q_EMIT delayLowercaseChanged();
    save();
}
void SettingsModel::setDelayUppercase(int v) {
    if (delayUppercase_ == v)
        return;
    delayUppercase_ = v;
    Q_EMIT delayUppercaseChanged();
    save();
}
void SettingsModel::setDelayLowercaseMin(int v) {
    if (delayLowercaseMin_ == v)
        return;
    delayLowercaseMin_ = v;
    Q_EMIT delayLowercaseMinChanged();
    save();
}
void SettingsModel::setDelayUppercaseMin(int v) {
    if (delayUppercaseMin_ == v)
        return;
    delayUppercaseMin_ = v;
    Q_EMIT delayUppercaseMinChanged();
    save();
}
void SettingsModel::setLeaderSpace(bool v) {
    if (leaderSpace_ == v)
        return;
    leaderSpace_ = v;
    Q_EMIT leaderSpaceChanged();
    save();
}
void SettingsModel::setLeaderLeft(bool v) {
    if (leaderLeft_ == v)
        return;
    leaderLeft_ = v;
    Q_EMIT leaderLeftChanged();
    save();
}
void SettingsModel::setLeaderRight(bool v) {
    if (leaderRight_ == v)
        return;
    leaderRight_ = v;
    Q_EMIT leaderRightChanged();
    save();
}
void SettingsModel::setLeaderUp(bool v) {
    if (leaderUp_ == v)
        return;
    leaderUp_ = v;
    Q_EMIT leaderUpChanged();
    save();
}
void SettingsModel::setLeaderDown(bool v) {
    if (leaderDown_ == v)
        return;
    leaderDown_ = v;
    Q_EMIT leaderDownChanged();
    save();
}
void SettingsModel::setLeaderAlt(bool v) {
    if (leaderAlt_ == v)
        return;
    leaderAlt_ = v;
    Q_EMIT leaderAltChanged();
    save();
}
void SettingsModel::setCustomKey1Enabled(bool v) {
    if (customKey1Enabled_ == v)
        return;
    customKey1Enabled_ = v;
    Q_EMIT customKey1EnabledChanged();
    save();
}
void SettingsModel::setCustomKey1(const QString &v) {
    if (customKey1_ == v)
        return;
    customKey1_ = v;
    Q_EMIT customKey1Changed();
    if (isValidLeaderKey(v))
        save();
}
void SettingsModel::setCustomKey2Enabled(bool v) {
    if (customKey2Enabled_ == v)
        return;
    customKey2Enabled_ = v;
    Q_EMIT customKey2EnabledChanged();
    save();
}
void SettingsModel::setCustomKey2(const QString &v) {
    if (customKey2_ == v)
        return;
    customKey2_ = v;
    Q_EMIT customKey2Changed();
    if (isValidLeaderKey(v))
        save();
}
// The only way a keycode enters the config: both halves land together, then one
// save. A code that cannot name a pressable key is stored as kNoKeyCode, which
// reads as "no key assigned" rather than as a leader that can never fire. The
// character is display only, so one the leader rules reject is simply not shown
// instead of blocking the capture.
void SettingsModel::captureCustomKey1(const QString &ch, int code) {
    customKey1Code_ = fcitx::isUsableKeyCode(code) ? code : kNoKeyCode;
    customKey1_ = isValidLeaderKey(ch) ? ch : QString();
    Q_EMIT customKey1CodeChanged();
    Q_EMIT customKey1Changed();
    save();
}
void SettingsModel::captureCustomKey2(const QString &ch, int code) {
    customKey2Code_ = fcitx::isUsableKeyCode(code) ? code : kNoKeyCode;
    customKey2_ = isValidLeaderKey(ch) ? ch : QString();
    Q_EMIT customKey2CodeChanged();
    Q_EMIT customKey2Changed();
    save();
}

void SettingsModel::setAppFilterMode(const QString &v) {
    if (appFilterMode_ == v)
        return;
    appFilterMode_ = v;
    Q_EMIT appFilterModeChanged();
    save();
}
void SettingsModel::setOverlayEnabled(bool v) {
    if (overlayEnabled_ == v)
        return;
    overlayEnabled_ = v;
    Q_EMIT overlayEnabledChanged();
    save();
}
void SettingsModel::setOverlayShowOnTrigger(bool v) {
    if (overlayShowOnTrigger_ == v)
        return;
    overlayShowOnTrigger_ = v;
    Q_EMIT overlayShowOnTriggerChanged();
    save();
}
void SettingsModel::setOverlayPlacement(const QString &v) {
    // Reject unknown values so the UI can't persist a placement that load()
    // would then ignore (mirrors setTheme's isValidTheme guard).
    if (!isValidPlacement(v) || overlayPlacement_ == v)
        return;
    overlayPlacement_ = v;
    Q_EMIT overlayPlacementChanged();
    save();
}
void SettingsModel::setOverlayProgressBar(bool v) {
    if (overlayProgressBar_ == v)
        return;
    overlayProgressBar_ = v;
    Q_EMIT overlayProgressBarChanged();
    save();
}

void SettingsModel::setOverlayPosition(const QString &v) {
    if (overlayPosition_ == v)
        return;
    overlayPosition_ = v;
    Q_EMIT overlayPositionChanged();
    save();
}
void SettingsModel::setOverlayCaretTheme(bool v) {
    if (overlayCaretTheme_ == v)
        return;
    overlayCaretTheme_ = v;
    Q_EMIT overlayCaretThemeChanged();
    // Turning it off restores the user's previous classicui theme. Turning it
    // on is driven from QML via applyCaretTheme() with the active colours.
    if (!v)
        clearCaretTheme();
    save();
}

void SettingsModel::writeCaretThemeFiles(const QString &background,
                                         const QString &text,
                                         const QString &highlight,
                                         const QString &onHighlight,
                                         const QString &border) {
    const QString themeConf = schnelle_umlaute::generateCaretThemeConf(
        background, text, highlight, onHighlight, border);
    writeTextFileAtomic(caretThemeDir() + QStringLiteral("/theme.conf"),
                        themeConf);

    // Back up the user's current classicui theme once (before we override it),
    // so turning the toggle off can put it back exactly.
    if (!QFile::exists(caretThemeBackupPath())) {
        const QMap<QString, QString> cur = readFlatIni(classicUiConfPath());
        QStringList backup;
        backup << QStringLiteral("Theme=") +
                      cur.value(QStringLiteral("Theme"),
                                QStringLiteral("default"));
        backup << QStringLiteral("UseDarkTheme=") +
                      cur.value(QStringLiteral("UseDarkTheme"),
                                QStringLiteral("False"));
        backup << QStringLiteral("UseAccentColor=") +
                      cur.value(QStringLiteral("UseAccentColor"),
                                QStringLiteral("True"));
        writeTextFileAtomic(caretThemeBackupPath(),
                            backup.join(QLatin1Char('\n')) +
                                QStringLiteral("\n"));
    }
    // Point classicui at our theme and stop it from following the desktop
    // accent/dark scheme (UseAccentColor=False is the real override switch;
    // there is no Plasma-specific key).
    setClassicUiKeys({{QStringLiteral("Theme"),
                       QStringLiteral("schnelle-umlaute")},
                      {QStringLiteral("UseDarkTheme"), QStringLiteral("False")},
                      {QStringLiteral("UseAccentColor"),
                       QStringLiteral("False")}});
}

void SettingsModel::restoreClassicUiTheme() {
    QMap<QString, QString> backup = readFlatIni(caretThemeBackupPath());
    if (backup.isEmpty()) {
        // No backup recorded: fall back to fcitx5 defaults.
        backup = {{QStringLiteral("Theme"), QStringLiteral("default")},
                  {QStringLiteral("UseDarkTheme"), QStringLiteral("False")},
                  {QStringLiteral("UseAccentColor"), QStringLiteral("True")}};
    }
    setClassicUiKeys(backup);
    QFile::remove(caretThemeBackupPath());
}

void SettingsModel::applyCaretTheme(const QString &background,
                                    const QString &text,
                                    const QString &highlight,
                                    const QString &onHighlight,
                                    const QString &border) {
    writeCaretThemeFiles(background, text, highlight, onHighlight, border);
    reloadClassicUiAddon();
}

void SettingsModel::clearCaretTheme() {
    restoreClassicUiTheme();
    reloadClassicUiAddon();
}

void SettingsModel::setTheme(const QString &v) {
    if (!isValidTheme(v) || theme_ == v)
        return;
    theme_ = v;
    Q_EMIT themeChanged();
    save();
    // Push to the overlay daemon so it switches palette immediately. The
    // client skips the call if the daemon isn't running — we don't want a
    // theme change to spawn it for users who never enabled the overlay.
    overlayClient_.sendTheme(theme_);
}

bool SettingsModel::isValidTheme(const QString &name) {
    return schnelle_umlaute::isValidTheme(name);
}

bool SettingsModel::isValidPlacement(const QString &name) {
    // Single source for the editor side; must stay in sync with the
    // OverlayPlacement enum in addon/src/config.h.
    static const QStringList kPlacements = {QStringLiteral("Grid"),
                                            QStringLiteral("MouseCursor"),
                                            QStringLiteral("TextCaret")};
    return kPlacements.contains(name);
}

void SettingsModel::addBlacklistEntry(const QString &entry) {
    auto trimmed = entry.trimmed();
    if (trimmed.isEmpty() || blacklist_.contains(trimmed))
        return;
    blacklist_ << trimmed;
    Q_EMIT blacklistChanged();
    save();
}
void SettingsModel::removeBlacklistEntry(int index) {
    if (index < 0 || index >= blacklist_.size())
        return;
    blacklist_.removeAt(index);
    Q_EMIT blacklistChanged();
    save();
}
void SettingsModel::addWhitelistEntry(const QString &entry) {
    auto trimmed = entry.trimmed();
    if (trimmed.isEmpty() || whitelist_.contains(trimmed))
        return;
    whitelist_ << trimmed;
    Q_EMIT whitelistChanged();
    save();
}
void SettingsModel::removeWhitelistEntry(int index) {
    if (index < 0 || index >= whitelist_.size())
        return;
    whitelist_.removeAt(index);
    Q_EMIT whitelistChanged();
    save();
}

bool SettingsModel::isActiveLeaderKey(const QString &key) const {
    if (key.isEmpty())
        return false;
    if (customKey1Enabled_ && customKey1_ == key)
        return true;
    if (customKey2Enabled_ && customKey2_ == key)
        return true;
    return false;
}

bool SettingsModel::isValidLeaderKey(const QString &s) {
    if (s.isEmpty())
        return true; // empty means "not set yet" — not an error
    auto ucs4 = s.toUcs4();
    if (ucs4.size() != 1)
        return false;
    return !QChar::isSpace(ucs4[0]);
}

void SettingsModel::load() {
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    QString section;
    QStringList blacklist, whitelist;
    // Row/Column accumulate across the Overlay section and are joined at
    // the end. Starting empty lets us detect "file doesn't have them yet"
    // (legacy Position= keeps overlayPosition_ authoritative) vs "at least
    // one of the new keys was present" (then Row+Column win).
    QString loadedOverlayRow, loadedOverlayCol;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const int eq = static_cast<int>(line.indexOf('='));
        if (eq < 0)
            continue;
        QString key = line.left(eq);
        QString val = line.mid(eq + 1);

        if (section == QLatin1String("Delay")) {
            if (key == "Lowercase")
                delayLowercase_ = val.toInt();
            else if (key == "Uppercase")
                delayUppercase_ = val.toInt();
            else if (key == "LowercaseMin")
                delayLowercaseMin_ = val.toInt();
            else if (key == "UppercaseMin")
                delayUppercaseMin_ = val.toInt();
        } else if (section == QLatin1String("Leader")) {
            if (key == "Space")
                leaderSpace_ = fromBool(val);
            else if (key == "Left")
                leaderLeft_ = fromBool(val);
            else if (key == "Right")
                leaderRight_ = fromBool(val);
            else if (key == "Up")
                leaderUp_ = fromBool(val);
            else if (key == "Down")
                leaderDown_ = fromBool(val);
            else if (key == "Alt")
                leaderAlt_ = fromBool(val);
        } else if (section == QLatin1String("Leader/Custom")) {
            if (key == "CustomKeyEnabled")
                customKey1Enabled_ = fromBool(val);
            else if (key == "CustomKey")
                customKey1_ = val;
            else if (key == "CustomKeyCode")
                customKey1Code_ = toKeyCode(val);
            else if (key == "CustomKey2Enabled")
                customKey2Enabled_ = fromBool(val);
            else if (key == "CustomKey2")
                customKey2_ = val;
            else if (key == "CustomKey2Code")
                customKey2Code_ = toKeyCode(val);
        } else if (section == QLatin1String("AppFilter")) {
            if (key == "Mode")
                appFilterMode_ = val;
        } else if (section == QLatin1String("AppFilter/Blacklist")) {
            bool ok = false;
            int idx = key.toInt(&ok);
            if (ok) {
                while (blacklist.size() <= idx)
                    blacklist << QString();
                blacklist[idx] = val;
            }
        } else if (section == QLatin1String("AppFilter/Whitelist")) {
            bool ok = false;
            int idx = key.toInt(&ok);
            if (ok) {
                while (whitelist.size() <= idx)
                    whitelist << QString();
                whitelist[idx] = val;
            }
        } else if (section == QLatin1String("Overlay")) {
            if (key == "Enabled")
                overlayEnabled_ = fromBool(val);
            else if (key == "ShowOnTrigger")
                overlayShowOnTrigger_ = fromBool(val);
            else if (key == "Placement") {
                // Ignore an unknown value so a corrupt/hand-edited Placement
                // keeps the in-memory default instead of round-tripping garbage
                // that the addon's enum would silently read as Grid anyway.
                if (isValidPlacement(val))
                    overlayPlacement_ = val;
            }
            // Pre-enum (1.2.3) wrote AtCursor=True/False for the mouse mode.
            // Map a leftover AtCursor=True onto MouseCursor unless an explicit
            // Placement already set something other than the Grid default.
            else if (key == "AtCursor") {
                if (fromBool(val) &&
                    overlayPlacement_ == QLatin1String("Grid"))
                    overlayPlacement_ = QStringLiteral("MouseCursor");
            } else if (key == "ProgressBar")
                overlayProgressBar_ = fromBool(val);
            else if (key == "CaretTheme")
                overlayCaretTheme_ = fromBool(val);
            // Pre-1.2 wrote a combined "Position=TopCenter" key. 1.2 splits
            // it into Row + Column because FCITX_CONFIG_ENUM caps at 12
            // values and we need 21 cells. Accept both formats on read so
            // an upgrade doesn't reset the user's choice.
            else if (key == "Position")
                overlayPosition_ = migrateLegacyPosition(val);
            else if (key == "Row")
                loadedOverlayRow = val;
            else if (key == "Column")
                loadedOverlayCol = val;
        } else if (section == QLatin1String("Theme")) {
            if (key == "Theme" && isValidTheme(val))
                theme_ = val;
        }
    }
    blacklist_ = blacklist;
    whitelist_ = whitelist;
    // Row+Column win over legacy Position= when either is present — the
    // new keys are what the fcitx5 addon will honor going forward.
    if (!loadedOverlayRow.isEmpty() || !loadedOverlayCol.isEmpty()) {
        const QString row = loadedOverlayRow.isEmpty() ? QStringLiteral("Top")
                                                       : loadedOverlayRow;
        const QString col = loadedOverlayCol.isEmpty() ? QStringLiteral("Col4")
                                                       : loadedOverlayCol;
        overlayPosition_ = row + col;
    }

    Q_EMIT delayLowercaseChanged();
    Q_EMIT delayUppercaseChanged();
    Q_EMIT delayLowercaseMinChanged();
    Q_EMIT delayUppercaseMinChanged();
    Q_EMIT leaderSpaceChanged();
    Q_EMIT leaderLeftChanged();
    Q_EMIT leaderRightChanged();
    Q_EMIT leaderUpChanged();
    Q_EMIT leaderDownChanged();
    Q_EMIT leaderAltChanged();
    Q_EMIT customKey1EnabledChanged();
    Q_EMIT customKey1Changed();
    Q_EMIT customKey1CodeChanged();
    Q_EMIT customKey2EnabledChanged();
    Q_EMIT customKey2Changed();
    Q_EMIT customKey2CodeChanged();
    Q_EMIT appFilterModeChanged();
    Q_EMIT blacklistChanged();
    Q_EMIT whitelistChanged();
    Q_EMIT overlayEnabledChanged();
    Q_EMIT overlayShowOnTriggerChanged();
    Q_EMIT overlayPlacementChanged();
    Q_EMIT overlayProgressBarChanged();
    Q_EMIT overlayPositionChanged();
    Q_EMIT overlayCaretThemeChanged();
    Q_EMIT themeChanged();
}

void SettingsModel::save() {
    QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
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
    out << "# Lowercase minimum hold (ms)\n";
    out << "LowercaseMin=" << delayLowercaseMin_ << "\n";
    out << "# Uppercase minimum hold (ms)\n";
    out << "UppercaseMin=" << delayUppercaseMin_ << "\n";
    out << "\n";
    out << "[Leader]\n";
    out << "# Space\n" << "Space=" << toBool(leaderSpace_) << "\n";
    out << "# Left Arrow\n" << "Left=" << toBool(leaderLeft_) << "\n";
    out << "# Right Arrow\n" << "Right=" << toBool(leaderRight_) << "\n";
    out << "# Up Arrow\n" << "Up=" << toBool(leaderUp_) << "\n";
    out << "# Down Arrow\n" << "Down=" << toBool(leaderDown_) << "\n";
    out << "# Alt/AltGr\n" << "Alt=" << toBool(leaderAlt_) << "\n";
    out << "\n";
    out << "[Leader/Custom]\n";
    out << "# Custom Leader 1\n"
        << "CustomKeyEnabled=" << toBool(customKey1Enabled_) << "\n";
    out << "#   \xe2\x86\xb3 Key\n"
        << "CustomKey=" << customKey1_ << "\n";
    out << "#   \xe2\x86\xb3 Key code\n"
        << "CustomKeyCode=" << customKey1Code_ << "\n";
    out << "# Custom Leader 2 (hand-split)\n"
        << "CustomKey2Enabled=" << toBool(customKey2Enabled_) << "\n";
    out << "#   \xe2\x86\xb3 Key\n"
        << "CustomKey2=" << customKey2_ << "\n";
    out << "#   \xe2\x86\xb3 Key code\n"
        << "CustomKey2Code=" << customKey2Code_ << "\n";
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
    out << "# Preview in the trigger window (all mapped keys)\n"
        << "ShowOnTrigger=" << toBool(overlayShowOnTrigger_) << "\n";
    out << "# Placement (Grid|MouseCursor|TextCaret)\n"
        << "Placement=" << overlayPlacement_ << "\n";
    out << "# Show timing progress bar\n"
        << "ProgressBar=" << toBool(overlayProgressBar_) << "\n";
    out << "# Match fcitx5 candidate window to the editor theme (caret mode)\n"
        << "CaretTheme=" << toBool(overlayCaretTheme_) << "\n";
    // Split "TopCol4" into Row=Top + Column=Col4 for the fcitx5 config
    // schema, which represents each as a small enum (capped at 12 values).
    const int splitAt =
        static_cast<int>(overlayPosition_.indexOf(QLatin1String("Col")));
    const QString row =
        splitAt > 0 ? overlayPosition_.left(splitAt) : QStringLiteral("Top");
    const QString col =
        splitAt > 0 ? overlayPosition_.mid(splitAt) : QStringLiteral("Col4");
    out << "# Vertical position\n" << "Row=" << row << "\n";
    out << "# Horizontal position\n" << "Column=" << col << "\n";
    out << "\n[Theme]\n";
    out << "# UI theme (schnelle-umlaute|dark|light|contrast)\n"
        << "Theme=" << theme_ << "\n";
    out.flush();
    f.commit();
    reloadFcitx();
}

void SettingsModel::reloadFcitx() { reloadSchnelleUmlauteAddon(); }
