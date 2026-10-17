"""The HTTP side of the bridge: accepts connections on 127.0.0.1, sends each request
on to the Flipper through the link, and answers /pc/* itself (pcroutes.py)."""

import socketserver
from urllib.parse import urlsplit

from . import activity, link, pcroutes
from .webhooks import HOOKS, classify

CLIENT_TIMEOUT_S = 10.0
HEAD_MAX = 16 * 1024

NOT_RUNNING_PAGE = b"""<!doctype html><meta charset=utf-8>
<meta http-equiv=refresh content=3><title>Gitphin</title>
<body style="font:16px system-ui;max-width:36em;margin:3em auto;padding:0 16px">
<h1>Gitphin</h1>
<p>The bridge runs, but the Gitphin app is not running on the Flipper.</p>
<p>On the Flipper, open <b>Apps &gt; Tools &gt; Gitphin</b>.
This page reloads every 3 seconds.</p>"""


def send_not_running(sock):
    page = NOT_RUNNING_PAGE
    if link.PROBLEM:
        page += b"<p><b>Bridge problem:</b> " + link.PROBLEM.encode() + b"</p>"
    sock.sendall(
        b"HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/html\r\n"
        b"Content-Length: %d\r\nConnection: close\r\n\r\n" % len(page) + page
    )


def read_head(sock):
    buf = b""
    while b"\r\n\r\n" not in buf:
        data = sock.recv(4096)
        if not data or len(buf) > HEAD_MAX:
            return None, b""
        buf += data
    head, _, rest = buf.partition(b"\r\n\r\n")
    return head, rest


def content_length(head):
    for line in head.split(b"\r\n")[1:]:
        name, _, value = line.partition(b":")
        if name.strip().lower() == b"content-length":
            try:
                return int(value.strip() or 0)
            except ValueError:
                return 0
    return 0


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        sock = self.request
        sock.settimeout(CLIENT_TIMEOUT_S)
        try:
            head, rest = read_head(sock)
        except OSError:
            return
        if head is None:
            return

        request_line = head.split(b"\r\n", 1)[0].decode("latin-1", "replace")
        parts = request_line.split(" ")
        method = parts[0]
        path = parts[1] if len(parts) > 1 else "/"
        # PC-side routes are answered by the bridge, not forwarded to the Flipper.
        if urlsplit(path).path.startswith("/pc/"):
            pcroutes.serve(sock, method, path)
            return
        if not link.LINK.connected:
            send_not_running(sock)
            return

        length = content_length(head)

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

        touched = classify(method, path)  # repos this request may change (webhooks)
        if touched:
            HOOKS.before(touched)
        activity.enter()
        try:
            sent = link.LINK.exchange(chunks(), sock)
        finally:
            activity.leave()
        if not sent:
            send_not_running(sock)
        elif touched:
            HOOKS.after(touched)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
    request_queue_size = 32
