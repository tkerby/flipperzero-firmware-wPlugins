# UI simulator

Runs the app's **real draw and input code** on the host, without a Flipper.

```
./sim/build.sh          # compile
./sim/out/run.sh        # run, writes sim/out/page*.pgm
```

What is real: `rf_survey.c` unchanged, the sweep worker on a real pthread, the input handlers,
and **the firmware's own u8g2 and font tables** compiled in — so glyphs and pixels match the
device, not an approximation.

What is stubbed: the radio returns a synthetic spectrum, storage is in memory, the display is a
framebuffer. The external CC1101 is never present.

Limits are modelled, not ignored: the heap is a fixed pool (`SIM_HEAP=12288 ./sim/out/run.sh`
to force allocation failures) and a block over budget returns NULL like the device does. Worker
stacks cannot be reproduced exactly — the host minimum is 16 KB — and the sim says so instead of
pretending.

## Driving another app

The feedback loop is general; only the entry symbol and the script are app-specific.

```sh
SIM_ENTRY=my_app ./sim/build.sh          # link a different entry point
SIM_SCRIPT=sim/tour.script ./sim/out/rfsim out/
```

`sim/tour.script` shows the format:

```
scene page0      # render the current screen to page0.pgm
key R!           # R L U D OK B, suffix ! = long press
wait 400         # let the workers run
record           # start CSV logging
walk             # run the config-row walk
```

What this does and does not cover:

- **Works today**: any FAP built on `ViewDispatcher` with its own `View` draw/input
  callbacks, using canvas, storage, flipper_format, threads, timers, stream buffers,
  notification and the sub-GHz device abstraction.
- **Needs more shim headers**: apps using `furi_hal.h`, `input/input.h`,
  `gui/scene_manager.h`, `dialogs/dialogs.h`, the sub-GHz block library.
- **Needs the firmware's GUI library compiled in**: apps that draw through stock
  widgets (Menu, Popup, Widget, Dialog, TextInput). Those render themselves inside the
  firmware; reimplementing them here would make the simulator lie the moment it drifted.
- **Never**: the physical layer. NFC, LF, IR, BLE and the CC1101's actual RF behaviour
  are stubs, and a passing run says nothing about them.

`SIM_VERBOSE=1` shows every log line the app emits.
