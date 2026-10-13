#!/usr/bin/env bash
# Build a test repository and five bare layouts of it for gitobj_test.
# Usage: pc/tests/make_test_repo.sh <outdir>
# Result in <outdir>:
#   src/            work tree (non-bare, loose objects)
#   loose.git       all loose objects
#   onepack.git     one pack: git repack -a -d -f --depth=50 (OFS_DELTA), packed refs
#   multi.git       three packs + loose objects, packed refs + overriding loose refs
#   refdelta.git    one pack made with git pack-objects without --delta-base-offset
#   bigoff.git      the onepack idx rewritten so every offset is in the 8-byte table
# Content: 200+ commits, nested folders, README.md, binary files, a 200 KB text
# file edited in every commit (stored as deltas), annotated + nested + light
# tags, branches, merges, an octopus merge, a symlink, a gitlink, an empty file.
set -euo pipefail

OUT="${1:?usage: make_test_repo.sh <outdir>}"
rm -rf "$OUT"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null GIT_TERMINAL_PROMPT=0
export GIT_AUTHOR_NAME="Zoë Müller" GIT_AUTHOR_EMAIL="zoe@example.com"
export GIT_COMMITTER_NAME="Cy Committer" GIT_COMMITTER_EMAIL="cy@example.org"

SRC="$OUT/src"
git init -q -b main "$SRC"
cd "$SRC"
git config gc.auto 0
git config core.quotepath false
git config commit.gpgsign false
git config tag.gpgsign false

TZS=(+0200 -0530 +0000 +0545 -0800 +0900)
N=0
tick() { # set deterministic dates
    N=$((N + 1))
    local tz="${TZS[$((N % ${#TZS[@]}))]}"
    export GIT_AUTHOR_DATE="$((1600000000 + N * 3600)) $tz"
    export GIT_COMMITTER_DATE="$((1600000000 + N * 3600 + 17)) ${TZS[$(((N + 2) % ${#TZS[@]}))]}"
}
commit() { tick; git add -A >/dev/null; git commit -q -m "$1"; }

python3 -I - <<'PY'
import random, os
r = random.Random(7)
os.makedirs("src/core/util", exist_ok=True)
os.makedirs("src/ui/deep/er/est", exist_ok=True)
os.makedirs("docs", exist_ok=True)
os.makedirs("data", exist_ok=True)
os.makedirs("bin", exist_ok=True)
open("README.md", "w").write("# Test repo\n\nHello from the Gitphin test repo.\n\n- one\n- two\n")
open("src/core/a.c", "w").write("int a(void) { return 1; }\n")
open("src/core/util/b.c", "w").write("int b(void) { return 2; }\n")
open("src/ui/deep/er/est/f.txt", "w").write("deepest file\n")
open("docs/guide.md", "w").write("# Guide\n\nText.\n")
open("docs/ünï.txt", "w").write("unicode name\n")
open("empty.txt", "w").write("")
lines = ["line %05d: the quick brown fox jumps over the lazy dog %d" % (i, r.randrange(10**6)) for i in range(4000)]
open("data/big.txt", "w").write("\n".join(lines) + "\n")
open("bin/blob.bin", "wb").write(bytes(r.randrange(256) for _ in range(4096)))
open("bin/big.bin", "wb").write(bytes(r.randrange(256) for _ in range(150000)))
PY
ln -s README.md latest-readme
commit "initial import"
# gitlink (submodule entry without a .gitmodules file)
git update-index --add --cacheinfo 160000,1111111111111111111111111111111111111111,vendor/sub
tick; git commit -q -m "add gitlink"

edit() { # edit <i>: change the big file and a small file
    python3 -I - "$1" <<'PY'
import sys, random
i = int(sys.argv[1])
r = random.Random(i)
p = "data/big.txt"
L = open(p).read().split("\n")
for k in range(6):
    j = r.randrange(len(L) - 1)
    L[j] = "line %05d: edited in commit %d slot %d %d" % (j, i, k, r.randrange(10**6))
if i % 7 == 0:
    L.insert(r.randrange(len(L) - 1), "inserted at commit %d" % i)
if i % 11 == 0:
    del L[r.randrange(len(L) - 1)]
open(p, "w").write("\n".join(L))
open("src/core/a.c", "a").write("// change %d\n" % i)
if i % 5 == 0:
    open("README.md", "a").write("- item %d\n" % i)
if i % 13 == 0:
    open("src/ui/deep/er/est/f.txt", "a").write("more %d\n" % i)
if i % 17 == 0:
    d = bytearray(open("bin/blob.bin", "rb").read())
    for k in range(20):
        d[r.randrange(len(d))] = r.randrange(256)
    open("bin/blob.bin", "wb").write(bytes(d))
if i % 3 == 0:
    open("docs/note-%d.md" % (i % 30), "a").write("note %d\n" % i)
PY
}

