#!/usr/bin/env python3
"""Test server for the web page: tools/screenshots/demo_server.py plus a writable fake SD card.

Usage: page_server.py <route_test binary> <demo root> <sd dir> <page index.html> [port]

On top of the demo server (git routes through the real route code) it answers the file API
(/api/list, GET/PUT /api/file, /api/mkdir, /api/delete) on <sd dir>, where
/ext/apps_data/gitphin/git is the demo root's git/ folder, and a fake /pc/zip.
Every request is logged to <sd dir>/../requests.log ("METHOD path").
"""
import importlib.util
import json
import os
import sys
from urllib.parse import parse_qs, urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("demo_server", os.path.join(HERE, "..", "..", "..", "tools", "screenshots", "demo_server.py"))
demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(demo)


class H(demo.Handler):
    def log(self):
        with open(LOG, "a") as f:
            f.write(self.command + " " + self.path + "\n")

    def fs(self, q):
        p = q.get("path", "")
        if not (p == "/ext" or p.startswith("/ext/")):
            return None
        real = os.path.realpath(os.path.join(SD, p[5:]))
        ok = real == os.path.realpath(SD) or real.startswith(os.path.realpath(SD) + os.sep) or real.startswith(os.path.realpath(demo.ROOT))
        return real if ok else None

    def err(self, code, msg):
        self.send(code, json.dumps({"ok": False, "error": msg}).encode())

    def do_GET(self):
        self.log()
        u = urlsplit(self.path)
        q = {k: v[0] for k, v in parse_qs(u.query).items()}
        if u.path == "/api/list":
            d = self.fs(q)
            if not d or not os.path.isdir(d):
                return self.err(404, "no such folder")
            ents = [{"name": n, "dir": os.path.isdir(os.path.join(d, n)), "size": os.path.getsize(os.path.join(d, n)) if os.path.isfile(os.path.join(d, n)) else 0} for n in sorted(os.listdir(d))]
            return self.send(200, json.dumps({"ok": True, "path": q["path"], "entries": ents}).encode())
        if u.path == "/api/file":
            f = self.fs(q)
            if not f or not os.path.isfile(f):
                return self.err(404, "file not found")
            ct = "image/png" if f.endswith(".png") else "application/octet-stream"
            return self.send(200, open(f, "rb").read(), ct)
        if u.path == "/pc/zip":
            if q.get("ref") == "boom":
                return self.err(500, "zip failed")
            return self.send(200, b"PK\x05\x06" + b"\0" * 18, "application/zip")
        return super().do_GET()

    def do_PUT(self):
        self.log()
        u = urlsplit(self.path)
        q = {k: v[0] for k, v in parse_qs(u.query).items()}
        if u.path == "/api/file":
            f = self.fs(q)
            body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            if not f or not os.path.isdir(os.path.dirname(f)):
                return self.err(500, "could not open file")
            open(f, "wb").write(body)
            return self.send(200, b'{"ok":true}')
        if u.path == "/api/mkdir":
            f = self.fs(q)
            if not f:
                return self.err(400, "path must be inside /ext")
            os.makedirs(f, exist_ok=True)
            return self.send(200, b'{"ok":true}')
        if u.path == "/api/delete":
            f = self.fs(q)
            try:
                os.remove(f) if os.path.isfile(f) else os.rmdir(f)
            except Exception as e:
                return self.err(500, str(e))
            return self.send(200, b'{"ok":true}')
        return self.err(403, "read-only demo")

    do_POST = do_PUT


if __name__ == "__main__":
    BIN, demo.ROOT, SD, page = [os.path.abspath(a) for a in sys.argv[1:5]]
    demo.BIN = BIN
    demo.PAGE = page
    LOG = os.path.join(os.path.dirname(SD), "requests.log")
    port = int(sys.argv[5]) if len(sys.argv) > 5 else 8098
    demo.http.server.ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
