#!/usr/bin/env python3
"""Demo server for README screenshots: the Gitphin web page on the PC, no Flipper.

Usage: demo_server.py <route_test binary> <root> [port]

Git routes (/api/git/..., /git/...) run through the real route code of the app
(pc/tests/route_test: hub_git.c, hub_gitview.c, hub_gitext.c on the host), one
process per request, with <root>/git/<name>.git as /ext/apps_data/gitphin/git/<name>.git.
Issues, pull requests, releases and wiki (/api/git/doc, /docs) are read from
<root>/git/<name>.git/hub/<key>, as the doc plugin does. /api/info and /pc/hub
answer fixed demo values. Read-only: writes answer 403.
"""

import http.server
import json
import mimetypes
import os
import subprocess
import sys
import zlib
from urllib.parse import parse_qs, urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, "..", "..", "flipper_app", "files", "index.html")

INFO = {
    "ok": True,
    "name": "Gitphin",
    "firmware": "1.4.1",
    "battery": 87,
    "charging": True,
    "sd_ok": True,
    "sd_total_kib": 31166976,
    "sd_free_kib": 30519296,
    "heap_free": 84 * 1024,
    "uptime_s": 5025,
}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt, *args):
        pass

    def send(self, status, body, ctype="application/json", extra=None):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlsplit(self.path)
        q = {k: v[0] for k, v in parse_qs(u.query).items()}
        if u.path == "/":
            with open(PAGE, "rb") as f:
                return self.send(200, f.read(), "text/html; charset=utf-8")
        if u.path == "/api/info":
            return self.send(200, json.dumps(INFO).encode())
        if u.path == "/pc/hub":
            return self.send(
                200, b'{"connected":true,"busy_s":null,"stuck_after_s":180}'
            )
        if u.path == "/api/file":
            return self.file(q.get("path", ""))
        if u.path in ("/api/git/doc", "/api/git/docs"):
            return self.doc(u.path, q)
        if u.path.startswith("/api/git/") or u.path.startswith("/git/"):
            return self.route(u.path, u.query)
        self.send(404, b'{"ok":false,"error":"not found"}')

    def do_PUT(self):
        self.send(403, b'{"ok":false,"error":"read-only demo"}')

    do_POST = do_PUT

    def doc(self, path, q):
        hub = os.path.join(ROOT, "git", q.get("name", "") + ".git", "hub")
        if path.endswith("/docs"):
            d = os.path.join(hub, q.get("prefix", ""))
            keys = []
            if os.path.isdir(d):
                for n in sorted(os.listdir(d)):
                    p = os.path.join(d, n)
                    if os.path.isfile(p):
                        keys.append(
                            {
                                "key": q.get("prefix", "") + "/" + n,
                                "size": os.path.getsize(p),
                            }
                        )
            return self.send(200, json.dumps({"ok": True, "keys": keys}).encode())
        p = os.path.join(hub, q.get("key", ""))
        if ".." in q.get("key", "") or not os.path.isfile(p):
            return self.send(404, b'{"ok":false,"error":"not found"}')
        with open(p, "rb") as f:
            body = f.read()
        ctype = mimetypes.guess_type(p)[0] or "application/octet-stream"
        return self.send(200, body, ctype, {"ETag": '"%08x"' % zlib.crc32(body)})

    def file(self, path):
        # read-only: only the repo folders (avatars are <repo>.git/hub/avatar.png)
        pre = "/ext/apps_data/gitphin/git/"
        rel = path[len(pre) :] if path.startswith(pre) else ""
        p = os.path.join(ROOT, "git", rel)
        if not rel or ".." in rel.split("/") or not os.path.isfile(p):
            return self.send(404, b'{"ok":false,"error":"file not found"}')
        with open(p, "rb") as f:
            return self.send(
                200, f.read(), mimetypes.guess_type(p)[0] or "application/octet-stream"
            )

    def route(self, path, query):
        p = subprocess.run([BIN, ROOT, "GET", path, query], capture_output=True)
        if not p.stdout:
            return self.send(404, b'{"ok":false,"error":"no route"}')
        self.wfile.write(p.stdout)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    BIN, ROOT = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 8099
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
