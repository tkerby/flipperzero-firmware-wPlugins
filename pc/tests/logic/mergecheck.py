"""logic_check parts: merge and wide-tree checks."""
import os, random, re, shutil, subprocess, sys, tempfile
from common import *
from common import S
from diffcheck import *
from handrepo import *
from cfilecheck import *

# ------------------------------------------------------------------ merge
def git_merge_tree(repo, a, b):
    r = git(repo, "merge-tree", "--write-tree", "--name-only", "--no-messages", a, b, raw=True, ok=False)
    lines = r.stdout.decode().splitlines()
    return r.returncode, lines[0] if lines else "", {l for l in lines[1:] if l}


def do_merge(repo, ours, theirs, label, expect_conflicts=None, exact=False):
    S.checks += 1
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
            fail("%s: we merged clean, git reports conflicts %s" % (label, sorted(gconf)))
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
        fail("%s: conflicts %s != expected %s" % (label, sorted(confl), sorted(expect_conflicts)))
    if rc == 0:
        return "content-merged-by-git"
    if exact and gconf != confl:
        fail("%s: conflicts %s, git %s" % (label, sorted(confl), sorted(gconf)))
    if not exact and not gconf <= confl:
        fail("%s: git conflicts %s not in ours %s" % (label, sorted(gconf), sorted(confl)))
    return "conflict"


def test_merge_repo(layout, rnd):
    src = os.path.join(REPOS, layout + ".git")
    repo = os.path.join(WORK, "mg_" + layout + ".git")
    shutil.copytree(src, repo)
    for r, d, fs in os.walk(repo):
        os.chmod(r, 0o755)
        for f in fs:
            os.chmod(os.path.join(r, f), 0o644)
    n0 = S.checks
    stats = {}
    main_c = git(repo, "rev-list", "--first-parent", "main").split()
    side = [git(repo, "rev-parse", b + "^{commit}").strip() for b in ("dev", "feature", "topic/with/slash")]
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
    for i, (pa, pb) in enumerate([("README.md", "README.md"), ("src/core/a.c", "src/core/b2.c"), ("docs/guide.md", "docs/guide.md"),
                                   ("m/x/y.txt", "m/x/z.txt"), ("data/big.txt", "data/other.txt")]):
        na = apply_cfile(repo, ca, pa, ("ours %d\n" % i).encode(), False, "ma%d" % i)
        nb = apply_cfile(repo, cb, pb, ("theirs %d\n" % i).encode(), False, "mb%d" % i)
        ca, cb = na or ca, nb or cb
        res = do_merge(repo, ca, cb, "div%d" % i)
        stats[res] = stats.get(res, 0) + 1
    git(repo, "update-ref", "refs/heads/ma", ca)
    r = git(repo, "fsck", "--strict", raw=True, ok=False)
    if r.returncode != 0:
        fail("fsck after merges on %s: %s" % (layout, r.stderr.decode()[:400]))
    print("merge %-9s %d merges %s" % (layout, S.checks - n0, stats))
    shutil.rmtree(repo)


