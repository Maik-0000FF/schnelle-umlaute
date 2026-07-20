#ifndef SCHNELLE_UMLAUTE_EDITOR_SETTINGS_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_SETTINGS_MODEL_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "OverlayDBusClient.h"
// Single source for kNoKeyCode and the evdev+8 keycode convention, so the
// editor writes exactly what the engine reads.
#include "../src/hand_classifier.h"

using fcitx::kNoKeyCode;

class SettingsModel : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // delayLowercase / delayUppercase are the accent window's UPPER bound
    // (max). delayLowercaseMin / delayUppercaseMin are the lower bound
    // (minimum hold, default 0). Together each case forms a [min, max] window.
    Q_PROPERTY(int delayLowercase READ delayLowercase WRITE setDelayLowercase
                   NOTIFY delayLowercaseChanged)
    Q_PROPERTY(int delayUppercase READ delayUppercase WRITE setDelayUppercase
                   NOTIFY delayUppercaseChanged)
    Q_PROPERTY(int delayLowercaseMin READ delayLowercaseMin WRITE
                   setDelayLowercaseMin NOTIFY delayLowercaseMinChanged)
    Q_PROPERTY(int delayUppercaseMin READ delayUppercaseMin WRITE
                   setDelayUppercaseMin NOTIFY delayUppercaseMinChanged)

    Q_PROPERTY(bool leaderSpace READ leaderSpace WRITE setLeaderSpace NOTIFY
                   leaderSpaceChanged)
    Q_PROPERTY(bool leaderLeft READ leaderLeft WRITE setLeaderLeft NOTIFY
                   leaderLeftChanged)
    Q_PROPERTY(bool leaderRight READ leaderRight WRITE setLeaderRight NOTIFY
                   leaderRightChanged)
    Q_PROPERTY(
        bool leaderUp READ leaderUp WRITE setLeaderUp NOTIFY leaderUpChanged)
    Q_PROPERTY(bool leaderDown READ leaderDown WRITE setLeaderDown NOTIFY
                   leaderDownChanged)
    Q_PROPERTY(bool leaderAlt READ leaderAlt WRITE setLeaderAlt NOTIFY
                   leaderAltChanged)
    Q_PROPERTY(bool leaderAltReverse READ leaderAltReverse WRITE
                   setLeaderAltReverse NOTIFY leaderAltReverseChanged)
    Q_PROPERTY(bool leaderAltGr READ leaderAltGr WRITE setLeaderAltGr NOTIFY
                   leaderAltGrChanged)
    Q_PROPERTY(bool leaderAltGrReverse READ leaderAltGrReverse WRITE
                   setLeaderAltGrReverse NOTIFY leaderAltGrReverseChanged)
    // Per-arrow cycle direction: false steps forward, true steps backward.
    // Orthogonal to the enable flags above, so any arrow can go either way.
    Q_PROPERTY(bool leaderLeftReverse READ leaderLeftReverse WRITE
                   setLeaderLeftReverse NOTIFY leaderLeftReverseChanged)
    Q_PROPERTY(bool leaderRightReverse READ leaderRightReverse WRITE
                   setLeaderRightReverse NOTIFY leaderRightReverseChanged)
    Q_PROPERTY(bool leaderUpReverse READ leaderUpReverse WRITE
                   setLeaderUpReverse NOTIFY leaderUpReverseChanged)
    Q_PROPERTY(bool leaderDownReverse READ leaderDownReverse WRITE
                   setLeaderDownReverse NOTIFY leaderDownReverseChanged)

    // Each custom leader is a physical key. One key press in CustomLeaderRow
    // captures both halves: the character, which is only displayed, and the
    // keycode, which is what the addon matches and hand-classifies.
    Q_PROPERTY(bool customKey1Enabled READ customKey1Enabled WRITE
                   setCustomKey1Enabled NOTIFY customKey1EnabledChanged)
    Q_PROPERTY(QString customKey1 READ customKey1 WRITE setCustomKey1 NOTIFY
                   customKey1Changed)
    // Read-only: a keycode is only ever set by capturing a key press, which
    // captureCustomKey1() stores together with its character. A writable
    // property would be a second way in, one that could store a code no key can
    // produce.
    Q_PROPERTY(int customKey1Code READ customKey1Code NOTIFY customKey1CodeChanged)
    // Whether a physical key has been captured. QML asks this instead of
    // comparing the code against kNoKeyCode, so the sentinel value stays
    // defined in exactly one place (hand_classifier.h) and is never restated.
    Q_PROPERTY(bool customKey1HasKey READ customKey1HasKey NOTIFY
                   customKey1CodeChanged)
    Q_PROPERTY(bool customKey1Reverse READ customKey1Reverse WRITE
                   setCustomKey1Reverse NOTIFY customKey1ReverseChanged)
    Q_PROPERTY(bool customKey2Enabled READ customKey2Enabled WRITE
                   setCustomKey2Enabled NOTIFY customKey2EnabledChanged)
    Q_PROPERTY(QString customKey2 READ customKey2 WRITE setCustomKey2 NOTIFY
                   customKey2Changed)
    Q_PROPERTY(int customKey2Code READ customKey2Code NOTIFY customKey2CodeChanged)
    Q_PROPERTY(bool customKey2HasKey READ customKey2HasKey NOTIFY
                   customKey2CodeChanged)
    Q_PROPERTY(bool customKey2Reverse READ customKey2Reverse WRITE
                   setCustomKey2Reverse NOTIFY customKey2ReverseChanged)

    Q_PROPERTY(QString appFilterMode READ appFilterMode WRITE setAppFilterMode
                   NOTIFY appFilterModeChanged)
    Q_PROPERTY(QStringList blacklist READ blacklist NOTIFY blacklistChanged)
    Q_PROPERTY(QStringList whitelist READ whitelist NOTIFY whitelistChanged)

    Q_PROPERTY(bool overlayEnabled READ overlayEnabled WRITE setOverlayEnabled
                   NOTIFY overlayEnabledChanged)
    Q_PROPERTY(bool overlayShowOnTrigger READ overlayShowOnTrigger WRITE
                   setOverlayShowOnTrigger NOTIFY overlayShowOnTriggerChanged)
    Q_PROPERTY(QString overlayPlacement READ overlayPlacement WRITE
                   setOverlayPlacement NOTIFY overlayPlacementChanged)
    Q_PROPERTY(bool overlayProgressBar READ overlayProgressBar WRITE
                   setOverlayProgressBar NOTIFY overlayProgressBarChanged)
    Q_PROPERTY(QString overlayPosition READ overlayPosition WRITE
                   setOverlayPosition NOTIFY overlayPositionChanged)
    // Opt-in (caret placement only): style fcitx5's candidate window to match
    // the editor theme. Writes a generated fcitx5 theme + points classicui at
    // it. Persisted so the toggle state round-trips; the actual styling lives
    // in the written files, not here.
    //
    // Asymmetric on purpose: setOverlayCaretTheme(true) only persists the flag.
    // Generating the theme files needs the active Theme.* colours, so it is
    // driven from QML via applyCaretTheme(); there is no C++-side counterpart
    // to the false branch (which calls clearCaretTheme() directly). A manual
    // CaretTheme=True in the config therefore persists but writes no files
    // until the editor next applies the theme.
    Q_PROPERTY(bool overlayCaretTheme READ overlayCaretTheme WRITE
                   setOverlayCaretTheme NOTIFY overlayCaretThemeChanged)

    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)

    // Compositor capability for wlr-layer-shell. Sampled once at
    // construction from XDG_SESSION_TYPE / XDG_CURRENT_DESKTOP. Drives
    // whether the overlay toggle is shown as enabled in the UI.
    Q_PROPERTY(bool layerShellAvailable READ layerShellAvailable CONSTANT)
    Q_PROPERTY(QString layerShellSession READ layerShellSession CONSTANT)
    Q_PROPERTY(QString layerShellReason READ layerShellReason CONSTANT)

