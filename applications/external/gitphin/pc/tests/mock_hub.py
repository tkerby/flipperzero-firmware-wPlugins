#!/usr/bin/env python3
"""Mock Gitphin server for the git helper tests (follows docs/git-design.md).

Usage: python3 pc/tests/mock_hub.py ROOT [--host 127.0.0.1] [--port 8099] [--verbose]

ROOT is served as /ext. Implements GET /git/..., /api/git/*, and the existing
PUT /api/file, GET /api/list, POST /api/delete.
"""

import argparse
import json
import mimetypes
import os
import re
import shutil
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlsplit

ZERO = "0" * 40
NAME_RE = re.compile(r"^[A-Za-z0-9_-][A-Za-z0-9._-]{0,63}$")
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
BAD_CHARS = re.compile(r"[\x00-\x20\x7f~^:?*\[\\]")
TEXT_FILES = ("info/refs", "HEAD", "objects/info/packs")

ROOT = "."
VERBOSE = False


class Fail(Exception):
    def __init__(self, status, msg):
        super().__init__(msg)
        self.status, self.msg = status, msg


def valid_name(name):
    return bool(NAME_RE.match(name)) and not name.endswith(".git")


def valid_ref(ref, prefixes=("refs/heads/", "refs/tags/")):
    if not ref.startswith(prefixes) or BAD_CHARS.search(ref):
        return False
    if ".." in ref or "//" in ref or "@{" in ref or ref.endswith(("/", ".", ".lock")):
        return False
    return all(p and not p.startswith(".") and not p.endswith(".lock") for p in ref.split("/"))


def fs_path(path):
    """Map /ext/... to ROOT/..., refusing '..'."""
    parts = path.split("/")
    if parts[:2] != ["", "ext"] or ".." in parts:
        raise Fail(400, "bad path")
    return os.path.join(ROOT, *[p for p in parts[2:] if p])


# The repo folder on the device, /ext/apps_data/gitphin/git, below ROOT.
GIT_ROOT = ("apps_data", "gitphin", "git")


def repo_dir(name):
    if not valid_name(name or ""):
        raise Fail(400, "bad repo name")
    return os.path.join(ROOT, *GIT_ROOT, name + ".git")


def existing_repo(name):
    d = repo_dir(name)
    if not os.path.isdir(d):
        raise Fail(404, "no such repo")
    return d


def read_text(path, default=""):
    try:
        with open(path, "r") as f:
            return f.read()
    except OSError:
        return default


def write_text(path, text):
    with open(path, "w") as f:
        f.write(text)


# ---- git state

def packed_refs(d):
    refs = {}
    for line in read_text(os.path.join(d, "packed-refs")).splitlines():
        if line and line[0] not in "#^":
            sha, _, name = line.partition(" ")
            refs[name] = sha
    return refs


def all_refs(d):
    refs = packed_refs(d)
    for top in ("refs/heads", "refs/tags"):
        for dirpath, _, files in os.walk(os.path.join(d, top)):
            for fn in files:
                full = os.path.join(dirpath, fn)
                sha = read_text(full).strip()
                if SHA_RE.match(sha):
                    refs[os.path.relpath(full, d).replace(os.sep, "/")] = sha
    return dict(sorted(refs.items()))


def server_info(d):
    refs = all_refs(d)
    write_text(os.path.join(d, "info", "refs"), "".join(f"{s}\t{n}\n" for n, s in refs.items()))
    packs = sorted(f for f in os.listdir(os.path.join(d, "objects", "pack")) if f.endswith(".pack"))
    write_text(os.path.join(d, "objects", "info", "packs"), "".join(f"P {p}\n" for p in packs) + "\n")


def head_branch(d):
    m = re.match(r"ref: refs/heads/(.+)", read_text(os.path.join(d, "HEAD")).strip())
    return m.group(1) if m else ""


def count_packs(d):
    try:
        return sum(1 for f in os.listdir(os.path.join(d, "objects", "pack")) if f.endswith(".pack"))
    except OSError:
        return 0


# ---- /api/git handlers: each returns a dict merged into {"ok": true}

def g_repos(q, body):
    repos = []
    gd = os.path.join(ROOT, *GIT_ROOT)
    for fn in sorted(os.listdir(gd)) if os.path.isdir(gd) else []:
        d = os.path.join(gd, fn)
        if fn.endswith(".git") and os.path.isdir(d):
            repos.append({"name": fn[:-4], "description": read_text(os.path.join(d, "description")).strip(),
                          "head": head_branch(d), "packs": count_packs(d)})
    return {"repos": repos}


def g_create(q, body):
    d = repo_dir(q.get("name"))
    branch = q.get("branch") or "main"
    if not valid_ref("refs/heads/" + branch):
        raise Fail(400, "bad branch")
    if os.path.exists(d):
        raise Fail(409, "exists")
    for sub in ("refs/heads", "refs/tags", "objects/pack", "objects/info", "info"):
        os.makedirs(os.path.join(d, sub))
    write_text(os.path.join(d, "HEAD"), f"ref: refs/heads/{branch}\n")
    write_text(os.path.join(d, "config"),
               "[core]\n\trepositoryformatversion = 0\n\tfilemode = false\n\tbare = true\n")
    write_text(os.path.join(d, "description"), q.get("description") or "Unnamed repository")
    write_text(os.path.join(d, "info", "refs"), "")
    write_text(os.path.join(d, "objects", "info", "packs"), "\n")
    return {}


def g_delete(q, body):
    shutil.rmtree(existing_repo(q.get("name")))
    return {}


def g_rename(q, body):
    src = existing_repo(q.get("name"))
    dst = repo_dir(q.get("to"))
    if os.path.exists(dst):
        raise Fail(409, "target exists")
    os.rename(src, dst)
    return {}


