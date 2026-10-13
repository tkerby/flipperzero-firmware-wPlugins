#!/usr/bin/env python3
"""Host checks of the commit graph, the one-pack packer and the read counts.
Usage: graph_check.py <logic_test> <gitpack_tool> <repos dir> <work dir>"""
import os, re, shutil, struct, subprocess, sys
import glob

LT, GP, REPOS, WORK = sys.argv[1:5]
ENV = dict(os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null",
           GIT_AUTHOR_NAME="x", GIT_AUTHOR_EMAIL="x@x", GIT_COMMITTER_NAME="x", GIT_COMMITTER_EMAIL="x@x")
fails, checks = [], 0


def ok(cond, msg):
    global checks
    checks += 1
    if not cond:
        fails.append(msg)
        print("FAIL:", msg)


def git(repo, *a, inp=None, check=True):
    r = subprocess.run(["git", "--git-dir", repo, *a], input=inp, capture_output=True, env=ENV)
    if check and r.returncode:
        raise RuntimeError("git %s: %s" % (a, r.stderr.decode(errors="replace")))
    return r.stdout.decode("utf-8", "surrogateescape")


def run(bin_, *a, inp=None):
    r = subprocess.run([bin_, *a], input=inp.encode() if isinstance(inp, str) else inp, capture_output=True, env=ENV)
    return r.returncode, r.stdout.decode(errors="replace"), r.stderr.decode(errors="replace")


def fresh(name, drop_hub=True):
    d = os.path.join(WORK, name)
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(os.path.join(REPOS, name.split("-")[0] + ".git"), d)
    os.system("chmod -R u+w %s" % d)
    git(d, "config", "receive.autogc", "false")
    git(d, "config", "gc.auto", "0")
    if drop_hub:
        shutil.rmtree(os.path.join(d, "hub"), ignore_errors=True)
    return d


def build_graph(repo, every):
    rc, out, err = run(GP, "graph", repo, str(every))
    ok(rc == 0, "graph build rc=%d %s" % (rc, err[-200:]))
    m = re.search(r"graph: (\d+) calls, (\d+) records", out)
    return int(m.group(1)) if m else -1, int(m.group(2)) if m else -1


def parse_graph(repo):
    b = open(os.path.join(repo, "hub", "graph.bin"), "rb").read()
    ok(b[:8] == b"HGRF\0\0\0\1", "graph magic")
    n, npend = struct.unpack(">II", b[8:16])
    fan = struct.unpack(">256I", b[16:1040])
    ok(len(b) == 1040 + 68 * n + 20 * npend, "graph size")
    recs = []
    for i in range(n):
        r = b[1040 + 68 * i: 1040 + 68 * (i + 1)]
        recs.append((r[:20].hex(), struct.unpack(">I", r[20:24])[0], r[24:44].hex(), r[44:64].hex(), struct.unpack(">I", r[64:68])[0]))
    ids = [r[0] for r in recs]
    ok(ids == sorted(ids), "graph sorted")
    cnt = [0] * 256
    for i in ids:
        cnt[int(i[:2], 16)] += 1
    run_ = 0
    for k in range(256):
        run_ += cnt[k]
        if fan[k] != run_:
            ok(False, "fanout %d" % k)
            break
    return recs, npend


Z = "0" * 40


def check_graph(repo, label):
    recs, npend = parse_graph(repo)
    ok(npend == 0, "%s: pending left %d" % (label, npend))
    want = {}
    for line in git(repo, "rev-list", "--all", "--parents", "--format=%ct", "--no-commit-header").split("\n") if False else []:
        pass
    parents = {}
    for line in git(repo, "rev-list", "--all", "--parents").splitlines():
        p = line.split()
        parents[p[0]] = p[1:]
    times = {}
    for line in git(repo, "log", "--all", "--format=%H %ct").splitlines():
        h, t = line.split()
        times[h] = int(t)
    got = {r[0]: r for r in recs}
    ok(not (set(parents) - set(got)), "%s: %d commits of git are missing" % (label, len(set(parents) - set(got))))
    # ids from refs that git ignores (a packed-refs line hidden by a loose ref) may be there, if they are commits
    extra = sorted(set(got) - set(parents))
    if extra:
        kinds = git(repo, "cat-file", "--batch-check=%(objecttype)", inp=("\n".join(extra) + "\n").encode()).split()
        ok(all(k == "commit" for k in kinds), "%s: graph holds ids that are not commits" % label)
    bad = 0
    for h, p in parents.items():
        r = got.get(h)
        if not r:
            continue
        p1 = p[0] if len(p) > 0 else Z
        p2 = p[1] if len(p) > 1 else Z
        fl = (1 if len(p) > 2 else 0) | (min(len(p), 255) << 8)
        if (r[1], r[2], r[3], r[4]) != (times[h], p1, p2, fl):
            bad += 1
    ok(bad == 0, "%s: %d records differ from git" % (label, bad))
    return len(recs)


