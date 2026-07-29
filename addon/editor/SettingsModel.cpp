#include "SettingsModel.h"
#include "FcitxReload.h"
#include "../caret_theme_io.h"
#include "../config_keys.h"
#include "../src/layer_shell_capability.h"
#include "../themes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStyleHints>
#include <QTextStream>

// The key names the overlay daemon reads back out of the file this model
// writes. Aliased so both sides spell them the same way.
namespace keys = schnelle_umlaute::keys;

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
    connect(this, &SettingsModel::leaderSpaceReverseChanged, this,
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
    connect(this, &SettingsModel::leaderAltReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderAltGrChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderAltGrReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderLeftReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderRightReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderUpReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::leaderDownReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey1EnabledChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey1Changed, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey1ReverseChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey2EnabledChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey2Changed, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey2ReverseChanged, this,
            &SettingsModel::leadersChanged);
    // A captured (or cleared) key flips whether a custom leader counts as
    // effective, so it must refresh the leader summary and the effective count.
    connect(this, &SettingsModel::customKey1CodeChanged, this,
            &SettingsModel::leadersChanged);
    connect(this, &SettingsModel::customKey2CodeChanged, this,
            &SettingsModel::leadersChanged);

    // Everything the derivation reads feeds one umbrella signal, so QML rebinds
    // the rendered theme from a single hook no matter which input moved.
    connect(this, &SettingsModel::themeChanged, this,
            &SettingsModel::effectiveThemeChanged);
    connect(this, &SettingsModel::themeAutoChanged, this,
            &SettingsModel::effectiveThemeChanged);
    connect(this, &SettingsModel::themeLightChanged, this,
            &SettingsModel::effectiveThemeChanged);
    connect(this, &SettingsModel::themeDarkChanged, this,
            &SettingsModel::effectiveThemeChanged);
    // The desktop switching between light and dark is the fourth input. The
    // daemon watches it too and reaches the same answer from the same config,
    // rather than being told, so the two stay in step even when the editor is
    // closed for the switch that matters.
    //
    // QStyleHints learned about the colour scheme in Qt 6.5, and the oldest
    // supported distros (Ubuntu 24.04, Linux Mint 22) still ship 6.4. There the
    // scheme reads as Unknown, which the derivation already handles by keeping
    // the manual pick, so the build stays whole and the mode is simply inert.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints())
        connect(hints, &QStyleHints::colorSchemeChanged, this,
                [this](Qt::ColorScheme) { Q_EMIT effectiveThemeChanged(); });
#endif

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
int SettingsModel::effectiveLeaderCount() const {
    int n = 0;
    if (leaderSpace_)
        ++n;
    if (leaderLeft_)
        ++n;
    if (leaderRight_)
        ++n;
    if (leaderUp_)
        ++n;
    if (leaderDown_)
        ++n;
    if (leaderAlt_)
        ++n;
    if (leaderAltGr_)
        ++n;
    // A custom leader counts only when it is enabled AND has a key captured; an
    // enabled-but-unassigned one cannot trigger anything.
    if (customKey1Enabled_ && customKey1HasKey())
        ++n;
    if (customKey2Enabled_ && customKey2HasKey())
        ++n;
    return n;
}
bool SettingsModel::allowLeaderOff(bool stillEffective) {
    if (stillEffective && effectiveLeaderCount() == 1) {
        Q_EMIT leaderRemovalBlocked();
        return false;
    }
    return true;
}
void SettingsModel::setLeaderSpace(bool v) {
    if (leaderSpace_ == v)
        return;
    if (!v && !allowLeaderOff(leaderSpace_))
        return;
    leaderSpace_ = v;
    Q_EMIT leaderSpaceChanged();
    save();
}
void SettingsModel::setLeaderSpaceReverse(bool v) {
    if (leaderSpaceReverse_ == v)
        return;
    leaderSpaceReverse_ = v;
    Q_EMIT leaderSpaceReverseChanged();
    save();
}
void SettingsModel::setLeaderLeft(bool v) {
    if (leaderLeft_ == v)
        return;
    if (!v && !allowLeaderOff(leaderLeft_))
        return;
    leaderLeft_ = v;
    Q_EMIT leaderLeftChanged();
    save();
}
void SettingsModel::setLeaderRight(bool v) {
    if (leaderRight_ == v)
        return;
    if (!v && !allowLeaderOff(leaderRight_))
        return;
    leaderRight_ = v;
    Q_EMIT leaderRightChanged();
    save();
}
void SettingsModel::setLeaderUp(bool v) {
    if (leaderUp_ == v)
        return;
    if (!v && !allowLeaderOff(leaderUp_))
        return;
    leaderUp_ = v;
    Q_EMIT leaderUpChanged();
    save();
}
void SettingsModel::setLeaderDown(bool v) {
    if (leaderDown_ == v)
        return;
    if (!v && !allowLeaderOff(leaderDown_))
        return;
    leaderDown_ = v;
    Q_EMIT leaderDownChanged();
    save();
}
void SettingsModel::setLeaderAlt(bool v) {
    if (leaderAlt_ == v)
        return;
    if (!v && !allowLeaderOff(leaderAlt_))
        return;
    leaderAlt_ = v;
    Q_EMIT leaderAltChanged();
    save();
}
void SettingsModel::setLeaderAltReverse(bool v) {
    if (leaderAltReverse_ == v)
        return;
    leaderAltReverse_ = v;
    Q_EMIT leaderAltReverseChanged();
    save();
}
void SettingsModel::setLeaderAltGr(bool v) {
    if (leaderAltGr_ == v)
        return;
    if (!v && !allowLeaderOff(leaderAltGr_))
        return;
    leaderAltGr_ = v;
    Q_EMIT leaderAltGrChanged();
    save();
}
void SettingsModel::setLeaderAltGrReverse(bool v) {
    if (leaderAltGrReverse_ == v)
        return;
    leaderAltGrReverse_ = v;
    Q_EMIT leaderAltGrReverseChanged();
    save();
}
void SettingsModel::setLeaderLeftReverse(bool v) {
    if (leaderLeftReverse_ == v)
        return;
    leaderLeftReverse_ = v;
    Q_EMIT leaderLeftReverseChanged();
    save();
}
void SettingsModel::setLeaderRightReverse(bool v) {
    if (leaderRightReverse_ == v)
        return;
    leaderRightReverse_ = v;
    Q_EMIT leaderRightReverseChanged();
    save();
}
void SettingsModel::setLeaderUpReverse(bool v) {
    if (leaderUpReverse_ == v)
        return;
    leaderUpReverse_ = v;
    Q_EMIT leaderUpReverseChanged();
    save();
}
void SettingsModel::setLeaderDownReverse(bool v) {
    if (leaderDownReverse_ == v)
        return;
    leaderDownReverse_ = v;
    Q_EMIT leaderDownReverseChanged();
    save();
}
void SettingsModel::setCustomKey1Enabled(bool v) {
    if (customKey1Enabled_ == v)
        return;
    if (!v && !allowLeaderOff(customKey1Enabled_ && customKey1HasKey()))
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
void SettingsModel::setCustomKey1Reverse(bool v) {
    if (customKey1Reverse_ == v)
        return;
    customKey1Reverse_ = v;
    Q_EMIT customKey1ReverseChanged();
    save();
}
void SettingsModel::setCustomKey2Enabled(bool v) {
    if (customKey2Enabled_ == v)
        return;
    if (!v && !allowLeaderOff(customKey2Enabled_ && customKey2HasKey()))
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
void SettingsModel::setCustomKey2Reverse(bool v) {
    if (customKey2Reverse_ == v)
        return;
    customKey2Reverse_ = v;
    Q_EMIT customKey2ReverseChanged();
    save();
}
// The capture rejects a held modifier, but it cannot reject CapsLock: Qt does
// not report it in the event's modifiers at all. So a press under CapsLock still
// arrives as 'A' rather than 'a'. Fold the character down here, where Qt's full
// Unicode case mapping is available and handles 'Ä' as readily as 'A'. The
// keycode is unaffected either way, so only the label and the mapped-input
// collision check depend on getting this right.
static QString leaderChar(const QString &raw) {
    if (!SettingsModel::isValidLeaderKey(raw))
        return QString();
    const QString folded = raw.toLower();
    // Full case mapping can turn one codepoint into two: Turkish 'İ' (U+0130)
    // folds to 'i' plus a combining dot. That would break the single-codepoint
    // invariant the label and the collision check rely on, and the field would
    // then flag a perfectly good leader as invalid. Keep the unfolded character
    // in that case; it is still exactly one codepoint.
    return SettingsModel::isValidLeaderKey(folded) ? folded : raw;
}

// The only way a keycode enters the config: both halves land together, then one
// save. A code that cannot name a pressable key is stored as kNoKeyCode, which
// reads as "no key assigned" rather than as a leader that can never fire.
void SettingsModel::captureCustomKey1(const QString &ch, int code) {
    const int newCode = fcitx::isUsableKeyCode(code) ? code : kNoKeyCode;
    // Unassigning the key (kNoKeyCode) drops this leader's effectiveness, so
    // guard it exactly like the enable-off case: refuse when it is the sole
    // effective leader. Assigning a real key only adds effectiveness, never
    // guarded.
    if (newCode == kNoKeyCode &&
        !allowLeaderOff(customKey1Enabled_ && customKey1HasKey()))
        return;
    customKey1Code_ = newCode;
    customKey1_ = leaderChar(ch);
    Q_EMIT customKey1CodeChanged();
    Q_EMIT customKey1Changed();
    save();
}
void SettingsModel::captureCustomKey2(const QString &ch, int code) {
    const int newCode = fcitx::isUsableKeyCode(code) ? code : kNoKeyCode;
    if (newCode == kNoKeyCode &&
        !allowLeaderOff(customKey2Enabled_ && customKey2HasKey()))
        return;
    customKey2Code_ = newCode;
    customKey2_ = leaderChar(ch);
    Q_EMIT customKey2CodeChanged();
    Q_EMIT customKey2Changed();
    save();
}
namespace {
// X keycodes (evdev code + 8) of the no-character navigation keys offered as
// custom leaders. These are the values captured as nativeScanCode and matched
// by the addon; on Linux/fcitx5 they are stable.
constexpr int kKeyHome = 110;
constexpr int kKeyEnd = 115;
constexpr int kKeyPageUp = 112;
constexpr int kKeyPageDown = 117;
constexpr int kKeyInsert = 118;
constexpr int kKeyMenu = 135;
} // namespace
QString SettingsModel::specialLeaderName(int keyCode) const {
    switch (keyCode) {
    case kKeyHome:
        return QStringLiteral("Home");
    case kKeyEnd:
        return QStringLiteral("End");
    case kKeyPageUp:
        return QStringLiteral("Page Up");
    case kKeyPageDown:
        return QStringLiteral("Page Down");
    case kKeyInsert:
        return QStringLiteral("Insert");
    case kKeyMenu:
        return QStringLiteral("Menu");
    default:
        return QString();
    }
}
void SettingsModel::clearCustomKey1() {
    if (customKey1Code_ == kNoKeyCode && customKey1_.isEmpty())
        return; // already clear
    if (!allowLeaderOff(customKey1Enabled_ && customKey1HasKey()))
        return; // would remove the last effective leader
    customKey1Code_ = kNoKeyCode;
    customKey1_.clear();
    Q_EMIT customKey1CodeChanged();
    Q_EMIT customKey1Changed();
    save();
}
void SettingsModel::clearCustomKey2() {
    if (customKey2Code_ == kNoKeyCode && customKey2_.isEmpty())
        return; // already clear
    if (!allowLeaderOff(customKey2Enabled_ && customKey2HasKey()))
        return; // would remove the last effective leader
    customKey2Code_ = kNoKeyCode;
    customKey2_.clear();
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
    // The daemon needs it: only the at-caret placement makes it write the
    // candidate-window theme on an automatic switch.
    overlayClient_.sendReloadConfig();
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
    // Same reason as the placement: it decides whether an automatic switch
    // touches the candidate window at all.
    overlayClient_.sendReloadConfig();
}

void SettingsModel::applyCaretTheme(const QString &background,
                                    const QString &text,
                                    const QString &highlight,
                                    const QString &onHighlight,
                                    const QString &border) {
    if (!schnelle_umlaute::caret::apply(background, text, highlight,
                                        onHighlight, border))
        Q_EMIT errorOccurred(tr("Could not write the candidate window theme"));
}

void SettingsModel::clearCaretTheme() {
    if (!schnelle_umlaute::caret::restore())
        Q_EMIT errorOccurred(
            tr("Could not restore the previous candidate window theme"));
}

void SettingsModel::setSortByFrequency(bool v) {
    if (sortByFrequency_ == v)
        return;
    sortByFrequency_ = v;
    Q_EMIT sortByFrequencyChanged();
    save();
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
    pushEffectiveTheme();
}

void SettingsModel::setThemeAuto(bool v) {
    if (themeAuto_ == v)
        return;
    themeAuto_ = v;
    Q_EMIT themeAutoChanged();
    save();
    pushEffectiveTheme();
}

void SettingsModel::setThemeLight(const QString &v) {
    if (!isValidTheme(v) || themeLight_ == v)
        return;
    themeLight_ = v;
    Q_EMIT themeLightChanged();
    save();
    pushEffectiveTheme();
}

void SettingsModel::setThemeDark(const QString &v) {
    if (!isValidTheme(v) || themeDark_ == v)
        return;
    themeDark_ = v;
    Q_EMIT themeDarkChanged();
    save();
    pushEffectiveTheme();
}

// Qt's report translated into the framework-free enum the derivation takes.
// Unknown on Qt below 6.5, which has no colour-scheme hint at all.
schnelle_umlaute::SystemScheme SettingsModel::systemScheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto *hints = QGuiApplication::styleHints();
    if (!hints)
        return schnelle_umlaute::SystemScheme::Unknown;
    switch (hints->colorScheme()) {
    case Qt::ColorScheme::Light:
        return schnelle_umlaute::SystemScheme::Light;
    case Qt::ColorScheme::Dark:
        return schnelle_umlaute::SystemScheme::Dark;
    default:
        return schnelle_umlaute::SystemScheme::Unknown;
    }
#else
    return schnelle_umlaute::SystemScheme::Unknown;
#endif
}

QString SettingsModel::effectiveTheme() const {
    return schnelle_umlaute::effectiveTheme(themeAuto_, theme_, themeLight_,
                                            themeDark_, systemScheme());
}

// Two messages, two jobs. SetTheme renders the new palette straight away, so a
// pick is visible without a round trip through the file. ReloadConfig makes the
// daemon re-read what was just saved, so its own derivation is current for the
// next desktop light/dark switch, which usually happens with the editor closed
// and nobody left to push anything.
void SettingsModel::pushEffectiveTheme() {
    overlayClient_.sendTheme(effectiveTheme());
    overlayClient_.sendReloadConfig();
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
            else if (key == "SpaceReverse")
                leaderSpaceReverse_ = fromBool(val);
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
            else if (key == "AltReverse")
                leaderAltReverse_ = fromBool(val);
            else if (key == "AltGr")
                leaderAltGr_ = fromBool(val);
            else if (key == "AltGrReverse")
                leaderAltGrReverse_ = fromBool(val);
            else if (key == "LeftReverse")
                leaderLeftReverse_ = fromBool(val);
            else if (key == "RightReverse")
                leaderRightReverse_ = fromBool(val);
            else if (key == "UpReverse")
                leaderUpReverse_ = fromBool(val);
            else if (key == "DownReverse")
                leaderDownReverse_ = fromBool(val);
        } else if (section == QLatin1String("Leader/Custom")) {
            if (key == "CustomKeyEnabled")
                customKey1Enabled_ = fromBool(val);
            else if (key == "CustomKey")
                customKey1_ = val;
            else if (key == "CustomKeyCode")
                customKey1Code_ = toKeyCode(val);
            else if (key == "CustomKeyReverse")
                customKey1Reverse_ = fromBool(val);
            else if (key == "CustomKey2Enabled")
                customKey2Enabled_ = fromBool(val);
            else if (key == "CustomKey2")
                customKey2_ = val;
            else if (key == "CustomKey2Code")
                customKey2Code_ = toKeyCode(val);
            else if (key == "CustomKey2Reverse")
                customKey2Reverse_ = fromBool(val);
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
            else if (key == QLatin1String(keys::kPlacement)) {
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
            else if (key == QLatin1String(keys::kCaretTheme))
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
        } else if (section == QLatin1String("Behavior")) {
            if (key == "SortByFrequency")
                sortByFrequency_ = fromBool(val);
        } else if (section == QLatin1String(keys::kThemeSection)) {
            if (key == QLatin1String(keys::kTheme) && isValidTheme(val))
                theme_ = val;
            else if (key == QLatin1String(keys::kThemeAuto))
                themeAuto_ = fromBool(val);
            else if (key == QLatin1String(keys::kThemeLight) &&
                     isValidTheme(val))
                themeLight_ = val;
            else if (key == QLatin1String(keys::kThemeDark) &&
                     isValidTheme(val))
                themeDark_ = val;
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
    Q_EMIT leaderSpaceReverseChanged();
    Q_EMIT leaderLeftChanged();
    Q_EMIT leaderRightChanged();
    Q_EMIT leaderUpChanged();
    Q_EMIT leaderDownChanged();
    Q_EMIT leaderAltChanged();
    Q_EMIT leaderAltReverseChanged();
    Q_EMIT leaderAltGrChanged();
    Q_EMIT leaderAltGrReverseChanged();
    Q_EMIT leaderLeftReverseChanged();
    Q_EMIT leaderRightReverseChanged();
    Q_EMIT leaderUpReverseChanged();
    Q_EMIT leaderDownReverseChanged();
    Q_EMIT customKey1EnabledChanged();
    Q_EMIT customKey1Changed();
    Q_EMIT customKey1CodeChanged();
    Q_EMIT customKey1ReverseChanged();
    Q_EMIT customKey2EnabledChanged();
    Q_EMIT customKey2Changed();
    Q_EMIT customKey2CodeChanged();
    Q_EMIT customKey2ReverseChanged();
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
    Q_EMIT sortByFrequencyChanged();
}

void SettingsModel::save() {
    QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reportSaveError(f.errorString());
        return;
    }
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
    out << "# Space (+ reverse direction)\n"
        << "Space=" << toBool(leaderSpace_) << "\n"
        << "SpaceReverse=" << toBool(leaderSpaceReverse_) << "\n";
    out << "# Left Arrow (+ reverse direction)\n"
        << "Left=" << toBool(leaderLeft_) << "\n"
        << "LeftReverse=" << toBool(leaderLeftReverse_) << "\n";
    out << "# Right Arrow (+ reverse direction)\n"
        << "Right=" << toBool(leaderRight_) << "\n"
        << "RightReverse=" << toBool(leaderRightReverse_) << "\n";
    out << "# Up Arrow (+ reverse direction)\n"
        << "Up=" << toBool(leaderUp_) << "\n"
        << "UpReverse=" << toBool(leaderUpReverse_) << "\n";
    out << "# Down Arrow (+ reverse direction)\n"
        << "Down=" << toBool(leaderDown_) << "\n"
        << "DownReverse=" << toBool(leaderDownReverse_) << "\n";
    out << "# Alt (+ reverse direction)\n"
        << "Alt=" << toBool(leaderAlt_) << "\n"
        << "AltReverse=" << toBool(leaderAltReverse_) << "\n";
    out << "# AltGr (+ reverse direction)\n"
        << "AltGr=" << toBool(leaderAltGr_) << "\n"
        << "AltGrReverse=" << toBool(leaderAltGrReverse_) << "\n";
    out << "\n";
    out << "[Leader/Custom]\n";
    out << "# Custom Leader 1\n"
        << "CustomKeyEnabled=" << toBool(customKey1Enabled_) << "\n";
    out << "#   \xe2\x86\xb3 Key\n"
        << "CustomKey=" << customKey1_ << "\n";
    out << "#   \xe2\x86\xb3 Key code\n"
        << "CustomKeyCode=" << customKey1Code_ << "\n";
    out << "#   \xe2\x86\xb3 reverse direction\n"
        << "CustomKeyReverse=" << toBool(customKey1Reverse_) << "\n";
    out << "# Custom Leader 2 (hand-split)\n"
        << "CustomKey2Enabled=" << toBool(customKey2Enabled_) << "\n";
    out << "#   \xe2\x86\xb3 Key\n"
        << "CustomKey2=" << customKey2_ << "\n";
    out << "#   \xe2\x86\xb3 Key code\n"
        << "CustomKey2Code=" << customKey2Code_ << "\n";
    out << "#   \xe2\x86\xb3 reverse direction\n"
        << "CustomKey2Reverse=" << toBool(customKey2Reverse_) << "\n";
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
        << keys::kPlacement << "=" << overlayPlacement_ << "\n";
    out << "# Show timing progress bar\n"
        << "ProgressBar=" << toBool(overlayProgressBar_) << "\n";
    out << "# Match fcitx5 candidate window to the editor theme (caret mode)\n"
        << keys::kCaretTheme << "=" << toBool(overlayCaretTheme_) << "\n";
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
    out << "\n[Behavior]\n";
    out << "# Sort each key's variants by how often you use them\n"
        << "SortByFrequency=" << toBool(sortByFrequency_) << "\n";
    out << "\n[" << keys::kThemeSection << "]\n";
    out << "# UI theme, see the editor's theme picker for the full list\n"
        << keys::kTheme << "=" << theme_ << "\n";
    out << "# Follow the desktop's light/dark setting instead of Theme=\n"
        << keys::kThemeAuto << "=" << toBool(themeAuto_) << "\n";
    out << "# The pair Auto= switches between\n"
        << keys::kThemeLight << "=" << themeLight_ << "\n"
        << keys::kThemeDark << "=" << themeDark_ << "\n";
    out.flush();
    // Only a committed file is worth reloading for: a failed commit leaves the
    // old config on disk, so telling the addon to re-read it would just make it
    // load the previous values back while the UI shows the new ones.
    if (!f.commit()) {
        reportSaveError(f.errorString());
        return;
    }
    clearSaveError();
    reloadFcitx();
}

void SettingsModel::reportSaveError(const QString &message) {
    // Same cause, still inside the window: a burst (a slider drag), and the
    // message it would repeat is the one already on screen.
    if (lastSaveError_ == message && lastSaveErrorAt_.isValid() &&
        lastSaveErrorAt_.elapsed() < kSaveErrorRepeatMs)
        return;
    lastSaveError_ = message;
    lastSaveErrorAt_.restart();
    Q_EMIT errorOccurred(message);
}

void SettingsModel::clearSaveError() {
    lastSaveError_.clear();
    lastSaveErrorAt_.invalidate();
}

void SettingsModel::reloadFcitx() { reloadSchnelleUmlauteAddon(); }