def g_describe(q, body):
    write_text(os.path.join(existing_repo(q.get("name")), "description"), q.get("text", ""))
    return {}


def g_refs(q, body):
    d = existing_repo(q.get("name"))
    refs = [{"name": n, "sha": s} for n, s in all_refs(d).items()]
    m = re.match(r"ref: (.+)", read_text(os.path.join(d, "HEAD")).strip())
    return {"head": m.group(1) if m else "", "refs": refs}


def g_ref(q, body):
    d = existing_repo(q.get("name"))
    ref, old, new = q.get("ref", ""), q.get("old", ""), q.get("new", "")
    if not valid_ref(ref) or not SHA_RE.match(old) or not SHA_RE.match(new):
        raise Fail(400, "bad ref or sha")
    if all_refs(d).get(ref, ZERO) != old:
        raise Fail(409, "ref changed")
    loose = os.path.join(d, *ref.split("/"))
    if new == ZERO:
        if os.path.isfile(loose):
            os.unlink(loose)
        lines = read_text(os.path.join(d, "packed-refs")).splitlines()
        if lines:
            keep = [l for l in lines if not l.endswith(" " + ref)]
            write_text(os.path.join(d, "packed-refs"), "".join(l + "\n" for l in keep))
    else:
        os.makedirs(os.path.dirname(loose), exist_ok=True)
        write_text(loose, new + "\n")
    server_info(d)
    return {}


def g_head(q, body):
    d = existing_repo(q.get("name"))
    if not valid_ref("refs/heads/" + q.get("branch", "")):
        raise Fail(400, "bad branch")
    write_text(os.path.join(d, "HEAD"), f"ref: refs/heads/{q['branch']}\n")
    return {}


def g_server_info(q, body):
    server_info(existing_repo(q.get("name")))
    return {}


GIT_ROUTES = {
    ("GET", "repos"): g_repos, ("POST", "create"): g_create, ("POST", "delete"): g_delete,
    ("POST", "rename"): g_rename, ("POST", "describe"): g_describe, ("GET", "refs"): g_refs,
    ("POST", "ref"): g_ref, ("POST", "head"): g_head, ("POST", "server-info"): g_server_info,
}


# ---- existing file API

def f_list(q, body):
    p = fs_path(q.get("path", "/ext"))
    if not os.path.isdir(p):
        raise Fail(404, "no such folder")
    entries = []
    for fn in sorted(os.listdir(p)):
        full = os.path.join(p, fn)
        isdir = os.path.isdir(full)
        entries.append({"name": fn, "dir": isdir, "size": 0 if isdir else os.path.getsize(full)})
    return {"path": q.get("path", "/ext"), "entries": entries}


def f_put(q, body):
    p = fs_path(q.get("path", ""))
    if not os.path.isdir(os.path.dirname(p)) or os.path.isdir(p):
        raise Fail(400, "cannot write there")
    with open(p, "wb") as f:
        f.write(body)
    return {}


def f_delete(q, body):
    p = fs_path(q.get("path", ""))
    if os.path.isdir(p):
        try:
            os.rmdir(p)
        except OSError:
            raise Fail(409, "folder not empty")
    elif os.path.isfile(p):
        os.unlink(p)
    else:
        raise Fail(404, "not found")
    return {}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt, *args):
        if VERBOSE:
            print("[mock-hub]", fmt % args, file=sys.stderr)

    def send(self, status, data, ctype):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def json(self, status, obj):
        self.send(status, json.dumps(obj).encode(), "application/json")

    def serve(self, method):
        url = urlsplit(self.path)
        q = {k: v[0] for k, v in parse_qs(url.query, keep_blank_values=True).items()}
        path = unquote(url.path)
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n) if n else b""
        try:
            if path.startswith("/git/") and method == "GET":
                return self.raw_file(path)
            handler = None
            if path.startswith("/api/git/"):
                handler = GIT_ROUTES.get((method, path[len("/api/git/"):]))
            else:
                handler = {("GET", "/api/list"): f_list, ("PUT", "/api/file"): f_put,
                           ("POST", "/api/delete"): f_delete}.get((method, path))
            if handler is None:
                raise Fail(404, "no such route")
            self.json(200, {"ok": True, **handler(q, body)})
        except Fail as e:
            self.json(e.status, {"ok": False, "error": e.msg})
        except Exception as e:  # keep the mock alive, show the bug
            self.json(500, {"ok": False, "error": f"mock crashed: {e!r}"})

    def raw_file(self, path):
        m = re.match(r"^/git/([^/]+)\.git/(.+)$", path)
        if not m or any(p in ("", "..") for p in m.group(2).split("/")) or not valid_name(m.group(1)):
            raise Fail(404, "not found")
        full = os.path.join(ROOT, *GIT_ROOT, m.group(1) + ".git", *m.group(2).split("/"))
        if not os.path.isfile(full):
            raise Fail(404, "not found")
        with open(full, "rb") as f:
            data = f.read()
        ctype = "text/plain" if m.group(2) in TEXT_FILES else "application/octet-stream"
        self.send(200, data, ctype)

    def do_GET(self):
        self.serve("GET")

    def do_PUT(self):
        self.serve("PUT")

    def do_POST(self):
        self.serve("POST")


def main():
    global ROOT, VERBOSE
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8099)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    ROOT, VERBOSE = os.path.abspath(args.root), args.verbose
    os.makedirs(ROOT, exist_ok=True)
    print(f"[mock-hub] {args.host}:{args.port} serving {ROOT} as /ext", file=sys.stderr, flush=True)
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
