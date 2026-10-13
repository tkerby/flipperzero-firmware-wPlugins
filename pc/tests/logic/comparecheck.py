"""logic_check parts: compare checks."""
import os, random, re, shutil, subprocess, sys, tempfile
from common import *
from common import S
from diffcheck import *
from handrepo import *

# ------------------------------------------------------------------ compare
def check_compare(repo, base, head, label, cap=None):
    S.checks += 1
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
            fail("%s compare %s..%s ahead_count %s != %d" % (label, base[:8], head[:8], kv["ahead_count"], len(ahead)))
        if behind_n != int(kv["behind_count"]):
            fail("%s compare %s..%s behind_count %s != %d" % (label, head[:8], base[:8], kv["behind_count"], behind_n))
        mb = kv["merge_base"]
        if (mb == "none") != (not mbs) or (mb != "none" and mb not in mbs):
            fail("%s compare %s %s merge_base %s not in %s" % (label, base[:8], head[:8], mb, mbs))
        isff = git(repo, "merge-base", "--is-ancestor", base, head, raw=True, ok=False).returncode == 0
        if isff != (kv["ff"] == "1"):
            fail("%s compare %s %s ff %s expected %s" % (label, base[:8], head[:8], kv["ff"], isff))
        listed = re.findall(r"^ahead (\w+)$", out, re.M)
        times = dict(l.split() for l in ahead)
        want_n = min(50, len(ahead))
        if len(listed) != want_n or not all(x in times for x in listed):
            fail("%s compare ahead list size/members (%d vs %d)" % (label, len(listed), want_n))
        else:
            got_t = [int(times[x]) for x in listed]
            want_t = sorted((int(t) for t in times.values()), reverse=True)[:want_n]
            if got_t != want_t:
                fail("%s compare ahead list is not the newest %d first" % (label, want_n))
    else:
        if int(kv["ahead_count"]) > len(ahead) or int(kv["behind_count"]) > behind_n:
            fail("%s compare capped counts exceed the real ones" % label)


def test_compare_repo(name, rnd):
    repo = os.path.join(REPOS, name + ".git")
    refs = git(repo, "for-each-ref", "--format=%(refname)").split()
    tips = sorted({git(repo, "rev-parse", r + "^{commit}").strip() for r in refs})
    commits = git(repo, "rev-list", "--all").split()
    n0 = S.checks
    for a in tips:
        for b in tips:
            check_compare(repo, a, b, name)
    for _ in range(60):
        a, b = rnd.sample(commits, 2)
        check_compare(repo, a, b, name)
    main = git(repo, "rev-parse", "main").strip() if "refs/heads/main" in refs else tips[0]
    # cap test: capped flag appears when the history is longer than the cap
    old = commits[-1]
    out = lt("compare", repo, "compare", old, main, "40")
    if "capped=1" not in out:
        fail("%s compare cap 40 not reported as capped" % name)
    out = lt("compare", repo, "compare", old, main, "100000")
    if "capped=0" not in out:
        fail("%s compare big cap reported capped" % name)
    print("compare %-8s %d pairs" % (name, S.checks - n0))


def test_compare_hand(h):
    # two roots, criss-cross merges, and a long line
    r1, _ = h.commit({"r1": "1\n"})
    r2, _ = h.commit({"r2": "2\n"})
    a1, _ = h.commit({"a": "1\n"}, [r1])
    b1, _ = h.commit({"b": "1\n"}, [r1])
    m1, _ = h.commit({"m1": "1\n"}, [a1, b1])   # merge a1 b1
    m2, _ = h.commit({"m2": "1\n"}, [b1, a1])   # merge b1 a1: criss-cross with m1
    x1, _ = h.commit({"x": "1\n"}, [m1])
    y1, _ = h.commit({"y": "1\n"}, [m2])
    tips = [r1, r2, a1, b1, m1, m2, x1, y1]
    for a in tips:
        for b in tips:
            check_compare(h.repo, a, b, "hand")
    print("compare hand   %d pairs" % (len(tips) ** 2))


