# Upgrading

## Upgrading from v1.0.0 to v1.1.0

No migration required. v1.1.0 is fully config-compatible with v1.0.0 — your existing `mappings.txt` and `schnelle-umlaute.conf` continue to work unchanged.

New in v1.1.0: an optional **App Filter** that can disable the addon in selected applications (or restrict it to a whitelist). See [Configuration → App Filter](CONFIGURATION.md#app-filter).

## Upgrading from v0.x to v1.0.0

Version 1.0.0 introduced a new configuration format that is **not compatible** with previous versions:

- **Mappings** are now stored in `~/.config/fcitx5/schnelle-umlaute/mappings.txt` using `Input=Output` format (previously stored as sections in `schnelle-umlaute.conf`)
- **Settings** (delays, leader keys) remain in `~/.config/fcitx5/conf/schnelle-umlaute.conf`

### Recommended upgrade path

Run `./uninstall.sh` first (choose "y" to remove user configuration), then `./install.sh`. Your mappings will be reset to defaults — reconfigure them via `fcitx5-config-qt` or edit `mappings.txt` manually.