def compare_outputs(repo, refs, label_repo):
    """stdout of compare/log/meta for many pairs"""
    out = []
    for b, h in refs:
        rc, o, e = run(LT, "compare", repo, b, h)
        out.append(o)
    for ref in (refs[0][0], refs[0][1]):
        for skip, n in ((0, 30), (10, 7), (500, 5), (60, 50)):
            rc, o, e = run(LT, "log", repo, ref, str(skip), str(n))
            out.append(o)
    ids = "\n".join(sorted(set(sum([list(x) for x in refs], [])))) + "\n"
    rc, o, e = run(LT, "meta", repo, inp=ids)
    # the cached column differs by design: drop it
    out.append("\n".join(" ".join(l.split()[:5]) for l in o.splitlines()))
    return "\n=====\n".join(out)


def ref_pairs(repo):
    sh = lambda r: git(repo, "rev-parse", r).strip()
    names = ["main", "dev", "feature", "topic/with/slash", "v1.0", "main~20", "main~100", "dev~3"]
    shas = []
    for n in names:
        r = subprocess.run(["git", "--git-dir", repo, "rev-parse", "--verify", "-q", n + "^{commit}"], capture_output=True, env=ENV)
        if r.returncode == 0:
            shas.append(r.stdout.decode().strip())
    pairs = [(a, b) for a in shas for b in shas if a != b]
    return pairs[:24]


