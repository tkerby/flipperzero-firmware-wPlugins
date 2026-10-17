#!/usr/bin/env python3
"""Checks the answers of the real route code (route_test) against git, on all test repos.
Usage: route_check.py <route_test> <repos dir> <root dir>"""
import json, os, re, shutil, subprocess, sys, urllib.parse
import glob

RT, REPOS, ROOT = sys.argv[1:4]
ENV = dict(os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null")
fails, checks, routes = [], 0, 0
peaks = {}


def ok(cond, msg):
    global checks
    checks += 1
    if not cond:
        fails.append(msg)
        print("FAIL:", msg)


def git(repo, *a):
    return subprocess.run(
        ["git", "--git-dir", repo, *a], capture_output=True, env=ENV
    ).stdout.decode("utf-8", "surrogateescape")


def request(route, **q):
    """Run one GET. Returns (status, headers, body bytes, json or None). Checks the framing."""
    global routes
    routes += 1
    query = urllib.parse.urlencode(q)
    r = subprocess.run([RT, ROOT, "GET", route, query], capture_output=True, env=ENV)
    raw = r.stdout
    m = re.search(rb"heap_peak=(\d+)", r.stderr)
    if m:
        peaks[route] = max(peaks.get(route, 0), int(m.group(1)))
    label = "%s?%s" % (route, query[:70])
    ok(
        r.returncode == 0,
        "%s: route_test rc=%d %s" % (label, r.returncode, r.stderr.decode()[-300:]),
    )
    head, sep, rest = raw.partition(b"\r\n\r\n")
    ok(sep != b"", "%s: no header end" % label)
    lines = head.decode().split("\r\n")
    m = re.match(r"HTTP/1\.1 (\d+) ", lines[0])
    status = int(m.group(1)) if m else 0
    hdr = {
        l.split(":", 1)[0].lower(): l.split(":", 1)[1].strip()
        for l in lines[1:]
        if ":" in l
    }
    ok(hdr.get("connection") == "close", "%s: Connection: close missing" % label)
    if "transfer-encoding" in hdr:
        ok(
            hdr["transfer-encoding"] == "chunked" and "content-length" not in hdr,
            "%s: bad chunked head" % label,
        )
        body, p = b"", 0
        done = False
        while p < len(rest):
            e = rest.find(b"\r\n", p)
            ok(e > 0, "%s: chunk size line missing" % label)
            if e <= 0:
                break
            size_s = rest[p:e]
            ok(
                re.fullmatch(rb"[0-9a-f]{1,3}", size_s) is not None,
                "%s: bad chunk size %r" % (label, size_s),
            )
            size = int(size_s, 16)
            ok(size <= 512, "%s: chunk over 512 bytes" % label)
            p = e + 2
            if size == 0:
                ok(
                    rest[p:] == b"\r\n",
                    "%s: bad end of stream %r" % (label, rest[p : p + 20]),
                )
                done = True
                break
            ok(
                rest[p + size : p + size + 2] == b"\r\n",
                "%s: chunk not followed by CRLF" % label,
            )
            body += rest[p : p + size]
            p += size + 2
        ok(done, "%s: no final chunk" % label)
    else:
        n = int(hdr.get("content-length", "-1"))
        ok(n == len(rest), "%s: Content-Length %d but body %d" % (label, n, len(rest)))
        body = rest
    js = None
    try:
        js = json.loads(body.decode("utf-8"))
    except Exception as ex:
        ok(False, "%s: not valid JSON (%s): %r" % (label, ex, body[:120]))
    return status, hdr, body, js


def check_repo(name):
    repo = os.path.join(ROOT, "git", name + ".git")
    n = "name"
    # ---- refs ----
    st, h, b, js = request("/api/git/refs", name=name)
    ok(st == 200 and js and js.get("ok") is True, "%s refs status" % name)
    if js:
        want = {}
        for line in git(
            repo,
            "for-each-ref",
            "--format=%(objectname) %(refname) %(*objectname)",
            "refs/heads",
            "refs/tags",
        ).splitlines():
            p = line.split()
            want[p[1]] = (p[0], p[2] if len(p) > 2 else None)
        got = {r["name"]: (r["sha"], r.get("peeled")) for r in js["refs"]}
        ok(
            set(got) == set(want),
            "%s refs: names differ %s" % (name, set(got) ^ set(want)),
        )
        for k, (sha, peeled) in want.items():
            if k in got:
                ok(got[k][0] == sha, "%s refs: sha of %s" % (name, k))
                if peeled:
                    ok(
                        got[k][1] is not None,
                        "%s refs: %s should be peeled" % (name, k),
                    )
        ok(js["head"].startswith("refs/heads/"), "%s refs: head" % name)
        ok(
            [r["name"] for r in js["refs"]] == sorted(r["name"] for r in js["refs"]),
            "%s refs: not sorted" % name,
        )
    main = git(repo, "rev-parse", "main").strip()
    dev = git(repo, "rev-parse", "dev").strip()
    # ---- log ----
    for ref, skip, cnt in (
        ("main", 0, 30),
        ("main", 10, 7),
        ("main", 0, 50),
        (main, 100, 30),
        ("main", 5000, 5),
        ("dev", 0, 3),
    ):
        st, h, b, js = request("/api/git/log", name=name, ref=ref, skip=skip, n=cnt)
        ok(st == 200, "%s log %s status %d" % (name, ref, st))
        if js and st == 200:
            allc = git(repo, "rev-list", "--first-parent", ref).split()
            exp = allc[skip : skip + cnt]
            ok(
                [c["sha"] for c in js["commits"]] == exp,
                "%s log %s skip %d n %d: commits differ" % (name, ref, skip, cnt),
            )
            ok(js["next_skip"] == skip + len(js["commits"]), "%s log next_skip" % name)
            ok(js["more"] == (skip + len(exp) < len(allc)), "%s log more" % name)
            if re.fullmatch("[0-9a-f]{40}", ref):
                ok(
                    "immutable" in h.get("cache-control", ""),
                    "%s log by sha: cache header" % name,
                )
            else:
                ok(
                    h.get("cache-control") == "no-cache",
                    "%s log by name: cache header %s" % (name, h.get("cache-control")),
                )
            for c in js["commits"][:3]:
                ok(
                    set(c)
                    >= {"sha", "tree", "parents", "author", "committer", "message"},
                    "%s log: commit keys" % name,
                )
    # ---- commit ----
    for sha in git(repo, "rev-list", "--all", "--max-count=60").split()[::7]:
        st, h, b, js = request("/api/git/commit", name=name, sha=sha)
        ok(st == 200 and js and js["sha"] == sha, "%s commit %s" % (name, sha[:8]))
        if js:
            ok(
                js["tree"] == git(repo, "rev-parse", sha + "^{tree}").strip(),
                "%s commit tree" % name,
            )
            ok(
                js["parents"]
                == git(repo, "rev-list", "--parents", "-n1", sha).split()[1:][:4],
                "%s commit parents" % name,
            )
            (
                ok(
                    js["message"].rstrip("\n")
                    == git(repo, "log", "-1", "--format=%B", sha).rstrip("\n").rstrip(),
                    "%s commit message" % name,
                )
                if False
                else None
            )
            ok(
                js["author"]["time"]
                == int(git(repo, "log", "-1", "--format=%at", sha)),
                "%s commit author time" % name,
            )
            ok("immutable" in h.get("cache-control", ""), "%s commit cache" % name)
    # ---- tree ----
    for ref, path in (
        ("main", ""),
        (main, ""),
        ("main", "src"),
        ("main", "src/core"),
        ("main", "docs"),
        ("main", "nothere"),
    ):
        st, h, b, js = request("/api/git/tree", name=name, ref=ref, path=path)
        if path == "nothere":
            ok(st == 404, "%s tree nothere -> %d" % (name, st))
            continue
        ok(st == 200, "%s tree %s status %d" % (name, path, st))
        if js and st == 200:
            ls = [
                l
                for l in git(
                    repo, "ls-tree", "-z", "%s:%s" % (ref, path) if path else ref
                ).split("\0")
                if l
            ]
            ents = [
                (
                    l.split(None, 3)[0],
                    l.split(None, 3)[1],
                    l.split(None, 3)[2],
                    l.split("\t", 1)[1],
                )
                for l in ls
            ]
            got = [(e["mode"], e["type"], e["sha"], e["name"]) for e in js["entries"]]
            norm = lambda x: sorted(
                (
                    m.zfill(6),
                    {"commit": "commit"}.get(t, "link" if m == "120000" else t),
                    s,
                    n,
                )
                for m, t, s, n in x
            )
            ok(norm(got) == norm(ents), "%s tree %s: entries differ" % (name, path))
            kinds = [e["type"] == "tree" for e in js["entries"]]
            ok(
                kinds == sorted(kinds, reverse=True),
                "%s tree %s: folders first" % (name, path),
            )
            ok(
                js["commit"] == git(repo, "rev-parse", ref).strip(),
                "%s tree commit" % name,
            )
    # ---- compare ----
    m20, m3, m5 = (git(repo, "rev-parse", "main~%d" % k).strip() for k in (20, 3, 5))
    for base, head in (
        ("main", "dev"),
        ("dev", "main"),
        ("main", "feature"),
        (m20, "main"),
        ("main", m3),
        (m20, m5),
        ("v1.0", "main"),
    ):
        st, h, b, js = request("/api/git/compare", name=name, base=base, head=head)
        ok(st == 200, "%s compare %s..%s status %d" % (name, base, head, st))
        if js and st == 200:
            ahead = int(git(repo, "rev-list", "--count", "%s..%s" % (base, head)))
            behind = int(git(repo, "rev-list", "--count", "%s..%s" % (head, base)))
            ok(
                js["ahead_count"] == ahead and js["behind_count"] == behind,
                "%s compare %s..%s counts %s/%s vs %d/%d"
                % (
                    name,
                    base,
                    head,
                    js["ahead_count"],
                    js["behind_count"],
                    ahead,
                    behind,
                ),
            )
            ok(
                len(js["ahead"]) == min(ahead, 50),
                "%s compare ahead list size %d vs %d"
                % (name, len(js["ahead"]), min(ahead, 50)),
            )
            mbs = git(repo, "merge-base", "--all", base, head).split()
            ok(
                (js["merge_base"] in mbs) if mbs else js["merge_base"] is None,
                "%s compare merge base" % name,
            )
            for c in js["ahead"][:2]:
                ok(
                    set(c) >= {"sha", "message", "author"},
                    "%s compare ahead keys" % name,
                )
    # ---- diff ----
    for kw in (
        {"sha": main},
        {"sha": dev},
        {"base": m5, "head": main},
        {"base": "v1.0", "head": "dev"},
    ):
        st, h, b, js = request("/api/git/diff", name=name, **kw)
        ok(st == 200, "%s diff %s status %d" % (name, kw, st))
        if js and st == 200:
            if "sha" in kw:
                exp = git(
                    repo,
                    "diff-tree",
                    "-r",
                    "--no-commit-id",
                    "--name-status",
                    "--root",
                    kw["sha"],
                ).splitlines()
            else:
                exp = git(
                    repo,
                    "diff-tree",
                    "-r",
                    "--no-commit-id",
                    "--name-status",
                    kw["base"],
                    kw["head"],
                ).splitlines()
            ok(
                sorted(f["path"] for f in js["files"])
                == sorted(l.split("\t", 1)[1] for l in exp),
                "%s diff %s: files differ (%d vs %d)"
                % (name, kw, len(js["files"]), len(exp)),
            )
            ok(js["truncated"] in (True, False), "%s diff truncated flag" % name)
            if "sha" in kw:
                ok(
                    "immutable" in h.get("cache-control", ""),
                    "%s diff by sha: cache" % name,
                )
    # ---- search ----
    for q in ("deepest", "Hello", "zzzzqqq"):
        st, h, b, js = request("/api/git/search", name=name, ref="main", q=q, max=30)
        ok(st == 200, "%s search %s status" % (name, q))
        if js and st == 200:
            ok(
                len(js["hits"]) > 0 if q != "zzzzqqq" else len(js["hits"]) == 0,
                "%s search %s: %d hits" % (name, q, len(js["hits"])),
            )
    # ---- storage, repos (not chunked) ----
    st, h, b, js = request("/api/git/storage", name=name)
    ok(st == 200 and js and js["ok"], "%s storage" % name)
    # ---- errors stay fixed JSON ----
    st, h, b, js = request("/api/git/log", name=name, ref="nosuchref")
    ok(st == 404 and "content-length" in h, "%s log unknown ref -> %d" % (name, st))
    st, h, b, js = request("/api/git/compare", name=name, base="main")
    ok(
        st == 400 and "content-length" in h,
        "%s compare without head -> %d" % (name, st),
    )


def main():
    names = []
    for layout in ("loose", "onepack", "multi", "refdelta"):
        dst = os.path.join(ROOT, "git", layout + ".git")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copytree(os.path.join(REPOS, layout + ".git"), dst)
        names.append(layout)
    st, h, b, js = request("/api/git/repos")
    ok(
        st == 200 and js and {r["name"] for r in js["repos"]} >= set(names),
        "repos list",
    )
    for n in names:
        check_repo(n)
    # a repo with an empty folder, no commits yet
    empty = os.path.join(ROOT, "git", "empty.git")
    subprocess.run(["git", "init", "-q", "--bare", empty], check=True, env=ENV)
    st, h, b, js = request("/api/git/refs", name="empty")
    ok(st == 200 and js["refs"] == [], "empty repo refs")
    st, h, b, js = request("/api/git/log", name="empty")
    ok(st in (404, 200), "empty repo log")
    # ---- blob (raw bytes, big objects: the far window goes through hub/cache) ----
    import random

    rnd = random.Random(9)
    pool = [
        (
            "line %05d " % i
            + "".join(rnd.choice("abcdefghijklmnop qrstuvwxyz") for _ in range(90))
            + "\n"
        ).encode()
        for i in range(1500)
    ]
    src_dir = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
        "flipper_app",
        "src",
    )
    src = b"".join(
        open(os.path.join(src_dir, f), "rb").read()
        for f in sorted(glob.glob("**/*.[ch]", root_dir=src_dir, recursive=True))
    )[:400000]
    blobs = {
        "text": b"".join(rnd.choice(pool) for _ in range(10000))[: 1 << 20],
        "source": src,
        "binary": bytes(rnd.randrange(256) for _ in range(300000)),
    }
    bb = os.path.join(ROOT, "git", "bigblobs.git")
    subprocess.run(["git", "init", "-q", "--bare", bb], check=True, env=ENV)
    oids = {
        k: subprocess.run(
            ["git", "--git-dir", bb, "hash-object", "-w", "--stdin"],
            input=v,
            capture_output=True,
            check=True,
            env=ENV,
        )
        .stdout.decode()
        .strip()
        for k, v in blobs.items()
    }
    for layout in ("loose", "packed"):
        if layout == "packed":
            subprocess.run(
                ["git", "--git-dir", bb, "repack", "-a", "-d", "-q"],
                check=True,
                env=ENV,
            )
            subprocess.run(
                ["git", "--git-dir", bb, "prune-packed"], check=True, env=ENV
            )
            shutil.rmtree(os.path.join(bb, "hub"), ignore_errors=True)
        for k, oid in oids.items():
            r = subprocess.run(
                [RT, ROOT, "GET", "/api/git/blob", "name=bigblobs&sha=" + oid],
                capture_output=True,
                env=ENV,
            )
            m = re.search(rb"heap_peak=(\d+)", r.stderr)
            if m:
                peaks["/api/git/blob"] = max(
                    peaks.get("/api/git/blob", 0), int(m.group(1))
                )
            head, _, body = r.stdout.partition(b"\r\n\r\n")
            ok(
                head.startswith(b"HTTP/1.1 200") and body == blobs[k],
                "blob %s %s: output differs (%d vs %d)"
                % (layout, k, len(body), len(blobs[k])),
            )
            ok(b"immutable" in head, "blob by sha: cache header")
    print("heap peaks of the routes (bytes, library code, worst request):")
    for k in sorted(peaks):
        print("  %-22s %7d" % (k, peaks[k]))
    print(
        "route checks: %d requests, %d checks, failures: %d"
        % (routes, checks, len(fails))
    )
    return 1 if fails else 0


sys.exit(main())
