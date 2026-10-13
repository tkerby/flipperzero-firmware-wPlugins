"""The bridge as a client of its own HTTP port.

ZIP, webhooks and backup read the Flipper through http://127.0.0.1:<port>, so
their requests wait in the same queue as the browser's and git's requests. The
Flipper serves one request at a time; these calls send one small request each.
"""

import http.client
import json
from urllib.parse import quote, urlencode

PORT = 8080
TIMEOUT_S = 330.0  # a little over the link's answer timeout


class UpstreamError(Exception):
    def __init__(self, msg, status=0):
        super().__init__(msg)
        self.status = status


def url(route, **query):
    q = {k: v for k, v in query.items() if v is not None}
    return route + ("?" + urlencode(q, quote_via=quote, safe="/") if q else "")


def request(method, route, body=None, **query):
    """One request. Returns (status, headers, body bytes). Raises UpstreamError when
    the bridge or the Flipper cannot be reached (status 502 or 503 seen as such)."""
    conn = http.client.HTTPConnection("127.0.0.1", PORT, timeout=TIMEOUT_S)
    try:
        conn.request(method, url(route, **query), body=body)
        resp = conn.getresponse()
        data = resp.read()
        return resp.status, resp, data
    except (OSError, http.client.HTTPException) as e:
        raise UpstreamError(f"cannot read from the Flipper: {e}", 502) from e
    finally:
        conn.close()


def api(route, **query):
    """GET a JSON route. Returns the decoded dict; raises UpstreamError on ok:false or an HTTP error."""
    status, _, raw = request("GET", route, **query)
    try:
        data = json.loads(raw.decode("utf-8", "replace"))
    except ValueError:
        data = {}
    if status == 503:
        raise UpstreamError("the Flipper app is not running", 503)
    if status >= 400 or not isinstance(data, dict) or not data.get("ok", False):
        raise UpstreamError((data.get("error") if isinstance(data, dict) else None) or f"HTTP {status}", status)
    return data


def raw(route, **query):
    """GET raw bytes (a blob). Returns the bytes; raises UpstreamError on an HTTP error."""
    status, _, data = request("GET", route, **query)
    if status != 200:
        msg = "the Flipper app is not running" if status == 503 else f"HTTP {status}"
        try:
            msg = json.loads(data.decode("utf-8", "replace")).get("error") or msg
        except (ValueError, AttributeError):
            pass
        raise UpstreamError(msg, status)
    return data
