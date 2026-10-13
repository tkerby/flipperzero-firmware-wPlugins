#!/usr/bin/env python3
"""Gitphin bridge: http://localhost:8080 <-> HTTP server on the Flipper.

Usage: .venv/bin/python -I pc/bridge.py [--port 8080] [--open-browser]

The HTTP server runs on the Flipper (Apps > Tools > Gitphin). This bridge:
  1. starts the Flipper app through the CLI port when the Flipper is plugged in,
  2. carries each TCP connection as frames over the app's USB serial channel,
  3. answers the PC-side /pc/* routes itself (Collector tab), using collector.py.
It exits when the Flipper is unplugged; systemd starts it again on the next plug.
"""

import argparse
import glob
import json
import os
import shutil
import socket
import socketserver
import subprocess
import sys
import threading
import time

import serial

# The service runs this with `python -I`, which drops the script's own directory
# from sys.path, so add it back before importing the local collector module.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import collector  # noqa: E402

CLI_GLOB = "/dev/serial/by-id/usb-Flipper_Devices_Inc.*-if00"
HUB_GLOB = "/dev/serial/by-id/usb-Flipper_Devices_Inc.*-if02"
APP_PATH = "/ext/apps/Tools/gitphin.fap"

UNPLUG_GRACE_S = 5.0  # USB re-enumerates when the app starts or stops
STUCK_S = 180.0  # app port present but no PONG for this long: recover
RESPONSE_TIMEOUT_S = 300.0  # merge, pack and search can run for minutes on the Flipper
CLIENT_TIMEOUT_S = 10.0
HEAD_MAX = 16 * 1024

SYNC = 0xA5
F_OPEN, F_DATA, F_CLOSE, F_PING, F_PONG = 0x01, 0x02, 0x03, 0x10, 0x11
MAX_PAYLOAD = 1024


def log(msg):
    print(f"[gitphin] {msg}", file=sys.stderr, flush=True)


def frame(ftype, payload=b""):
    return bytes([SYNC, ftype, len(payload) & 0xFF, len(payload) >> 8]) + payload


class HubLink:
    """The serial channel to the Flipper app. One HTTP exchange at a time."""

    def __init__(self):
        self.port = None
        self.lock = threading.Lock()
        self.last_ok = time.monotonic()  # last PONG or finished exchange
        self.down_since = time.monotonic()  # when the link last went down
        self.up = False  # the link passed a PING since it was opened
        self.busy_since = None  # start of the request that runs now

    @property
    def connected(self):
        return self.port is not None

    def connect(self, path):
        port = serial.Serial(path, timeout=0.2, write_timeout=30)
        port.reset_input_buffer()
        # Hold the lock for the whole handshake, so no request can use a port
        # that is not confirmed yet.
        with self.lock:
            self.port = port
            try:
                port.write(frame(F_PING))
                deadline = time.monotonic() + 2.0
                while time.monotonic() < deadline:
                    got = self._read_frame(deadline - time.monotonic())
                    if got and got[0] == F_PONG:
                        self.last_ok = time.monotonic()
                        self.up = True
                        return True
            except (serial.SerialException, OSError):
                pass
            self._close()
        return False

    def close(self):
        """Close from outside a request. A request that still runs closes the
        port itself when the serial error reaches it."""
        if self.lock.acquire(timeout=5):
            try:
                self._close()
            finally:
                self.lock.release()

    def _close(self):
        if self.up:
            # Only a working link going down starts the stuck timer; failed
            # connect attempts must not reset it.
            self.down_since = time.monotonic()
            self.up = False
        if self.port:
            try:
                self.port.close()
            except Exception:
                pass
        self.port = None

    def _read_exact(self, n, deadline):
        buf = bytearray()
        while len(buf) < n:
            if time.monotonic() > deadline:
                return None
            chunk = self.port.read(n - len(buf))
            buf += chunk
        return bytes(buf)

    def _read_frame(self, timeout):
        deadline = time.monotonic() + timeout
        while True:
            b = self._read_exact(1, deadline)
            if b is None:
                return None
            if b[0] != SYNC:
                continue
            head = self._read_exact(3, deadline)
            if head is None:
                return None
            ftype, length = head[0], head[1] | (head[2] << 8)
            if ftype not in (F_DATA, F_CLOSE, F_PONG) or length > MAX_PAYLOAD:
                continue
            payload = self._read_exact(length, deadline) if length else b""
            if payload is None:
                return None
            return ftype, payload

    def exchange(self, request_chunks, client):
        """Send one request, copy the response to the client socket.
        Returns False when the link was down before anything was sent."""
        with self.lock:
            if self.port is None:
                return False
            self.busy_since = time.monotonic()
            try:
                return self._exchange(request_chunks, client)
            finally:
                self.busy_since = None

    def _exchange(self, request_chunks, client):
        """The body of exchange(); the caller holds the lock."""
        try:
            self.port.write(frame(F_OPEN))
            for chunk in request_chunks:
                for i in range(0, len(chunk), MAX_PAYLOAD):
                    self.port.write(frame(F_DATA, chunk[i : i + MAX_PAYLOAD]))
            client_ok = True
            while True:
                got = self._read_frame(RESPONSE_TIMEOUT_S)
                if got is None:
                    log("no answer from the Flipper, link reset")
                    self._close()
                    return True
                ftype, payload = got
                if ftype == F_CLOSE:
                    self.last_ok = time.monotonic()
                    return True
                if ftype == F_DATA and client_ok:
                    try:
                        client.sendall(payload)
                    except OSError:
                        client_ok = False  # keep reading to stay in sync
        except (serial.SerialException, OSError) as e:
            log(f"serial error: {e}")
            self._close()
        return True