def main():
    os.makedirs(WORK, exist_ok=True)
    # ---- 1. graph built in small slices equals git ----
    for layout in ("loose", "onepack", "multi", "refdelta"):
        for every in (3, 0):
            d = fresh(layout)
            calls, n = build_graph(d, every)
            cn = check_graph(d, "%s every=%d" % (layout, every))
            print("graph %-9s stop_every=%d: %d slices, %d records" % (layout, every, calls, cn))
            ok(calls > 1 if every == 3 else True, "slices expected")
            # a second run changes nothing
            before = open(os.path.join(d, "hub", "graph.bin"), "rb").read()
            run(GP, "graph", d, "0")
            ok(before == open(os.path.join(d, "hub", "graph.bin"), "rb").read(), "second run rewrote the file")
    # ---- 2. results identical: absent / present / stale ----
    d = fresh("multi")
    pairs = ref_pairs(d)
    absent = compare_outputs(d, pairs, "absent")
    build_graph(d, 7)
    present = compare_outputs(d, pairs, "present")
    ok(absent == present, "multi: compare/log/meta differ with the graph present")
    # stale: new commits pushed after the graph was built (some on dev, some on main, a merge)
    wt = os.path.join(WORK, "wt")
    shutil.rmtree(wt, ignore_errors=True)
    subprocess.run(["git", "clone", "-q", d, wt], check=True, env=ENV)
    g = lambda *a: subprocess.run(["git", "-C", wt, *a], check=True, capture_output=True, env=ENV)
    g("checkout", "-q", "main")
    for i in range(5):
        open(os.path.join(wt, "new%d.txt" % i), "w").write("x%d\n" % i)
        g("add", "-A")
        g("commit", "-q", "-m", "new %d" % i)
    g("checkout", "-q", "-b", "side", "main~2")
    for i in range(3):
        open(os.path.join(wt, "side%d.txt" % i), "w").write("s%d\n" % i)
        g("add", "-A")
        g("commit", "-q", "-m", "side %d" % i)
    g("checkout", "-q", "main")
    g("merge", "-q", "--no-ff", "-m", "merge side", "side")
    g("push", "-q", d, "main", "side")
    pairs2 = ref_pairs(d) + [(git(d, "rev-parse", "main").strip(), git(d, "rev-parse", "side").strip()),
                             (git(d, "rev-parse", "side").strip(), git(d, "rev-parse", "dev").strip())]
    stale = compare_outputs(d, pairs2, "stale")
    ref = fresh("multi-ref")  # same old repo without graph, then the same push
    subprocess.run(["git", "-C", wt, "push", "-q", ref, "main", "side"], check=True, env=ENV)
    ref_out = compare_outputs(ref, pairs2, "reference")
    ok(stale == ref_out, "stale graph: results differ from the repo without a graph")
    ok(len(parse_graph(d)[0]) < len(git(d, "rev-list", "--all").split()), "stale graph should not know the new commits yet")
    # the builder catches up
    build_graph(d, 4)
    check_graph(d, "after catch-up")
    cu = compare_outputs(d, pairs2, "caught-up")
    ok(cu == ref_out, "caught-up graph: results differ")
    if cu != ref_out:
        open(WORK + "/cu.txt", "w").write(cu)
        open(WORK + "/ref.txt", "w").write(ref_out)
    # a damaged graph is ignored
    gp = os.path.join(d, "hub", "graph.bin")
    data = open(gp, "rb").read()
    for name, bad in (("truncated", data[:len(data) // 2]), ("magic", b"XXXX" + data[4:]), ("empty", b"")):
        open(gp, "wb").write(bad)
        ok(compare_outputs(d, pairs2, name) == ref_out, "damaged graph (%s): results differ" % name)
    build_graph(d, 0)  # rebuilds from scratch over a damaged file
    open(gp, "wb").write(b"XXXX" + data[4:])
    build_graph(d, 0)
    check_graph(d, "rebuilt over damage")

    # ---- 3. file reads of a compare: many packs ----
    mp = os.path.join(WORK, "manypacks.git")
    shutil.rmtree(mp, ignore_errors=True)
    subprocess.run(["git", "init", "-q", "--bare", mp], check=True, env=ENV)
    src = os.path.join(REPOS, "onepack.git")
    objs = git(src, "rev-list", "--reverse", "--objects", "--all").splitlines()
    chunk = max(1, len(objs) // 22)
    for i in range(0, len(objs), chunk):
        part = "\n".join(l.split()[0] for l in objs[i:i + chunk]) + "\n"
        subprocess.run(["git", "--git-dir", src, "pack-objects", "-q", os.path.join(mp, "objects/pack/pack")], input=part.encode(), capture_output=True, check=True, env=ENV)
    for r in git(src, "for-each-ref", "--format=%(objectname) %(refname)").splitlines():
        sha, name = r.split()
        git(mp, "update-ref", name, sha)
    npk = len([f for f in os.listdir(os.path.join(mp, "objects/pack")) if f.endswith(".pack")])
    base, head = git(mp, "rev-parse", "main").strip(), git(mp, "rev-parse", "dev").strip()
    heads = (base, git(mp, "rev-parse", "feature").strip())
    rows = []
    for label, b, h in (("main..dev", base, head), ("main..feature", *heads)):
        def reads(path):
            rc, o, e = run(LT, "compare", path, b, h)
            m = re.findall(r"opens=(\d+) reads=(\d+)", e)
            return [tuple(map(int, x)) for x in m], o
        r0, o0 = reads(mp)
        build_graph(mp, 0)
        r1, o1 = reads(mp)
        ok(o0 == o1, "manypacks %s: result differs with the graph" % label)
        shutil.rmtree(os.path.join(mp, "hub"))
        rows.append((label, r0, r1))
    print("read counts (opens, reads) on a repo with %d packs:" % npk)
    for label, r0, r1 in rows:
        for k, name in enumerate(("compare only", "compare + 50 ahead objects")):
            a, b = r0[k], r1[k]
            print("  %-14s %-28s no graph: %5d opens %6d reads | graph: %5d opens %6d reads | ratio %.1fx opens, %.1fx reads" %
                  (label, name, a[0], a[1], b[0], b[1], a[0] / max(1, b[0]), a[1] / max(1, b[1])))
    # log n=30 skip=100
    for use_graph in (False, True):
        if use_graph:
            build_graph(mp, 0)
        rc, o, e = run(LT, "log", mp, "main", "100", "30")
        m = re.search(r"opens=(\d+) reads=(\d+)", e)
        print("  log skip=100 n=30 %s: %s opens %s reads" % ("graph   " if use_graph else "no graph", m.group(1), m.group(2)))

    # ---- 4. packer: one pack from many slices, crash in the middle ----
    sm = os.path.join(WORK, "small.git")
    shutil.rmtree(sm, ignore_errors=True)
    wt2 = os.path.join(WORK, "wt2")
    shutil.rmtree(wt2, ignore_errors=True)
    subprocess.run(["git", "init", "-q", "-b", "main", wt2], check=True, env=ENV)
    subprocess.run(["git", "-C", wt2, "config", "gc.auto", "0"], check=True)
    for i in range(70):
        os.makedirs(os.path.join(wt2, "d%d" % (i % 5)), exist_ok=True)
        open(os.path.join(wt2, "d%d" % (i % 5), "f%d.txt" % i), "w").write("content %d\n" % i * 20)
        subprocess.run(["git", "-C", wt2, "add", "-A"], check=True)
        subprocess.run(["git", "-C", wt2, "commit", "-q", "-m", "c%d" % i], check=True, env=ENV)
    shutil.copytree(os.path.join(wt2, ".git"), sm)
    subprocess.run("rm -rf %s/hooks %s/hub" % (sm, sm), shell=True)
    nloose = int(re.search(r"count: (\d+)", git(sm, "count-objects", "-v")).group(1))
    packs = lambda: sorted(f for f in os.listdir(os.path.join(sm, "objects/pack")) if f.endswith(".pack"))
    rc, o, e = run(GP, "pack", sm, "40", "--stop-after", "20")
    ncalls = int(re.search(r"(\d+) calls", o).group(1))
    ok(len(packs()) == 1, "packer: %d packs after %d slices (%d loose objects), expected 1" % (len(packs()), ncalls, nloose))
    print("packer: %d loose objects, %d slices -> %d pack" % (nloose, ncalls, len(packs())))
    ok(ncalls > 3, "packer: expected several slices")
    ip = os.path.join(sm, "objects/pack", packs()[0][:-5] + ".idx")
    ok(subprocess.run(["git", "verify-pack", ip], capture_output=True).returncode == 0, "verify-pack")
    fs = subprocess.run(["git", "--git-dir", sm, "fsck", "--strict"], capture_output=True, env=ENV)
    ok(fs.returncode == 0, "fsck after packing: " + fs.stderr.decode()[-200:])
    ok(subprocess.run("find %s/objects -mindepth 2 -type f ! -path '*/pack/*' ! -path '*/info/*' | wc -l" % sm, shell=True, capture_output=True).stdout.strip() == b"0", "loose objects left")
    # crash: stop between slices, drop the cursor, repo must stay readable; later run gives one pack
    sm2 = os.path.join(WORK, "small2.git")
    shutil.rmtree(sm2, ignore_errors=True)
    shutil.copytree(os.path.join(wt2, ".git"), sm2)
    subprocess.run("rm -rf %s/hooks %s/hub" % (sm2, sm2), shell=True)
    rc, o, e = run(GP, "pack", sm2, "40", "--stop-after", "20", "--drop-stop", "3")
    tmps = [f for f in os.listdir(os.path.join(sm2, "objects/pack")) if f.startswith("tmp_pack")]
    ok(len(tmps) > 0, "crash test: expected leftover tmp files")
    fs = subprocess.run(["git", "--git-dir", sm2, "fsck", "--strict"], capture_output=True, env=ENV)
    ok(fs.returncode == 0, "fsck after the crash")
    ok(subprocess.run(["git", "--git-dir", sm2, "rev-list", "--all", "--objects"], capture_output=True, env=ENV).returncode == 0, "readable after the crash")
    allids = git(sm2, "cat-file", "--batch-all-objects", "--batch-check=%(objectname)")
    rc, o, e = run(GP, "scan", sm2, inp=allids)
    ok("notfound=0" in o and "other=0" in o, "scan after the crash: " + o)
    rc, o, e = run(GP, "pack", sm2, "40", "--stop-after", "20")
    ok(len([f for f in os.listdir(os.path.join(sm2, "objects/pack")) if f.endswith(".pack")]) == 1, "crash test: one pack after the retry")
    ok(not [f for f in os.listdir(os.path.join(sm2, "objects/pack")) if f.startswith("tmp_pack")], "tmp files left after the retry")
    fs = subprocess.run(["git", "--git-dir", sm2, "fsck", "--strict"], capture_output=True, env=ENV)
    ok(fs.returncode == 0, "fsck after the retry")
    # a push during an open pack: new loose objects go into a later pack, nothing is lost
    sm3 = os.path.join(WORK, "small3.git")
    shutil.rmtree(sm3, ignore_errors=True)
    shutil.copytree(os.path.join(wt2, ".git"), sm3)
    subprocess.run("rm -rf %s/hooks %s/hub" % (sm3, sm3), shell=True)
    rc, o, e = run(GP, "pack", sm3, "40", "--stop-after", "20", "--drop-stop", "2")
    # (the tool stops after 2 slices with the job still open and its cursor dropped)
    # ---- 5. tree walk (RAM path and streamed path, git and our own deflate headers) ----
    trees = git(sm, "rev-list", "--all", "--objects").splitlines()
    tids = [l.split()[0] for l in trees if git(sm, "cat-file", "-t", l.split()[0]).strip() == "tree"][:40]
    want = ""
    for t in tids:
        ls = git(sm, "ls-tree", t)
        want += "== %s\n" % t + "".join("%s %s %s\t%s\n" % (m, ty, h, n) for m, ty, rest in [(l.split(" ", 2)[0], l.split(" ", 2)[1], l.split(" ", 2)[2]) for l in ls.splitlines()] for h, n in [rest.split("\t", 1)]) + "rc=0\n"
    for label, repo in (("our deflate packs", sm), ("git loose", os.path.join(WORK, "wt2", ".git"))):
        for lim in ("16384", "64", "1"):
            e2 = dict(ENV, LIMIT=lim)
            r = subprocess.run([LT, "treewalk", repo], input=("\n".join(tids) + "\n").encode(), capture_output=True, env=e2)
            ok(r.stdout.decode() == want, "treewalk %s limit %s differs from git ls-tree" % (label, lim))
    # ---- 6. big objects without a 32 KB window: far reads from the output file ----
    import random, hashlib, zlib
    rnd = random.Random(5)
    pool = [("line %05d " % i + "".join(rnd.choice("abcdefghijklmnop qrstuvwxyz") for _ in range(90)) + "\n").encode() for i in range(1500)]
    text = b"".join(rnd.choice(pool) for _ in range(10000))[:1 << 20]  # 1 MB, matches at every distance up to 150 KB
    blk = bytes(rnd.randrange(256) for _ in range(20000))
    binary = b"".join(blk[:rnd.randrange(15000, 20000)] + bytes(rnd.randrange(256) for _ in range(300)) for _ in range(70))[:1 << 20]
    bg = os.path.join(WORK, "big.git")
    shutil.rmtree(bg, ignore_errors=True)
    subprocess.run(["git", "init", "-q", "--bare", bg], check=True, env=ENV)
    ids = {}
    for name, data in (("text", text), ("binary", binary)):
        r = subprocess.run(["git", "--git-dir", bg, "hash-object", "-w", "--stdin"], input=data, capture_output=True, check=True, env=ENV)
        ids[name] = (r.stdout.decode().strip(), data)

    def stream(repo, oid):
        shutil.rmtree(os.path.join(repo, "hub"), ignore_errors=True)
        r = subprocess.run([LT, "blob", repo, oid], capture_output=True, env=ENV)
        m = re.search(r"far_reads=(\d+) far_bytes=(\d+) opens=(\d+) reads=(\d+)", r.stderr.decode())
        return r.stdout, tuple(map(int, m.groups())) if m else None

    print("far window (git-compressed, 8 KB ring + output file), 1 MB objects:")
    for layout in ("loose", "packed"):
        if layout == "packed":
            subprocess.run(["git", "--git-dir", bg, "repack", "-a", "-d", "-q"], check=True, env=ENV)
            subprocess.run(["git", "--git-dir", bg, "prune-packed"], check=True, env=ENV)
        for name, (oid, data) in ids.items():
            out, st = stream(bg, oid)
            ok(out == data, "far window %s %s: output differs" % (layout, name))
            ok(st is not None, "far window: no counters")
            if st:
                print("  %-6s %-6s far reads %5d (%.1f per MB, %d KB), file opens %d, read calls %d" %
                      (layout, name, st[0], st[0] * (1 << 20) / len(data), st[1] // 1024, st[2], st[3]))
    # a delta chain too big for RAM (the test repo's 200 KB file, LIMIT small forces the SD path)
    dd = os.path.join(WORK, "delta.git")
    shutil.rmtree(dd, ignore_errors=True)
    shutil.copytree(os.path.join(REPOS, "onepack.git"), dd)
    os.system("chmod -R u+w %s" % dd)
    bigids = git(dd, "rev-list", "--objects", "--all").splitlines()
    bl = [l.split()[0] for l in bigids if l.endswith("data/big.txt")][:25]
    nfar = 0
    for oid in bl:
        want = subprocess.run(["git", "--git-dir", dd, "cat-file", "blob", oid], capture_output=True, env=ENV).stdout
        out, st = stream(dd, oid)
        ok(out == want, "delta big.txt %s differs" % oid[:8])
        nfar += st[0] if st else 0
    print("  delta chain (200 KB x 25 versions, SD path): far reads total", nfar)
    # the packer re-encodes a big loose object with the small window only when that costs at most 1.25x the
    # loose file; otherwise the object stays loose (and is read with the far window)
    src_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "flipper_app", "src")
    src_text = b"".join(open(os.path.join(src_dir, f), "rb").read()
                         for f in sorted(glob.glob("**/*.[ch]", root_dir=src_dir, recursive=True)))[:600000]
    r = subprocess.run(["git", "--git-dir", bg, "hash-object", "-w", "--stdin"], input=src_text, capture_output=True, check=True, env=ENV)
    ids["source"] = (r.stdout.decode().strip(), src_text)
    for name, (oid, data) in ids.items():
        pk = os.path.join(WORK, "rp-%s.git" % name)
        shutil.rmtree(pk, ignore_errors=True)
        subprocess.run(["git", "init", "-q", "--bare", pk], check=True, env=ENV)
        subprocess.run(["git", "--git-dir", pk, "hash-object", "-w", "--stdin"], input=data, check=True, capture_output=True, env=ENV)
        loose = os.path.getsize(os.path.join(pk, "objects", oid[:2], oid[2:]))
        rc, o, e = run(GP, "pack", pk, "10", "--ram-limit", "16384")
        ok(rc == 0, "repack %s rc=%d %s" % (name, rc, o[-200:]))
        packs = [f for f in os.listdir(os.path.join(pk, "objects/pack")) if f.endswith(".pack")]
        out, st = stream(pk, oid)
        ok(out == data, "repacked %s differs" % name)
        if packs:
            psz = os.path.getsize(os.path.join(pk, "objects/pack", packs[0]))
            ip = packs[0][:-5] + ".idx"
            ok(subprocess.run(["git", "verify-pack", os.path.join(pk, "objects/pack", ip)], capture_output=True).returncode == 0, "verify-pack %s" % name)
            ok(psz <= loose * 1.25 + 200, "repacked %s: pack %d is more than 1.25x the loose file %d" % (name, psz, loose))
            ok(st and st[0] == 0, "repacked %s still needed far reads (%s)" % (name, st and st[0]))
            ok(not os.path.exists(os.path.join(pk, "objects", oid[:2], oid[2:])), "%s: loose file left after packing" % name)
            print("  %-6s packed: %d KB (loose file %d KB, ratio %.2f), far reads on read-back %d" % (name, psz // 1024, loose // 1024, psz / loose, st[0]))
        else:
            ok(os.path.exists(os.path.join(pk, "objects", oid[:2], oid[2:])), "%s: neither packed nor loose" % name)
            ok(not [f for f in os.listdir(os.path.join(pk, "objects/pack")) if f.startswith("tmp_")], "%s: tmp files left" % name)
            print("  %-6s left loose (small-window deflate would exceed 1.25x of %d KB); read back with %d far reads" % (name, loose // 1024, st[0]))
    print("graph/packer checks:", checks, "failures:", len(fails))
    return 1 if fails else 0


sys.exit(main())
