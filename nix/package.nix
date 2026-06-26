# Nix build of the schnelle-umlaute fcitx5 addon: the addon .so + fcitx5
# config, the standalone QML editor (schnelle-umlaute-editor) and the Wayland
# overlay daemon (schnelle-umlaute-overlay).
#
# `src` is the WHOLE repo (not just addon/): the editor and overlay reference
# ../docs (icon symlink + install), so the full tree must be present. The
# CMake project itself lives in addon/.
{
  stdenv,
  lib,
  cmake,
  pkg-config,
  gettext,
  kdePackages,
  fcitx5,
  libxkbcommon,
  qt6,
  src,
  version,
}:

stdenv.mkDerivation {
  pname = "fcitx5-schnelle-umlaute";
  inherit version src;

  # The CMake project is the addon/ subdirectory. `src` unpacks under a single
  # top-level directory (whose name differs between `self` and fetchFromGitHub),
  # so glob for its addon/ subdir instead of hard-coding the unpack name.
  setSourceRoot = "sourceRoot=$(echo */addon)";

  # The overlay's CMake hard-codes the DBus service install into
  # /usr/share/dbus-1/services, which the Nix sandbox forbids. Make it relative
  # so CMake prepends the $out prefix -> $out/share/dbus-1/services.
  postPatch = ''
    substituteInPlace overlay/CMakeLists.txt \
      --replace-fail "DESTINATION /usr/share/dbus-1/services" "DESTINATION share/dbus-1/services"
  '';

  nativeBuildInputs = [
    cmake
    pkg-config
    gettext
    kdePackages.extra-cmake-modules # ECM + Fcitx5*-CMake modules
    qt6.wrapQtAppsHook # wraps the Qt6/QML editor + overlay
  ];

  buildInputs = [
    fcitx5 # Fcitx5::Core
    kdePackages.fcitx5-qt # Qt6 integration
    kdePackages.layer-shell-qt # LayerShellQt: required to build the overlay daemon
    libxkbcommon
    qt6.qtbase # also provides QtDBus (editor + overlay)
    qt6.qtdeclarative # Qml/Quick/QuickControls2
    qt6.qtwayland # Wayland platform plugin (editor + overlay)
    qt6.qtsvg # likely removable now the in-app icon is PNG; verify before dropping
  ];

  cmakeFlags = [
    # Install into $out, not the read-only fcitx5 store path. The CMake default
    # (ON) installs into the found fcitx5's system paths, correct on FHS distros
    # but wrong on Nix; the fcitx5-with-addons wrapper merges $out.
    "-DFCITX_INSTALL_USE_FCITX_SYS_PATHS=OFF"
    "-DBUILD_TESTING=OFF"
  ];

  meta = {
    description = "Fast accent/umlaut input for fcitx5 (hold+space), with an optional Wayland overlay and a standalone QML editor";
    homepage = "https://github.com/Maik-0000FF/schnelle-umlaute";
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.linux;
    mainProgram = "schnelle-umlaute-editor";
  };
}
