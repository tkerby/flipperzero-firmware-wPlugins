"""Names, errors and the HTTP client for the Flipper server (through the bridge, or the mock)."""

import http.client
import json
import os
import re
import sys
from urllib.parse import quote, urlencode, urlsplit

ZERO = "0" * 40
DEFAULT_URL = "http://127.0.0.1:8080"
API_TIMEOUT_S = 120.0  # metadata calls; transfers have no timeout (about 120 KB/s)
NAME_RE = re.compile(r"^[A-Za-z0-9_-][A-Za-z0-9._-]{0,63}$")
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
PACK_RE = re.compile(r"^pack-[0-9a-f]{40,64}\.pack$")
REF_BAD_CHARS = re.compile(r"[\x00-\x20\x7f~^:?*\[\\]")


def log(msg):
    print(f"[flipper-git] {msg}", file=sys.stderr, flush=True)


class HubError(Exception):
    def __init__(self, msg, status=0):
        super().__init__(msg)
        self.status = status


def valid_name(name):
    return bool(NAME_RE.match(name)) and not name.endswith(".git")


def check_name(name):
    if not valid_name(name):
        raise HubError(f"bad repo name: {name!r}")
    return name


def valid_ref(ref):
    """Full ref name under refs/heads or refs/tags, rules from docs/git-design.md."""
    if not (ref.startswith("refs/heads/") or ref.startswith("refs/tags/")):
        return False
    if REF_BAD_CHARS.search(ref) or ".." in ref or "//" in ref or "@{" in ref:
        return False
    if ref.endswith("/") or ref.endswith(".") or ref.endswith(".lock"):
        return False
    for part in ref.split("/"):
        if part.startswith(".") or part.endswith(".lock") or not part:
            return False
    return True


def repo_path(name, rest=""):
    return f"/ext/apps_data/gitphin/git/{name}.git" + (f"/{rest}" if rest else "")


class Hub:
    """Plain HTTP client for the Flipper server (or the mock)."""

    def __init__(self, base=None):
        base = base or os.environ.get("GITPHIN_URL") or os.environ.get("FLIPPER_HUB_URL") or DEFAULT_URL
        parts = urlsplit(base if "//" in base else "http://" + base)
        self.host = parts.hostname or "127.0.0.1"
        self.port = parts.port or 80
        self.base = f"http://{self.host}:{self.port}"
        self.progress = True

    def _open(self, method, path, query=None, body=None, headers=None, timeout=API_TIMEOUT_S):
        if query:
            q = {k: v for k, v in query.items() if v is not None}
            path += "?" + urlencode(q, quote_via=quote, safe="/")
        conn = http.client.HTTPConnection(self.host, self.port, timeout=timeout)
        try:
            conn.request(method, path, body=body, headers=headers or {})
            return conn, conn.getresponse()
        except OSError as e:
            conn.close()
            raise HubError(f"cannot reach {self.base}: {e}") from e

    def api(self, method, route, **query):
        """JSON call. Returns the decoded dict; raises HubError on ok:false or 4xx/5xx."""
        conn, resp = self._open(method, route, query)
        try:
            return self._json(resp)
        finally:
            conn.close()

    @staticmethod
    def _json(resp):
        raw = resp.read()
        try:
            data = json.loads(raw.decode("utf-8", "replace"))
        except ValueError:
            data = {}
        if resp.status >= 400 or not data.get("ok", False):
            msg = data.get("error") or f"HTTP {resp.status}"
            raise HubError(msg, resp.status)
        return data

    def download(self, path, dest, label):
        conn, resp = self._open("GET", path, timeout=None)
        try:
            if resp.status != 200:
                resp.read()
                raise HubError(f"GET {path}: HTTP {resp.status}", resp.status)
            total = int(resp.getheader("Content-Length") or 0)
            meter = Meter(label, total, self.progress)
            with open(dest, "wb") as f:
                while True:
                    chunk = resp.read(65536)
                    if not chunk:
                        break
                    f.write(chunk)
                    meter.add(len(chunk))
            meter.done()
        finally:
            conn.close()

    def upload(self, remote_path, local_path, label):
        size = os.path.getsize(local_path)
        meter = Meter(label, size, self.progress)
        with open(local_path, "rb") as f:
            body = MeteredFile(f, meter)
            headers = {"Content-Length": str(size), "Content-Type": "application/octet-stream"}
            conn, resp = self._open("PUT", "/api/file", {"path": remote_path}, body, headers, None)
            try:
                self._json(resp)
            finally:
                conn.close()
        meter.done()

    # -- typed calls
    def refs(self, name):
        """Remote refs dict {ref: sha} and HEAD ref, or None when the repo is missing."""
        try:
            d = self.api("GET", "/api/git/refs", name=name)
        except HubError as e:
            if e.status == 404:
                return None
            raise
        return {r["name"]: r["sha"] for r in d.get("refs", [])}, d.get("head") or ""

    def packs(self, name):
        """Names of every file in the remote objects/pack folder (empty when missing)."""
        try:
            d = self.api("GET", "/api/list", path=repo_path(name, "objects/pack"))
        except HubError as e:
            if e.status == 404:
                return []
            raise
        return sorted(e["name"] for e in d.get("entries", []) if not e.get("dir"))

    def server_info(self, name):
        self.api("POST", "/api/git/server-info", name=name)

    def delete_file(self, name, rest):
        self.api("POST", "/api/delete", path=repo_path(name, rest))


class Meter:
    """Transfer progress on stderr. One line on a pipe, a live percentage on a tty."""

    def __init__(self, label, total, enabled):
        self.label, self.total, self.seen = label, total, 0
        self.enabled = enabled
        self.live = enabled and sys.stderr.isatty()
        if enabled and not self.live:
            log(f"{label} ({total // 1024} KB)")

    def add(self, n):
        self.seen += n
        if self.live and self.total:
            print(f"\r[flipper-git] {self.label} {100 * self.seen // self.total}%",
                  end="", file=sys.stderr, flush=True)

    def done(self):
        if self.live:
            print(f"\r[flipper-git] {self.label} done ({self.seen // 1024} KB)    ",
                  file=sys.stderr, flush=True)


class MeteredFile:
    """File wrapper for http.client: counts the bytes it hands out."""

    def __init__(self, f, meter):
        self.f, self.meter = f, meter

    def read(self, n=-1):
        data = self.f.read(n)
        self.meter.add(len(data))
        return data
