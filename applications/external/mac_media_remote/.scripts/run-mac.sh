#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$root/dist/Flipper Mac Now Playing.app/Contents/MacOS/flipper-mac-now-playing"
