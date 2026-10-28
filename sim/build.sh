#!/bin/sh
# Build the UI simulator. Needs the firmware source for u8g2 and its fonts.
set -eu
FW=${FLIPPER_FW:-$HOME/Repos/flipper/flipperzero-firmware}
[ -d "$FW/lib/u8g2" ] || { echo "set FLIPPER_FW to the firmware checkout" >&2; exit 1; }
cd "$(dirname "$0")/.."
mkdir -p sim/out

# u8g2_glue.c pulls in furi_hal (panel driver); the sim renders into its own framebuffer
SRCS=""
for f in "$FW"/lib/u8g2/*.c; do
    case "$f" in *u8g2_glue.c) continue ;; esac
    SRCS="$SRCS $f"
done

ENTRY=${SIM_ENTRY:-rf_survey_app}
CFLAGS="-std=c11 -O1 -g -DSIM_ENTRY=$ENTRY -ffunction-sections -fdata-sections -I sim/include -I . -I $FW/lib/u8g2 -I $FW/lib/u8g2/.."

# The app is compiled on its own with the allocation override, so its malloc/free go through the
# simulator's budgeted heap and the OOM numbers mean something.
cc $CFLAGS -include sim/alloc_override.h -c rf_survey.c -o sim/out/rf_survey.o
cc $CFLAGS sim/run.c sim/shim.c sim/gui.c sim/demo_menu.c sim/out/rf_survey.o $SRCS \
   -o sim/out/rfsim -lm -lpthread -Wl,-dead_strip
echo "built sim/out/rfsim"
