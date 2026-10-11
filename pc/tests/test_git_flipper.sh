#!/usr/bin/env bash
# End-to-end test of pc/git-remote-flipper against pc/tests/mock_hub.py.
# Usage: pc/tests/test_git_flipper.sh        (starts and stops the mock on 127.0.0.1:8099)
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
PC="$(dirname "$HERE")"
PORT=8099
BASE=/tmp/claude-1000
mkdir -p "$BASE"
W="$(mktemp -d "$BASE/flipper-git-test.XXXXXX")"
SRV="$W/ext"            # the mock's "SD card"
REPO="$SRV/git"
MOCK_PID=""

export PATH="$PC:$PATH"
export GITPHIN_URL="http://127.0.0.1:$PORT"
export W REPO
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null GIT_TERMINAL_PROMPT=0
export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@example.com GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@example.com

cleanup() {
    [[ -n "$MOCK_PID" ]] && kill "$MOCK_PID" 2>/dev/null
    rm -rf "$W"
}
trap cleanup EXIT

PASSED=(); FAILED=()
# check <name> <command...>: pass when the command exits 0
check() {
    local name="$1"; shift
    if "$@" >"$W/last.log" 2>&1; then PASSED+=("$name"); else FAILED+=("$name"); echo "--- FAIL: $name"; tail -n 15 "$W/last.log"; fi
}
# refuse <name> <command...>: pass when the command exits non-zero
refuse() {
    local name="$1"; shift
    if "$@" >"$W/last.log" 2>&1; then FAILED+=("$name"); echo "--- FAIL (should be refused): $name"; tail -n 15 "$W/last.log"; else PASSED+=("$name"); fi
}
npacks() { find "$REPO/$1.git/objects/pack" -name '*.pack' | wc -l; }
remote_ref() { git -C "$REPO/$1.git" rev-parse --verify -q "$2" 2>/dev/null || git -C "$REPO/$1.git" show-ref -s "$2" 2>/dev/null; }
same() { [[ "$1" == "$2" && -n "$1" ]]; }
commit() { echo "$2" >>"$1/f.txt"; git -C "$1" add f.txt; git -C "$1" commit -q -m "$2"; }

python3 "$HERE/mock_hub.py" "$SRV" --port $PORT 2>"$W/mock.log" &
MOCK_PID=$!
for _ in $(seq 50); do curl -s "$GITPHIN_URL/api/git/repos" >/dev/null && break; sleep 0.1; done
mkdir -p "$SRV"/{git,ext}

# ---- push to create
git init -q -b main "$W/a"; commit "$W/a" one
git -C "$W/a" remote add origin flipper://demo
check "push-to-create" git -C "$W/a" push -q origin main
check "created: one pack" test "$(npacks demo)" = 1
check "created: ref on server" same "$(remote_ref demo refs/heads/main)" "$(git -C "$W/a" rev-parse main)"
check "created: HEAD is main" grep -qx 'ref: refs/heads/main' "$REPO/demo.git/HEAD"
check "created: objects/info/packs lists the pack" grep -q '^P pack-' "$REPO/demo.git/objects/info/packs"
check "created: info/refs lists main" grep -q 'refs/heads/main$' "$REPO/demo.git/info/refs"

# ---- clones
check "clone flipper://" git clone -q flipper://demo "$W/b"
check "clone flipper:// content" same "$(cat "$W/b/f.txt")" "$(cat "$W/a/f.txt")"
check "clone dumb http" git clone -q "$GITPHIN_URL/git/demo.git" "$W/c"
check "clone dumb http content" same "$(git -C "$W/c" rev-parse HEAD)" "$(git -C "$W/a" rev-parse HEAD)"
check "ls-remote shows HEAD" git ls-remote flipper://demo HEAD

# ---- fast-forward push and fetch
commit "$W/a" two
check "second push (fast-forward)" git -C "$W/a" push -q origin main
check "second push: two packs" test "$(npacks demo)" = 2
check "fetch in clone" git -C "$W/b" fetch -q origin
check "fetch got the commit" same "$(git -C "$W/b" rev-parse origin/main)" "$(git -C "$W/a" rev-parse main)"
check "pull in clone" git -C "$W/b" merge -q --ff-only origin/main
check "fetch again is a no-op" git -C "$W/b" fetch -q origin

