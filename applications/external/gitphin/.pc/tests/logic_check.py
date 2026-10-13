#!/usr/bin/env python3
"""Compare hub_gitdiff / hub_gitcommit (via logic_test) with git.
Usage: logic_check.py <logic_test binary> <repos dir from make_test_repo.sh> <work dir>
"""
import os, random, re, shutil, subprocess, sys, tempfile

BIN, REPOS, WORK = sys.argv[1:4]
ENV = dict(
    os.environ,
    GIT_CONFIG_GLOBAL="/dev/null",
    GIT_CONFIG_SYSTEM="/dev/null",
    GIT_AUTHOR_NAME="x",
    GIT_AUTHOR_EMAIL="x@x",
    GIT_COMMITTER_NAME="x",
    GIT_COMMITTER_EMAIL="x@x",
)
fails = []
checks = 0
peaks = {}


def fail(msg):
    fails.append(msg)
    print("FAIL:", msg)


def git(repo, *args, inp=None, env=None, ok=True, raw=False):
    e = dict(ENV)
    if env:
        e.update(env)
    r = subprocess.run(
        ["git", "--git-dir", repo] + list(args), input=inp, capture_output=True, env=e
    )
    if ok and r.returncode != 0:
        raise RuntimeError(
            "git %s failed: %s" % (args, r.stderr.decode(errors="replace"))
        )
    return r if raw else r.stdout.decode("utf-8", "surrogateescape")


def lt(kind, repo, *args, inp=None, env=None):
    e = dict(ENV)
    if env:
        e.update(env)
    if isinstance(inp, str):
        inp = inp.encode()
    r = subprocess.run(
        [BIN, *args[:1], repo, *args[1:]], input=inp, capture_output=True, env=e
    )
    m = re.search(rb"heap_peak=(\d+)", r.stderr)
    if m:
        peaks[kind] = max(peaks.get(kind, 0), int(m.group(1)))
    if r.returncode != 0:
        fail(
            "%s crashed/failed rc=%d: %s"
            % (args, r.returncode, r.stderr.decode(errors="replace")[:500])
        )
    return r.stdout.decode("utf-8", "surrogateescape")


ZERO = "0" * 40


# ------------------------------------------------------------------ diff
def git_diff_files(repo, a, b):
    """files-only list from git, in git order: (status, path, omode, nmode, osha, nsha)"""
    out = []
    if a is None and b is None:
        return out
    if a is None or b is None:
        t = b or a
        data = git(repo, "ls-tree", "-r", "-z", t)
        for rec in data.split("\0"):
            if not rec:
                continue
            meta, path = rec.split("\t", 1)
            mode, typ, sha = meta.split()
            if a is None:
                out.append(("A", path, "000000", mode, ZERO, sha))
            else:
                out.append(("D", path, mode, "000000", sha, ZERO))
        return out
    data = git(repo, "diff-tree", "-r", "-t", "--no-renames", "-z", a, b)
    parts = data.split("\0")
    i = 0
    while i < len(parts) and parts[i]:
        meta = parts[i]
        path = parts[i + 1]
        i += 2
        om, nm, os_, ns, st = meta[1:].split()
        if om == "040000" or nm == "040000":
            continue
        out.append((st, path, om, nm, os_, ns))
    return out


def parse_diff(text):
    res = {}
    cur = None
    for line in text.splitlines():
        if line.startswith("== "):
            _, a, b = line.split()
            cur = (a, b)
            res[cur] = {"files": [], "trunc": None, "rc": None}
        elif line.startswith("end "):
            m = re.match(r"end rc=(-?\d+) truncated=(\d)", line)
            res[cur]["rc"], res[cur]["trunc"] = int(m.group(1)), m.group(2) == "1"
        else:
            st, om, nm, os_, ns, path = line.split(" ", 5)
            res[cur]["files"].append((st, path, om, nm, os_, ns))
    return res


