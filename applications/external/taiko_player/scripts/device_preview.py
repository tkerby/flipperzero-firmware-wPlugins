#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 ZeroNinx

"""Capture Flipper framebuffer screenshots and measure USB stream FPS.

Requires pyserial and Pillow (also available in the uFBT toolchain Python).
The app must already be running. This script does not change GPIO or send keys.
"""

import argparse
from pathlib import Path
import time

import serial
from PIL import Image


def varint(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def read_varint(port):
    value = shift = 0
    for _ in range(5):
        byte = port.read(1)
        if not byte:
            raise TimeoutError("No RPC response from Flipper")
        value |= (byte[0] & 127) << shift
        if not byte[0] & 128:
            return value
        shift += 7
    raise ValueError("Invalid RPC varint")


def fields(data):
    result = {}
    position = 0

    def read():
        nonlocal position
        value = shift = 0
        while True:
            byte = data[position]
            position += 1
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
            shift += 7

    while position < len(data):
        tag = read()
        key, kind = tag >> 3, tag & 7
        if kind == 0:
            result[key] = read()
        elif kind == 2:
            length = read()
            result[key] = data[position:position + length]
            position += length
        else:
            raise ValueError(f"Unsupported protobuf wire type: {kind}")
    return result


def send(port, command, tag):
    body = b"\x08" + varint(command) + varint((tag << 3) | 2) + b"\x00"
    port.write(varint(len(body)) + body)


def save_frame(raw, path):
    image = Image.new("1", (128, 64), 1)
    for y in range(64):
        for x in range(128):
            image.putpixel((x, y), 0 if raw[(y // 8) * 128 + x] & (1 << (y % 8)) else 1)
    image.resize((512, 256), Image.Resampling.NEAREST).save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=5)
    parser.add_argument("--output", type=Path, default=Path("build/playback.png"))
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    changed = 0
    previous_raw = None
    first = last = None
    raw = None
    with serial.Serial(args.port, timeout=min(1, args.seconds), write_timeout=3) as port:
        port.write(b"\r")
        time.sleep(0.2)
        port.reset_input_buffer()
        port.write(b"start_rpc_session\r")
        port.read_until(b"\r\n")
        send(port, 1, 20)  # StartScreenStreamRequest
        try:
            deadline = time.monotonic() + args.seconds
            saved_second = -1
            while time.monotonic() < deadline:
                try:
                    length = read_varint(port)
                except TimeoutError:
                    if count:
                        continue  # Static views need not submit another framebuffer.
                    raise
                message = fields(port.read(length))
                if 22 not in message:
                    if message.get(2, 0):
                        raise RuntimeError(f"RPC error: {message[2]}")
                    continue
                frame = fields(message[22])
                raw = frame[1]
                if len(raw) != 1024 or frame.get(2, 0) != 0:
                    raise ValueError("Expected a horizontal 128x64 framebuffer")
                last = time.monotonic()
                if first is None:
                    first = last
                count += 1
                if raw != previous_raw:
                    changed += 1
                previous_raw = raw
                second = int(last - first)
                if second % 5 == 0 and second != saved_second:
                    snapshot = args.output.with_name(f"{args.output.stem}-{second:02d}.png")
                    save_frame(raw, snapshot)
                    saved_second = second
        finally:
            send(port, 2, 21)  # StopScreenStreamRequest
            time.sleep(0.1)
            send(port, 3, 19)  # StopSession
            time.sleep(0.1)
    if raw is None:
        raise RuntimeError("No screen frames received")
    save_frame(raw, args.output)
    fps = (count - 1) / (last - first) if count > 1 and last > first else 0
    print(f"USB framebuffer stream: {count} frames, {fps:.2f} FPS")
    changed_fps = (changed - 1) / (last - first) if count > 1 and last > first else 0
    print(f"Changed frames: {changed}, {changed_fps:.2f} FPS (excludes identical refreshes)")
    print(f"Saved {args.output}; stream rate does not measure LCD pixel response.")


if __name__ == "__main__":
    main()
