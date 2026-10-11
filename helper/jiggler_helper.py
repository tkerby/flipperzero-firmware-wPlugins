#!/usr/bin/env python3
"""Desktop helper for the Flipper Zero Jiggler app.

Once a second it tells the Flipper how long ago you last touched the real
mouse or keyboard, and whether the screen is locked, so the jiggler can hold
off while you are working. The Flipper answers with its own status, which is
shown on one line in the terminal.

The Flipper's own moves reset the computer's idle timer too, so the helper
discounts any input that lines up with a move the Flipper reports.

Needs the hidapi package:  pip install hidapi
"""

import argparse
import struct
import sys
import time

try:
    import hid
except ImportError:
    sys.exit("The hidapi package is missing. Install it with:  pip install hidapi")

VENDOR_PAGE = 0xFF4A
PRODUCT_NAME = "Jiggler"
MAGIC = 0x4A
VERSION = 1

FLAG_HAS_IDLE = 1 << 0
FLAG_HAS_LOCK = 1 << 1
FLAG_LOCKED = 1 << 2

STATES = {0: "stopped", 1: "jiggling", 2: "paused, you're busy", 3: "paused, PC locked", 4: "finished"}

# ---------------------------------------------------------------- idle time per platform


def _mac_source():
    import ctypes

    cg = ctypes.cdll.LoadLibrary("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    cf = ctypes.cdll.LoadLibrary("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
    cg.CGEventSourceSecondsSinceLastEventType.restype = ctypes.c_double
    cg.CGEventSourceSecondsSinceLastEventType.argtypes = [ctypes.c_int, ctypes.c_uint32]
    cg.CGSessionCopyCurrentDictionary.restype = ctypes.c_void_p
    cf.CFStringCreateWithCString.restype = ctypes.c_void_p
    cf.CFStringCreateWithCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
    cf.CFDictionaryGetValue.restype = ctypes.c_void_p
    cf.CFDictionaryGetValue.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    cf.CFBooleanGetValue.restype = ctypes.c_bool
    cf.CFBooleanGetValue.argtypes = [ctypes.c_void_p]
    cf.CFRelease.argtypes = [ctypes.c_void_p]
    key = cf.CFStringCreateWithCString(None, b"CGSSessionScreenIsLocked", 0x08000100)

    def idle():
        # 1 = combined session state, 0xFFFFFFFF = any input event
        return cg.CGEventSourceSecondsSinceLastEventType(1, 0xFFFFFFFF)

    def locked():
        d = cg.CGSessionCopyCurrentDictionary()
        if not d:
            return None
        v = cf.CFDictionaryGetValue(d, key)
        result = bool(v) and cf.CFBooleanGetValue(v)
        cf.CFRelease(d)
        return result

    return idle, locked


def _windows_source():
    import ctypes
    from ctypes import wintypes

    class LASTINPUTINFO(ctypes.Structure):
        _fields_ = [("cbSize", wintypes.UINT), ("dwTime", wintypes.DWORD)]

    user32 = ctypes.windll.user32
    kernel32 = ctypes.windll.kernel32
    info = LASTINPUTINFO()
    info.cbSize = ctypes.sizeof(LASTINPUTINFO)

    def idle():
        if not user32.GetLastInputInfo(ctypes.byref(info)):
            return None
        return ((kernel32.GetTickCount() - info.dwTime) & 0xFFFFFFFF) / 1000.0

    def locked():
        # The input desktop cannot be opened while the lock screen is up
        DESKTOP_SWITCHDESKTOP = 0x0100
        h = user32.OpenInputDesktop(0, False, DESKTOP_SWITCHDESKTOP)
        if h:
            user32.CloseDesktop(h)
            return False
        return True

    return idle, locked


def _linux_source():
    import ctypes
    import os

    if not os.environ.get("DISPLAY"):
        return None, None

    class XScreenSaverInfo(ctypes.Structure):
        _fields_ = [
            ("window", ctypes.c_ulong),
            ("state", ctypes.c_int),
            ("kind", ctypes.c_int),
            ("til_or_since", ctypes.c_ulong),
            ("idle", ctypes.c_ulong),
            ("event_mask", ctypes.c_ulong),
        ]

    try:
        xlib = ctypes.cdll.LoadLibrary("libX11.so.6")
        xss = ctypes.cdll.LoadLibrary("libXss.so.1")
    except OSError:
        return None, None
    xlib.XOpenDisplay.restype = ctypes.c_void_p
    xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
    xlib.XDefaultRootWindow.restype = ctypes.c_ulong
    xlib.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    xss.XScreenSaverAllocInfo.restype = ctypes.POINTER(XScreenSaverInfo)
    xss.XScreenSaverQueryInfo.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XScreenSaverInfo)]
    dpy = xlib.XOpenDisplay(None)
    if not dpy:
        return None, None
    root = xlib.XDefaultRootWindow(dpy)
    info = xss.XScreenSaverAllocInfo()

    def idle():
        xss.XScreenSaverQueryInfo(dpy, root, info)
        return info.contents.idle / 1000.0

    return idle, None


def activity_source():
    if sys.platform == "darwin":
        return _mac_source()
    if sys.platform.startswith("win"):
        return _windows_source()
    return _linux_source()


# ---------------------------------------------------------------- the Flipper

