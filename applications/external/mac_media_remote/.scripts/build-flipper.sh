#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
firmware=${FLIPPER_FIRMWARE_DIR:-"$root/.cache/flipper-firmware"}
tag=${FLIPPER_FIRMWARE_TAG:-1.4.3}
if [ ! -d "$firmware/.git" ]; then
  mkdir -p "$(dirname "$firmware")"
  git clone --depth 1 --branch "$tag" --recurse-submodules --shallow-submodules \
    https://github.com/flipperdevices/flipperzero-firmware.git "$firmware"
fi
actual=$(git -C "$firmware" describe --tags --exact-match 2>/dev/null || true)
if [ "$actual" != "$tag" ]; then
  echo "Firmware SDK checkout is $actual, expected $tag" >&2
  exit 1
fi
mkdir -p "$firmware/applications_user/now_playing" "$root/dist"
cp -R "$root/flipper/now_playing/." "$firmware/applications_user/now_playing/"
toolchain=${FBT_TOOLCHAIN_PATH:-"$root/.cache/flipper-toolchain"}
(cd "$firmware" && FBT_TOOLCHAIN_PATH="$toolchain" ./fbt fap_now_playing)
api=$(awk -F, '/^Version/{print $3; exit}' "$firmware/targets/f7/api_symbols.csv")
cp "$firmware/build/f7-firmware-D/.extapps/now_playing.fap" "$root/dist/now_playing-fw${tag}-api${api}.fap"
printf '%s\n' "$root/dist/now_playing-fw${tag}-api${api}.fap"
