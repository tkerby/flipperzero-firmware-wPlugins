"""GET /pc/zip?name=<repo>&ref=<branch|tag|sha>[&path=sub/dir] : download a folder as .zip.

The bridge builds the archive with Python's zipfile and streams it as chunked
HTTP. It reads the repository through the Flipper's own routes, one request per
folder (/api/git/tree) and per file (/api/git/blob?sha=), each through the
bridge's queue, so the page and git keep working between two files.

Like `git archive`: top folder "<repo>-<ref>/", unix modes (executable bit,
symlinks), a folder entry for each folder, a submodule as an empty folder,
the commit id as zip comment, file times = commit time. Files of an already
compressed type are stored, the rest is deflated.
"""

import json
import os
import re
import sys
import time
import zipfile

from . import upstream

NAME_RE = re.compile(r"^[A-Za-z0-9_-][A-Za-z0-9._-]{0,63}$")
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
CHUNK = 64 * 1024
STORED_EXT = {
    ".zip",
    ".gz",
    ".tgz",
    ".bz2",
    ".xz",
    ".zst",
    ".7z",
    ".rar",
    ".lz4",
    ".jar",
    ".apk",
    ".whl",
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".webp",
    ".avif",
    ".heic",
    ".mp3",
    ".mp4",
    ".m4a",
    ".mkv",
    ".webm",
    ".ogg",
    ".opus",
    ".flac",
    ".aac",
    ".mov",
    ".woff",
    ".woff2",
    ".pdf",
    ".docx",
    ".xlsx",
    ".pptx",
    ".odt",
    ".ods",
    ".epub",
}


class ZipError(Exception):
    def __init__(self, status, msg):
        super().__init__(msg)
        self.status, self.msg = status, msg


def clean_path(path):
    """Normalise a sub folder: no leading/trailing "/", no "." or ".." parts."""
    parts = [p for p in path.split("/") if p]
    if any(p in (".", "..") or "\\" in p or "\0" in p for p in parts):
        raise ZipError(400, "bad path")
    return "/".join(parts)


def safe_label(text):
    return re.sub(r"[^A-Za-z0-9._-]+", "-", text).strip("-") or "ref"


def validate(query):
    name = query.get("name", "")
    ref = query.get("ref", "")
    if not NAME_RE.match(name) or name.endswith(".git"):
        raise ZipError(400, "bad repo name")
    if not ref or len(ref) > 200 or re.search(r"[\x00-\x1f\x7f\\]", ref) or ".." in ref:
        raise ZipError(400, "bad ref")
    return name, ref, clean_path(query.get("path", ""))


def tree(name, commit_or_ref, path):
    try:
        return upstream.api(
            "/api/git/tree", name=name, ref=commit_or_ref, path=path or None
        )
    except upstream.UpstreamError as e:
        if e.status in (400, 404):
            raise ZipError(404, e.args[0]) from e
        raise ZipError(502, e.args[0]) from e


def commit_time(name, sha):
    """Committer time of the commit, or now when it cannot be read."""
    try:
        c = upstream.api("/api/git/commit", name=name, sha=sha)
        return int(
            c.get("committer", {}).get("time")
            or c.get("author", {}).get("time")
            or time.time()
        )
    except (upstream.UpstreamError, ValueError, AttributeError):
        return int(time.time())


class ChunkedOut:
    """File-like writer for zipfile: every write becomes HTTP chunks on the socket."""

    def __init__(self, sock):
        self.sock = sock
        self.buf = bytearray()

    def write(self, data):
        self.buf += data
        if len(self.buf) >= CHUNK:
            self.flush()
        return len(data)

    def flush(self):
        if self.buf:
            self.sock.sendall(b"%x\r\n" % len(self.buf) + bytes(self.buf) + b"\r\n")
            self.buf.clear()

    def finish(self):
        self.flush()
        self.sock.sendall(b"0\r\n\r\n")


