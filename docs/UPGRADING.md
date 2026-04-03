# Upgrading from v0.x to v1.0.0

Version 1.0.0 introduces a new configuration format that is **not compatible** with previous versions:

- **Mappings** are now stored in `~/.config/fcitx5/schnelle-umlaute/mappings.txt` using `Input=Output` format (previously stored as sections in `schnelle-umlaute.conf`)
- **Settings** (delays, leader keys) remain in `~/.config/fcitx5/conf/schnelle-umlaute.conf`

## Recommended upgrade path

Run `./uninstall.sh` first (choose "y" to remove user configuration), then `./install.sh`. Your mappings will be reset to defaults — reconfigure them via `fcitx5-config-qt` or edit `mappings.txt` manually.
