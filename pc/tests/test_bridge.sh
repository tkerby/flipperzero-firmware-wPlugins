#!/usr/bin/env bash
# End-to-end test of the bridge (pc/bridge.py --upstream) against pc/tests/mock_hub.py:
# ZIP download vs git archive, webhooks, backup on plug-in, mirror-add.
# Usage: pc/tests/test_bridge.sh        (mock on 127.0.0.1:8101, bridge on 8102, sinks on 8103/8104)
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
PC="$(dirname "$HERE")"
MOCK_PORT=8101; BR_PORT=8102; SINK_A=8103; SINK_B=8104
BASE="${TMPDIR:-/tmp}"
W="$(mktemp -d "$BASE/gitphin-bridge-test.XXXXXX")"
SRV="$W/ext"; REPO="$SRV/apps_data/gitphin/git"
PIDS=()

mkdir -p "$W/bin" "$W/cfg" "$W/home" "$SRV"
# A fake notify-send records the text, so the test shows the desktop notification and no popup.
cat >"$W/bin/notify-send" <<'EOS'
#!/usr/bin/env bash
echo "${@: -1}" >>"$NOTIFY_LOG"
EOS
chmod +x "$W/bin/notify-send"

export PATH="$W/bin:$PC:$PATH"
export HOME="$W/home" GITPHIN_CONFIG_DIR="$W/cfg" NOTIFY_LOG="$W/notify.log"
export GITPHIN_URL="http://127.0.0.1:$BR_PORT" W REPO
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null GIT_TERMINAL_PROMPT=0
export GIT_AUTHOR_NAME=Tester GIT_AUTHOR_EMAIL=t@example.com GIT_COMMITTER_NAME=Tester GIT_COMMITTER_EMAIL=t@example.com
export GIT_AUTHOR_DATE="1700000000 +0000" GIT_COMMITTER_DATE="1700000000 +0000"
: >"$NOTIFY_LOG"

cleanup() {
    for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null; done
    wait 2>/dev/null
    rm -rf "$W"
}
trap cleanup EXIT

PASSED=(); FAILED=()
check() { local name="$1"; shift
    if "$@" >"$W/last.log" 2>&1; then PASSED+=("$name"); else FAILED+=("$name"); echo "--- FAIL: $name"; tail -n 15 "$W/last.log"; fi; }
refuse() { local name="$1"; shift
    if "$@" >"$W/last.log" 2>&1; then FAILED+=("$name"); echo "--- FAIL (should be refused): $name"; tail -n 15 "$W/last.log"; else PASSED+=("$name"); fi; }
