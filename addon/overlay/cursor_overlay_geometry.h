#ifndef SCHNELLE_UMLAUTE_CURSOR_OVERLAY_GEOMETRY_H
#define SCHNELLE_UMLAUTE_CURSOR_OVERLAY_GEOMETRY_H

// Pure geometry + wire-format helpers for the "show at mouse cursor"
// overlay mode. Deliberately free of Qt/DBus/fcitx5 deps so it is
// unit-testable (see tests/testcursorgeometry.cpp) and shared by the
// overlay daemon's renderer without dragging in the layer-shell stack.

#include <algorithm>
#include <string>

namespace schnelle_umlaute {

// Marker the addon prepends to the position string when the cursor mode is
// on: "Cursor:" + the grid fallback (e.g. "Cursor:TopCol4"). Defined once so
// the addon (writer) and the daemon (reader) can't drift.
inline const std::string &cursorPositionPrefix() {
    static const std::string kPrefix = "Cursor:";
    return kPrefix;
}

struct CursorPositionSpec {
    // True when the daemon should try to place the overlay at the pointer.
    bool atCursor;
    // The plain grid position ("TopCol4", ...). When atCursor is true this is
    // the fallback used if the cursor can't be resolved; otherwise it is the
    // position itself.
    std::string grid;
};

// Split a position string into its cursor flag and the grid part. A leading
// "Cursor:" turns the flag on and is stripped; anything else passes through
// unchanged as a plain grid position.
inline CursorPositionSpec parseCursorPosition(const std::string &position) {
    const std::string &prefix = cursorPositionPrefix();
    if (position.rfind(prefix, 0) == 0) {
        return {true, position.substr(prefix.size())};
    }
    return {false, position};
}

struct CursorMargins {
    // Layer-shell margins for a Top|Left-anchored surface, in the anchored
    // output's local pixels.
    int left;
    int top;
};

// Place the overlay's LOWER-LEFT corner at the cursor: the surface extends up
// and to the right of the pointer. (cursorX, cursorY) is a global desktop
// pixel; (screenX, screenY, screenW, screenH) is the geometry of the output
// the cursor is on. The result is clamped so the whole surface stays on that
// output (near an edge the corner drifts off the cursor rather than letting
// the overlay spill off-screen or ask for the negative margins layer-shell
// would reject).
inline CursorMargins cursorMargins(int cursorX, int cursorY, int screenX,
                                   int screenY, int screenW, int screenH,
                                   int overlayW, int overlayH) {
    // Cursor in the output's local coordinate space.
    const int localX = cursorX - screenX;
    const int localY = cursorY - screenY;
    // Left edge at the cursor x; top edge one overlay-height above the cursor
    // y so the bottom edge sits on it.
    int left = localX;
    int top = localY - overlayH;
    const int maxLeft = std::max(0, screenW - overlayW);
    const int maxTop = std::max(0, screenH - overlayH);
    left = std::clamp(left, 0, maxLeft);
    top = std::clamp(top, 0, maxTop);
    return {left, top};
}

} // namespace schnelle_umlaute

#endif
