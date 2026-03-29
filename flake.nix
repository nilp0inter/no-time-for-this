{
  description = "No Time For This - A Pebble watchface based on Timely";

  inputs = {
    pebble.url = "github:pebble-dev/pebble.nix";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { pebble, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import pebble.inputs.nixpkgs { inherit system; };
        pebbleShell = pebble.pebbleEnv.${system} {
          packages = [ pkgs.go-task ];
        };
      in
      {
        devShells.default = pebbleShell.overrideAttrs (old: {
          shellHook = (old.shellHook or "") + ''
            echo ""
            echo "  No Time For This - Pebble watchface dev environment"
            echo "  ─────────────────────────────────────────────────────"
            echo "  Available tasks (run 'task --list' for details):"
            echo ""
            echo "    task build              Build .pbw with nix"
            echo "    task emulator           Run in basalt emulator"
            echo "    task emulator:aplite    Run in aplite emulator"
            echo "    task logs               Show live emulator logs"
            echo "    task install            Install on watch (PHONE_IP=...)"
            echo "    task sideload           Copy .pbw for manual install"
            echo "    task clean              Remove build artifacts"
            echo ""
          '';
        });

        packages.default = pebble.buildPebbleApp.${system} {
          name = "no-time-for-this";
          src = ./.;
          type = "watchface";
          description = "A Pebble watchface based on Timely by cynorg.";
          releaseNotes = "Replaced with Timely watchface";
          screenshots = {
            aplite = [ "screenshots/aplite.png" ];
          };
        };
      }
    );
}
