#!/usr/bin/env python3
"""Webhook receiver for pc/tests/test_bridge.sh.

Usage: webhook_sink.py PORT OUTFILE [--fail-first N]
Appends one JSON line per POST to OUTFILE: {"path","headers","body"}. Answers 500 to the
first N requests (retry test), then 200.
"""

import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

OUT = sys.argv[2]
FAIL = (
    int(sys.argv[sys.argv.index("--fail-first") + 1])
    if "--fail-first" in sys.argv
    else 0
)
seen = 0


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_POST(self):
        global seen
        body = self.rfile.read(int(self.headers.get("Content-Length") or 0)).decode()
        seen += 1
        with open(OUT, "a") as f:
            f.write(
                json.dumps(
                    {"path": self.path, "headers": dict(self.headers), "body": body}
                )
                + "\n"
            )
        self.send_response(500 if seen <= FAIL else 200)
        self.send_header("Content-Length", "0")
        self.end_headers()


HTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
