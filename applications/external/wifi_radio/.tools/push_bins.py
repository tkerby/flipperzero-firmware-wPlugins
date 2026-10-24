#!/usr/bin/env python3
"""Push the ESP firmware images to the Flipper's SD card for ESP Flasher.

Needed when the devboard will not enter ROM download mode over USB -- a
crash-looping app floods the CDC port and esptool reads the boot log as a
malformed packet, and the BOOT button is not always cooperative. ESP Flasher
drives BOOT and EN electrically over the GPIO header, so it needs no buttons.

Transfer uses the Flipper's CLI `storage write_chunk <path> <size>`, which
prints "Ready" and then reads exactly that many raw bytes. The plain `storage
write` command reads text until Ctrl-C and would corrupt a binary.
"""
import sys
import time
from pathlib import Path

import serial
import serial.tools.list_ports

DEST = "/ext/apps_data/esp_flasher"
BUILD = Path(__file__).resolve().parent.parent / "esp32-firmware" / "build"
FILES = [
    (BUILD / "bootloader" / "bootloader.bin", "bootloader.bin"),
    (BUILD / "partition_table" / "partition-table.bin", "partition-table.bin"),
    (BUILD / "flipper_radio_esp.bin", "flipper_radio_esp.bin"),
]


def find_flipper():
    for p in serial.tools.list_ports.comports():
        if "flip" in p.device.lower() or "Flipper" in (p.description or ""):
            return p.device
    return None


def read_until(ser, needle, timeout=10.0):
    """Collect output until `needle` appears. Returns everything seen."""
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        buf += ser.read(512)
        if needle in buf:
            return buf
    raise TimeoutError(f"never saw {needle!r}; got {buf[-200:]!r}")


def main():
    port = find_flipper()
    if not port:
        print("No Flipper on USB. Plug it in and run this again.")
        return 1

    missing = [str(src) for src, _ in FILES if not src.is_file()]
    if missing:
        print("Missing build output:\n  " + "\n  ".join(missing))
        return 1

    print(f"Flipper on {port}")
    ser = serial.Serial(port, 115200, timeout=0.3)
    # The CLI greets on connect; clear it so the first prompt match is ours.
    ser.write(b"\r")
    time.sleep(0.4)
    ser.reset_input_buffer()

    ser.write(f"storage mkdir {DEST}\r".encode())
    read_until(ser, b">: ")  # already-exists is fine, we only need the prompt

    for src, name in FILES:
        data = src.read_bytes()
        dest = f"{DEST}/{name}"
        print(f"  {name}: {len(data)} bytes ... ", end="", flush=True)

        ser.write(f"storage remove {dest}\r".encode())
        read_until(ser, b">: ")

        ser.write(f"storage write_chunk {dest} {len(data)}\r".encode())
        read_until(ser, b"Ready")

        sent = 0
        while sent < len(data):
            sent += ser.write(data[sent : sent + 4096])
        ser.flush()
        read_until(ser, b">: ", timeout=90.0)
        print("ok")

    # Confirm what actually landed rather than trusting the writes.
    ser.write(f"storage list {DEST}\r".encode())
    out = read_until(ser, b">: ").decode("utf-8", "replace")
    ser.close()
    print("\n" + out.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
