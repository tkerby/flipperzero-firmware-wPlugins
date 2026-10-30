# Motion Lab

Turn a Flipper Zero with the [Video Game Module](https://docs.flipper.net/video-game-module)
into a motion measuring instrument.

Every other app for the module treats its motion sensor as a *controller* —
tilt to steer, tilt to move a mouse. Motion Lab treats it as an *instrument*:
it measures what the device actually did and gives you numbers.

## Modes

**Swing Meter** — arm it, then swing or hit something. You get:

- peak acceleration in g, with gravity removed
- peak rotation rate in deg/s
- swing tempo as a backswing:downswing ratio (a 3:1 golf tempo reads `3.00:1`)
- time to peak, and how long the event lasted
- a trace of the whole event, switchable between force and rotation
- session bests

**Motion Logger** — records to a CSV on the SD card with a live graph.
Files land in `/ext/apps_data/motion_lab/` as `motion_001.csv` and up, never
overwriting an earlier one.

```
t_ms,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps
```

Integer milli-units, so the file is exact and parses in any spreadsheet.

## Requirements

- Flipper Zero firmware 0.98.3 or later
- Video Game Module, running its **stock firmware** — nothing to reflash

The module's sensor is wired to both the module's RP2040 and the Flipper's GPIO
connector, so this app reads it directly over SPI and the module keeps working
normally for everything else.

## Using it

1. Attach the Video Game Module.
2. Open `Apps` → `Tools` → `Motion Lab`.
3. **Hold the Flipper still for a second at launch.** The gyro's zero offset is
   measured then; moving during it biases every rotation reading afterwards.
4. Pick a mode.

Controls: `OK` arms a new capture, or starts and stops recording.
`Left`/`Right` switch the result graph between force and rotation. `Back` exits.

## Ranges

Configured for the widest the sensor offers, because a bat or club swing
genuinely clips anything narrower:

| | |
|---|---|
| Acceleration | ±16 g |
| Rotation | ±2000 deg/s |
| Sample rate | 1 kHz |
| CSV rate | 100 Hz |

## Building

```sh
ufbt            # build
ufbt launch     # build, upload and run
```

The measurement core has no Flipper dependencies, so it is tested on a host:

```sh
cd test
cc -I.. -O1 -Wall -Wextra -o /tmp/test_capture test_capture.c ../capture.c -lm
/tmp/test_capture
```

## License

GPL-3.0. The ICM-42688-P driver in `sensors/` comes from Flipper Devices'
`flipperzero-good-faps` (GPL-3.0); the rest is original work under the same
license.