class ActivityTracker:
    """Tracks when the person last gave input, discounting the Flipper's own moves.

    The Flipper reports how long ago it last moved. A fresh input that lines up
    with that move is the Flipper's doing and does not count as the person.
    """

    def __init__(self):
        self.effective_last_input = None
        self.last_moves = None
        self.ignored_input = None  # an input time already attributed to the Flipper

    def idle(self, now):
        if self.effective_last_input is None:
            return 0.0
        return max(0.0, now - self.effective_last_input)

    def observe(self, now, last_input, reply):
        """Feed the host's last-input time and the Flipper's reply for this tick."""
        if last_input is None:
            return
        if self.effective_last_input is None:
            self.effective_last_input = last_input
        if self.ignored_input is not None and abs(last_input - self.ignored_input) < 0.05:
            if reply is not None:
                self.last_moves = reply["moves"]
            return
        if reply is not None:
            moved = self.last_moves is not None and reply["moves"] != self.last_moves
            self.last_moves = reply["moves"]
            if last_input > self.effective_last_input + 0.3:
                move_time = None
                if reply["since_ms"] != 0xFFFF:
                    move_time = now - reply["since_ms"] / 1000.0
                flipper_did_it = moved and move_time is not None and abs(last_input - move_time) < 1.5
                if flipper_did_it:
                    self.ignored_input = last_input
                else:
                    self.effective_last_input = last_input
        elif last_input > self.effective_last_input + 0.3:
            self.effective_last_input = last_input




def find_flipper(verbose=False):
    """Open the Jiggler's vendor collection. Returns an open hid.device or None."""
    found = hid.enumerate()
    preferred = [d for d in found if d.get("usage_page") == VENDOR_PAGE]
    others = [d for d in found if d.get("product_string") == PRODUCT_NAME and d not in preferred]
    for info in preferred + others:
        try:
            dev = hid.device()
            dev.open_path(info["path"])
            dev.set_nonblocking(False)
        except OSError as e:
            if verbose:
                print(f"  cannot open {info['path']!r}: {e}")
            continue
        # Only keep it if it answers like the jiggler
        try:
            send(dev, 0, 0)
            reply = receive(dev, 400)
        except OSError:
            reply = None
        if reply:
            return dev
        dev.close()
    return None


def send(dev, flags, idle_seconds):
    payload = struct.pack("<BBBHBBB", MAGIC, VERSION, flags, min(int(idle_seconds), 0xFFFF), 0, 0, 0)
    dev.write(bytes([0]) + payload)  # leading 0: the channel has no report id


def receive(dev, timeout_ms):
    data = dev.read(8, timeout_ms)
    if len(data) >= 8 and data[0] == MAGIC:
        state, next_s, since_ms, moves = struct.unpack("<BHHH", bytes(data[1:8]))
        return {"state": state, "next": next_s, "since_ms": since_ms, "moves": moves}
    return None


def fmt(seconds):
    seconds = int(seconds)
    if seconds >= 3600:
        return f"{seconds // 3600}:{seconds // 60 % 60:02d}:{seconds % 60:02d}"
    return f"{seconds // 60}:{seconds % 60:02d}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--interval", type=float, default=1.0, help="seconds between updates (default 1)")
    ap.add_argument("--once", action="store_true", help="send one update, print the reply and exit")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    idle_fn, locked_fn = activity_source()
    if idle_fn is None:
        print("Warning: cannot read idle time on this system; the Flipper will not pause for you.")

    dev = None
    tracker = ActivityTracker()
    while True:
        if dev is None:
            dev = find_flipper(args.verbose)
            if dev is None:
                if args.once:
                    sys.exit("No Flipper running the Jiggler app was found.")
                print("\rLooking for the Jiggler app on a Flipper...      ", end="", flush=True)
                time.sleep(2)
                continue
            print("\rConnected to the Flipper.                          ")
            tracker = ActivityTracker()

        now = time.monotonic()
        flags = 0
        last_input = None
        if idle_fn is not None:
            raw = idle_fn()
            if raw is not None:
                flags |= FLAG_HAS_IDLE
                last_input = now - raw
        if locked_fn is not None:
            lk = locked_fn()
            if lk is not None:
                flags |= FLAG_HAS_LOCK
                if lk:
                    flags |= FLAG_LOCKED

        # Report what we know so far; a correction, if one is needed, goes out next time
        if last_input is not None and tracker.effective_last_input is None:
            tracker.effective_last_input = last_input
        try:
            send(dev, flags, tracker.idle(now))
            reply = receive(dev, 500)
        except OSError:
            print("\rLost the Flipper.                                   ")
            dev.close()
            dev = None
            continue
        tracker.observe(now, last_input, reply)

        if last_input is not None:
            text_you = f"idle {fmt(tracker.idle(now))}"
            if flags & FLAG_LOCKED:
                text_you += ", locked"
        else:
            text_you = "idle unknown"
        if reply:
            st = STATES.get(reply["state"], "?")
            nxt = f", next move in {fmt(reply['next'])}" if reply["state"] == 1 else ""
            line = f"Flipper: {st}{nxt}, {reply['moves']} moves | you: {text_you}"
        else:
            line = f"Flipper: no reply | you: {text_you}"
        if args.verbose:
            print(line)
        else:
            print("\r" + line.ljust(78), end="", flush=True)
        if args.once:
            print()
            return
        time.sleep(args.interval)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print()
