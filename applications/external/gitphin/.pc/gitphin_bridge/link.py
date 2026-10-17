"""The link to the Flipper app: frames over the USB serial channel.

HubLink carries one HTTP exchange at a time. TcpLink has the same interface but
talks to an HTTP server over TCP (tests: pc/tests/mock_hub.py); it is used only
with `bridge.py --upstream host:port`.
"""

import socket
import sys
import threading
import time

try:
    import serial
except ImportError:  # only TcpLink works without pyserial
    serial = None

SERIAL_ERRORS = (OSError,) if serial is None else (serial.SerialException, OSError)

UNPLUG_GRACE_S = 5.0  # USB re-enumerates when the app starts or stops
STUCK_S = 180.0  # app port present but no PONG for this long: recover
RESPONSE_TIMEOUT_S = 300.0  # merge, pack and search can run for minutes on the Flipper


def log(msg):
    print(f"[gitphin] {msg}", file=sys.stderr, flush=True)


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
            except SERIAL_ERRORS:
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
        except SERIAL_ERRORS as e:
            log(f"serial error: {e}")
            self._close()
        return True


class TcpLink:
    """Test double of HubLink: one TCP connection per exchange to an HTTP server
    (the mock hub). It serves one request at a time, like the Flipper."""

    def __init__(self, host, port):
        self.addr = (host, port)
        self.lock = threading.Lock()
        self.busy_since = None
        self.down_since = time.monotonic()
        self.up = True
        self.port = True

    @property
    def connected(self):
        return True

    def exchange(self, request_chunks, client):
        with self.lock:
            self.busy_since = time.monotonic()
            try:
                with socket.create_connection(self.addr, timeout=RESPONSE_TIMEOUT_S) as up:
                    for chunk in request_chunks:
                        up.sendall(chunk)
                    client_ok = True
                    while True:
                        data = up.recv(8192)
                        if not data:
                            break
                        if client_ok:
                            try:
                                client.sendall(data)
                            except OSError:
                                client_ok = False
            except OSError as e:
                log(f"upstream error: {e}")
            finally:
                self.busy_since = None
        return True

    def close(self):
        pass


LINK = HubLink()  # main() may replace it by a TcpLink; always use link.LINK
PROBLEM = None  # shown on the 503 page when the bridge cannot reach the Flipper
