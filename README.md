# No Time For This

A Pebble watchface based on [Timely](https://github.com/cynorg/PebbleTimely) by Martin Norland.

Shows the current date and time, optional weather conditions and temperature, a 3-week calendar, and a status bar with connection, charging, and battery indicators. Fully localizable through the configuration screen.

## Building

This project uses a Nix-based build system via [pebble.nix](https://github.com/pebble-dev/pebble.nix).

```
nix build
```

The output `.pbw` file will be in `result/`.

## Attribution

This watchface is derived from **Timely** by Martin Norland ([@cynorg](https://github.com/cynorg)):
- Original repository: https://github.com/cynorg/PebbleTimely
- Copyright (C) 2013 Martin Norland <cyn@cyn.org>

Includes updates, contributions and improvements from Ben Johnson (https://github.com/cktben/PebbleTimely).

Based on the original **PebbleCalendarWatch** by William Heaton:
- Original repository: https://github.com/WilliamHeaton/PebbleCalendarWatch
- Copyright (C) 2013 William Heaton <William.G.Heaton@gmail.com>

**Climacons** weather icon font by [@adamwhitcroft](https://github.com/adamwhitcroft), compiled into TTF by [@christiannaths](https://github.com/christiannaths):
- http://adamwhitcroft.com/climacons/

## License

This project is licensed under the GNU General Public License v3.0 or later. See [LICENSE](LICENSE) for details.
