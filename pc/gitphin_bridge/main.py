"""Entry point of the bridge (see bridge.py for the usage)."""

import argparse
import glob
import threading
import time

from . import launcher, link, proxy, upstream
from .backup import BACKUP
from .link import SERIAL_ERRORS, STUCK_S, UNPLUG_GRACE_S, TcpLink, log

CLI_GLOB = "/dev/serial/by-id/usb-Flipper_Devices_Inc.*-if00"
HUB_GLOB = "/dev/serial/by-id/usb-Flipper_Devices_Inc.*-if02"

DESCRIPTION = "Gitphin bridge: http://localhost:8080 <-> HTTP server on the Flipper."


def run_device_loop(url, open_browser):
    """Watch the USB ports, start the app, keep the link up. Returns when the Flipper is gone."""
    L = link.LINK
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
            if hub and not L.connected and cli and now - L.down_since > STUCK_S:
                launcher.recover(cli[0])
                launched = False  # start the app again when its port is gone
                L.down_since = now  # give the next step a full STUCK_S
                next_try = now + 5.0
            elif hub and not L.connected and now >= next_try:
                if L.connect(hub[0]):
                    link.PROBLEM = None
                    log(f"connected to the hub app on {hub[0]}")
                    if not notified:
                        launcher.notify(url, open_browser)
                        notified = True
                    BACKUP.on_connect()
                else:
                    next_try = now + 2.0
            elif not hub and L.connected:
                log("hub app closed on the Flipper")
                L.close()
            elif cli and not hub and not launched and now >= next_try:
                # Start the app once per plug. If the user closes it, leave it closed.
                launcher.launch_app(cli[0])
                launched = True
                next_try = now + 3.0
        except SERIAL_ERRORS as e:
            if getattr(e, "errno", None) == 13:
                link.PROBLEM = "no permission for the serial port: install systemd/70-gitphin.rules (see README)"
                log(link.PROBLEM)
                next_try = now + 5.0
            else:
                log(f"serial error: {e}")
                next_try = now + 2.0

        time.sleep(0.5)


def main():
    ap = argparse.ArgumentParser(description=DESCRIPTION)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--open-browser", action="store_true")
    ap.add_argument("--upstream", metavar="HOST:PORT",
                    help="tests only: use an HTTP server (tests/mock_hub.py) instead of the Flipper")
    args = ap.parse_args()
    url = f"http://localhost:{args.port}"

    upstream.PORT = args.port
    server = proxy.Server(("127.0.0.1", args.port), proxy.Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    log(f"listening on {url}")

    if args.upstream:
        host, _, port = args.upstream.rpartition(":")
        link.LINK = TcpLink(host or "127.0.0.1", int(port))
        BACKUP.on_connect()
        while True:
            time.sleep(3600)
    return run_device_loop(url, args.open_browser)
