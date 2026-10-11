# Security

Please report security problems privately through GitHub: **Security → Report a
vulnerability** on this repository (or open an issue that only says you have something to
report, without details).

## What Gitphin exposes

- **Only `127.0.0.1`.** The bridge (`pc/bridge.py`) listens on `127.0.0.1:8080`. Other machines
  cannot reach it. There are no users or passwords: anyone who can open a connection to
  `127.0.0.1` on your PC — every local user and program — can read and change the repos and the
  files on the Flipper's SD card. Use Gitphin on a PC you trust.
- **The Flipper app** answers only on its USB serial channel. It reads and writes only below
  `/ext` and `/int`; Git and document paths are checked against `..`, empty segments and
  leading dots.
- **The bridge runs as your user**, never as root. The udev rule gives the desktop user access to
  the Flipper's serial ports only.

## Data safety

- Every write on the Flipper goes to a temp file and is renamed into place; refs change by
  compare-and-swap. A reboot or unplug does not corrupt a repository.
- Memory is checked before every route (guarded allocator), so a request that does not fit is
  refused with `503` instead of halting the device.

## The optional Wi-Fi password helper

The Collector tab can show the password of the connected Wi-Fi only after you install a
root-owned helper and a sudoers rule yourself ([docs/api.md](docs/api.md)). The rule lets your
user run exactly one fixed script that reads one connection's PSK; nothing else runs as root.
Without it, no password is read. Remove it with
`sudo rm /usr/local/sbin/gitphin-wifi-psk /etc/sudoers.d/gitphin-wifi`.

## Known limits (planned work)

- **Other web sites in your browser.** No CORS headers are sent, so other sites cannot read the
  answers. But the bridge does not check the `Origin` or `Host` header yet, so a malicious page
  open in your browser could send blind write requests (for example `POST /api/delete`) to
  `127.0.0.1:8080`, and a DNS-rebinding page could read it. Recent Chrome versions block many
  such requests to local addresses; other browsers may not. Keep the bridge stopped
  (`systemctl --user stop gitphin.path gitphin.service`) when you do not use Gitphin, and keep
  backups of your repos.

## Release downloads

Release files are built by GitHub Actions from the tagged commit and listed with their SHA-256 in
`SHA256SUMS`. Check them with `sha256sum -c SHA256SUMS --ignore-missing`. The checksums come from
the same release as the files (there is no separate signature yet).