def check_diff_pairs(repo, pairs, label, max_files=None):
    global checks
    inp = "".join("%s %s\n" % (a or "-", b or "-") for a, b in pairs)
    args = ["diff"] + ([str(max_files)] if max_files else [])
    res = parse_diff(lt("diff", repo, *args, inp=inp))
    for a, b in pairs:
        checks += 1
        r = res.get((a or "-", b or "-"))
        want = git_diff_files(repo, a, b)
        if r is None or r["rc"] != 0:
            fail("%s diff %s %s: rc %s" % (label, a, b, r and r["rc"]))
            continue
        limit = max_files or 300
        exp = want[:limit]
        if r["files"] != exp:
            if sorted(r["files"]) == sorted(exp):
                fail("%s diff %s %s: same set but different order" % (label, a, b))
            else:
                fail(
                    "%s diff %s %s: mismatch (%d vs %d files)"
                    % (label, a, b, len(r["files"]), len(exp))
                )
        if r["trunc"] != (len(want) > limit):
            fail(
                "%s diff %s %s: truncated flag %s, files %d"
                % (label, a, b, r["trunc"], len(want))
            )


def tree_of(repo, c):
    return git(repo, "rev-parse", c + "^{tree}").strip()


def test_diff_repo(name, rnd):
    repo = os.path.join(REPOS, name + ".git")
    commits = git(repo, "rev-list", "--all").split()
    pairs = []
    for c in commits:
        parents = git(repo, "rev-list", "--parents", "-n", "1", c).split()[1:]
        t = tree_of(repo, c)
        pairs.append((tree_of(repo, parents[0]) if parents else None, t))
    for _ in range(100):
        a, b = rnd.sample(commits, 2)
        pairs.append((tree_of(repo, a), tree_of(repo, b)))
    pairs.append((tree_of(repo, commits[0]), tree_of(repo, commits[0])))  # same tree
    pairs.append((tree_of(repo, commits[0]), None))
    check_diff_pairs(repo, pairs, name)
    print("diff %-9s %d pairs" % (name, len(pairs)))


# ------------------------------------------------------------------ hand made repos
class Hand:
    def __init__(self, path):
        self.repo = path
        subprocess.run(["git", "init", "-q", "--bare", path], check=True, env=ENV)
        self.n = 0

    def blob(self, data):
        if isinstance(data, str):
            data = data.encode()
        return git(self.repo, "hash-object", "-w", "--stdin", inp=data).strip()

    def commit(self, files, parents=(), msg="c"):
        """files: path -> content | ('x', content) | ('l', target) | ('g', sha)"""
        idx = os.path.join(WORK, "hand.idx")
        if os.path.exists(idx):
            os.remove(idx)
        e = {"GIT_INDEX_FILE": idx}
        for path, v in files.items():
            if isinstance(v, tuple):
                kind, val = v
            else:
                kind, val = "f", v
            if kind == "g":
                mode, sha = "160000", val
            else:
                mode = {"f": "100644", "x": "100755", "l": "120000"}[kind]
                sha = self.blob(val)
            git(
                self.repo,
                "update-index",
                "--add",
                "--cacheinfo",
                "%s,%s,%s" % (mode, sha, path),
                env=e,
            )
        tree = (
            git(self.repo, "write-tree", env=e).strip()
            if files
            else git(
                self.repo, "hash-object", "-t", "tree", "-w", "--stdin", inp=b""
            ).strip()
        )
        self.n += 1
        ce = {
            "GIT_AUTHOR_DATE": "%d +0000" % (1600000000 + self.n * 100),
            "GIT_COMMITTER_DATE": "%d +0000" % (1600000000 + self.n * 100),
        }
        args = ["commit-tree", tree, "-m", msg]
        for p in parents:
            args += ["-p", p]
        return git(self.repo, *args, env=ce).strip(), tree


