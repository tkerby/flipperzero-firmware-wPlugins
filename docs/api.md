# File API and Collector tab

The Git routes are in [git-design.md](git-design.md); this page lists the rest.

## Collector tab

A second header tab, **Collector**, shows data about the PC, not the Flipper. The bridge
answers these routes itself (it does not forward them to the Flipper):

| Method | Path | Returns |
|---|---|---|
| GET | `/pc/info` | hostname, OS, kernel, CPU, memory, uptime, IPs, active connections |
| GET | `/pc/wifi` | SSID, signal, and (only with the helper below) the saved password of the connected Wi-Fi |

Left column: this PC. Right column: the connected Wi-Fi. Linux / NetworkManager only for now.
Like everything the bridge serves, these routes are reachable only from `127.0.0.1`.

### Wi-Fi password (opt-in)

Reading a saved Wi-Fi password needs root, so by default `/pc/wifi` returns the SSID with
`password: null` and a note. To show your own network's password in the page (behind a
**Show** button, never written to the log), install the narrow sudo helper. It is root-owned,
so the rule cannot be abused, and it reads exactly one thing: the PSK of one connection.

```bash
cd /path/to/gitphin
sudo install -m 755 -o root -g root pc/wifi_psk.sh /usr/local/sbin/gitphin-wifi-psk
echo "$USER ALL=(root) NOPASSWD: /usr/local/sbin/gitphin-wifi-psk" | sudo tee /etc/sudoers.d/gitphin-wifi
sudo chmod 440 /etc/sudoers.d/gitphin-wifi
systemctl --user restart gitphin.service
```

To undo: `sudo rm /usr/local/sbin/gitphin-wifi-psk /etc/sudoers.d/gitphin-wifi`.

## File API

The page's **Files** tab uses the app's basic file API.

| Method | Path | Notes |
|---|---|---|
| GET | `/` | the web page |
| GET | `/api/info` | name, firmware, battery, SD space, free RAM, uptime |
| GET | `/api/list?path=/ext/...` | folder listing |
| GET | `/api/file?path=...[&dl=1]` | read a file (`dl=1` = download) |
| PUT | `/api/file?path=...` | write a file (request body = content) |
| POST | `/api/mkdir?path=...` | make a folder |
| POST | `/api/delete?path=...` | delete a file or an empty folder |

Paths must be inside `/ext` (SD card).