for i in $(seq 1 40); do edit "$i"; commit "main commit $i"; done
git tag -a v1.0 -m "release one" 
git tag -a nested -m "tag of a tag" v1.0 2>/dev/null || true
git tag light
git branch dev
git checkout -q dev
for i in 1 2 3; do echo "dev $i" >> dev.txt; commit "dev commit $i"; done
git checkout -q main
for i in $(seq 41 100); do edit "$i"; commit "main commit $i"; done
git checkout -q -b feature
mkdir -p feature
for i in $(seq 1 10); do echo "feature $i" >> "feature/f$i.txt"; commit "feature commit $i"; done
git checkout -q main
for i in $(seq 101 150); do edit "$i"; commit "main commit $i"; done
tick; git merge -q --no-ff -m "merge feature" feature
for i in $(seq 151 170); do edit "$i"; commit "main commit $i"; done
# octopus merge with disjoint files
for b in 1 2 3 4 5; do
    git checkout -q -b "oct$b" main
    echo "octopus $b" > "oct$b.txt"; commit "oct$b"
done
git checkout -q main
tick; git merge -q --no-edit oct1 oct2 oct3 oct4 oct5 >/dev/null
git branch -q -D oct1 oct2 oct3 oct4 oct5 2>/dev/null || true
for i in $(seq 171 215); do edit "$i"; commit "main commit $i with a longer message
 
Body line one.
Body line two with ünïcode."; done
git tag -a v2.0 -m "release two

with a body"
git branch -q 'topic/with/slash' HEAD~3

echo "commits: $(git rev-list --all --count)"

# ---- layouts ----
mk() { git clone -q --bare --no-hardlinks --local "$SRC" "$OUT/$1.git"; }

mk loose
test "$(find "$OUT/loose.git/objects/pack" -name '*.pack' | wc -l)" = 0

mk onepack
git -C "$OUT/onepack.git" repack -q -a -d -f --depth=50
git -C "$OUT/onepack.git" pack-refs --all --prune

mk refdelta
cd "$OUT/refdelta.git"
git rev-list --objects --all | git pack-objects -q --window=50 --depth=50 objects/pack/pack >/dev/null
git prune-packed
rm -f objects/pack/*.bitmap

mk multi
cd "$OUT/multi.git"
c1=$(git rev-list main | sed -n '120p'); c2=$(git rev-list main | sed -n '60p'); c3=$(git rev-list main | sed -n '15p')
git rev-list --objects "$c1" | git pack-objects -q --delta-base-offset objects/pack/pack >/dev/null
git rev-list --objects "$c2" "^$c1" | git pack-objects -q --delta-base-offset objects/pack/pack >/dev/null
git rev-list --objects "$c3" "^$c2" | git pack-objects -q --window=20 objects/pack/pack >/dev/null
git prune-packed
git pack-refs --all --prune
git update-ref refs/heads/dev "$c2"            # loose ref overrides packed-refs
git update-ref refs/heads/loose-only "$c3"
test "$(ls objects/pack/*.pack | wc -l)" = 3
echo "multi loose objects: $(git count-objects | cut -d' ' -f1)"

cp -r "$OUT/onepack.git" "$OUT/bigoff.git"
python3 -I - "$OUT/bigoff.git" <<'PY'
import sys, glob, struct, hashlib, os
for f in glob.glob(sys.argv[1] + "/objects/pack/*.idx"):
    d = open(f, "rb").read()
    assert d[:8] == b"\xfftOc\x00\x00\x00\x02"
    n = struct.unpack(">I", d[8 + 255 * 4:8 + 256 * 4])[0]
    p = 8 + 1024
    names = d[p:p + 20 * n]; p += 20 * n
    crc = d[p:p + 4 * n]; p += 4 * n
    offs = struct.unpack(">%dI" % n, d[p:p + 4 * n]); p += 4 * n
    assert all(not (o & 0x80000000) for o in offs)
    packsha = d[len(d) - 40:len(d) - 20]
    new = d[:8 + 1024] + names + crc
    # git wants at least one normal entry: keep the smallest offset in the 4-byte table
    small = min(range(n), key=lambda i: offs[i])
    big = [i for i in range(n) if i != small]
    slot = {i: k for k, i in enumerate(big)}
    new += struct.pack(">%dI" % n, *[offs[i] if i == small else 0x80000000 | slot[i] for i in range(n)])
    new += struct.pack(">%dQ" % len(big), *[offs[i] for i in big])
    new += packsha
    new += hashlib.sha1(new).digest()
    os.chmod(f, 0o644)
    open(f, "wb").write(new)
PY

# ---- report entry types in the packs ----
for l in onepack refdelta multi bigoff; do
python3 -I - "$OUT/$l.git" <<'PY'
import sys, glob, subprocess, collections
c = collections.Counter()
for idx in glob.glob(sys.argv[1] + "/objects/pack/*.idx"):
    pack = idx[:-4] + ".pack"
    out = subprocess.run(["git", "--git-dir", sys.argv[1], "verify-pack", "-v", pack],
                         capture_output=True, text=True).stdout
    data = open(pack, "rb").read()
    for line in out.splitlines():
        t = line.split()
        if len(t) >= 5 and len(t[0]) == 40 and t[1] in ("commit", "tree", "blob", "tag"):
            c[(data[int(t[4])] >> 4) & 7] += 1
print("%s: plain=%d ofs_delta=%d ref_delta=%d" % (sys.argv[1].rsplit("/", 1)[-1],
      c[1] + c[2] + c[3] + c[4], c[6], c[7]))
PY
done
echo "done: $OUT"