def test_diff_hand(work):
    h = Hand(os.path.join(work, "hand.git"))
    base = {
        "a": "one\n",
        "b": ("x", "exec\n"),
        "c/x": "cx\n",
        "link": ("l", "a"),
        "sub": ("g", "1" * 40),
        "d": "was file\n",
        "e/f": "ef\n",
        "same/z": "z\n",
        "uni/ü.txt": "u\n",
        "a.b": "ab\n",
        "a-b": "a-b\n",
        "a0": "a0\n",
    }
    c1, t1 = h.commit(base)
    new = dict(base)
    new["a"] = "two\n"
    new["b"] = "exec\n"  # exec bit off: M
    new["link"] = "now a file\n"  # symlink -> file: T
    new["sub"] = ("g", "2" * 40)  # gitlink moved: M
    del new["d"]
    new["d/q"] = "q\n"  # file -> dir
    del new["e/f"]
    new["e"] = "file now\n"  # dir -> file
    new["n/m/o.txt"] = "new\n"
    new["uni/ü.txt"] = "u2\n"
    new["a.c/c"] = "dir a.c next to a.b\n"  # tree "a.c/" sorts after a.b
    del new["a-b"]
    c2, t2 = h.commit(new, [c1])
    big = {"big/f%03d" % i: "n%d\n" % i for i in range(400)}
    c3, t3 = h.commit(big)
    empty = git(h.repo, "hash-object", "-t", "tree", "-w", "--stdin", inp=b"").strip()
    trees = [t1, t2, t3, empty]
    pairs = (
        [(a, b) for a in trees for b in trees]
        + [(None, t) for t in trees]
        + [(t, None) for t in trees]
    )
    check_diff_pairs(h.repo, pairs, "hand")
    check_diff_pairs(h.repo, [(None, t3)], "hand-max400", 400)
    check_diff_pairs(h.repo, [(None, t3), (t3, None)], "hand-max100", 100)
    check_diff_pairs(h.repo, [(None, t3)], "hand-max399", 399)
    # type change is reported as T, file/dir as D + A
    got = parse_diff(lt("diff", h.repo, "diff", inp="%s %s\n" % (t1, t2)))[(t1, t2)][
        "files"
    ]
    sts = {p: s for s, p, *_ in got}
    for p, s in (
        ("link", "T"),
        ("b", "M"),
        ("sub", "M"),
        ("d", "D"),
        ("d/q", "A"),
        ("e", "A"),
        ("e/f", "D"),
        ("a.c/c", "A"),
        ("a-b", "D"),
    ):
        if sts.get(p) != s:
            fail("hand statuses: %s expected %s got %s" % (p, s, sts.get(p)))
    print("diff hand      %d pairs" % len(pairs))
    return h


# ------------------------------------------------------------------ compare
def check_compare(repo, base, head, label, cap=None):
    global checks
    checks += 1
    args = ["compare", base, head] + ([str(cap)] if cap else [])
    out = lt("compare", repo, *args)
    kv = dict(re.findall(r"(\w+)=(\S+)", out))
    if kv.get("rc") != "0":
        fail("%s compare %s %s rc=%s" % (label, base[:8], head[:8], kv.get("rc")))
        return
    mbs = git(repo, "merge-base", "--all", base, head, ok=False).split()
    ahead = git(repo, "log", "--format=%H %ct", "%s..%s" % (base, head)).splitlines()
    behind_n = len(git(repo, "rev-list", "%s..%s" % (head, base)).split())
    capped = kv["capped"] == "1"
    if not capped:
        if len(ahead) != int(kv["ahead_count"]):
            fail(
                "%s compare %s..%s ahead_count %s != %d"
                % (label, base[:8], head[:8], kv["ahead_count"], len(ahead))
            )
        if behind_n != int(kv["behind_count"]):
            fail(
                "%s compare %s..%s behind_count %s != %d"
                % (label, head[:8], base[:8], kv["behind_count"], behind_n)
            )
        mb = kv["merge_base"]
        if (mb == "none") != (not mbs) or (mb != "none" and mb not in mbs):
            fail(
                "%s compare %s %s merge_base %s not in %s"
                % (label, base[:8], head[:8], mb, mbs)
            )
        isff = (
            git(
                repo, "merge-base", "--is-ancestor", base, head, raw=True, ok=False
            ).returncode
            == 0
        )
        if isff != (kv["ff"] == "1"):
            fail(
                "%s compare %s %s ff %s expected %s"
                % (label, base[:8], head[:8], kv["ff"], isff)
            )
        listed = re.findall(r"^ahead (\w+)$", out, re.M)
        times = dict(l.split() for l in ahead)
        want_n = min(50, len(ahead))
        if len(listed) != want_n or not all(x in times for x in listed):
            fail(
                "%s compare ahead list size/members (%d vs %d)"
                % (label, len(listed), want_n)
            )
        else:
            got_t = [int(times[x]) for x in listed]
            want_t = sorted((int(t) for t in times.values()), reverse=True)[:want_n]
            if got_t != want_t:
                fail(
                    "%s compare ahead list is not the newest %d first" % (label, want_n)
                )
    else:
        if int(kv["ahead_count"]) > len(ahead) or int(kv["behind_count"]) > behind_n:
            fail("%s compare capped counts exceed the real ones" % label)


