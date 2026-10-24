#!/bin/bash
# Flash the devboard over its USB-C port.
#
# Two strategies, in order of how much they ask of you:
#
#  1. --before usb_reset. Our firmware puts the ESP console on native USB CDC,
#     and esptool can drive the S2's USB-OTG reset over that same connection to
#     drop the chip into download mode. No buttons, ~5 s, works while the
#     firmware is running. This is the normal path.
#
#  2. --before no_reset in a retry loop. For when the board is NOT running our
#     firmware -- a bricked or blank chip has no CDC port to reset through, so
#     you hold BOOT, tap RESET, release BOOT while the loop hunts for it.
#
# esptool's exit status is captured directly, never through a pipe -- piping to
# tail reports tail's status and would claim success on every failure.
set -u
B="$(cd "$(dirname "$0")/.." && pwd)/esp32-firmware"
source ~/esp/esp-idf/export.sh >/dev/null 2>&1
cd "$B" || exit 1

# $1 = "dl" to accept ONLY a port in ROM download mode.
#
# The two USB identities matter here. Our firmware reports the chip's MAC as the
# serial; the ROM bootloader reports SER=0. A crash-looping app still presents a
# port, and esptool trying to sync against it reads the boot log as a malformed
# SLIP packet ("Invalid head of packet"). So when we are waiting for the button
# sequence we must hold out for SER=0 rather than grabbing whatever is there.
find_port() {
  "$IDF_PYTHON_ENV_PATH/bin/python" -c "
import sys, serial.tools.list_ports as lp
dl_only = len(sys.argv) > 1 and sys.argv[1] == 'dl'
for p in lp.comports():
    h = ((p.hwid or '') + ' ' + (p.description or '')).upper()
    if '303A' not in h and 'ESP32' not in h:
        continue
    if dl_only and 'SER=0' not in h.replace(' ', ''):
        continue
    print(p.device); break
" ${1:-} 2>/dev/null
}

flash() { # $1 = --before mode, $2 = connect attempts
  python3 -m esptool --chip esp32s2 -p "$PORT" --before "$1" --after no_reset \
      --connect-attempts "$2" write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m \
      0x1000 build/bootloader/bootloader.bin \
      0x8000 build/partition_table/partition-table.bin \
      0x10000 build/flipper_radio_esp.bin 2>&1
}

PORT=$(find_port)
if [ -n "$PORT" ]; then
  echo "found $PORT -- trying usb_reset (no buttons needed)"
  out=$(flash usb_reset 3); rc=$?
  if [ $rc -eq 0 ]; then
    echo "$out" | tail -4
    echo "=== flashed OK via usb_reset -- replug USB to boot it ==="
    exit 0
  fi
  echo "usb_reset failed: $(echo "$out" | grep -iE 'fatal|error' | head -1)"
fi

echo "falling back to the buttons. Most reliable order:"
echo "  unplug USB -> hold BOOT -> plug USB in while holding -> release after 2 s"
echo "waiting for a port in download mode (SER=0)..."
for i in $(seq 1 90); do
  PORT=$(find_port dl)
  if [ -z "$PORT" ]; then sleep 1; continue; fi
  echo "download mode on $PORT"
  out=$(flash no_reset 1); rc=$?
  if [ $rc -eq 0 ]; then
    echo "$out" | tail -4
    echo "=== flashed OK (attempt $i) -- replug USB to boot it ==="
    exit 0
  fi
  echo "attempt $i: $(echo "$out" | grep -iE 'fatal|error' | head -1)"
  sleep 1
done
echo "=== never caught the board in download mode ==="
exit 1
