#!/usr/bin/env bash
# Install the Flipper Hub bridge as a systemd user unit and the git helper (no sudo).
# Usage: ./install.sh            install and enable
#        ./install.sh --remove   disable and remove
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
UNITS="$HOME/.config/systemd/user"
BIN="$HOME/.local/bin"

if [[ "${1:-}" == "--remove" ]]; then
    systemctl --user disable --now flipper-hub.path 2>/dev/null || true
    systemctl --user stop flipper-hub.service 2>/dev/null || true
    rm -f "$UNITS/flipper-hub.path" "$UNITS/flipper-hub.service"
    systemctl --user daemon-reload
    rm -f "$BIN/git-remote-flipper" "$BIN/git-flipper"
    echo "Flipper Hub bridge removed."
    exit 0
fi

if [[ ! -x "$HERE/.venv/bin/python" ]]; then
    python3 -m venv "$HERE/.venv"
fi
"$HERE/.venv/bin/pip" install -q pyserial

mkdir -p "$UNITS"
ln -sf "$HERE/systemd/flipper-hub.path" "$UNITS/flipper-hub.path"
ln -sf "$HERE/systemd/flipper-hub.service" "$UNITS/flipper-hub.service"
systemctl --user daemon-reload
systemctl --user enable --now flipper-hub.path

mkdir -p "$BIN"
ln -sf "$HERE/pc/git-remote-flipper" "$BIN/git-remote-flipper"
ln -sf "$HERE/pc/git-remote-flipper" "$BIN/git-flipper"

echo "Installed. Plug in the Flipper, then open http://localhost:8080"
echo "Git helper: git clone flipper://<name>, git flipper list (needs $BIN in PATH)."

RULE=/etc/udev/rules.d/70-flipper-hub.rules
if ! cmp -s "$HERE/systemd/70-flipper-hub.rules" "$RULE"; then
    echo
    echo "One step needs root: the udev rule that gives you access to the Flipper port."
    echo "Run this once:"
    echo "  sudo install -m 644 '$HERE/systemd/70-flipper-hub.rules' $RULE && sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=tty"
fi
