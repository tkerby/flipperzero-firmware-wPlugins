#!/usr/bin/env python3
"""Read the FAP's own framebuffer dump and write a PNG.

The app registers a gui framebuffer callback and writes /ext/apps_data/rfsurvey/shot.hex
(1024 bytes as 64 lines of 32 hex chars, the u8g2 page layout). Reading it over the CLI costs
a CLI session, not an RPC session -- an RPC session takes ~20 KB of the FAP's heap and this app
needs 15 KB just to start.

Usage: flip_shot.py out.png
"""
import glob
import sys
import time

import serial
from PIL import Image

RAW = "/ext/apps_data/rfsurvey/shot.hex"
W, H = 128, 64


def port():
    for pat in ("/dev/cu.usbmodemflip_*", "/dev/tty.usbmodemflip_*"):
        m = sorted(glob.glob(pat))
        if m:
            return m[0]
    m = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not m:
        raise SystemExit("no Flipper serial port")
    return m[0]


def read_raw():
    p = serial.Serial(port(), 115200, timeout=0.7, write_timeout=5)
    try:
        p.reset_input_buffer()
        p.write(b"\r")
        time.sleep(0.25)
        p.read(4096)
        p.write(("storage read %s\r" % RAW).encode())
        time.sleep(0.9)
        data = p.read(65536)
        p.write(b"\r")
        time.sleep(0.1)
    finally:
        p.close()

    lines = []
    for ln in data.decode("ascii", "ignore").splitlines():
        ln = ln.strip()
        if len(ln) == 32 and all(c in "0123456789abcdefABCDEF" for c in ln):
            lines.append(ln)
    if len(lines) < 64:
        raise SystemExit("short dump: %d of 64 lines" % len(lines))
    lines = lines[-64:]
    return bytes.fromhex("".join(lines))


def to_image(raw):
    px = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            byte = raw[(y // 8) * W + x]
            px[y][x] = 0 if (byte >> (y % 8)) & 1 else 255
    img = Image.new("L", (W, H))
    img.putdata([px[y][x] for y in range(H) for x in range(W)])
    return img


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "shot.png"
    raw = read_raw()
    img = to_image(raw)
    img.resize((W * 4, H * 4), Image.NEAREST).save(out)
    print(out)


if __name__ == "__main__":
    main()