same() { [[ "$1" == "$2" && -n "$1" ]]; }
commit() { echo "$2" >>"$1/f.txt"; git -C "$1" add f.txt; git -C "$1" commit -q -m "$2"; }
# wait <seconds> <command...>: poll until the command succeeds
wait_for() { local n=$(( $1 * 10 )); shift; for _ in $(seq "$n"); do "$@" >/dev/null 2>&1 && return 0; sleep 0.1; done; return 1; }
lines() { [[ -f "$1" ]] && wc -l <"$1" || echo 0; }
# events <file> : one "event repo" line per delivery
events() { python3 -c '
import json,sys
for l in open(sys.argv[1]):
    d=json.loads(l); b=json.loads(d["body"]); print(b["event"], b.get("repo",""))' "$1"; }
has_event() { events "$1" 2>/dev/null | grep -qx "$2"; }

start_bridge() {
    python3 "$PC/bridge.py" --port $BR_PORT --upstream 127.0.0.1:$MOCK_PORT 2>"$W/bridge.log" &
    BRIDGE_PID=$!; PIDS+=("$BRIDGE_PID")
    wait_for 10 curl -s "http://127.0.0.1:$BR_PORT/pc/hub"
}
stop_bridge() { kill "$BRIDGE_PID" 2>/dev/null; wait "$BRIDGE_PID" 2>/dev/null; }

python3 "$HERE/mock_hub.py" "$SRV" --port $MOCK_PORT 2>"$W/mock.log" &
PIDS+=($!)
wait_for 5 curl -s "http://127.0.0.1:$MOCK_PORT/api/git/repos"
mkdir -p "$REPO"
python3 "$HERE/webhook_sink.py" $SINK_A "$W/sink_a.jsonl" &
PIDS+=($!)

echo '{"enabled": false, "dir": "'"$W"'/backup"}' >"$W/cfg/backup.json"   # off until the backup tests
start_bridge
check "bridge answers /pc/hub" bash -c "curl -s http://127.0.0.1:$BR_PORT/pc/hub | grep -q '\"connected\": true'"
check "bridge forwards to the hub" bash -c "curl -s http://127.0.0.1:$BR_PORT/api/git/repos | grep -q '\"ok\": true'"

# ---- webhooks: configuration through the CLI
URL_A="http://127.0.0.1:$SINK_A/hook"
check "webhook add" git flipper webhook add "$URL_A"
refuse "webhook add: bad URL" git flipper webhook add "ftp://x"
check "webhook secret" git flipper webhook secret s3cret
check "webhook list" bash -c "git flipper webhook list | grep -q '$URL_A'"
check "webhook config is 0600" test "$(stat -c %a "$W/cfg/webhooks.json")" = 600
check "webhook test" git flipper webhook test
check "webhook test: ping arrived" wait_for 3 has_event "$W/sink_a.jsonl" "ping "
cat >"$W/verify.py" <<'EOS'
import hashlib, hmac, json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
secret = sys.argv[2].encode()
for r in rows:
    h = {k.lower(): v for k, v in r["headers"].items()}
    want = "sha256=" + hmac.new(secret, r["body"].encode(), hashlib.sha256).hexdigest()
    assert h.get("x-gitphin-signature-256") == want, ("bad signature", h)
    assert h.get("user-agent") == "gitphin-bridge", h
    assert h.get("content-type") == "application/json", h
    assert h.get("x-gitphin-event") == json.loads(r["body"])["event"], h
    assert len(h.get("x-gitphin-delivery", "")) == 36, h
EOS
check "webhook headers and HMAC" python3 "$W/verify.py" "$W/sink_a.jsonl" s3cret

# ---- webhooks: events from real pushes through the bridge
git init -q -b main "$W/a"; commit "$W/a" one
git -C "$W/a" remote add origin flipper://demo
check "push to create via the bridge" git -C "$W/a" push -q origin main
check "webhook: repo_created" wait_for 8 has_event "$W/sink_a.jsonl" "repo_created demo"
check "webhook: push (first push)" wait_for 8 has_event "$W/sink_a.jsonl" "push demo"
cat >"$W/check_push.py" <<'EOS'
import json, sys
want_after, want_before, msg = sys.argv[2:5]
rows = [json.loads(l) for l in open(sys.argv[1])]
ev = [json.loads(r["body"]) for r in rows if json.loads(r["body"])["event"] == "push"][-1]
assert ev["repo"] == "demo" and ev["via"] == "usb" and ev["server"] == "gitphin" and ev["time"] > 0, ev
r = ev["refs"][0]
assert r == {"ref": "refs/heads/main", "before": want_before, "after": want_after}, (r, ev)
h = ev["head_commit"]
assert h["id"] == want_after and h["message"] == msg and h["author"] == "Tester" and h["time"] == 1700000000, h
EOS
Z=0000000000000000000000000000000000000000
S1="$(git -C "$W/a" rev-parse main)"
check "webhook: first push content" python3 "$W/check_push.py" "$W/sink_a.jsonl" "$S1" "$Z" one
commit "$W/a" two
S2="$(git -C "$W/a" rev-parse main)"
N=$(lines "$W/sink_a.jsonl")
check "second push via the bridge" git -C "$W/a" push -q origin main
check "webhook: second push arrived" wait_for 8 bash -c "[[ \$(wc -l <'$W/sink_a.jsonl') -gt $N ]]"
check "webhook: second push content" python3 "$W/check_push.py" "$W/sink_a.jsonl" "$S2" "$S1" two
sleep 3
check "webhook: one event per push" test "$(( $(lines "$W/sink_a.jsonl") - N ))" = 1
# debounce: three ref changes within 2 s make one event
N=$(lines "$W/sink_a.jsonl")
for b in b1 b2 b3; do curl -s -X POST "$GITPHIN_URL/api/git/ref?name=demo&ref=refs/heads/$b&old=$Z&new=$S2" >/dev/null; done
check "webhook: debounced event arrives" wait_for 8 bash -c "[[ \$(wc -l <'$W/sink_a.jsonl') -gt $N ]]"
sleep 3
check "webhook: debounce gives one event" test "$(( $(lines "$W/sink_a.jsonl") - N ))" = 1
check "webhook: debounced event has 3 refs" python3 -c '
import json
b=json.loads(open("'"$W"'/sink_a.jsonl").readlines()[-1])["body"]; e=json.loads(b)
assert e["event"]=="push" and sorted(r["ref"] for r in e["refs"])==["refs/heads/b1","refs/heads/b2","refs/heads/b3"], e'
# a delete of a ref
N=$(lines "$W/sink_a.jsonl")
git -C "$W/a" push -q origin :b1
check "webhook: ref delete event" wait_for 8 bash -c "[[ \$(wc -l <'$W/sink_a.jsonl') -gt $N ]] && tail -n1 '$W/sink_a.jsonl' | grep -q '\"after\\\\\":\\\\\"$Z'"
# the proxy does not wait for the hooks: a dead URL does not slow requests
git flipper webhook add "http://127.0.0.1:9/dead" >/dev/null
T0=$(date +%s%N); curl -s "$GITPHIN_URL/api/git/repos" >/dev/null; T1=$(date +%s%N)
check "requests do not wait for webhooks" test $(( (T1 - T0) / 1000000 )) -lt 1000
git flipper webhook remove "http://127.0.0.1:9/dead" >/dev/null
check "webhook status route" bash -c "curl -s $GITPHIN_URL/pc/webhooks | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d[\"secret_set\"] and d[\"urls\"][0][\"last\"][\"code\"]==200 and \"s3cret\" not in json.dumps(d)'"
# repo_deleted
git flipper create gone >/dev/null
check "webhook: repo_created (CLI create)" wait_for 8 has_event "$W/sink_a.jsonl" "repo_created gone"
git flipper delete gone --yes >/dev/null
check "webhook: repo_deleted" wait_for 8 has_event "$W/sink_a.jsonl" "repo_deleted gone"

# retries: a receiver that fails the first request
python3 "$HERE/webhook_sink.py" $SINK_B "$W/sink_b.jsonl" --fail-first 1 &
PIDS+=($!)
sleep 0.5
git flipper webhook remove "$URL_A" >/dev/null
git flipper webhook add "http://127.0.0.1:$SINK_B/hook" >/dev/null
commit "$W/a" three
git -C "$W/a" push -q origin main
check "webhook retry: two deliveries" wait_for 12 bash -c "[[ \$(wc -l <'$W/sink_b.jsonl') -ge 2 ]]"
check "webhook retry: same delivery id" python3 -c '
import json
r=[json.loads(l) for l in open("'"$W"'/sink_b.jsonl")]
h=[{k.lower():v for k,v in x["headers"].items()} for x in r]
assert h[0]["x-gitphin-delivery"]==h[1]["x-gitphin-delivery"] and r[0]["body"]==r[1]["body"]'
git flipper webhook remove "http://127.0.0.1:$SINK_B/hook" >/dev/null
check "no URLs: webhook test is refused" bash -c "! git flipper webhook test"

# ---- ZIP
Z1="$W/z"; git init -q -b main "$Z1"
mkdir -p "$Z1/src/deep/er" "$Z1/docs" "$Z1/empty-sub"
echo "hello" >"$Z1/README.md"; printf '#!/bin/sh\necho hi\n' >"$Z1/run.sh"; chmod +x "$Z1/run.sh"
seq 1 5000 >"$Z1/src/numbers.txt"; echo deep >"$Z1/src/deep/er/file.txt"; : >"$Z1/empty.txt"
head -c 3000 /dev/urandom >"$Z1/data.png"; head -c 3000 /dev/urandom >"$Z1/data.bin"
echo "unicode" >"$Z1/docs/Zoë Müller.txt"; echo "space" >"$Z1/docs/with space & amp.txt"
ln -s README.md "$Z1/link-to-readme"; ln -s src/deep "$Z1/link-to-dir"
git -C "$Z1" add -A
git -C "$Z1" update-index --add --cacheinfo 160000,1111111111111111111111111111111111111111,vendor/submod
git -C "$Z1" commit -q -m "zip test"
git -C "$Z1" tag v1; git -C "$Z1" tag -a -m annotated v2
git -C "$Z1" checkout -q -b feature/x; echo more >>"$Z1/README.md"; git -C "$Z1" commit -q -am "more"
git -C "$Z1" checkout -q main
git -C "$Z1" remote add origin flipper://zrepo
git -C "$Z1" push -q origin main feature/x v1 v2 2>/dev/null
zget() { curl -s -D "$W/zh.txt" -o "$W/$1.zip" -w '%{http_code}' "$GITPHIN_URL/pc/zip?$2"; }
zcmp() { # zcmp <ref-for-git-archive> <prefix> <our zip>  [subpath]
    git -C "$Z1" archive --format=zip --prefix="$2/" -o "$W/ref.zip" "$1${4:+:$4}"
    python3 "$HERE/zip_compare.py" "$W/ref.zip" "$3"; }
check "zip: branch HTTP 200" test "$(zget main 'name=zrepo&ref=main')" = 200
check "zip: Content-Disposition" grep -qi '^content-disposition: attachment; filename="zrepo-main.zip"' "$W/zh.txt"
check "zip: chunked" grep -qi '^transfer-encoding: chunked' "$W/zh.txt"
check "zip: same as git archive (main)" zcmp main zrepo-main "$W/main.zip"
check "zip: python testzip" python3 -m zipfile -t "$W/main.zip"
mkdir -p "$W/x1" "$W/x2"
git -C "$Z1" archive --format=zip --prefix=zrepo-main/ main >"$W/ref2.zip"
check "zip: unzip works" unzip -q "$W/main.zip" -d "$W/x1"
unzip -q "$W/ref2.zip" -d "$W/x2"
check "zip: diff -r with git archive" diff -r --no-dereference "$W/x1" "$W/x2"
check "zip: exec bit kept" test -x "$W/x1/zrepo-main/run.sh"
check "zip: symlink kept" test "$(readlink "$W/x1/zrepo-main/link-to-readme")" = README.md
check "zip: submodule is an empty folder" test -d "$W/x1/zrepo-main/vendor/submod"
check "zip: empty file kept" test -f "$W/x1/zrepo-main/empty.txt" -a ! -s "$W/x1/zrepo-main/empty.txt"
check "zip: text is deflated" python3 -c '
import zipfile
z=zipfile.ZipFile("'"$W"'/main.zip")
assert z.getinfo("zrepo-main/src/numbers.txt").compress_type==8
assert z.getinfo("zrepo-main/data.png").compress_type==0
assert z.comment.decode()=="'"$(git -C "$Z1" rev-parse main)"'"'
check "zip: tag HTTP 200" test "$(zget tag 'name=zrepo&ref=v1')" = 200
check "zip: tag same as git archive" zcmp v1 zrepo-v1 "$W/tag.zip"
check "zip: annotated tag" test "$(zget tag2 'name=zrepo&ref=v2')" = 200
check "zip: annotated tag same as git archive" zcmp v2 zrepo-v2 "$W/tag2.zip"
check "zip: branch with a slash" test "$(zget br 'name=zrepo&ref=feature/x')" = 200
check "zip: branch with a slash, file name" grep -qi 'filename="zrepo-feature-x.zip"' "$W/zh.txt"
check "zip: branch with a slash same" zcmp feature/x zrepo-feature-x "$W/br.zip"
SHA="$(git -C "$Z1" rev-parse main)"
check "zip: sha" test "$(zget sha "name=zrepo&ref=$SHA")" = 200
check "zip: sha same as git archive" zcmp "$SHA" "zrepo-$SHA" "$W/sha.zip"
check "zip: sub folder HTTP 200" test "$(zget sub 'name=zrepo&ref=main&path=src/deep')" = 200
check "zip: sub folder same as git archive" zcmp main zrepo-main "$W/sub.zip" src/deep
check "zip: unknown ref is 404 JSON" bash -c "[[ \$(curl -s -o '$W/e.json' -w '%{http_code}' '$GITPHIN_URL/pc/zip?name=zrepo&ref=nope') = 404 ]] && grep -q '\"ok\": false' '$W/e.json'"
check "zip: unknown repo is 404" test "$(zget e 'name=nope&ref=main')" = 404
check "zip: unknown path is 404" test "$(zget e 'name=zrepo&ref=main&path=nothing/here')" = 404
check "zip: bad name is 400" test "$(zget e 'name=../x&ref=main')" = 400
check "zip: missing ref is 400" test "$(zget e 'name=zrepo')" = 400
check "zip: path with .. is 400" test "$(zget e 'name=zrepo&ref=main&path=../x')" = 400
# other requests are served while a big zip streams
BIG="$W/big"; git init -q -b main "$BIG"; mkdir "$BIG/d"
for i in $(seq 1 120); do seq "$i" $((i + 300)) >"$BIG/d/f$i.txt"; done
git -C "$BIG" add -A; git -C "$BIG" commit -q -m big; git -C "$BIG" remote add origin flipper://big
git -C "$BIG" push -q origin main 2>/dev/null
curl -s -o "$W/big.zip" "$GITPHIN_URL/pc/zip?name=big&ref=main" &
ZP=$!
sleep 0.05
T0=$(date +%s%N); curl -s "$GITPHIN_URL/api/git/repos" >/dev/null; T1=$(date +%s%N)
check "other requests work during a zip" test $(( (T1 - T0) / 1000000 )) -lt 3000
wait $ZP
git -C "$BIG" archive --format=zip --prefix=big-main/ -o "$W/ref.zip" main
check "big zip is complete" python3 "$HERE/zip_compare.py" "$W/ref.zip" "$W/big.zip"

# ---- backup
stop_bridge
echo '{"enabled": true, "dir": "'"$W"'/backup"}' >"$W/cfg/backup.json"
: >"$NOTIFY_LOG"
start_bridge
check "backup: runs on plug-in and notifies" wait_for 60 grep -q 'Backup: 3 repos up to date' "$NOTIFY_LOG"
for r in demo zrepo big; do
    check "backup: $r mirrored" test -d "$W/backup/$r.git"
done
check "backup: demo is a mirror of the hub" same "$(git --git-dir="$W/backup/demo.git" rev-parse refs/heads/main)" "$(git -C "$W/a" rev-parse main)"
check "backup: tags mirrored" same "$(git --git-dir="$W/backup/zrepo.git" rev-parse refs/tags/v2)" "$(git -C "$Z1" rev-parse v2)"
check "backup: fsck" git --git-dir="$W/backup/big.git" fsck --strict
check "backup status route" bash -c "curl -s $GITPHIN_URL/pc/backup | python3 -c 'import json,sys; d=json.load(sys.stdin); assert not d[\"running\"] and len(d[\"repos\"])==3 and all(r[\"ok\"] for r in d[\"repos\"]), d'"
check "backup status CLI" bash -c "git flipper backup status | grep -q 'last run: 3 repos up to date'"
commit "$W/a" four
git -C "$W/a" push -q origin main
: >"$NOTIFY_LOG"
check "backup now (CLI)" git flipper backup now
check "backup now: update done" wait_for 60 grep -q 'Backup: 3 repos up to date' "$NOTIFY_LOG"
check "backup: update fetched the new commit" same "$(git --git-dir="$W/backup/demo.git" rev-parse refs/heads/main)" "$(git -C "$W/a" rev-parse main)"
check "backup: update, not clone" bash -c "curl -s $GITPHIN_URL/pc/backup | python3 -c 'import json,sys; d=json.load(sys.stdin); assert all(r[\"action\"]==\"update\" for r in d[\"repos\"]), d'"
check "backup off" git flipper backup off
stop_bridge; : >"$NOTIFY_LOG"; start_bridge
sleep 6
check "backup off: no run on plug-in" test ! -s "$NOTIFY_LOG"
check "backup on" git flipper backup on
check "backup dir" git flipper backup dir "$W/backup2"
check "backup dir saved" bash -c "git flipper backup status | grep -q 'folder $W/backup2'"
refuse "backup: bad subcommand" git flipper backup bogus

# ---- mirror-add / mirror-remove
git init -q --bare -b main "$W/gh.git"
git init -q -b main "$W/m"; commit "$W/m" m1
git -C "$W/m" remote add origin "$W/gh.git"
check "mirror-add" git -C "$W/m" flipper mirror-add --name mirrored
check "mirror-add: original push URL first" test "$(git -C "$W/m" config --get-all remote.origin.pushurl | sed -n 1p)" = "$W/gh.git"
check "mirror-add: Flipper URL second" test "$(git -C "$W/m" config --get-all remote.origin.pushurl | sed -n 2p)" = "flipper://mirrored"
check "mirror-add: fetch URL unchanged" test "$(git -C "$W/m" config --get remote.origin.url)" = "$W/gh.git"
check "mirror-add: repo created on the Flipper" test -d "$REPO/mirrored.git"
check "mirror-add twice keeps two URLs" bash -c "git -C '$W/m' flipper mirror-add --name mirrored && [[ \$(git -C '$W/m' config --get-all remote.origin.pushurl | wc -l) = 2 ]]"
check "git push updates both" git -C "$W/m" push -q origin main
check "push: first target" same "$(git --git-dir="$W/gh.git" rev-parse main)" "$(git -C "$W/m" rev-parse main)"
check "push: Flipper target" same "$(git --git-dir="$REPO/mirrored.git" rev-parse main)" "$(git -C "$W/m" rev-parse main)"
check "mirror-remove" git -C "$W/m" flipper mirror-remove
check "mirror-remove: no push URL left" test -z "$(git -C "$W/m" config --get-all remote.origin.pushurl)"
refuse "mirror-add: unknown remote" git -C "$W/m" flipper mirror-add nope --name mirrored
# default name from the folder, http URL when git-remote-flipper is not on PATH
mkdir "$W/nohelper"; ln -s "$PC/git-remote-flipper" "$W/nohelper/git-flipper"
git init -q -b main "$W/proj"; git -C "$W/proj" remote add origin "$W/gh.git"
check "mirror-add: name from folder, http URL" bash -c "PATH='$W/bin:$W/nohelper:/usr/bin:/bin' git -C '$W/proj' flipper mirror-add && [[ \$(git -C '$W/proj' config --get-all remote.origin.pushurl | sed -n 2p) = '$GITPHIN_URL/git/proj.git' ]]"

echo
echo "passed: ${#PASSED[@]}"
printf '  ok   %s\n' "${PASSED[@]}"
echo "failed: ${#FAILED[@]}"
[[ ${#FAILED[@]} -gt 0 ]] && { printf '  FAIL %s\n' "${FAILED[@]}"; echo "--- bridge log"; tail -n 30 "$W/bridge.log"; }
[[ ${#FAILED[@]} -eq 0 ]]
