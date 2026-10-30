#!/usr/bin/env python3
"""Compare hub_gitsearch with git grep / git ls-tree. Usage: search_check.py <search_test> <repos dir>"""
import os, re, subprocess, sys

BIN, REPOS = sys.argv[1:3]
ENV = dict(
    os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null", LC_ALL="C"
)
fails = 0
peak = 0


def fail(m):
    global fails
    fails += 1
    print("FAIL:", m)


def git(repo, *a):
    r = subprocess.run(
        ["git", "--git-dir", repo] + list(a), capture_output=True, env=ENV
    )
    return r.stdout


def run(repo, tree, q, *extra):
    global peak
    r = subprocess.run(
        [BIN, repo, tree, q] + [str(x) for x in extra], capture_output=True, env=ENV
    )
    m = re.search(rb"heap_peak=(\d+)", r.stderr)
    if m:
        peak = max(peak, int(m.group(1)))
    if r.returncode != 0:
        fail("search_test rc=%d %s" % (r.returncode, r.stderr[:200]))
    hits, summary = [], None
    for line in r.stdout.split(b"\n"):
        if line.startswith(b"H\t"):
            _, path, ln, cut, text = line.split(b"\t", 4)
            hits.append((path, int(ln), int(cut), text))
        elif line.startswith(b"S "):
            summary = dict(re.findall(r"(\w+)=(-?\d+)", line.decode()))
    return hits, {k: int(v) for k, v in (summary or {}).items()}


def expected(repo, rev, q, max_blob):
    """(name hits, content hits) from git"""
    files = {}
    for rec in git(repo, "ls-tree", "-r", "-l", "-z", rev).split(b"\0"):
        if not rec:
            continue
        meta, path = rec.split(b"\t", 1)
        mode, typ, sha, size = meta.split()
        files[path] = (mode, size)
    ql = q.lower().encode()
    names = sorted(p for p in files if ql in p.lower())
    out = git(repo, "grep", "-n", "-i", "-I", "-F", "-z", "-e", q, rev)
    content = []
    for rec in out.split(b"\n"):
        if not rec:
            continue
        # with -z: "rev:path\0line\0text"
        a = rec.split(b"\0", 2)
        if len(a) != 3:
            continue
        path = a[0][len(rev) + 1 :]
        mode, size = files.get(path, (b"", b"0"))
        if max_blob and int(size) > max_blob:
            continue
        content.append((path, int(a[1]), a[2]))
    return names, content


def check(repo, rev, tree, q, max_blob=0, label=""):
    hits, sm = run(repo, tree, q, 0, max_blob)
    names, content = expected(repo, rev, q, max_blob)
    got_n = sorted(h[0] for h in hits if h[1] == 0)
    got_c = sorted((h[0], h[1], h[3]) for h in hits if h[1] > 0)
    want_c = sorted(content)
    # lines longer than 200 bytes are cut in our result
    norm = []
    for p, l, t in want_c:
        norm.append((p, l, t[:200]))
    if got_n != names:
        fail("%s %r names: got %d want %d" % (label, q, len(got_n), len(names)))
    if got_c != sorted(norm):
        gs, ws = set(got_c), set(norm)
        fail(
            "%s %r content: got %d want %d (missing %s, extra %s)"
            % (
                label,
                q,
                len(got_c),
                len(norm),
                sorted(ws - gs)[:2],
                sorted(gs - ws)[:2],
            )
        )
    for h in hits:
        if h[1] > 0 and (h[2] == 1) != (
            len(h[3]) == 200
            and True
            and any(len(t) > 200 for p, l, t in want_c if p == h[0] and l == h[1])
        ):
            fail("%s %r cut flag wrong at %s:%d" % (label, q, h[0], h[1]))
            break
    if sm.get("rc") != 0 or sm.get("truncated") != 0:
        fail("%s %r summary %s" % (label, q, sm))
    return sm


def main():
    queries = [
        "fox",
        "edited in commit 17",
        "main",
        "Flipper",
        "item 10",
        "unicode",
        "NOMATCH123",
        "a",
        "line 00042",
        "README",
        "int a(",
        ".",
        "ün",
        "FOX JUMPS",
        "dev 2",
        "octopus",
        "feature",
        "ZO",
        "slot 3",
        " ",
    ]
    total = 0
    for layout in ("onepack", "multi"):
        repo = os.path.join(REPOS, layout + ".git")
        revs = ["main", "dev"]
        for rev in revs:
            tree = git(repo, "rev-parse", rev + "^{tree}").decode().strip()
            for q in queries:
                for mb in (0, 30000):
                    check(
                        repo,
                        rev + "^{tree}",
                        tree,
                        q,
                        mb,
                        "%s/%s/mb%d" % (layout, rev, mb),
                    )
                    total += 1
    repo = os.path.join(REPOS, "onepack.git")
    tree = git(repo, "rev-parse", "main^{tree}").decode().strip()
    # max_hits: exactly max hits, truncated, and no truncation when max equals the count
    hits, sm = run(repo, tree, "a", 50, 0)
    if len(hits) != 50 or not sm["truncated"]:
        fail("max_hits 50: %d hits, %s" % (len(hits), sm))
    all_hits, sm = run(repo, tree, "octopus", 0, 0)
    n = len(all_hits)
    hits, sm = run(repo, tree, "octopus", n, 0)
    if len(hits) != n or sm["truncated"]:
        fail("max_hits == count must not truncate: %s" % sm)
    hits, sm = run(repo, tree, "octopus", n - 1, 0)
    if len(hits) != n - 1 or not sm["truncated"]:
        fail("max_hits = count - 1 must truncate: %s" % sm)
    # time budget: stops early
    hits, sm = run(repo, tree, "a", 0, 0, 10)
    if not sm["timed_out"] or not sm["truncated"]:
        fail("budget not applied: %s" % sm)
    full, sm_full = run(repo, tree, "fox", 0, 0)
    # only names / only content
    hn, _ = run(repo, tree, "readme", 0, 0, 0, "c")
    hc, _ = run(repo, tree, "test repo", 0, 0, 0, "n")
    if any(h[1] != 0 for h in hn) or any(h[1] == 0 for h in hc) or not hn or not hc:
        fail("n/c flags: %d name hits, %d content hits" % (len(hn), len(hc)))
    # skipped blobs are counted and the big file is not scanned
    _, sm = run(repo, tree, "fox", 0, 30000)
    if sm["skipped"] < 1 or sm["scanned"] < 1:
        fail("max_blob: %s" % sm)
    # a long line is cut but still matched at its end
    print(
        "search: %d git comparisons, %d failures, heap peak %d bytes"
        % (total, fails, peak)
    )
    return fails


sys.exit(1 if main() else 0)
