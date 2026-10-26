#!/usr/bin/env python3
"""
Capture a Flipper screenshot for the Apps Catalog.

qFlipper's own screenshot button is not usable for this app. qFlipper applies
the frame's orientation before saving (`ScreenCanvas::canvas()` ->
`canvasTransform()`), so a rotated app is saved as a portrait image -- and the
catalog bundler rejects those outright: "Only landscape screenshots are
allowed". It also demands an exact 4x or 8x multiple of 128x64.

So this grabs the raw, unrotated framebuffer over the RPC screen stream and
writes it at 4x, which is what the bundler accepts. The app's UI appears on its
side in the result; that is unavoidable for a rotated app and is how it will
look in the catalog.

    python3 tools/screenshot.py docs/screenshots/ss0.png [--preview] [--port DEV]

Needs pyserial and Pillow. The app must already be showing the view you want:
synthetic input does not reach it (see docs/SPECIFICATION.md section 7), so
neither this script nor the CLI can scroll the grid. Either scroll the device
by hand, or set the focus row and scroll offset in `onkyo_layout_init`, build,
capture, and revert -- the render is a pure function of that state, so the
result is identical to reaching it with the D-pad. docs/screenshots/ss1.png was
made that way, with row 6 and ONKYO_SCROLL_MAX: the state you land in after
scrolling to the end of the list and stepping back up to Bluetooth.
"""

import sys
import time

import serial
from serial.tools import list_ports
from PIL import Image

# The Flipper's CDC port. Its device node moves when it is replugged, so find
# it by USB ID rather than pinning /dev/ttyACM<n>.
FLIPPER_VID, FLIPPER_PID = 0x0483, 0x5740
SCALE = 4  # bundler accepts 4x or 8x of 128x64

# Published catalog screenshots are black on the Flipper's screen orange. The
# bundler keys off BLACK_THRESHOLD = (15, 15, 15): ink stays black, anything
# lighter becomes transparent, so the orange drops out on the app page.
INK = (0, 0, 0)
BACKGROUND = (254, 138, 44)


def find_port():
    for port in list_ports.comports():
        if port.vid == FLIPPER_VID and port.pid == FLIPPER_PID:
            return port.device
    raise SystemExit("no Flipper found; pass --port /dev/ttyACMx")
F_START, F_STOP, F_FRAME = 20, 21, 22  # PB_Main field numbers


def varint(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def read_varint(port):
    value = shift = 0
    while True:
        byte = port.read(1)
        if not byte:
            return None
        value |= (byte[0] & 0x7F) << shift
        if not byte[0] & 0x80:
            return value
        shift += 7


def parse(buf):
    """Minimal protobuf reader: {field number: int or bytes}."""
    fields, i = {}, 0

    def take_varint():
        nonlocal i
        value = shift = 0
        while True:
            byte = buf[i]
            i += 1
            value |= (byte & 0x7F) << shift
            if not byte & 0x80:
                return value
            shift += 7

    while i < len(buf):
        tag = take_varint()
        number, wire = tag >> 3, tag & 7
        if wire == 0:
            fields[number] = take_varint()
        elif wire == 2:
            length = take_varint()
            fields[number] = buf[i : i + length]
            i += length
        else:
            raise ValueError(f"unsupported wire type {wire}")
    return fields


def request(field, payload=b""):
    body = varint((field << 3) | 2) + varint(len(payload)) + payload
    return varint(len(body)) + body


def to_image(data):
    """1024 bytes of u8g2 page format -> a 128x64 image, black ink."""
    img = Image.new("1", (128, 64), 1)
    for page in range(8):
        for x in range(128):
            byte = data[page * 128 + x]
            for bit in range(8):
                if byte & (1 << bit):
                    img.putpixel((x, page * 8 + bit), 0)
    return img


def preview(img):
    """Print the upright view, two pixel rows per line."""
    upright = img.transpose(Image.ROTATE_270)
    pixels = upright.load()
    width, height = upright.size
    for y in range(0, height, 2):
        line = ""
        for x in range(width):
            top = pixels[x, y] == 0
            bottom = pixels[x, y + 1] == 0 if y + 1 < height else False
            line += "█" if top and bottom else "▀" if top else "▄" if bottom else " "
        print("|" + line + "|")


def main():
    argv = sys.argv[1:]
    args = [a for a in argv if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 1
    destination = args[0]

    device = argv[argv.index("--port") + 1] if "--port" in argv else find_port()
    port = serial.Serial(device, timeout=1)
    try:
        port.write(b"\x03")
        time.sleep(0.4)
        port.reset_input_buffer()
        port.write(b"start_rpc_session\r")
        time.sleep(0.8)
        port.read_all()

        port.write(request(F_START))
        frame = None
        deadline = time.time() + 8
        while time.time() < deadline and frame is None:
            length = read_varint(port)
            if not length or length > 8192:
                continue
            message = parse(port.read(length))
            if F_FRAME in message:
                frame = parse(message[F_FRAME])
        port.write(request(F_STOP))
    finally:
        port.close()

    if frame is None:
        print("no screen frame received", file=sys.stderr)
        return 1

    img = to_image(frame.get(1, b""))
    ink_mask = img.convert("L").point(lambda v: 255 if v < 128 else 0)
    styled = Image.new("RGB", img.size, BACKGROUND)
    styled.paste(INK, mask=ink_mask)
    styled.resize((128 * SCALE, 64 * SCALE), Image.NEAREST).save(destination)
    print(
        f"wrote {destination} at {128 * SCALE}x{64 * SCALE} "
        f"from {device} (orientation={frame.get(2, 0)})"
    )
    if "--preview" in sys.argv:
        preview(img)
    return 0


if __name__ == "__main__":
    sys.exit(main())
