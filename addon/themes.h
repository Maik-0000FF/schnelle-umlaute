#ifndef SCHNELLE_UMLAUTE_THEMES_H
#define SCHNELLE_UMLAUTE_THEMES_H

#include <QLatin1String>
#include <QString>

namespace schnelle_umlaute {

// Single source of truth for theme names. Must stay in sync with the
// palettes dicts in addon/editor/Theme.qml and addon/overlay/Overlay.qml
// — adding a name here without adding a matching palette entry there
// lets the name pass isValidTheme() but renders with the fallback.
inline bool isValidTheme(const QString &name) {
    return name == QLatin1String("schnelle-umlaute") ||
           name == QLatin1String("dark") ||
           name == QLatin1String("light") ||
           name == QLatin1String("contrast");
}

} // namespace schnelle_umlaute

#endif