public:
    explicit SettingsModel(QObject *parent = nullptr);

    int delayLowercase() const { return delayLowercase_; }
    int delayUppercase() const { return delayUppercase_; }
    int delayLowercaseMin() const { return delayLowercaseMin_; }
    int delayUppercaseMin() const { return delayUppercaseMin_; }
    bool leaderSpace() const { return leaderSpace_; }
    bool leaderLeft() const { return leaderLeft_; }
    bool leaderRight() const { return leaderRight_; }
    bool leaderUp() const { return leaderUp_; }
    bool leaderDown() const { return leaderDown_; }
    bool leaderAlt() const { return leaderAlt_; }
    bool leaderAltReverse() const { return leaderAltReverse_; }
    bool leaderAltGr() const { return leaderAltGr_; }
    bool leaderAltGrReverse() const { return leaderAltGrReverse_; }
    bool leaderLeftReverse() const { return leaderLeftReverse_; }
    bool leaderRightReverse() const { return leaderRightReverse_; }
    bool leaderUpReverse() const { return leaderUpReverse_; }
    bool leaderDownReverse() const { return leaderDownReverse_; }
    bool customKey1Enabled() const { return customKey1Enabled_; }
    QString customKey1() const { return customKey1_; }
    int customKey1Code() const { return customKey1Code_; }
    bool customKey1HasKey() const { return customKey1Code_ != kNoKeyCode; }
    bool customKey1Reverse() const { return customKey1Reverse_; }
    bool customKey2Enabled() const { return customKey2Enabled_; }
    QString customKey2() const { return customKey2_; }
    int customKey2Code() const { return customKey2Code_; }
    bool customKey2HasKey() const { return customKey2Code_ != kNoKeyCode; }
    bool customKey2Reverse() const { return customKey2Reverse_; }

    // One captured key press sets both halves of a leader. Going through the
    // individual setters would write the config file twice and, in between,
    // leave the new keycode paired with the previous key's character.
    Q_INVOKABLE void captureCustomKey1(const QString &ch, int code);
    Q_INVOKABLE void captureCustomKey2(const QString &ch, int code);
    QString appFilterMode() const { return appFilterMode_; }
    QStringList blacklist() const { return blacklist_; }
    QStringList whitelist() const { return whitelist_; }
    bool overlayEnabled() const { return overlayEnabled_; }
    bool overlayShowOnTrigger() const { return overlayShowOnTrigger_; }
    QString overlayPlacement() const { return overlayPlacement_; }
    bool overlayProgressBar() const { return overlayProgressBar_; }
    QString overlayPosition() const { return overlayPosition_; }
    bool overlayCaretTheme() const { return overlayCaretTheme_; }
    QString theme() const { return theme_; }

    // Generate an fcitx5 theme from the given editor-palette colors (hex
    // strings) and point classicui at it, or restore the user's previous
    // classicui theme. Called from QML with the active Theme.* colors so
    // Theme.qml stays the single colour source. Colours are #rrggbb.
    Q_INVOKABLE void applyCaretTheme(const QString &background,
                                     const QString &text,
                                     const QString &highlight,
                                     const QString &onHighlight,
                                     const QString &border);
    Q_INVOKABLE void clearCaretTheme();
    bool layerShellAvailable() const { return layerShellAvailable_; }
    QString layerShellSession() const { return layerShellSession_; }
    QString layerShellReason() const { return layerShellReason_; }

    void setDelayLowercase(int v);
    void setDelayUppercase(int v);
    void setDelayLowercaseMin(int v);
    void setDelayUppercaseMin(int v);
    void setLeaderSpace(bool v);
    void setLeaderLeft(bool v);
    void setLeaderRight(bool v);
    void setLeaderUp(bool v);
    void setLeaderDown(bool v);
    void setLeaderAlt(bool v);
    void setLeaderAltReverse(bool v);
    void setLeaderAltGr(bool v);
    void setLeaderAltGrReverse(bool v);
    void setLeaderLeftReverse(bool v);
    void setLeaderRightReverse(bool v);
    void setLeaderUpReverse(bool v);
    void setLeaderDownReverse(bool v);
    void setCustomKey1Enabled(bool v);
    void setCustomKey1(const QString &v);
    void setCustomKey1Reverse(bool v);
    void setCustomKey2Enabled(bool v);
    void setCustomKey2(const QString &v);
    void setCustomKey2Reverse(bool v);
    void setAppFilterMode(const QString &v);
    void setOverlayEnabled(bool v);
    void setOverlayShowOnTrigger(bool v);
    void setOverlayPlacement(const QString &v);
    void setOverlayProgressBar(bool v);
    void setOverlayPosition(const QString &v);
    void setOverlayCaretTheme(bool v);
    void setTheme(const QString &v);

    static bool isValidTheme(const QString &name);
    // The three OverlayPlacement enum names the fcitx5 addon understands
    // (config.h: Grid|MouseCursor|TextCaret). Guards load()/setter against a
    // corrupt or hand-edited value that would diverge editor and addon.
    static bool isValidPlacement(const QString &name);

    Q_INVOKABLE void addBlacklistEntry(const QString &entry);
    Q_INVOKABLE void removeBlacklistEntry(int index);
    Q_INVOKABLE void addWhitelistEntry(const QString &entry);
    Q_INVOKABLE void removeWhitelistEntry(int index);
    Q_INVOKABLE bool isActiveLeaderKey(const QString &key) const;

    static bool isValidLeaderKey(const QString &s);

