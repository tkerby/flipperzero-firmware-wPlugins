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

`SIM_VERBOSE=1` shows every log line the app emits.
