"""Webhooks of the bridge: tell URLs when a repository changes.

The Flipper does not send events (the bridge compares refs). After a request that
may change refs, the bridge waits until the repo is quiet for 2 s (at most 10 s),
reads the refs again (/api/git/refs), compares them with the last snapshot and
posts events: push, repo_created, repo_deleted. Same JSON as Gitphin Droid:

  {"event":"push","repo":"x","refs":[{"ref","before","after"}],
   "head_commit":{"id","message","author","time"},"via":"usb","server":"gitphin","time":N}

Delivery runs in background threads (the proxy never waits). A failed delivery
is tried again after 2, 8 and 30 s. Settings: ~/.config/gitphin/webhooks.json.
"""

import sys
import threading
import time
import uuid
from urllib.parse import parse_qs, unquote
import re

from . import config, upstream, webhook_core

ZERO = "0" * 40
DEBOUNCE_S = 2.0
MAX_HOLD_S = 10.0
BACKOFF_S = (2, 8, 30)
MUTATING = {"PUT", "POST", "DELETE", "MKCOL", "MOVE", "COPY", "PROPPATCH"}
WRITE_ROUTES = {"commit-file", "merge", "ref", "create", "delete", "rename"}
GIT_PATH = re.compile(r"^/git/([A-Za-z0-9_-][A-Za-z0-9._-]{0,63})\.git(/|$)")
MAX_EVENT_REFS = 100


def log(msg):
    print(f"[gitphin] webhooks: {msg}", file=sys.stderr, flush=True)


def classify(method, target):
    """Names of the repos a request may change: [(name, role)]. role: "", "from", "to"."""
    method = method.upper()
    route, _, qs = target.partition("?")
    route = unquote(route)
    if method not in MUTATING:
        return []
    m = GIT_PATH.match(route)
    if m:
        return [(m.group(1), "")]
    if (
        method == "POST"
        and route.startswith("/api/git/")
        and route[len("/api/git/") :] in WRITE_ROUTES
    ):
        q = parse_qs(qs)
        name = (q.get("name") or [""])[0]
        if not name:
            return []
        if route.endswith("/rename"):
            to = (q.get("to") or [""])[0]
            return [(name, "from")] + ([(to, "to")] if to else [])
        return [(name, "")]
    return []


def snapshot(name):
    """{ref: sha} of a repo, or None when it does not exist. Raises UpstreamError otherwise."""
    try:
        data = upstream.api("/api/git/refs", name=name)
    except upstream.UpstreamError as e:
        if e.status == 404:
            return None
        raise
    return {
        r["name"]: r["sha"] for r in data.get("refs", []) if "name" in r and "sha" in r
    }


def diff_refs(before, after):
    out = []
    for ref in sorted(set(before) | set(after)):
        b, a = before.get(ref, ZERO), after.get(ref, ZERO)
        if b != a:
            out.append({"ref": ref, "before": b, "after": a})
    return out[:MAX_EVENT_REFS]


class Hooks:
    def __init__(self):
        self.lock = threading.Lock()
        self.cache = {}  # repo -> {ref: sha} or None (missing); the last known state
        self.pending = {}  # repo -> [first touch time, timer, role]
        self.status = {}  # url -> last delivery result

    # ---- called by the proxy
    @staticmethod
    def active():
        return bool(config.webhooks()["urls"])

    def before(self, names):
        """Before a request is sent on: make sure the 'before' snapshot exists."""
        if not self.active():
            with self.lock:
                self.cache.clear()  # stale once hooks are off
            return
        for name, _ in names:
            with self.lock:
                known = name in self.cache
            if not known:
                try:
                    snap = snapshot(name)
                except upstream.UpstreamError as e:
                    log(f"cannot read refs of {name}: {e}")
                    continue
                with self.lock:
                    self.cache.setdefault(name, snap)

    def after(self, names):
        if not self.active():
            return
        now = time.monotonic()
        with self.lock:
            for name, role in names:
                first, timer, old_role = self.pending.get(name, (now, None, ""))
                if timer:
                    timer.cancel()
                delay = max(0.0, min(DEBOUNCE_S, first + MAX_HOLD_S - now))
                timer = threading.Timer(delay, self._check, args=(name,))
                timer.daemon = True
                self.pending[name] = (first, timer, role or old_role)
                timer.start()

    # ---- after the quiet time
    def _check(self, name):
        with self.lock:
            _, _, role = self.pending.pop(name, (0, None, ""))
        try:
            after = snapshot(name)
        except upstream.UpstreamError as e:
            log(f"cannot read refs of {name}: {e}")
            return
        with self.lock:
            had = name in self.cache
            before = self.cache.get(name)
            self.cache[name] = after
        if not had:
            return  # no 'before': cannot tell what changed
        for event in self._events(name, before, after, role):
            self.deliver(event)

    def _events(self, name, before, after, role):
        now = int(time.time())
        base = {"repo": name}
        tail = {"via": "usb", "server": "gitphin", "time": now}
        if before is None and after is not None:
            yield {"event": "repo_created", **base, **tail}
            before = {}
            if role == "to":  # a rename: the refs are not new
                return
        elif before is not None and after is None:
            yield {"event": "repo_deleted", **base, **tail}
            return
        elif before is None:
            return
        refs = diff_refs(before, after)
        if not refs:
            return
        event = {"event": "push", **base, "refs": refs}
        head = self._head_commit(name, refs)
        if head:
            event["head_commit"] = head
        yield {**event, **tail}

    @staticmethod
    def _head_commit(name, refs):
        for r in refs:
            if r["ref"].startswith("refs/heads/") and r["after"] != ZERO:
                try:
                    c = upstream.api("/api/git/commit", name=name, sha=r["after"])
                except upstream.UpstreamError:
                    return None
                author = c.get("author") or {}
                return {
                    "id": r["after"],
                    "message": (c.get("message") or "").split("\n", 1)[0],
                    "author": author.get("name", ""),
                    "time": author.get("time", 0),
                }
        return None

    # ---- delivery
    def deliver(self, event):
        body = webhook_core.body_of(event)
        delivery = str(uuid.uuid4())
        for url in config.webhooks()["urls"]:
            threading.Thread(
                target=self._send,
                args=(url, body, event["event"], delivery),
                daemon=True,
                name="webhook",
            ).start()

    def _send(self, url, body, event, delivery):
        for attempt in range(len(BACKOFF_S) + 1):
            result = webhook_core.post(
                url, body, event, delivery, config.webhooks()["secret"]
            )
            retry = (
                not webhook_core.ok(result)
                and attempt < len(BACKOFF_S)
                and not webhook_core.permanent(result["code"])
            )
            with self.lock:
                self.status[url] = {
                    **result,
                    "event": event,
                    "tries": attempt + 1,
                    "retrying": retry,
                }
            if not webhook_core.ok(result):
                log(
                    f"{event} to {url}: {result['error']}"
                    + (" (will retry)" if retry else "")
                )
            if not retry:
                return
            time.sleep(BACKOFF_S[attempt])

    def describe(self):
        cfg = config.webhooks()
        with self.lock:
            last = dict(self.status)
        return {
            "urls": [{"url": u, "last": last.get(u)} for u in cfg["urls"]],
            "secret_set": bool(cfg["secret"]),
        }


HOOKS = Hooks()
