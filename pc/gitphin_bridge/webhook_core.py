"""One webhook delivery: the signed POST. Used by the bridge and by `git flipper webhook test`.

Headers: Content-Type, User-Agent: gitphin-bridge, X-Gitphin-Event, X-Gitphin-Delivery (uuid),
and X-Gitphin-Signature-256: sha256=<hex hmac of the body> when a secret is set (like GitHub).
"""

import hmac
import hashlib
import json
import time
import urllib.error
import urllib.request

TIMEOUT_S = 10.0
USER_AGENT = "gitphin-bridge"


def signature(secret, body):
    return "sha256=" + hmac.new(secret.encode(), body, hashlib.sha256).hexdigest()


def body_of(event):
    return json.dumps(event, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def ping_event():
    return {"event": "ping", "server": "gitphin", "message": "Test from gitphin-bridge",
            "time": int(time.time())}


def permanent(code):
    """An answer that a retry will not change."""
    return 400 <= code < 500 and code not in (408, 429)


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


_OPENER = urllib.request.build_opener(_NoRedirect)


def post(url, body, event, delivery, secret):
    """One try. Never raises. Returns {"time","code","error"}; code 0 = no answer."""
    result = {"time": int(time.time()), "code": 0, "error": ""}
    headers = {"Content-Type": "application/json", "User-Agent": USER_AGENT,
               "X-Gitphin-Event": event, "X-Gitphin-Delivery": delivery}
    if secret:
        headers["X-Gitphin-Signature-256"] = signature(secret, body)
    try:
        req = urllib.request.Request(url, data=body, headers=headers, method="POST")
        with _OPENER.open(req, timeout=TIMEOUT_S) as resp:
            resp.read(4096)
            result["code"] = resp.status
    except urllib.error.HTTPError as e:
        result["code"] = e.code
        result["error"] = f"HTTP {e.code}"
    except (OSError, ValueError) as e:  # URLError, timeouts, bad URL
        result["error"] = f"{type(e).__name__}: {e}"
    if not 200 <= result["code"] < 300 and not result["error"]:
        result["error"] = f"HTTP {result['code']}"
    return result


def ok(result):
    return 200 <= result["code"] < 300
