"""Counts the requests that the bridge forwards now, so background jobs (backup)
can wait until the Flipper is idle."""

import threading
import time

_lock = threading.Lock()
_active = 0
_last_end = time.monotonic()


def enter():
    global _active
    with _lock:
        _active += 1


def leave():
    global _active, _last_end
    with _lock:
        _active = max(0, _active - 1)
        _last_end = time.monotonic()


def idle_for():
    """Seconds since the last request ended; 0 while one is in flight."""
    with _lock:
        return 0.0 if _active else time.monotonic() - _last_end
