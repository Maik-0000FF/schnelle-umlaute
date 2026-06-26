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
      mkPkg = pkgs: pkgs.callPackage ./nix/package.nix { src = self; };
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

      # All-in-one NixOS module, a convenience for a greenfield setup: it enables
      # fcitx5 with the addon and sets the input-method env vars. Every scalar
      # option uses lib.mkDefault, so importing it never overrides an existing
      # fcitx5 configuration (and addons is a list, so it merges). If you already
      # configure fcitx5, do NOT import this: just add
      # `inputs.schnelle-umlaute.packages.<system>.default` to your
      # i18n.inputMethod.fcitx5.addons list instead.
      nixosModules.default =
        {
          config,
          lib,
          pkgs,
          ...
        }:
        let
          cfg = config.programs.schnelle-umlaute;
          # Built against the consumer's nixpkgs (same fcitx5/Qt as the system).
          addon = pkgs.callPackage ./nix/package.nix { src = self; };
        in
        {
          options.programs.schnelle-umlaute.enable = lib.mkEnableOption "the schnelle-umlaute fcitx5 addon (accent input, Wayland overlay, QML editor)";

          config = lib.mkIf cfg.enable {
            i18n.inputMethod = {
              enable = lib.mkDefault true;
              type = lib.mkDefault "fcitx5";
              fcitx5 = {
                waylandFrontend = lib.mkDefault true;
                # A list: merges with any addons already configured elsewhere.
                addons = [ addon ];
              };
            };

            # With waylandFrontend NixOS sets only XMODIFIERS; GTK_IM_MODULE and
            # QT_IM_MODULE stay empty, so XWayland apps get no input method and the
            # editor reports the environment as unconfigured. Set them here.
            environment.sessionVariables = {
              GTK_IM_MODULE = lib.mkDefault "fcitx";
              QT_IM_MODULE = lib.mkDefault "fcitx";
            };
          };
        };
    };
}