LINK = HubLink()
PROBLEM = None  # shown on the 503 page when the bridge cannot reach the Flipper

NOT_RUNNING_PAGE = b"""<!doctype html><meta charset=utf-8>
<meta http-equiv=refresh content=3><title>Gitphin</title>
<body style="font:16px system-ui;max-width:36em;margin:3em auto;padding:0 16px">
<h1>Gitphin</h1>
<p>The bridge runs, but the Gitphin app is not running on the Flipper.</p>
<p>On the Flipper, open <b>Apps &gt; Tools &gt; Gitphin</b>.
This page reloads every 3 seconds.</p>"""


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        sock = self.request
        sock.settimeout(CLIENT_TIMEOUT_S)
        try:
            head, rest = self._read_head(sock)
        except OSError:
            return
        if head is None:
            return

        # PC-side routes are answered by the bridge, not forwarded to the Flipper.
        request_line = head.split(b"\r\n", 1)[0].decode("latin-1", "replace")
        parts = request_line.split(" ")
        path = parts[1] if len(parts) > 1 else "/"
        if path.split("?", 1)[0].startswith("/pc/"):
            self._serve_pc(sock, path)
            return

        if not LINK.connected:
            self._send_not_running(sock)
            return

        length = 0
        for line in head.split(b"\r\n")[1:]:
            name, _, value = line.partition(b":")
            if name.strip().lower() == b"content-length":
                length = int(value.strip() or 0)

        def chunks():
            yield head + b"\r\n\r\n"
            body = rest[:length]
            if body:
                yield body
            left = length - len(body)
            while left > 0:
                data = sock.recv(min(left, 8192))
                if not data:
                    return
                left -= len(data)
                yield data

        if not LINK.exchange(chunks(), sock):
            self._send_not_running(sock)

    @staticmethod
    def _send_not_running(sock):
        page = NOT_RUNNING_PAGE
        if PROBLEM:
            page += b"<p><b>Bridge problem:</b> " + PROBLEM.encode() + b"</p>"
        sock.sendall(
            b"HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/html\r\n"
            b"Content-Length: %d\r\nConnection: close\r\n\r\n" % len(page) + page
        )

    @staticmethod
    def _serve_pc(sock, path):
        route = path.split("?", 1)[0]
        if route == "/pc/hub":
            # Link state for the page's status dot. Answered by the bridge,
            # so the page can watch it without loading the Flipper.
            busy = LINK.busy_since
            body = json.dumps(
                {
                    "connected": LINK.connected,
                    "busy_s": round(time.monotonic() - busy, 1) if busy else None,
                    "stuck_after_s": STUCK_S,
                }
            ).encode()
        else:
            kind = {"/pc/info": "info", "/pc/wifi": "wifi"}.get(route)
            body = collector.as_json(kind) if kind else None
        if body is None:
            sock.sendall(
                b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
            )
            return
        sock.sendall(
            b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            b"Cache-Control: no-store\r\nContent-Length: %d\r\nConnection: close\r\n\r\n"
            % len(body)
            + body
        )

    @staticmethod
    def _read_head(sock):
        buf = b""
        while b"\r\n\r\n" not in buf:
            data = sock.recv(4096)
            if not data or len(buf) > HEAD_MAX:
                return None, b""
            buf += data
        head, _, rest = buf.partition(b"\r\n\r\n")
        return head, rest


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def launch_app(cli_path):
    """Open the hub app on the Flipper through its CLI."""
    with serial.Serial(cli_path, timeout=0.5) as cli:
        cli.write(b"\r")
        time.sleep(0.3)
        cli.reset_input_buffer()
        cli.write(f"loader open {APP_PATH}\r".encode())
        time.sleep(1.0)
        try:
            answer = cli.read_all().decode(errors="replace")
        except (serial.SerialException, OSError):
            # The app switches USB to two ports at once, so the CLI port drops.
            log("started app on the Flipper (USB switched)")
            return
    lines = [l.strip() for l in answer.splitlines() if l.strip() and ">:" not in l]
    log(f"started app on the Flipper: {' | '.join(lines[1:]) or 'ok'}")


