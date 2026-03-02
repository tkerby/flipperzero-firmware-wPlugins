# LIDAR Emulator

This app can be used to emulate infrared signals of different LIDARs.

## v0.2: External GPIO Support!

Now supports external IR transmitters via GPIO! Configure which IR output to use:
- Built-in Flipper IR LED (default)
- External IR module on GPIO pin A7 (pin 2)
- Auto-detect external modules

## Features

- 25 predefined LIDAR gun timing profiles
- Support for internal IR LED or external GPIO IR module
- Configurable 5V power output for external modules
- Persistent settings (saved between sessions)
- Auto-detection of external IR modules

## What good is this?

You can use it to test a LIDAR jammer responsiveness for different signals.
It is unable to jam police LIDARs.

## Limitations

The shortest pulse width that can be emitted with internal infrared emitters is around 2 µs,
but real LIDAR guns are emitting 10-30 ns wide pulses.

**Quick build:**
```bash
pip install ufbt
cd flipperzero-lidar_emulator
ufbt launch
```

## Licensing

This code is open-source (GPL 3.0).