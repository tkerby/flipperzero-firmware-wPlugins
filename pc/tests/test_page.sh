#!/usr/bin/env bash
# Tests of the web page: the bundle matches the committed files, size limit of the sources, JS syntax,
# unit tests (highlighter, Files guard) and a browser check against the real route code on the host.
# Usage: pc/tests/test_page.sh      (KEEP=1 keeps the scratch dir; CHROMIUM=/path to pick the browser)
# Needs gcc, zlib, git, python3, Node.js, Chromium and (once, for playwright-core) npm with network.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
WEB="$TOP/flipper_app/web"
mkdir -p /tmp/claude-1000
W="$(mktemp -d /tmp/claude-1000/test_page.XXXXXX)"
PIDS=()
cleanup() { for p in "${PIDS[@]:-}"; do [[ -n "$p" ]] && kill "$p" 2>/dev/null; done; if [[ -n "${KEEP:-}" ]]; then echo "kept $W"; else rm -rf "$W"; fi; }
trap cleanup EXIT
PASSED=(); FAILED=()
check() { local n="$1"; shift
    if "$@" >"$W/last.log" 2>&1; then PASSED+=("$n"); else FAILED+=("$n"); echo "--- FAIL: $n"; tail -n 20 "$W/last.log"; fi; }

# 1. the bundle
mkdir -p "$W/out"
sh "$TOP/pc/tools/build_page.sh" "$W/out" || { echo "build_page.sh failed"; exit 1; }
t_same() { cmp "$W/out/index.html" "$TOP/flipper_app/files/index.html" && cmp "$W/out/index.html.gz" "$TOP/flipper_app/files/index.html.gz"; }
check "bundle equals the committed index.html and index.html.gz (cmp)" t_same
t_gz() { cmp <(gzip -dc "$TOP/flipper_app/files/index.html.gz") "$TOP/flipper_app/files/index.html"; }
check "index.html.gz unpacks to index.html" t_gz
t_size() { local f bad=0; while IFS= read -r f; do [[ $(wc -l <"$f") -le 500 ]] || { echo "over 500 lines: $f"; bad=1; }; done < <(find "$WEB" -type f); return $bad; }
check "every file in flipper_app/web has at most 500 lines" t_size
t_order() { local f; while read -r kind p; do [[ "$kind" == \#* || -z "$kind" ]] && continue; [[ -f "$WEB/$p" ]] || { echo "missing $p"; return 1; }; done <"$WEB/order.txt"
    while IFS= read -r f; do grep -q " ${f#$WEB/}\$" "$WEB/order.txt" || [[ "$f" == */order.txt || "$f" == */index.html || "$f" == */README.md ]] || { echo "not in order.txt: $f"; return 1; }; done < <(find "$WEB" -type f); }
check "order.txt lists every source file, and only existing ones" t_order

# 2. syntax
if command -v node >/dev/null; then
    t_js() { local f; for f in $(find "$WEB/js" -name '*.js'); do cp "$f" "$W/chk.js"; node --check "$W/chk.js" || { echo "$f"; return 1; }; done; }
    check "node --check on each JS source file" t_js
    t_inl() { node -e '
const h = require("fs").readFileSync(process.argv[1], "utf8"), s = [...h.matchAll(/<script>\n([\s\S]*?)<\/script>/g)];
if (s.length !== 1) throw new Error("scripts: " + s.length);
new (require("vm").Script)(s[0][1]); console.log("inlined script ok, " + s[0][1].length + " bytes");' "$TOP/flipper_app/files/index.html"; }
    check "node syntax check of the inlined script" t_inl
    check "unit tests: highlighter and Files guard" node "$HERE/page/unit.cjs" "$WEB"
else
    echo "node not found: JS tests skipped"; FAILED+=("node missing")
fi

# 3. browser check on the host with the real route code
CT="${ROUTE_TOP:-$TOP}"   # ROUTE_TOP=<other checkout> builds the host route code from there
APP="$CT/flipper_app"
findf() { find "$APP" "$CT/pc/tests" -name "$1" -not -path '*/node_modules/*' | head -1; }
build_routes() {
    mkdir -p "$W/b" && cd "$W/b" || return 1
    local STUB; STUB="$(find "$CT/pc/tests" -type d -name furi_stub | head -1)"
    local INC=""; for d in $(find "$APP" -name '*.h' -printf '%h\n' | sort -u); do INC="$INC -I$d"; done
    local CF="-O1 -I$STUB $INC -Wno-unused-parameter -include $(findf malloc_wrap.h)"
    local f; for f in hub_chunk hub_gitview hub_git hub_gitobj hub_inflate hub_gitdiff hub_gitsearch; do
        gcc $CF -c "$(findf $f.c)" -o "$f.o" || return 1
    done
    gcc $CF -DHUB_EXT_QUERY -c "$(findf hub_gitext.c)" -o hub_gitext.o || return 1
    gcc -O1 -I"$STUB" $INC -c "$(findf gitio_stdio.c)" -o gitio.o || return 1
    gcc $CF -c "$(findf furi_stub.c)" -o furi_stub.o || return 1
    gcc $CF -c "$(findf route_test.c)" -o route_test.o || return 1
    gcc -O1 -c "$(findf route_heap.c)" -o route_heap.o || return 1
    gcc *.o -lz -o route_test
}
if ! command -v node >/dev/null || [[ ! -x "${CHROMIUM:-/usr/bin/chromium}" ]]; then
    echo "node or chromium missing: browser check skipped"; FAILED+=("browser check skipped")
else
    check "host route code builds" build_routes
    if [[ -x "$W/b/route_test" ]]; then
        export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
        check "demo repos" bash "$TOP/tools/screenshots/make_demo.sh" "$W/root"
        mkdir -p "$W/sd/apps_data/gitphin" "$W/sd/apps_assets/gitphin" "$W/sd/music"
        ln -s "$W/root/git" "$W/sd/apps_data/gitphin/git"
        : >"$W/sd/apps_assets/gitphin/index.html.gz"; : >"$W/requests.log"
        PORT="${PORT:-8098}"
        python3 -I "$HERE/page/page_server.py" "$W/b/route_test" "$W/root" "$W/sd" "$TOP/flipper_app/files/index.html" "$PORT" &
        PIDS+=($!)
        for _ in $(seq 50); do curl -s "http://127.0.0.1:$PORT/api/info" >/dev/null && break; sleep 0.1; done
        mkdir -p "$W/pw" && (cd "$W/pw" && npm init -y >/dev/null 2>&1 && npm i -s playwright-core@1.55.0 >/dev/null 2>&1)
        export NODE_PATH="$W/pw/node_modules" CHROMIUM="${CHROMIUM:-/usr/bin/chromium}"
        t_browser() { node "$HERE/page/browser.cjs" "http://127.0.0.1:$PORT" "$W/sd" "$W/requests.log" | tee "$W/browser.log"; [[ ${PIPESTATUS[0]} -eq 0 ]]; }
        check "browser: lists, repo page, highlight, Files guard, avatar, ZIP" t_browser
    fi
fi

echo "passed: ${#PASSED[@]}  failed: ${#FAILED[@]}"
printf '  ok: %s\n' "${PASSED[@]}"
if [[ ${#FAILED[@]} -eq 0 ]]; then echo "ALL TESTS PASSED"; else printf '  FAILED: %s\n' "${FAILED[@]}"; exit 1; fi
