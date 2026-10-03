#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root/mac"
mkdir -p .cache/clang .cache/swiftpm .cache/config .cache/security
export CLANG_MODULE_CACHE_PATH="$root/mac/.cache/clang"
if [ -z "${SDKROOT:-}" ] && [ -d /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk ]; then
  export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
fi
swift build --disable-sandbox --configuration release \
  --cache-path .cache/swiftpm --config-path .cache/config \
  --security-path .cache/security --manifest-cache local
bundle="$root/dist/Flipper Mac Now Playing.app"
mkdir -p "$bundle/Contents/MacOS"
cp Info.plist "$bundle/Contents/Info.plist"
cp .build/release/flipper-mac-now-playing "$bundle/Contents/MacOS/"
codesign --force --sign - "$bundle"
printf '%s\n' "$bundle"
