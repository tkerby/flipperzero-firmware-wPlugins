#!/usr/bin/env python3
"""
Capture the Flipper's real screen via the RPC protocol (the same channel
qFlipper's GUI "Remote Control" feature uses), decode the raw 1-bit page
framebuffer, and save it as an upright, correctly-colored PNG.

Requires (in the ufbt venv): flipperzero-protobuf (--no-deps), pyserial,
protobuf. Usage: python3 capture_screen.py [output.png] [serial_port]
"""
import sys

from flipperzero_protobuf.flipper_proto import FlipperProto

W, H = 128, 64


def decode_and_save(raw: bytes, out_path: str) -> None:
    pages = H // 8
    pixels = [[0] * W for _ in range(H)]
    for page in range(pages):
        for col in range(W):
            byte = raw[page * W + col]
            for bit in range(8):
                y = page * 8 + bit
                pixels[y][col] = (byte >> bit) & 1

    pbm_path = out_path + ".tmp.pbm"
    with open(pbm_path, "w") as f:
        f.write(f"P1\n{W} {H}\n")
        for row in pixels:
            # raw bit 1 = black ink (ColorBlack), PBM '1' = black - no inversion needed
            f.write(" ".join(str(p) for p in row) + "\n")

    import subprocess

    # Rotate 90 CW: the raw buffer is in the physical panel's native
    # landscape layout; this undoes that for a vertical-orientation app.
    # Drop -rotate for a horizontal-orientation view (e.g. REM Pod).
    subprocess.run(
        [
            "magick",
            pbm_path,
            "-rotate",
            "90",
            "-filter",
            "point",
            "-resize",
            "600%",
            out_path,
        ],
        check=True,
    )


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "screen.png"
    serial_port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyACM0"

    proto = FlipperProto(serial_port=serial_port)
    raw = proto.rpc_gui_snapshot_screen()
    decode_and_save(raw, out_path)
    print(f"Saved {out_path} ({len(raw)} raw bytes)")


if __name__ == "__main__":
    main()
