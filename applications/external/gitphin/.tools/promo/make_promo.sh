#!/usr/bin/env bash
# Makes the promo video and GIF: docs/promo/gitphin.mp4 (1280x720, 44 s) and
# docs/promo/gitphin.gif (the push and browser part, smaller), from docs/screenshots
# (run tools/screenshots/capture.sh first), flipper_app/screenshots and push.txt.
# push.txt is the output of a real plain-git push (GIT_SMART_HTTP=0, WebDAV, as git talks to the
# Flipper) of the demo repo weather-station to the Gitphin core running on a PC (gitphind of
# Gitphin Droid), port changed to 8080.
# Usage: tools/promo/make_promo.sh
# Needs Node.js, Chromium (CHROMIUM=/path, default: Playwright's) and ffmpeg with libx264.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
OUT="$TOP/docs/promo"
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
mkdir -p "$OUT"
(cd "$W" && npm init -y >/dev/null && npm i -s playwright-core@1.55.0 >/dev/null)
NODE_PATH="$W/node_modules" node "$HERE/promo.cjs" "$W/frames"
ffmpeg -loglevel error -y -framerate 30 -i "$W/frames/%05d.png" -c:v libx264 -preset slow -crf 20 \
    -pix_fmt yuv420p -movflags +faststart "$OUT/gitphin.mp4"
# the GIF: the push and the browser part (seconds 3.5 to 24), 12 fps, 720 px wide
ffmpeg -loglevel error -y -ss 3.5 -t 20.5 -i "$OUT/gitphin.mp4" \
    -vf "fps=12,scale=720:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" \
    "$OUT/gitphin.gif"
ls -la "$OUT"
