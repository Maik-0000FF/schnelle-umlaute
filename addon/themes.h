#ifndef SCHNELLE_UMLAUTE_THEMES_H
#define SCHNELLE_UMLAUTE_THEMES_H

#include <QString>
#include <QStringList>

namespace schnelle_umlaute {

// The C++ side's list of accepted theme names, used by the editor's SettingsModel
// (setTheme guard) and the overlay's OverlayController (SetTheme guard). Must
// stay in sync with the palettes in addon/palette/Palettes.qml (the shared
// module both QML sides read): adding a name here without a matching palette
// entry there lets the name pass but renders with the fallback, and vice versa.
inline bool isValidTheme(const QString &name) {
    static const QStringList kThemes = {
        QStringLiteral("schnelle-umlaute"), QStringLiteral("dark"),
        QStringLiteral("light"),            QStringLiteral("contrast"),
        QStringLiteral("catppuccin-mocha"), QStringLiteral("catppuccin-latte"),
        QStringLiteral("nord"),             QStringLiteral("gruvbox-dark"),
        QStringLiteral("dracula"),          QStringLiteral("tokyo-night"),
        QStringLiteral("rose-pine"),        QStringLiteral("solarized-light"),
        QStringLiteral("eldritch"),         QStringLiteral("kanagawa")};
    return kThemes.contains(name);
}

// The theme every fresh config starts on, and the answer when the automatic
// mode has nothing to go on.
inline QString defaultTheme() {
    return QStringLiteral("schnelle-umlaute");
}

// The pair the automatic mode starts with. Both are the neutral house themes
// rather than one of the ports, so a first switch looks like the app and not
// like someone else's palette.
inline QString defaultLightTheme() { return QStringLiteral("light"); }
inline QString defaultDarkTheme() { return QStringLiteral("dark"); }

// What the desktop reports about its colour scheme. Mirrors Qt::ColorScheme
// without pulling QtGui in: this header is read by the fcitx5-free test binary
// too, and the callers translate their own source into it.
enum class SystemScheme { Unknown, Light, Dark };

// The theme actually rendered. With the automatic mode off this is simply the
// user's manual pick, which is why the manual choice is never overwritten while
// the mode is on: it is the value to come back to. With it on the pair decides,
// and a desktop that reports nothing (X11, older environments) falls back to
// the default theme rather than to one half of the pair, which would silently
// promote one of the two.
//
// A pair entry that is not a known theme falls back the same way, so a
// hand-edited config cannot leave the overlay on a nameless palette.
inline QString effectiveTheme(bool automatic, const QString &manual,
                              const QString &light, const QString &dark,
                              SystemScheme scheme) {
    if (!automatic)
        return manual;
    const QString &picked = scheme == SystemScheme::Light ? light : dark;
    if (scheme == SystemScheme::Unknown || !isValidTheme(picked))
        return defaultTheme();
    return picked;
}

} // namespace schnelle_umlaute

#endif