def test_compare_repo(name, rnd):
    repo = os.path.join(REPOS, name + ".git")
    refs = git(repo, "for-each-ref", "--format=%(refname)").split()
    tips = sorted({git(repo, "rev-parse", r + "^{commit}").strip() for r in refs})
    commits = git(repo, "rev-list", "--all").split()
    n0 = checks
    for a in tips:
        for b in tips:
            check_compare(repo, a, b, name)
    for _ in range(60):
        a, b = rnd.sample(commits, 2)
        check_compare(repo, a, b, name)
    main = (
        git(repo, "rev-parse", "main").strip() if "refs/heads/main" in refs else tips[0]
    )
    # cap test: capped flag appears when the history is longer than the cap
    old = commits[-1]
    out = lt("compare", repo, "compare", old, main, "40")
    if "capped=1" not in out:
        fail("%s compare cap 40 not reported as capped" % name)
    out = lt("compare", repo, "compare", old, main, "100000")
    if "capped=0" not in out:
        fail("%s compare big cap reported capped" % name)
    print("compare %-8s %d pairs" % (name, checks - n0))


def test_compare_hand(h):
    # two roots, criss-cross merges, and a long line
    r1, _ = h.commit({"r1": "1\n"})
    r2, _ = h.commit({"r2": "2\n"})
    a1, _ = h.commit({"a": "1\n"}, [r1])
    b1, _ = h.commit({"b": "1\n"}, [r1])
    m1, _ = h.commit({"m1": "1\n"}, [a1, b1])  # merge a1 b1
    m2, _ = h.commit({"m2": "1\n"}, [b1, a1])  # merge b1 a1: criss-cross with m1
    x1, _ = h.commit({"x": "1\n"}, [m1])
    y1, _ = h.commit({"y": "1\n"}, [m2])
    tips = [r1, r2, a1, b1, m1, m2, x1, y1]
    for a in tips:
        for b in tips:
            check_compare(h.repo, a, b, "hand")
    print("compare hand   %d pairs" % (len(tips) ** 2))


# ------------------------------------------------------------------ commit_file
MSG = os.path.join(WORK, "msg.txt")