# ---- non-fast-forward
git -C "$W/a" commit -q --amend -m "two amended"
refuse "non-fast-forward rejected by git" git -C "$W/a" push -q origin main
OLD="$(remote_ref demo refs/heads/main)"
check "rejected push left the ref alone" test "$OLD" != "$(git -C "$W/a" rev-parse main)"
# helper's own check (git would not send this without +)
P="$(printf 'list for-push\npush refs/heads/main:refs/heads/main\n\n' | (cd "$W/a" && git-remote-flipper origin flipper://demo 2>/dev/null))"
check "helper itself answers non-fast-forward" grep -q '^error refs/heads/main non-fast-forward' <<<"$P"
check "force push accepted" git -C "$W/a" push -q --force origin main
check "force push moved the ref" same "$(remote_ref demo refs/heads/main)" "$(git -C "$W/a" rev-parse main)"

# fetch first: remote has a commit the pusher lacks
git clone -q flipper://demo "$W/d"; commit "$W/d" three; git -C "$W/d" push -q origin main
commit "$W/a" four
refuse "behind remote: push rejected" git -C "$W/a" push -q origin main
P="$(printf 'list for-push\npush refs/heads/main:refs/heads/main\n\n' | (cd "$W/a" && git-remote-flipper origin flipper://demo 2>/dev/null))"
check "helper answers fetch first" grep -q '^error refs/heads/main fetch first' <<<"$P"
check "force over unknown commit" git -C "$W/a" push -q --force origin main