Q_SIGNALS:
    void delayLowercaseChanged();
    void delayUppercaseChanged();
    void delayLowercaseMinChanged();
    void delayUppercaseMinChanged();
    void leaderSpaceChanged();
    void leaderLeftChanged();
    void leaderRightChanged();
    void leaderUpChanged();
    void leaderDownChanged();
    void leaderAltChanged();
    void leaderAltReverseChanged();
    void leaderAltGrChanged();
    void leaderAltGrReverseChanged();
    void leaderLeftReverseChanged();
    void leaderRightReverseChanged();
    void leaderUpReverseChanged();
    void leaderDownReverseChanged();
    void customKey1EnabledChanged();
    void customKey1Changed();
    void customKey1CodeChanged();
    void customKey1ReverseChanged();
    void customKey2EnabledChanged();
    void customKey2Changed();
    void customKey2CodeChanged();
    void customKey2ReverseChanged();
    void leadersChanged();
    void appFilterModeChanged();
    void blacklistChanged();
    void whitelistChanged();
    void overlayEnabledChanged();
    void overlayShowOnTriggerChanged();
    void overlayPlacementChanged();
    void overlayProgressBarChanged();
    void overlayPositionChanged();
    void overlayCaretThemeChanged();
    void themeChanged();

private:
    void load();
    void save();
    void reloadFcitx();
    // Write the generated fcitx5 theme.conf from the given colors and point
    // classicui at it (backing up the user's previous classicui theme first),
    // or restore that backup. Helpers for applyCaretTheme/clearCaretTheme.
    void writeCaretThemeFiles(const QString &background, const QString &text,
                              const QString &highlight,
                              const QString &onHighlight,
                              const QString &border);
    void restoreClassicUiTheme();

    int delayLowercase_ = 400;
    int delayUppercase_ = 700;
    int delayLowercaseMin_ = 0;
    int delayUppercaseMin_ = 0;
    bool leaderSpace_ = true;
    bool leaderLeft_ = false;
    bool leaderRight_ = false;
    bool leaderUp_ = false;
    bool leaderDown_ = false;
    bool leaderAlt_ = false;
    bool leaderAltReverse_ = false;
    bool leaderAltGr_ = false;
    bool leaderAltGrReverse_ = false;
    bool leaderLeftReverse_ = false;
    bool leaderRightReverse_ = false;
    bool leaderUpReverse_ = false;
    bool leaderDownReverse_ = false;
    bool customKey1Enabled_ = false;
    QString customKey1_;
    int customKey1Code_ = kNoKeyCode;
    bool customKey1Reverse_ = false;
    bool customKey2Enabled_ = false;
    QString customKey2_;
    int customKey2Code_ = kNoKeyCode;
    bool customKey2Reverse_ = false;
    QString appFilterMode_ = "Disabled";
    QStringList blacklist_;
    QStringList whitelist_;
    bool overlayEnabled_ = false;
    bool overlayShowOnTrigger_ = false;
    QString overlayPlacement_ = "Grid";
    bool overlayProgressBar_ = false;
    QString overlayPosition_ = "TopCol4";
    bool overlayCaretTheme_ = false;
    QString theme_ = "schnelle-umlaute";
    bool layerShellAvailable_ = false;
    QString layerShellSession_;
    QString layerShellReason_;
    OverlayDBusClient overlayClient_;
};

#endif
