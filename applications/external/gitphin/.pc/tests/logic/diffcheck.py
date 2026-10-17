"""logic_check parts: diff checks against git."""

import os, random, re, shutil, subprocess, sys, tempfile
from common import *
from common import S


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
    inp = "".join("%s %s\n" % (a or "-", b or "-") for a, b in pairs)
    args = ["diff"] + ([str(max_files)] if max_files else [])
    res = parse_diff(lt("diff", repo, *args, inp=inp))
    for a, b in pairs:
        S.checks += 1
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
