"""logic_check parts: hand made repos and diff on them."""

import os, random, re, shutil, subprocess, sys, tempfile
from common import *
from common import S
from diffcheck import *


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
