#!/bin/sh
# Teach libccid (pcscd's USB driver) about the Flipper CCID reader.
#
# libccid only talks to USB IDs listed in its Info.plist, so the Flipper's VID:PID
# must be added once. Run with sudo. A backup of the plist is kept next to it.
#
#   sudo ./install_ccid_linux.sh            # add the entry
#   sudo ./install_ccid_linux.sh --remove   # restore the backup
#
# VID/PID must match NB_USB_VID / NB_USB_PID in nb_usb_ccid.h.
set -eu

VID=0x0483
PID=0x5748
NAME="Flipper NFC Reader"

PLIST=""
for p in \
    /usr/lib/pcsc/drivers/ifd-ccid.bundle/Contents/Info.plist \
    /usr/lib64/pcsc/drivers/ifd-ccid.bundle/Contents/Info.plist \
    /usr/lib/x86_64-linux-gnu/pcsc/drivers/ifd-ccid.bundle/Contents/Info.plist \
    /usr/local/lib/pcsc/drivers/ifd-ccid.bundle/Contents/Info.plist; do
    [ -f "$p" ] && PLIST="$p" && break
done
if [ -z "$PLIST" ]; then
    echo "Could not find libccid's Info.plist. Install it first:" >&2
    echo "  sudo apt install pcscd libccid pcsc-tools" >&2
    exit 1
fi

if [ "${1:-}" = "--remove" ]; then
    if [ -f "$PLIST.flipper-bak" ]; then
        cp "$PLIST.flipper-bak" "$PLIST"
        echo "Restored $PLIST"
    else
        echo "No backup found at $PLIST.flipper-bak" >&2
        exit 1
    fi
    systemctl restart pcscd 2>/dev/null || service pcscd restart 2>/dev/null || true
    exit 0
fi

if grep -qi "$PID" "$PLIST" && grep -q "$NAME" "$PLIST"; then
    echo "Already present in $PLIST"
else
    [ -f "$PLIST.flipper-bak" ] || cp "$PLIST" "$PLIST.flipper-bak"
    python3 - "$PLIST" "$VID" "$PID" "$NAME" <<'PY'
import re, sys
path, vid, pid, name = sys.argv[1:5]
s = open(path, encoding="utf-8").read()

def add(key, value):
    global s
    pat = re.compile(r"(<key>%s</key>\s*<array>)(.*?)(</array>)" % key, re.S)
    m = pat.search(s)
    if not m:
        sys.exit("key %s not found in Info.plist" % key)
    indent = "\t\t"
    s = s[:m.end(2)] + "%s<string>%s</string>\n\t" % (indent, value) + s[m.end(2):]

add("ifdVendorID", vid)
add("ifdProductID", pid)
add("ifdFriendlyName", name)
open(path, "w", encoding="utf-8").write(s)
print("Added %s:%s to %s" % (vid, pid, path))
PY
fi

systemctl restart pcscd 2>/dev/null || service pcscd restart 2>/dev/null || true
echo "Done. Plug the Flipper in (PC/SC mode) and run: pcsc_scan"