def write_file(path, data):
    with open(path, "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode())


def expect_tree(repo, tip, path, content, delete):
    """what git builds: (tree, err) with a scratch index"""
    idx = os.path.join(WORK, "exp.idx")
    if os.path.exists(idx):
        os.remove(idx)
    e = {"GIT_INDEX_FILE": idx}
    git(repo, "read-tree", tip, env=e)
    if delete:
        names = [
            l.split("\t", 1)[1]
            for l in git(repo, "ls-files", "--stage", env=e).splitlines()
        ]
        if path not in names:
            if any(n.startswith(path + "/") for n in names):
                return None, "conflict"
            return None, "notfound"
        git(
            repo,
            "update-index",
            "--index-info",
            inp=("0 %s\t%s\n" % (ZERO, path)).encode(),
            env=e,
        )
    else:
        ex = git(repo, "ls-tree", tip, "--", path).strip()
        mode = "100644"
        if ex:
            m, typ, _ = ex.split(None, 2)
            if typ == "tree" or m == "160000":
                return None, "conflict"
            mode = m
        blob = git(repo, "hash-object", "-w", "--stdin", inp=content).strip()
        r = git(
            repo,
            "update-index",
            "--add",
            "--cacheinfo",
            "%s,%s,%s" % (mode, blob, path),
            env=e,
            raw=True,
            ok=False,
        )
        if r.returncode != 0:
            return None, "conflict"
    return git(repo, "write-tree", env=e).strip(), None


def apply_cfile(repo, tip, path, content, delete, label, tolerate=()):
    global checks
    checks += 1
    write_file(MSG, "message for %s\n\nsecond line\n" % label)
    if delete:
        out = lt("cfile", repo, "cfile", tip, path, "del", "-", MSG)
    else:
        cf = os.path.join(WORK, "content.bin")
        write_file(cf, content)
        out = lt("cfile", repo, "cfile", tip, path, "put", cf, MSG)
    want_tree, err = expect_tree(repo, tip, path, content, delete)
    m = re.match(r"commit (\w+) tree (\w+)", out)
    if err:
        codes = {"notfound": "-1", "conflict": "-20"}
        if out.strip() != "error " + codes[err]:
            fail("%s %s: expected %s, got %r" % (label, path, err, out.strip()))
        return None
    if want_tree == tree_of(repo, tip):
        if out.strip() != "error -21":
            fail("%s %s: expected unchanged (-21), got %r" % (label, path, out.strip()))
        return None
    if not m:
        fail("%s %s: expected a commit, got %r" % (label, path, out.strip()))
        return None
    commit, tree = m.groups()
    if tree != want_tree:
        fail("%s %s: tree %s != git %s" % (label, path, tree, want_tree))
    ce = {
        "GIT_AUTHOR_NAME": "T Er",
        "GIT_AUTHOR_EMAIL": "t@example.com",
        "GIT_AUTHOR_DATE": "1700000000 +0000",
        "GIT_COMMITTER_NAME": "T Er",
        "GIT_COMMITTER_EMAIL": "t@example.com",
        "GIT_COMMITTER_DATE": "1700000000 +0000",
    }
    want_c = git(repo, "commit-tree", want_tree, "-p", tip, "-F", MSG, env=ce).strip()
    if commit != want_c:
        fail("%s %s: commit id %s != git %s" % (label, path, commit, want_c))
    return commit


def test_commit_file(layout, rnd):
    src = os.path.join(REPOS, layout + ".git")
    repo = os.path.join(WORK, "cf_" + layout + ".git")
    shutil.copytree(src, repo)
    for r, d, fs in os.walk(repo):
        os.chmod(r, 0o755)
        for f in fs:
            os.chmod(os.path.join(r, f), 0o644)
    tip = git(repo, "rev-parse", "main").strip()
    n0 = checks
    cases = [
        ("README.md", b"# changed\n", False),
        ("data/big.txt", b"small now\n", False),
        ("new-root.txt", b"hi\n", False),
        ("src/core/new.c", b"int n;\n", False),
        ("src/core/a-b.c", b"x\n", False),
        ("src/core/a.c0", b"y\n", False),
        ("a/b/c/d/e.txt", b"deep new\n", False),
        ("src/ui/deep/er/est/more.txt", b"m\n", False),
        ("src/ui/deep/new/er/x", b"m\n", False),
        ("docs/ünï.txt", b"unicode replace\n", False),
        ("docs/nüe.txt", b"unicode new\n", False),
        ("empty.txt", b"not empty\n", False),
        ("latest-readme", b"target\n", False),
        ("zzz", b"", False),
        ("src/ui/deep/er/est/f.txt", b"", True),
        ("empty.txt", b"", True),
        ("bin/blob.bin", b"", True),
        ("docs/guide.md", b"", True),
        ("feature-missing", b"", True),
        ("src/nope/x", b"", True),
        ("README.md/x", b"conflict\n", False),
        ("docs", b"folder\n", False),
        ("docs", b"", True),
        ("src/core/a.c/x", b"", True),
        ("vendor/sub/file", b"under gitlink\n", False),
        ("vendor/sub", b"over gitlink\n", False),
    ]
    for path, content, delete in cases:
        apply_cfile(repo, tip, path, content, delete, "case")
    # unchanged put
    cur = git(repo, "show", "main:README.md")
    out = lt(
        "cfile",
        repo,
        "cfile",
        tip,
        "README.md",
        "put",
        (write_file(os.path.join(WORK, "u.bin"), cur) or os.path.join(WORK, "u.bin")),
        MSG,
    )
    if out.strip() != "error -21":
        fail("unchanged put not reported: %r" % out)
    for bad in ["", "../x", "a//b", "/a", "a/", ".git/x", "a/./b", "a/../b", "a/.git"]:
        cf = os.path.join(WORK, "content.bin")
        write_file(cf, b"x")
        out = lt("cfile", repo, "cfile", tip, bad, "put", cf, MSG)
        if out.strip() != "error -22":
            fail("bad path %r gave %r" % (bad, out.strip()))
    # exec bit stays
    ex = git(repo, "ls-tree", "-r", tip).splitlines()
    # a chain of random edits, each checked against git
    files = [
        l.split("\t", 1)[1]
        for l in ex
        if l.split()[1] == "blob" and l.split()[0] != "120000"
    ]
    cur_tip = tip
    for step in range(40):
        files_now = [
            f for f in git(repo, "ls-tree", "-r", "--name-only", cur_tip).splitlines()
        ]
        r = rnd.random()
        if r < 0.35 and files_now:
            path, content, delete = (
                rnd.choice(files_now),
                ("edit %d\n" % step).encode(),
                False,
            )
        elif r < 0.65 and files_now:
            path, content, delete = rnd.choice(files_now), b"", True
        else:
            dirs = sorted({os.path.dirname(f) for f in files_now} | {"", "n1", "n1/n2"})
            d = rnd.choice(dirs)
            nm = rnd.choice(["a", "a.b", "a-b", "a0", "Z", "_u", "n%d" % step, "q.txt"])
            path = (d + "/" + nm) if d else nm
            content, delete = ("new %d\n" % step).encode(), False
        c = apply_cfile(repo, cur_tip, path, content, delete, "chain%d" % step)
        if c:
            cur_tip = c
    git(repo, "update-ref", "refs/heads/chain", cur_tip)
    r = git(repo, "fsck", "--strict", raw=True, ok=False)
    if r.returncode != 0:
        fail("fsck after commit_file on %s: %s" % (layout, r.stderr.decode()[:400]))
    # tiny limit: the big folder cannot be rewritten
    os.environ["LIMIT"] = "300"
    cf = os.path.join(WORK, "content.bin")
    write_file(cf, b"x")
    out = lt(
        "cfile",
        repo,
        "cfile",
        tip,
        "docs/limit.txt",
        "put",
        cf,
        MSG,
        env={"LIMIT": "100"},
    )
    if out.strip() != "error -2":
        fail("tiny limit not reported as TOOLARGE: %r" % out.strip())
    os.environ.pop("LIMIT")
    print("cfile %-9s %d cases" % (layout, checks - n0))
    shutil.rmtree(repo)


# ------------------------------------------------------------------ merge
def git_merge_tree(repo, a, b):
    r = git(
        repo,
        "merge-tree",
        "--write-tree",
        "--name-only",
        "--no-messages",
        a,
        b,
        raw=True,
        ok=False,
    )
    lines = r.stdout.decode().splitlines()
    return r.returncode, lines[0] if lines else "", {l for l in lines[1:] if l}


def do_merge(repo, ours, theirs, label, expect_conflicts=None, exact=False):
    global checks
    checks += 1
    mb = git(repo, "merge-base", ours, theirs, ok=False).split()
    base = mb[0] if len(mb) == 1 else "-"
    if len(mb) > 1:
        return None
    write_file(MSG, "Merge %s\n" % label)
    out = lt("merge", repo, "merge", ours, theirs, base, MSG)
    rc, gtree, gconf = git_merge_tree(repo, ours, theirs)
    m = re.match(r"commit (\w+) tree (\w+)", out)
    if m:
        if rc != 0:
            fail(
                "%s: we merged clean, git reports conflicts %s" % (label, sorted(gconf))
            )
            return None
        if m.group(2) != gtree:
            fail("%s: merged tree %s != git %s" % (label, m.group(2), gtree))
        parents = git(repo, "rev-list", "--parents", "-n", "1", m.group(1)).split()[1:]
        if parents != [ours, theirs]:
            fail("%s: parents %s" % (label, parents))
        return "clean"
    confl = set(re.findall(r"^conflict (.*)$", out, re.M))
    if not confl or not out.count("error -23"):
        fail("%s: unexpected output %r" % (label, out[:200]))
        return None
    if expect_conflicts is not None and confl != expect_conflicts:
        fail(
            "%s: conflicts %s != expected %s"
            % (label, sorted(confl), sorted(expect_conflicts))
        )
    if rc == 0:
        return "content-merged-by-git"
    if exact and gconf != confl:
        fail("%s: conflicts %s, git %s" % (label, sorted(confl), sorted(gconf)))
    if not exact and not gconf <= confl:
        fail(
            "%s: git conflicts %s not in ours %s"
            % (label, sorted(gconf), sorted(confl))
        )
    return "conflict"


def test_merge_repo(layout, rnd):
    src = os.path.join(REPOS, layout + ".git")
    repo = os.path.join(WORK, "mg_" + layout + ".git")
    shutil.copytree(src, repo)
    for r, d, fs in os.walk(repo):
        os.chmod(r, 0o755)
        for f in fs:
            os.chmod(os.path.join(r, f), 0o644)
    n0 = checks
    stats = {}
    main_c = git(repo, "rev-list", "--first-parent", "main").split()
    side = [
        git(repo, "rev-parse", b + "^{commit}").strip()
        for b in ("dev", "feature", "topic/with/slash")
    ]
    ours_list = [main_c[i] for i in (0, 5, 30, 60, 120, 200) if i < len(main_c)]
    for o in ours_list:
        for t in side:
            res = do_merge(repo, o, t, "%s-%s" % (o[:6], t[:6]))
            stats[res] = stats.get(res, 0) + 1
    # random pairs of commits
    allc = git(repo, "rev-list", "--all").split()
    for _ in range(40):
        a, b = rnd.sample(allc, 2)
        res = do_merge(repo, a, b, "rand-%s-%s" % (a[:6], b[:6]))
        stats[res] = stats.get(res, 0) + 1
    # divergent edits made with commit_file on two commits
    base = main_c[40]
    ca = base
    cb = base
    for i, (pa, pb) in enumerate(
        [
            ("README.md", "README.md"),
            ("src/core/a.c", "src/core/b2.c"),
            ("docs/guide.md", "docs/guide.md"),
            ("m/x/y.txt", "m/x/z.txt"),
            ("data/big.txt", "data/other.txt"),
        ]
    ):
        na = apply_cfile(repo, ca, pa, ("ours %d\n" % i).encode(), False, "ma%d" % i)
        nb = apply_cfile(repo, cb, pb, ("theirs %d\n" % i).encode(), False, "mb%d" % i)
        ca, cb = na or ca, nb or cb
        res = do_merge(repo, ca, cb, "div%d" % i)
        stats[res] = stats.get(res, 0) + 1
    git(repo, "update-ref", "refs/heads/ma", ca)
    r = git(repo, "fsck", "--strict", raw=True, ok=False)
    if r.returncode != 0:
        fail("fsck after merges on %s: %s" % (layout, r.stderr.decode()[:400]))
    print("merge %-9s %d merges %s" % (layout, checks - n0, stats))
    shutil.rmtree(repo)


def test_merge_hand(h):
    B = {
        "m1": "base\n",
        "m2": "base\n",
        "dir/x": "x\n",
        "dir/y": "y\n",
        "sub/a/b": "b\n",
        "del_me": "d\n",
        "keep": "k\n",
        "dd/e/f": "f\n",
        "same": "s\n",
        "z": "z\n",
    }
    b, _ = h.commit(B)
    ours = dict(B)
    ours["m1"] = "ours\n"
    ours["dir/x"] = "x-ours\n"
    ours["oa"] = "oa\n"
    ours["sub/a/b"] = "b-ours\n"
    ours["newsame"] = "same new\n"
    ours["dd/e/g"] = "g-ours\n"
    theirs = dict(B)
    theirs["m2"] = "theirs\n"
    theirs["dir/y"] = "y-theirs\n"
    theirs["ta"] = "ta\n"
    theirs["sub/a/c"] = "c\n"
    theirs["newsame"] = "same new\n"
    theirs["dd/e/h"] = "h-theirs\n"
    del theirs["z"]
    o, _ = h.commit(ours, [b])
    t, _ = h.commit(theirs, [b])
    print("merge hand clean:", do_merge(h.repo, o, t, "hand-clean"))
    # conflicts: m1 both changed, del_me modify/delete, newf add/add, nested sub/a/b both changed
    ours2 = dict(ours)
    theirs2 = dict(theirs)
    theirs2["m1"] = "theirs\n"
    del ours2["del_me"]
    theirs2["del_me"] = "d-theirs\n"
    ours2["newf"] = "o\n"
    theirs2["newf"] = "t\n"
    theirs2["sub/a/b"] = "b-theirs\n"
    o2, _ = h.commit(ours2, [b])
    t2, _ = h.commit(theirs2, [b])
    print(
        "merge hand conflicts:",
        do_merge(
            h.repo,
            o2,
            t2,
            "hand-conflict",
            expect_conflicts={"m1", "del_me", "newf", "sub/a/b"},
            exact=True,
        ),
    )
    # identical change on both sides, one side unchanged, both deleted
    both = dict(B)
    both["m1"] = "both\n"
    del both["keep"]
    o3, _ = h.commit(both, [b])
    t3, _ = h.commit(both, [b])
    print("merge hand same:", do_merge(h.repo, o3, t3, "hand-same"))
    print(
        "merge hand ff:",
        do_merge(h.repo, o, b, "hand-ours-ahead"),
        do_merge(h.repo, b, t, "hand-theirs-ahead"),
    )
    # both sides add a folder with different files; one side empties a folder
    A = dict(B)
    A["nf/a"] = "a\n"
    del A["dd/e/f"]
    Bt = dict(B)
    Bt["nf/b"] = "b\n"
    o4, _ = h.commit(A, [b])
    t4, _ = h.commit(Bt, [b])
    print("merge hand folders:", do_merge(h.repo, o4, t4, "hand-folders"))
    # file on one side, folder at the same name on the other (added both): ours file "p", theirs folder "p/"
    P1 = dict(B)
    P1["p"] = "file\n"
    P2 = dict(B)
    P2["p/q"] = "q\n"
    o5, _ = h.commit(P1, [b])
    t5, _ = h.commit(P2, [b])
    out = lt("merge", h.repo, "merge", o5, t5, b, MSG)
    if "error" not in out:
        fail("file/folder add-add not reported: %r" % out)
    else:
        print("merge hand file-vs-folder:", out.strip().replace("\n", " | "))


def test_wide(h):
    # one folder of about 24 KB (the biggest tree the default Flipper limit allows)
    global checks
    wide = {"wide/file-%04d.txt" % i: "w%d\n" % i for i in range(560)}
    wide["top"] = "t\n"
    w1, t1 = h.commit(wide)
    size = int(
        git(
            h.repo, "cat-file", "-s", git(h.repo, "rev-parse", w1 + ":wide").strip()
        ).strip()
    )
    print("wide folder tree: %d bytes" % size)
    env = {"LIMIT": "24576"}
    w2f = dict(wide)
    w2f["wide/file-0100.txt"] = "changed ours\n"
    w2f["wide/zz-new"] = "n\n"
    w3f = dict(wide)
    w3f["wide/file-0400.txt"] = "changed theirs\n"
    w3f["top2"] = "t2\n"
    w2, t2 = h.commit(w2f, [w1])
    w3, t3 = h.commit(w3f, [w1])
    out = lt("wide_diff", h.repo, "diff", inp="%s %s\n" % (t1, t2), env=env)
    if "rc=0" not in out:
        fail("wide diff: %s" % out[-200:])
    checks += 1
    write_file(MSG, "wide merge\n")
    out = lt("wide_merge", h.repo, "merge", w2, w3, w1, MSG, env=env)
    _, gtree, _ = git_merge_tree(h.repo, w2, w3)
    if gtree not in out:
        fail("wide merge: %r vs %s" % (out, gtree))
    cf = os.path.join(WORK, "content.bin")
    write_file(cf, b"edit\n")
    out = lt(
        "wide_cfile", h.repo, "cfile", w1, "wide/file-0300.txt", "put", cf, MSG, env=env
    )
    want, _ = expect_tree(h.repo, w1, "wide/file-0300.txt", b"edit\n", False)
    if want not in out:
        fail("wide cfile: %r vs %s" % (out, want))
    out = lt(
        "wide_cfile", h.repo, "cfile", w1, "wide/file-0999.txt", "put", cf, MSG, env=env
    )
    want, _ = expect_tree(h.repo, w1, "wide/file-0999.txt", b"edit\n", False)
    if want not in out:
        fail("wide cfile new: %r vs %s" % (out, want))
    # default compare cap
    base = git(h.repo, "rev-parse", "main").strip() if False else w1
    lt("compare_default", h.repo, "compare", w1, w2)


def main():
    rnd = random.Random(42)
    os.makedirs(WORK, exist_ok=True)
    for name in ("onepack", "loose", "multi", "refdelta"):
        test_diff_repo(name, rnd)
    h = test_diff_hand(WORK)
    for name in ("onepack", "multi", "loose"):
        test_compare_repo(name, rnd)
    test_compare_hand(h)
    for name in ("loose", "onepack"):
        test_commit_file(name, rnd)
    for name in ("loose", "onepack"):
        test_merge_repo(name, rnd)
    test_merge_hand(h)
    test_wide(h)
    print("\nchecks: %d" % checks)
    print(
        "heap peaks (bytes, library code incl. gitobj/inflate, default limit 32 KB): "
        + ", ".join("%s=%d" % kv for kv in sorted(peaks.items()))
    )
    if fails:
        print("%d FAILURES" % len(fails))
        sys.exit(1)
    print("python checks passed")


main()
