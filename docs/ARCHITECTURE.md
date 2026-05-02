# Architecture

This is a **native Fcitx5 addon** written in **C++**, using the Fcitx5 InputMethodEngineV2 API.

## Key Components

- `addon/src/schnelle-umlaute.cpp` - Main addon logic with Hold & Wait implementation
- `addon/CMakeLists.txt` - Build configuration
- `addon/data/schnelle-umlaute.conf` - Fcitx5 addon registration

## How it works internally

1. Fcitx5 calls our `keyEvent()` handler for every key
2. App Filter check: if the current app is excluded by the configured blacklist or absent from the whitelist, the key passes through unchanged
3. When accent key (a/o/u/s) is pressed: suppress output, start timer
4. If Space within delay (400ms lowercase, 700ms uppercase): call `commitString(umlaut)` for direct insertion
5. If timeout or key released: call `commitString(normalLetter)`
6. No clipboard, no key simulation - pure text insertion!

## Comparison with Other Approaches

| Approach | Clipboard-Free | No Root | X11 | Wayland | Complexity |
|----------|---------------|---------|-----|---------|------------|
| **Fcitx5 Addon (This)** | ✅ | ✅ | ✅ | ✅ | Medium |
| evdev-rs + xclip | ❌ | ❌ | ✅ | ⚠️ | Low |
| IBus | ✅ | ✅ | ✅ | ✅ | High |
| XTest | ✅ | ❌ | ✅ | ❌ | Low |
