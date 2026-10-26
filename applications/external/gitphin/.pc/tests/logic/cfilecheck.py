"""logic_check parts: commit_file checks."""

import os, random, re, shutil, subprocess, sys, tempfile
from common import *
from common import S
from diffcheck import *
from handrepo import *

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
    S.checks += 1
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
    n0 = S.checks
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
    print("cfile %-9s %d cases" % (layout, S.checks - n0))
    shutil.rmtree(repo)
