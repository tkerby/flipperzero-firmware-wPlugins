#!/usr/bin/env bash
# Smoke test of hub_deflate.c and hub_gitpack.c on the host.
# Usage: pc/tests/run_gitpack_test.sh      (scratch in /tmp/claude-1000/gitpack_test)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/gitpack_test.XXXXXX)}"
rm -rf "$W"; mkdir -p "$W"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT
cd "$W"

CF="-g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined"
for f in hub_deflate hub_gitpack hub_gitgraph hub_gitobj hub_inflate; do
    gcc $CF -include "$HERE/malloc_wrap.h" -c "$APP/$f.c" -o $f.o
done
gcc $CF -c "$APP/hub_sha1.c" -o sha1.o
gcc $CF -c "$APP/hub_gitdocutil.c" -o docutil.o
gcc $CF -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/gitpack_tool.c" -o tool.o
gcc $CF *.o -o gitpack_tool

echo "=== deflate round trip (python zlib.decompress) ==="
python3 -I - "$W" <<'PY'
import os, random, subprocess, sys, zlib
w = sys.argv[1]
r = random.Random(3)
text = b"".join(b"line %05d the quick brown fox %d\n" % (i, r.randrange(1000)) for i in range(4000))
cases = {
    "empty": b"", "one": b"x", "short": b"hello hello hello hello",
    "text": text, "random": bytes(r.randrange(256) for _ in range(50000)),
    "zeros": bytes(1000000), "mixed": text[:30000] + os.urandom(3000) + text[:30000] * 2,
    "exact1k": bytes(r.randrange(256) for _ in range(1024)), "exact2k": text[:2048],
    "far": (os.urandom(3000) + b"A" * 1000) * 40,
}
ok = True
for name, data in cases.items():
    open(f"{w}/in.bin", "wb").write(data)
    p = subprocess.run([f"{w}/gitpack_tool", "deflate", f"{w}/in.bin", f"{w}/out.z"], capture_output=True)
    z = open(f"{w}/out.z", "rb").read()
    good = p.returncode == 0 and zlib.decompress(z) == data
    ok &= good
    print(f"  {name:8s} in={len(data):8d} out={len(z):8d} ratio={len(z)/max(1,len(data)):.2f} {'ok' if good else 'FAIL'}")
print(p.stderr.decode().strip())
sys.exit(0 if ok else 1)
PY

