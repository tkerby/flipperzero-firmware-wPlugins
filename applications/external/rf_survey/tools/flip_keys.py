#!/usr/bin/env python3
"""Send key presses to the Flipper over the CLI in ONE port session.

No RPC: an RPC session costs ~20 KB of the FAP's heap and this app needs ~15 KB just
to start, so a remote capture of a deep screen is an OOM crash.

Each key is PRESS + SHORT + RELEASE, which is what a FAP needs; a lone SHORT is
swallowed. A key written with a trailing `!` is a LONG press (press, hold, release).

Usage: flip_keys.py ok right! right! --settle 0.12
"""
import glob
import sys
import time

import serial


def port():
    for pat in ("/dev/cu.usbmodemflip_*", "/dev/tty.usbmodemflip_*"):
        m = sorted(glob.glob(pat))
        if m:
            return m[0]
    for pat in ("/dev/cu.usbmodem*",):
        m = sorted(glob.glob(pat))
        if m:
            return m[0]
    raise SystemExit("no Flipper serial port found")


def main():
    args = sys.argv[1:]
    settle = 0.12
    hold = 0.55
    keys = []
    i = 0
    while i < len(args):
        if args[i] == "--settle" and i + 1 < len(args):
            settle = float(args[i + 1])
            i += 2
            continue
        if args[i] == "--hold" and i + 1 < len(args):
            hold = float(args[i + 1])
            i += 2
            continue
        keys.append(args[i])
        i += 1

    p = serial.Serial(port(), 115200, timeout=0.3, write_timeout=5)
    try:
        p.reset_input_buffer()
        p.write(b"\r")
        time.sleep(0.3)
        p.read(8192)
        for k in keys:
            long = k.endswith("!")
            k = k.rstrip("!")
            if long:
                p.write(("input send %s press\r" % k.lower()).encode())
                time.sleep(hold)
                p.write(("input send %s release\r" % k.lower()).encode())
            else:
                for t in ("press", "short", "release"):
                    p.write(("input send %s %s\r" % (k.lower(), t)).encode())
                    time.sleep(settle)
            time.sleep(settle)
        time.sleep(0.3)
        p.read(65536)
    finally:
        p.close()
    print("sent %d key(s)" % len(keys))


if __name__ == "__main__":
    main()
