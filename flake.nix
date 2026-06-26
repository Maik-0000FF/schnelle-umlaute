{
  description = "Schnelle Umlaute: a fcitx5 addon for fast accent/umlaut input (hold+space), with an optional Wayland overlay and a standalone QML editor";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      mkPkg =
        pkgs:
        pkgs.callPackage ./nix/package.nix {
          src = self;
          # Project version from addon/CMakeLists.txt; the rev pins the exact tree.
          version = "1.2.2";
        };
    in
    {
      packages = forAllSystems (
        system:
        let
          p = mkPkg nixpkgs.legacyPackages.${system};
        in
        {
          schnelle-umlaute = p;
          default = p;
        }
      );

      # Consumers can add this overlay and use pkgs.schnelle-umlaute, e.g. in
      # i18n.inputMethod.fcitx5.addons.
      overlays.default = _final: prev: {
        schnelle-umlaute = mkPkg prev;
      };
    };
}