def zinfo(name, when, mode, is_dir=False):
    year = max(time.localtime(when).tm_year, 1980)
    t = time.localtime(when)
    zi = zipfile.ZipInfo(
        name + ("/" if is_dir else ""),
        (year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec & ~1),
    )
    zi.create_system = 3
    zi.external_attr = (mode << 16) | (0x10 if is_dir else 0)
    return zi


def method_for(entry_name):
    ext = os.path.splitext(entry_name)[1].lower()
    return zipfile.ZIP_STORED if ext in STORED_EXT else zipfile.ZIP_DEFLATED


def add_file(zf, top, rel, entry, when, name):
    mode = entry.get("mode", "100644")
    data = upstream.raw("/api/git/blob", name=name, sha=entry["sha"])
    if entry["type"] == "link":
        zi = zinfo(top + rel, when, 0o120777)
        zf.writestr(zi, data, compress_type=zipfile.ZIP_STORED)
        return
    zi = zinfo(top + rel, when, 0o100755 if mode == "100755" else 0o100644)
    zf.writestr(
        zi, data, compress_type=zipfile.ZIP_STORED if not data else method_for(rel)
    )


def write_archive(zf, name, commit, sub, root, when, top):
    """Walk the tree with an explicit stack; `root` is the already read listing of `sub`."""
    base = sub + "/" if sub else ""
    zf.writestr(zinfo(top.rstrip("/"), when, 0o40755, True), b"")
    stack = [("", root)]
    while stack:
        rel, listing = stack.pop()
        for e in listing["entries"]:
            ename = e.get("name", "")
            if (
                not ename
                or ename in (".", "..")
                or "/" in ename
                or "\\" in ename
                or "\0" in ename
            ):
                continue
            path = rel + ename
            kind = e.get("type")
            if kind == "tree":
                zf.writestr(zinfo(top + path, when, 0o40755, True), b"")
                try:
                    stack.append(
                        (
                            path + "/",
                            upstream.api(
                                "/api/git/tree", name=name, ref=commit, path=base + path
                            ),
                        )
                    )
                except upstream.UpstreamError as err:
                    raise ZipError(502, f"{path}: {err.args[0]}") from err
            elif kind == "commit":  # submodule: git archive writes an empty folder
                zf.writestr(zinfo(top + path, when, 0o40755, True), b"")
            elif kind in ("blob", "link"):
                try:
                    add_file(zf, top, path, e, when, name)
                except upstream.UpstreamError as err:
                    raise ZipError(502, f"{path}: {err.args[0]}") from err


def send_json_error(sock, status, msg):
    body = json.dumps({"ok": False, "error": msg}).encode()
    reason = {400: "Bad Request", 404: "Not Found", 502: "Bad Gateway"}.get(
        status, "Error"
    )
    sock.sendall(
        b"HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n"
        b"Content-Length: %d\r\nConnection: close\r\n\r\n"
        % (status, reason.encode(), len(body))
        + body
    )


def serve(sock, query):
    """Answer one /pc/zip request on sock. Never raises."""
    try:
        name, ref, sub = validate(query)
        root = tree(name, ref, sub)
        commit = root.get("commit", "")
        if not SHA_RE.match(commit):
            raise ZipError(502, "the Flipper sent no commit id")
    except ZipError as e:
        send_json_error(sock, e.status, e.msg)
        return
    except OSError:
        return
    when = commit_time(name, commit)
    label = f"{name}-{safe_label(ref)}"
    head = (
        b"HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\nCache-Control: no-store\r\n"
        b'Content-Disposition: attachment; filename="%s.zip"\r\n'
        b"Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n" % label.encode()
    )
    try:
        sock.sendall(head)
        out = ChunkedOut(sock)
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
            zf.comment = commit.encode()
            write_archive(zf, name, commit, sub, root, when, label + "/")
        out.finish()
    except ZipError as e:
        # The head is out already: end the stream without the last chunk, so the
        # client sees a broken download, not a short but valid zip.
        print(f"[gitphin] zip {name}@{ref}: {e.msg}", file=sys.stderr, flush=True)
    except OSError:
        pass  # the client went away