def test_merge_hand(h):
    B = {"m1": "base\n", "m2": "base\n", "dir/x": "x\n", "dir/y": "y\n", "sub/a/b": "b\n", "del_me": "d\n",
         "keep": "k\n", "dd/e/f": "f\n", "same": "s\n", "z": "z\n"}
    b, _ = h.commit(B)
    ours = dict(B)
    ours["m1"] = "ours\n"; ours["dir/x"] = "x-ours\n"; ours["oa"] = "oa\n"; ours["sub/a/b"] = "b-ours\n"
    ours["newsame"] = "same new\n"; ours["dd/e/g"] = "g-ours\n"
    theirs = dict(B)
    theirs["m2"] = "theirs\n"; theirs["dir/y"] = "y-theirs\n"; theirs["ta"] = "ta\n"; theirs["sub/a/c"] = "c\n"
    theirs["newsame"] = "same new\n"; theirs["dd/e/h"] = "h-theirs\n"; del theirs["z"]
    o, _ = h.commit(ours, [b])
    t, _ = h.commit(theirs, [b])
    print("merge hand clean:", do_merge(h.repo, o, t, "hand-clean"))
    # conflicts: m1 both changed, del_me modify/delete, newf add/add, nested sub/a/b both changed
    ours2 = dict(ours); theirs2 = dict(theirs)
    theirs2["m1"] = "theirs\n"
    del ours2["del_me"]; theirs2["del_me"] = "d-theirs\n"
    ours2["newf"] = "o\n"; theirs2["newf"] = "t\n"
    theirs2["sub/a/b"] = "b-theirs\n"
    o2, _ = h.commit(ours2, [b])
    t2, _ = h.commit(theirs2, [b])
    print("merge hand conflicts:", do_merge(h.repo, o2, t2, "hand-conflict", expect_conflicts={"m1", "del_me", "newf", "sub/a/b"}, exact=True))
    # identical change on both sides, one side unchanged, both deleted
    both = dict(B); both["m1"] = "both\n"; del both["keep"]
    o3, _ = h.commit(both, [b]); t3, _ = h.commit(both, [b])
    print("merge hand same:", do_merge(h.repo, o3, t3, "hand-same"))
    print("merge hand ff:", do_merge(h.repo, o, b, "hand-ours-ahead"), do_merge(h.repo, b, t, "hand-theirs-ahead"))
    # both sides add a folder with different files; one side empties a folder
    A = dict(B); A["nf/a"] = "a\n"; del A["dd/e/f"]
    Bt = dict(B); Bt["nf/b"] = "b\n"
    o4, _ = h.commit(A, [b]); t4, _ = h.commit(Bt, [b])
    print("merge hand folders:", do_merge(h.repo, o4, t4, "hand-folders"))
    # file on one side, folder at the same name on the other (added both): ours file "p", theirs folder "p/"
    P1 = dict(B); P1["p"] = "file\n"
    P2 = dict(B); P2["p/q"] = "q\n"
    o5, _ = h.commit(P1, [b]); t5, _ = h.commit(P2, [b])
    out = lt("merge", h.repo, "merge", o5, t5, b, MSG)
    if "error" not in out:
        fail("file/folder add-add not reported: %r" % out)
    else:
        print("merge hand file-vs-folder:", out.strip().replace("\n", " | "))


def test_wide(h):
    # one folder of about 24 KB (the biggest tree the default Flipper limit allows)
    wide = {"wide/file-%04d.txt" % i: "w%d\n" % i for i in range(560)}
    wide["top"] = "t\n"
    w1, t1 = h.commit(wide)
    size = int(git(h.repo, "cat-file", "-s", git(h.repo, "rev-parse", w1 + ":wide").strip()).strip())
    print("wide folder tree: %d bytes" % size)
    env = {"LIMIT": "24576"}
    w2f = dict(wide); w2f["wide/file-0100.txt"] = "changed ours\n"; w2f["wide/zz-new"] = "n\n"
    w3f = dict(wide); w3f["wide/file-0400.txt"] = "changed theirs\n"; w3f["top2"] = "t2\n"
    w2, t2 = h.commit(w2f, [w1]); w3, t3 = h.commit(w3f, [w1])
    out = lt("wide_diff", h.repo, "diff", inp="%s %s\n" % (t1, t2), env=env)
    if "rc=0" not in out:
        fail("wide diff: %s" % out[-200:])
    S.checks += 1
    write_file(MSG, "wide merge\n")
    out = lt("wide_merge", h.repo, "merge", w2, w3, w1, MSG, env=env)
    _, gtree, _ = git_merge_tree(h.repo, w2, w3)
    if gtree not in out:
        fail("wide merge: %r vs %s" % (out, gtree))
    cf = os.path.join(WORK, "content.bin")
    write_file(cf, b"edit\n")
    out = lt("wide_cfile", h.repo, "cfile", w1, "wide/file-0300.txt", "put", cf, MSG, env=env)
    want, _ = expect_tree(h.repo, w1, "wide/file-0300.txt", b"edit\n", False)
    if want not in out:
        fail("wide cfile: %r vs %s" % (out, want))
    out = lt("wide_cfile", h.repo, "cfile", w1, "wide/file-0999.txt", "put", cf, MSG, env=env)
    want, _ = expect_tree(h.repo, w1, "wide/file-0999.txt", b"edit\n", False)
    if want not in out:
        fail("wide cfile new: %r vs %s" % (out, want))
    # default compare cap
    base = git(h.repo, "rev-parse", "main").strip() if False else w1
    lt("compare_default", h.repo, "compare", w1, w2)


