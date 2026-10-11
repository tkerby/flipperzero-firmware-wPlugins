#!/usr/bin/env bash
# Host tests for hub_sha1.c, hub_gitwrite.c and hub_gitdocutil.c.
# Usage: pc/tests/run_write_test.sh        (KEEP=1 keeps the scratch repo)
# Needs gcc, git, python3, sha1sum. Scratch goes to /tmp/claude-1000/write_test.
# git is the judge: ids must equal `git hash-object` / `git mktree` /
# `git commit-tree`, and `git fsck --strict` must pass on the repo we wrote.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/write_test.XXXXXX)}"
rm -rf "$W"; mkdir -p "$W"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT

CF="-g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cd "$W"
gcc $CF -c "$APP/hub_sha1.c" -o sha1.o
gcc $CF -c "$APP/hub_gitwrite.c" -o gitwrite.o
gcc $CF -c "$HERE/gitwrite_stdio.c" -o stdio.o
gcc $CF -c "$HERE/gitwrite_test.c" -o test.o
gcc $CF sha1.o gitwrite.o stdio.o test.o -o gw
gcc $CF "$APP/hub_gitdocutil.c" "$HERE/gitdoc_test.c" -o gd

fail=0
bad() { echo "FAIL: $*"; fail=1; }
T=./gw

echo "=== document store helpers ==="
./gd || fail=1
for n in 0 1 5 511 512 513 65536; do
    head -c "$n" /dev/urandom > crc.bin
    want=$(python3 -c 'import zlib,sys;print("%08x"%zlib.crc32(open("crc.bin","rb").read()))')
    got=$(./gd crc crc.bin)
    [[ "$want" == "$got" ]] || bad "crc32 size $n: $got != $want"
done
echo "crc32 vs python zlib: done"

echo "=== SHA-1 vs sha1sum ==="
for n in 0 1 3 54 55 56 57 63 64 65 119 120 127 128 129 1000 4096 65535 65536 100000 1048576; do
    head -c "$n" /dev/urandom > s.bin
    want=$(sha1sum s.bin | cut -d' ' -f1)
    for chunk in 1 7 64 1000 2000000; do
        [[ $n -ge 100000 && $chunk -eq 1 ]] && continue
        got=$($T sha1 s.bin $chunk)
        [[ "$want" == "$got" ]] || bad "sha1 size $n chunk $chunk"
    done
done
printf 'abc' > s.bin
[[ "$($T sha1 s.bin 3)" == "a9993e364706816aba3e25717850c26c9cd0d89d" ]] || bad "sha1 abc"
echo "sha1: done"

echo "=== self checks ==="
git init -q --bare self.git
$T self self.git || bad "self checks"
git -C self.git fsck --strict 2>&1 | grep -v '^Checking' || true

echo "=== blobs ==="
git init -q --bare r.git
# sizes where header + content hits the stored block limits (65535 per block)
sizes=$(python3 - <<'PY'
sizes = {0, 1, 2, 100, 511, 512, 513, 4096, 65535, 65536, 65537, 131070, 131071, 200000, 1048576}
for target in (65535, 65536, 65534, 131070, 131071, 196605):
    for s in range(max(0, target - 20), target):
        if s + len(b"blob %d\0" % s) == target:
            sizes.add(s)
print(*sorted(sizes))
PY
)
for n in $sizes; do
    head -c "$n" /dev/urandom > b.bin
    want=$(git hash-object b.bin)
    for mode in "mem 0" "stream 1" "stream 777" "stream 65536" "stream 70000"; do
        [[ $n -gt 300000 && "$mode" == "stream 1" ]] && continue
        set -- $mode
        out=$($T blob r.git b.bin $1 $2 x 2>&1) || { bad "blob $n $mode: $out"; continue; }
        got=${out%% *}
        [[ "$got" == "$want" ]] || bad "blob $n $mode id $got != $want"
    done
    git -C r.git cat-file blob "$want" | cmp - b.bin || bad "blob $n content"
    # second write is skipped
    out=$($T blob r.git b.bin mem 0 x)
    [[ "$out" == "$want existed" ]] || bad "blob $n not skipped: $out"
done
echo "blobs: done ($(echo $sizes | wc -w) sizes)"
ls r.git/objects | grep -q tmp_obj && bad "temp file left behind"

