#!/usr/bin/env bash
# Normalize any audio files into 128 kbps CBR MP3 for Flipper Radio.
# Usage: ./convert_music.sh <input-dir-or-file>... -> writes to ./converted/
set -euo pipefail

OUT="./converted"
mkdir -p "$OUT"

for src in "$@"; do
  if [ -d "$src" ]; then
    find "$src" -type f \( -iname '*.mp3' -o -iname '*.m4a' -o -iname '*.flac' \
      -o -iname '*.wav' -o -iname '*.aac' -o -iname '*.ogg' \) -print0 \
      | while IFS= read -r -d '' f; do "$0" "$f"; done
    continue
  fi
  base="$(basename "${src%.*}")"
  echo "converting: $src -> $OUT/$base.mp3"
  ffmpeg -nostdin -y -i "$src" \
    -codec:a libmp3lame -b:a 128k -minrate 128k -maxrate 128k -bufsize 128k \
    -ar 44100 -ac 2 -map_metadata -1 \
    "$OUT/$base.mp3" >/dev/null 2>&1
done

echo "done -> $OUT/"
