#!/usr/bin/env bash
# Print the saved PSK of one NetworkManager connection. Needs root.
# Used by the bridge through a narrow NOPASSWD sudoers rule, so the bridge
# itself does not run as root. It prints only the password, nothing else.
#
# Install it to a root-owned path and allow exactly that path, so the rule
# cannot be abused by editing the script:
#   sudo install -m 755 -o root -g root wifi_psk.sh /usr/local/sbin/gitphin-wifi-psk
#   echo '<user> ALL=(root) NOPASSWD: /usr/local/sbin/gitphin-wifi-psk' \
#     | sudo tee /etc/sudoers.d/gitphin-wifi && sudo chmod 440 /etc/sudoers.d/gitphin-wifi
set -euo pipefail

conn="${1:-}"
[ -n "$conn" ] || { echo "usage: wifi_psk.sh <connection-name>" >&2; exit 2; }

exec nmcli -s -g 802-11-wireless-security.psk connection show "$conn"