echo "=== trees ==="
EB=$(printf '' > empty.bin; $T blob r.git empty.bin mem 0 x | cut -d' ' -f1)   # e69de29...
ET=4b825dc642cb6eb9a060e54bf8d69288fbee4904                                      # empty tree
: > emptytree.spec
[[ "$($T tree r.git emptytree.spec)" == "$ET" ]] || bad "empty tree id"
git -C r.git cat-file -e $ET || bad "empty tree not stored"
echo 'x' > one.bin; B1=$($T blob r.git one.bin mem 0 x | cut -d' ' -f1)
# edge: a-b a.b a0 ab, tree "a" ("a/" sorts between a.b and a0), exec, link, gitlink
cat > t1.spec <<SPEC
100644 a0 $B1
40000 a $ET
100644 ab $B1
100644 a.b $B1
100644 a-b $B1
100755 run $B1
120000 ln $B1
160000 sub 1111111111111111111111111111111111111111
100644 Z $B1
100644 _x $B1
040000 a.c $ET
SPEC
got=$($T tree r.git t1.spec)
want=$(awk '{t=($1=="100644"||$1=="100755"||$1=="120000")?"blob":($1=="160000"?"commit":"tree"); m=$1; if(m=="40000"||m=="040000")m="040000"; printf "%s %s %s\t%s\n", m, t, $3, $2}' t1.spec | git -C r.git mktree --missing)
[[ "$got" == "$want" ]] || bad "tree edge ids: $got != $want"
git -C r.git ls-tree "$got" | awk '{print $4}' | tr '\n' ' ' > order.txt
[[ "$(cat order.txt)" == "Z _x a-b a.b a.c a a0 ab ln run sub " ]] || bad "tree order: $(cat order.txt)"
# rejected trees
printf '100644 a %s\n100644 a.b %s\n40000 a %s\n' $B1 $B1 $ET > dup.spec
$T tree r.git dup.spec >/dev/null && bad "duplicate name accepted"
printf '100644 a/b %s\n' $B1 > slash.spec
$T tree r.git slash.spec >/dev/null && bad "slash in name accepted"
# random trees
python3 - "$W" "$B1" "$ET" <<'PY'
import random, sys
w, b1, et = sys.argv[1:4]
random.seed(7)
alpha = "ab.-_0/"[:-1] + "Aß"  # a b . - _ 0 A and a high byte pair
for case in range(40):
    names = set()
    for _ in range(random.randint(1, 40)):
        names.add("".join(random.choice(alpha) for _ in range(random.randint(1, 4))))
    names = [n for n in names if n not in (".", "..") and n.lower() != ".git"]
    random.shuffle(names)
    ents = [(random.choice(["100644", "100755", "40000", "120000"]), n) for n in names]
    with open(f"{w}/rt{case}.spec", "w") as f:
        for m, n in ents:
            f.write(f"{m} {n} {et if m == '40000' else b1}\n")
    with open(f"{w}/rt{case}.mk", "wb") as f:
        for m, n in ents:
            t = "tree" if m == "40000" else "blob"
            mm = "040000" if m == "40000" else m
            f.write(f"{mm} {t} {et if m == '40000' else b1}\t{n}\n".encode("utf-8"))
PY
for i in $(seq 0 39); do
    got=$($T tree r.git rt$i.spec)
    want=$(git -C r.git mktree < rt$i.mk)
    [[ "$got" == "$want" ]] || bad "random tree $i: $got != $want"
done
echo "trees: done"

echo "=== commits ==="
printf 'first\n' > m1.txt; printf 'no newline at end' > m2.txt
printf 'subject\n\nbody line 1\nbody line 2\n\n' > m3.txt; : > m4.txt
T0=1700000000
mk() { # tree time tz msgfile [parents...]
    local tree=$1 t=$2 tz=$3 msg=$4; shift 4
    local args=() p
    for p in "$@"; do args+=(-p "$p"); done
    GIT_AUTHOR_NAME="A U Thor" GIT_AUTHOR_EMAIL=author@example.com GIT_AUTHOR_DATE="$t $tz" \
    GIT_COMMITTER_NAME="C O Mitter" GIT_COMMITTER_EMAIL=committer@example.com GIT_COMMITTER_DATE="$((t+5)) $tz" \
        git -C r.git commit-tree "$tree" "${args[@]}" < "$msg"
}
# commit-tree adds no newline of its own: for the one without a final newline
# we compare against a message that has it (our builder adds it, like git commit)
printf 'no newline at end\n' > m2g.txt
c1w=$($T commit r.git $ET $T0 +0000 m1.txt);                 c1g=$(mk $ET $T0 +0000 m1.txt)
[[ "$c1w" == "$c1g" ]] || bad "root commit $c1w != $c1g"
c2w=$($T commit r.git $ET $((T0+60)) -0530 m2.txt $c1w);     c2g=$(mk $ET $((T0+60)) -0530 m2g.txt $c1g)
[[ "$c2w" == "$c2g" ]] || bad "child commit $c2w != $c2g"
c3w=$($T commit r.git $ET $((T0+120)) +0900 m3.txt $c1w $c2w); c3g=$(mk $ET $((T0+120)) +0900 m3.txt $c1g $c2g)
[[ "$c3w" == "$c3g" ]] || bad "merge commit $c3w != $c3g"
c4w=$($T commit r.git $ET $((T0+180)) +0000 m4.txt $c3w);    c4g=$(mk $ET $((T0+180)) +0000 m4.txt $c3g)
[[ "$c4w" == "$c4g" ]] || bad "empty message commit $c4w != $c4g"
# a real tree with content as the commit tree
printf '100644 hello.txt %s\n' "$B1" > ct.spec
TREE=$($T tree r.git ct.spec)
c5w=$($T commit r.git $TREE $((T0+240)) +0000 m1.txt $c4w)
git -C r.git symbolic-ref HEAD refs/heads/main
git -C r.git update-ref refs/heads/main "$c5w"
[[ "$(git -C r.git cat-file -p $c5w | head -1)" == "tree $TREE" ]] || bad "commit tree line"
[[ "$(git -C r.git log --format=%an,%cn,%ae --first-parent -1 main)" == "A U Thor,C O Mitter,author@example.com" ]] || bad "git log idents"
[[ "$(git -C r.git rev-list --count main)" == 5 ]] || bad "rev-list count"
[[ "$(git -C r.git show main:hello.txt)" == "x" ]] || bad "file via tree"
echo "commits: done"

echo "=== git fsck --strict ==="
if out=$(git -C r.git fsck --strict 2>&1); then echo "fsck ok"; else echo "$out"; bad "fsck r.git"; fi
echo "objects: $(git -C r.git count-objects | tr '\n' ' ')"
if out=$(git clone -q r.git clone 2>&1 && git -C clone log --oneline | wc -l); then echo "clone ok, $out commits"; else echo "$out"; bad clone; fi

echo
if [[ $fail = 0 ]]; then echo "ALL TESTS PASSED"; else echo "TESTS FAILED"; fi
exit $fail
