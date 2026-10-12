{
  description = "NCS Visualizer for Noctalia: the aurora-ncs GPU renderer";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        aurora-ncs = pkgs.callPackage ./nix/package.nix { };
        default = aurora-ncs;
      });

      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = nixpkgs.lib.getExe self.packages.${pkgs.stdenv.hostPlatform.system}.aurora-ncs;
          meta.description = "Run the aurora-ncs renderer";
        };
      });

      overlays.default = final: _prev: {
        aurora-ncs = final.callPackage ./nix/package.nix { };
      };

      devShells = forAllSystems (
        pkgs:
        let
          qtDeps = with pkgs.kdePackages; [
            qtbase
            qtdeclarative
            qtwayland
            layer-shell-qt
          ];
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.aurora-ncs ];
            packages = [
              pkgs.cava
              pkgs.gdb
              pkgs.clang-tools
            ];
            shellHook = ''
              export QT_PLUGIN_PATH="${pkgs.lib.makeSearchPath "lib/qt-6/plugins" qtDeps}''${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
              export QML2_IMPORT_PATH="${pkgs.lib.makeSearchPath "lib/qt-6/qml" qtDeps}''${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
            '';
          };
        }
      );

      checks = forAllSystems (pkgs: {
        inherit (self.packages.${pkgs.stdenv.hostPlatform.system}) aurora-ncs;
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