def cli_command(cli_path, command):
    """Run one CLI command on the Flipper. The port may drop on purpose."""
    try:
        with serial.Serial(cli_path, timeout=0.5) as cli:
            cli.write(b"\r")
            time.sleep(0.3)
            cli.reset_input_buffer()
            cli.write(command.encode() + b"\r")
            time.sleep(1.0)
    except (serial.SerialException, OSError):
        pass


def recover(cli_path):
    """The app runs (its port exists) but has not answered for STUCK_S.
    Reboot the Flipper. Do not try `loader close` first: closing an app whose
    worker is stuck makes the loader wait for that thread forever, which
    freezes the whole Flipper. Writes on the Flipper use temp + rename, so a
    reboot does not corrupt repositories."""
    log(f"app has not answered for {STUCK_S:.0f} s, rebooting the Flipper")
    cli_command(cli_path, "power reboot")


def notify(url, open_browser):
    if shutil.which("notify-send"):
        subprocess.Popen(["notify-send", "-a", "Gitphin", "Gitphin", f"Open {url}"])
    if open_browser and shutil.which("xdg-open"):
        subprocess.Popen(["xdg-open", url])


def main():
    global PROBLEM
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--open-browser", action="store_true")
    args = ap.parse_args()
    url = f"http://localhost:{args.port}"

    server = Server(("127.0.0.1", args.port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    log(f"listening on {url}")

    last_seen = time.monotonic()
    launched = False
    next_try = 0.0
    notified = False

    while True:
        now = time.monotonic()
        cli = glob.glob(CLI_GLOB)
        hub = glob.glob(HUB_GLOB)

        if cli or hub:
            last_seen = now
        elif now - last_seen > UNPLUG_GRACE_S:
            log("Flipper unplugged, exit")
            return 0

        try:
            if hub and not LINK.connected and cli and now - LINK.down_since > STUCK_S:
                recover(cli[0])
                launched = False  # start the app again when its port is gone
                LINK.down_since = now  # give the next step a full STUCK_S
                next_try = now + 5.0
            elif hub and not LINK.connected and now >= next_try:
                if LINK.connect(hub[0]):
                    PROBLEM = None
                    log(f"connected to the hub app on {hub[0]}")
                    if not notified:
                        notify(url, args.open_browser)
                        notified = True
                else:
                    next_try = now + 2.0
            elif not hub and LINK.connected:
                log("hub app closed on the Flipper")
                LINK.close()
            elif cli and not hub and not launched and now >= next_try:
                # Start the app once per plug. If the user closes it, leave it closed.
                launch_app(cli[0])
                launched = True
                next_try = now + 3.0
        except (serial.SerialException, OSError) as e:
            if getattr(e, "errno", None) == 13:
                PROBLEM = "no permission for the serial port: install systemd/70-gitphin.rules (see README)"
                log(PROBLEM)
                next_try = now + 5.0
            else:
                log(f"serial error: {e}")
                next_try = now + 2.0

        time.sleep(0.5)


if __name__ == "__main__":
    sys.exit(main())
