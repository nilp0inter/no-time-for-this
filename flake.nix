{
  description = "No Time For This - A Pebble watchface";

  inputs = {
    pebble.url = "github:pebble-dev/pebble.nix";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { pebble, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      {
        devShells.default = pebble.pebbleEnv.${system} { };

        packages.default = pebble.buildPebbleApp.${system} {
          name = "no-time-for-this";
          src = ./.;
          type = "watchface";
          description = "A minimal Pebble watchface that displays the current time.";
          releaseNotes = "Initial release";
          screenshots = {
            aplite = [ "screenshots/aplite.png" ];
          };
        };
      }
    );
}