# stale info: the ref moves between list and push
S1="$(git -C "$W/a" rev-parse main)"
P="$( { printf 'list for-push\n'; sleep 0.7
        curl -s -X POST "$GITPHIN_URL/api/git/ref?name=demo&ref=refs/heads/main&old=$S1&new=$(git -C "$W/a" rev-parse main~1)" >/dev/null
        commit "$W/a" five
        printf 'push refs/heads/main:refs/heads/main\n\n'; } | (cd "$W/a" && git-remote-flipper origin flipper://demo 2>/dev/null))"
check "stale info on CAS conflict" grep -q '^error refs/heads/main \(stale info\|non-fast-forward\)' <<<"$P"
git -C "$W/a" push -q --force origin main

# ---- tags
git -C "$W/a" tag v1
check "tag push" git -C "$W/a" push -q origin v1
check "tag on server" same "$(remote_ref demo refs/tags/v1)" "$(git -C "$W/a" rev-parse v1)"
git -C "$W/a" tag -f v1 main~1 >/dev/null
refuse "tag overwrite rejected" git -C "$W/a" push -q origin v1
check "tag overwrite left the tag" same "$(remote_ref demo refs/tags/v1)" "$(git -C "$W/a" rev-parse main)"
P="$(printf 'list for-push\npush refs/tags/v1:refs/tags/v1\n\n' | (cd "$W/a" && git-remote-flipper origin flipper://demo 2>/dev/null))"
check "helper answers already exists" grep -q '^error refs/tags/v1 already exists' <<<"$P"
check "tag overwrite with force" git -C "$W/a" push -q origin +refs/tags/v1
check "tag moved" same "$(remote_ref demo refs/tags/v1)" "$(git -C "$W/a" rev-parse v1)"
git -C "$W/a" tag -a -m annotated v2
check "annotated tag push" git -C "$W/a" push -q origin v2
check "annotated tag fetched" git -C "$W/b" fetch -q origin --tags

# ---- nothing new: no empty pack
BEFORE="$(npacks demo)"
git -C "$W/a" branch same-tip
check "push of an already-known commit" git -C "$W/a" push -q origin same-tip
check "no empty pack uploaded (branch)" test "$(npacks demo)" = "$BEFORE"
git -C "$W/a" tag v3
check "push of a tag on a known commit" git -C "$W/a" push -q origin v3
check "no empty pack uploaded (tag)" test "$(npacks demo)" = "$BEFORE"
check "no empty pack file" bash -c "! find \"\$REPO/demo.git/objects/pack\" -name '*.pack' -size -33c | grep ."

# ---- branch delete
check "branch delete" git -C "$W/a" push -q origin :same-tip
check "branch gone on server" test -z "$(remote_ref demo refs/heads/same-tip)"
check "branch gone from info/refs" bash -c "! grep -q same-tip '$REPO/demo.git/info/refs'"
check "branch delete via --delete" bash -c "git -C '$W/a' push -q origin feature-x 2>/dev/null || { git -C '$W/a' branch feature-x && git -C '$W/a' push -q origin feature-x; }; git -C '$W/a' push -q --delete origin feature-x"
refuse "delete of a missing branch fails" git -C "$W/a" push -q origin :nope

# ---- HEAD fix when only master is pushed
git init -q -b master "$W/m"; commit "$W/m" m1
git -C "$W/m" remote add origin flipper://masterrepo
check "push only master" git -C "$W/m" push -q origin master
check "HEAD fixed to master" grep -qx 'ref: refs/heads/master' "$REPO/masterrepo.git/HEAD"
check "clone gets a checkout" bash -c "git clone -q flipper://masterrepo '$W/m2' && test -f '$W/m2/f.txt'"

# ---- git flipper CLI
check "cli list" bash -c "git flipper list | grep -q '^demo '"
check "cli create" git flipper create tmp1 -d "first text" -b trunk
check "cli create: HEAD" grep -qx 'ref: refs/heads/trunk' "$REPO/tmp1.git/HEAD"
refuse "cli create duplicate fails" git flipper create tmp1
refuse "cli create bad name fails" git flipper create "bad name"
check "cli describe" git flipper describe tmp1 "new text here"
check "cli describe: file" grep -qx 'new text here' "$REPO/tmp1.git/description"
check "cli list shows description" bash -c "git flipper list | grep -q 'tmp1 .*trunk.*new text here'"
check "cli rename" git flipper rename tmp1 tmp2
check "cli rename: moved" test -d "$REPO/tmp2.git" -a ! -d "$REPO/tmp1.git"
check "cli url" bash -c "[[ \"\$(git flipper url tmp2)\" == \$'flipper://tmp2\n$GITPHIN_URL/git/tmp2.git' ]]"
refuse "cli delete needs --yes" git flipper delete tmp2
check "cli delete" git flipper delete tmp2 --yes
check "cli delete: gone" test ! -d "$REPO/tmp2.git"
refuse "cli unknown command fails" git flipper bogus

# ---- gc
check "before gc: several packs" test "$(npacks demo)" -gt 1
check "gc" git flipper gc demo
check "after gc: exactly one pack" test "$(npacks demo)" = 1
check "after gc: one idx" test "$(find "$REPO/demo.git/objects/pack" -name '*.idx' | wc -l)" = 1
check "after gc: objects/info/packs has one entry" test "$(grep -c '^P ' "$REPO/demo.git/objects/info/packs")" = 1
check "after gc: refs intact" same "$(remote_ref demo refs/heads/main)" "$(git -C "$W/a" rev-parse main)"
check "after gc: fsck of the server repo" git --git-dir="$REPO/demo.git" fsck --strict
check "after gc: clone flipper://" git clone -q flipper://demo "$W/e"
check "after gc: clone dumb http" git clone -q "$GITPHIN_URL/git/demo.git" "$W/f"
check "after gc: clone content" same "$(git -C "$W/e" rev-parse origin/main)" "$(git -C "$W/a" rev-parse main)"
check "gc again is stable" git flipper gc demo
check "gc again: one pack" test "$(npacks demo)" = 1
commit "$W/a" six
check "push after gc" git -C "$W/a" push -q origin main

echo
echo "passed: ${#PASSED[@]}"
printf '  ok   %s\n' "${PASSED[@]}"
echo "failed: ${#FAILED[@]}"
[[ ${#FAILED[@]} -gt 0 ]] && printf '  FAIL %s\n' "${FAILED[@]}"
[[ ${#FAILED[@]} -eq 0 ]]