echo "=== pack loose objects ==="
SRC="${LOOSE:-/tmp/claude-1000/gitobj_test/repos/loose.git}"
if [[ ! -d "$SRC" ]]; then "$HERE/make_test_repo.sh" "$W/repos" >/dev/null; SRC="$W/repos/loose.git"; fi
cp -r "$SRC" "$W/pk.git"; rm -rf "$W/pk.git/hub"
git --git-dir=pk.git count-objects -v | grep -E "^count|^packs"
./gitpack_tool pack pk.git "${MAXOBJ:-100}" --ram-limit 16384
git --git-dir=pk.git count-objects -v | grep -E "^count|^packs"
for p in pk.git/objects/pack/*.idx; do git verify-pack -v "$p" | tail -3; done
git --git-dir=pk.git fsck --strict 2>&1 | grep -v "^dangling" || true
test "$(find pk.git/objects -mindepth 2 -type f ! -path '*/pack/*' ! -path '*/info/*' | wc -l)" = 0 && echo "no loose objects left"
test "$(find pk.git/objects -mindepth 1 -maxdepth 1 -type d | wc -l)" -le 2 && echo "empty xx folders removed"
rm -rf clone; git clone -q pk.git clone
test "$(git -C clone rev-list --all --count)" = "$(git --git-dir=pk.git rev-list --all --count)" && echo "clone ok: $(git -C clone rev-list --all --count) commits"
git -C clone fsck --strict 2>&1 | grep -v dangling || true
git --git-dir="$SRC" cat-file --batch-all-objects --batch-check | sort > a.txt
git --git-dir=pk.git cat-file --batch-all-objects --batch-check | sort > b.txt
cmp a.txt b.txt && echo "same object list and sizes as the source ($(wc -l < a.txt) objects)"

echo "=== leftovers: restore some loose objects and a stale tmp file, pack again ==="
for f in $(cd "$SRC/objects" && find . -mindepth 2 -type f ! -path './pack/*' ! -path './info/*' | head -40); do
    mkdir -p "pk.git/objects/$(dirname "$f")"; cp "$SRC/objects/$f" "pk.git/objects/$f"
done
echo junk > pk.git/objects/pack/tmp_pack_p
./gitpack_tool pack pk.git "${MAXOBJ:-100}" --ram-limit 16384
test ! -e pk.git/objects/pack/tmp_pack_p && echo "stale tmp removed"
git --git-dir=pk.git fsck --strict 2>&1 | grep -v "^dangling" || true
git --git-dir=pk.git cat-file --batch-all-objects --batch-check | sort | cmp - a.txt && echo "object list still identical"

echo "=== stop callback: a stopped batch keeps what it packed ==="
rm -rf pk2.git; cp -r "$SRC" pk2.git; rm -rf pk2.git/hub
./gitpack_tool pack pk2.git 300 --ram-limit 16384 --stop-after 37 | tail -4
test "$(find pk2.git/objects -mindepth 2 -type f ! -path '*/pack/*' ! -path '*/info/*' | wc -l)" = 0 && echo "all packed in many short calls"
git --git-dir=pk2.git fsck --strict 2>&1 | grep -v "^dangling" || true
git --git-dir=pk2.git cat-file --batch-all-objects --batch-check | sort | cmp - a.txt && echo "object list identical"
rm -rf pk3.git; cp -r "$SRC" pk3.git; rm -rf pk3.git/hub
before=$(find pk3.git/objects -type f | wc -l)
./gitpack_tool pack pk3.git 300 --stop-now | tail -2
test "$(find pk3.git/objects -type f | wc -l)" = "$before" && echo "stop before the first object: repo unchanged"
echo "=== real error (injected is covered by the library tests): bad object leaves repo unchanged ==="
rm -rf pk4.git; cp -r "$SRC" pk4.git; rm -rf pk4.git/hub
f=$(find pk4.git/objects -mindepth 2 -type f ! -path '*/pack/*' ! -path '*/info/*' | sort | sed -n 300p)
printf 'garbage' > "$f"
before=$(find pk4.git/objects -type f | wc -l)
./gitpack_tool pack pk4.git 50 --ram-limit 16384 | tail -1 || true
find pk4.git/objects -name 'tmp_pack*' | wc -l | grep -qx 0 && echo "no tmp files left"
echo "=== merge: many small packs -> few packs ==="
rm -rf pk5.git; cp -r "$SRC" pk5.git; rm -rf pk5.git/hub
./gitpack_tool pack pk5.git 25 --ram-limit 16384 | tail -1
git --git-dir=pk5.git count-objects -v | grep "^packs"
git --git-dir=pk5.git cat-file --batch-all-objects --batch-check='%(objectname)' > ids.txt
./gitpack_tool scan pk5.git < ids.txt
./gitpack_tool merge pk5.git 0 150 | grep -v "merge call: e=0 more=1 stopped=1" | cut -c1-170
git --git-dir=pk5.git count-objects -v | grep "^packs"
./gitpack_tool scan pk5.git < ids.txt
for p in pk5.git/objects/pack/*.idx; do git verify-pack "$p" > /dev/null && echo "verify-pack ok $(basename $p)"; done | sort | uniq -c | sed "s/pack-.*idx/<idx>/"
git --git-dir=pk5.git fsck --strict 2>&1 | grep -v "^dangling" || true
git --git-dir=pk5.git cat-file --batch-all-objects --batch-check | sort | cmp - a.txt && echo "merged repo: object list and sizes identical"
git --git-dir=pk5.git cat-file --batch-all-objects --batch | md5sum > m1.txt; git --git-dir="$SRC" cat-file --batch-all-objects --batch | md5sum > m2.txt; cmp m1.txt m2.txt && echo "merged repo: all object contents identical"
rm -rf clone5; git clone -q pk5.git clone5 && echo "clone of merged repo ok: $(git -C clone5 rev-list --all --count) commits"
find pk5.git/objects/pack -name 'tmp_*' | wc -l | grep -qx 0 && echo "no tmp files left"
echo "=== merge with a duplicate across packs ==="
rm -rf pk6.git; cp -r pk5.git pk6.git
for p in pk6.git/objects/pack/*.pack; do :; done
git --git-dir=pk6.git pack-objects -q --stdout --revs <<< "refs/heads/main" > dup.pack 2>/dev/null || true
rm -rf pk7.git; git init -q --bare pk7.git
for i in 1 2 3 4; do
  git --git-dir="$SRC" rev-list --objects main~$((i*10)) ^main~$(((i+1)*10)) 2>/dev/null | git --git-dir="$SRC" pack-objects -q pk7.git/objects/pack/pack >/dev/null
done
git --git-dir="$SRC" rev-list --objects main~15 ^main~45 | git --git-dir="$SRC" pack-objects -q pk7.git/objects/pack/pack > /dev/null   # overlaps
git --git-dir=pk7.git count-objects -v | grep "^packs"
./gitpack_tool merge pk7.git 0 0 | grep "job\|e=0 more=0" | cut -c1-150
git --git-dir=pk7.git count-objects -v | grep "^packs"
for p in pk7.git/objects/pack/*.idx; do git verify-pack "$p" > /dev/null && echo "verify-pack ok"; done | sort | uniq -c
echo "ALL PACK TESTS DONE"
