"""The /pc/* routes: answered by the bridge itself, never forwarded to the Flipper.

  GET  /pc/hub        link state (the page's status dot)
  GET  /pc/info       this PC (Collector tab)      GET /pc/wifi   its Wi-Fi
  GET  /pc/zip        ?name=&ref=[&path=]  download a repo folder as .zip (zipdl.py)
  GET  /pc/webhooks   configured URLs and last delivery (never the secret)
  GET  /pc/backup     backup settings, last run, result per repo
  POST /pc/backup/now start a backup run now
"""

import json
import time
from urllib.parse import parse_qs, urlsplit

from . import collector, link, zipdl
from .backup import BACKUP
from .webhooks import HOOKS


def respond(sock, status, reason, body=b"", ctype="application/json"):
    sock.sendall(
        b"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\n"
        b"Content-Length: %d\r\nConnection: close\r\n\r\n"
        % (status, reason.encode(), ctype.encode(), len(body))
        + body
    )


def hub_state():
    busy = link.LINK.busy_since
    return {
        "connected": link.LINK.connected,
        "busy_s": round(time.monotonic() - busy, 1) if busy else None,
        "stuck_after_s": link.STUCK_S,
    }


def serve(sock, method, target):
    url = urlsplit(target)
    route = url.path
    if route == "/pc/zip" and method in ("GET", "HEAD"):
        query = {
            k: v[0] for k, v in parse_qs(url.query, keep_blank_values=True).items()
        }
        zipdl.serve(sock, query)
        return
    if route == "/pc/backup/now" and method == "POST":
        started = BACKUP.start()
        respond(sock, 200, "OK", json.dumps({"ok": True, "started": started}).encode())
        return
    body = None
    if method in ("GET", "HEAD"):
        if route == "/pc/hub":
            body = json.dumps(hub_state()).encode()
        elif route == "/pc/webhooks":
            body = json.dumps(HOOKS.describe()).encode()
        elif route == "/pc/backup":
            body = json.dumps(BACKUP.status()).encode()
        else:
            kind = {"/pc/info": "info", "/pc/wifi": "wifi"}.get(route)
            body = collector.as_json(kind) if kind else None
    if body is None:
        respond(sock, 404, "Not Found")
    else:
        respond(sock, 200, "OK", body)
