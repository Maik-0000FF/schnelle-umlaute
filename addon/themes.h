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
        QStringLiteral("eldritch")};
    return kThemes.contains(name);
}

} // namespace schnelle_umlaute

#endif
