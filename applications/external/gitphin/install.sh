#!/usr/bin/env bash
# Install the Gitphin bridge as a systemd user unit and the git helper (no sudo).
# Usage: ./install.sh            install and enable
#        ./install.sh --remove   disable and remove
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
UNITS="$HOME/.config/systemd/user"
BIN="$HOME/.local/bin"

# Units of releases before 1.0 (then called "Flipper Hub").
remove_legacy() {
    if [[ -e "$UNITS/flipper-hub.path" || -e "$UNITS/flipper-hub.service" ]]; then
        systemctl --user disable --now flipper-hub.path 2>/dev/null || true
        systemctl --user stop flipper-hub.service 2>/dev/null || true
        rm -f "$UNITS/flipper-hub.path" "$UNITS/flipper-hub.service"
        echo "Removed the old flipper-hub units."
    fi
}

if [[ "${1:-}" == "--remove" ]]; then
    systemctl --user disable --now gitphin.path 2>/dev/null || true
    systemctl --user stop gitphin.service 2>/dev/null || true
    rm -f "$UNITS/gitphin.path" "$UNITS/gitphin.service"
    remove_legacy
    systemctl --user daemon-reload
    rm -f "$BIN/git-remote-flipper" "$BIN/git-flipper"
    echo "Gitphin bridge removed."
    exit 0
fi

if [[ ! -x "$HERE/.venv/bin/python" ]]; then
    python3 -m venv "$HERE/.venv"
fi
"$HERE/.venv/bin/pip" install -q pyserial

mkdir -p "$UNITS"
remove_legacy
ln -sf "$HERE/systemd/gitphin.path" "$UNITS/gitphin.path"
# The service names this folder, so it is written, not linked.
rm -f "$UNITS/gitphin.service"
sed "s|@GITPHIN_DIR@|$HERE|g" "$HERE/systemd/gitphin.service" > "$UNITS/gitphin.service"
systemctl --user daemon-reload
systemctl --user enable --now gitphin.path

mkdir -p "$BIN"
ln -sf "$HERE/pc/git-remote-flipper" "$BIN/git-remote-flipper"
ln -sf "$HERE/pc/git-remote-flipper" "$BIN/git-flipper"

echo "Installed. Plug in the Flipper, then open http://localhost:8080"
echo "Git helper: git clone flipper://<name>, git flipper list (needs $BIN in PATH)."

RULE=/etc/udev/rules.d/70-gitphin.rules
if ! cmp -s "$HERE/systemd/70-gitphin.rules" "$RULE"; then
    echo
    echo "One step needs root: the udev rule that gives you access to the Flipper port."
    echo "Run this once:"
    echo "  sudo install -m 644 '$HERE/systemd/70-gitphin.rules' $RULE && sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=tty"
    if [[ -e /etc/udev/rules.d/70-flipper-hub.rules ]]; then
        echo "  sudo rm /etc/udev/rules.d/70-flipper-hub.rules   # the old rule, same content"
    fi
fi
