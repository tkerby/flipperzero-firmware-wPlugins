# Gitphin

**Git hosting in your pocket.** Gitphin runs **on the Flipper Zero**: it keeps
Git repositories on the SD card, serves them to plain `git`, and has a
Gitea-like web page (code, commits, diffs, branches, tags, issues, pull
requests, releases, wiki, search). Plug the Flipper in by USB, open
<http://localhost:8080>, and use git. Nothing else to install on the PC.

```
git / browser ──TCP──> pc/bridge.py ──USB serial, channel 2──> Gitphin app (HTTP + Git server)
localhost:8080         (copies bytes only)                      core + plugins, repos on SD
```

- `flipper_app/` – the Flipper app (Apps > Tools > Gitphin; internal id `flipper_hub`).
  - Resident core: `hub_http.c` (HTTP, web page, file API), `hub_transport.c`
    (USB framing), `hub_git.c` / `hub_gitview.c` (repos, refs, browse),
    `hub_gitobj.c` + `hub_inflate.c` (Git object reader), `hub_mem.c` (guarded
    allocator), `flipper_hub.c` (screen).
  - Plugins (`.fal`, loaded on demand, one at a time): `dav` (WebDAV for
    `git push`), `pack` (idle packing, pack merging, commit graph), `write`
    (web commits, merge), `query` (diff, compare, search, log, commit), `doc`
    (issues, pull requests, releases, wiki).
  - `files/index.html` – the web page (served gzipped when
    `files/index.html.gz` exists; rebuild it with `pc/tools/build_page.sh`).
- `pc/bridge.py` – starts the app when the Flipper is plugged in, maps each TCP
  connection on `127.0.0.1:8080` onto the USB channel, and reboots a Flipper
  whose app has not answered for 3 minutes. No HTTP logic.
- `pc/git-remote-flipper` – optional helper for `flipper://<repo>` remotes
  (faster pack pushes, `git flipper gc`). Nothing depends on it.
- `systemd/` – user units: the `.path` unit starts the bridge when a Flipper CLI
  port appears; the bridge exits when the Flipper is unplugged.
- `docs/git-design.md` – the full design: routes, formats, resource limits.

## Use git with the Flipper

Plug the Flipper in. The bridge starts Gitphin by itself. Then:

```bash
# put an existing repo on the Flipper (the repo is created on the first push)
git remote add flipper http://127.0.0.1:8080/git/myproject.git
git push flipper main --tags

# clone, pull, push as usual
git clone http://127.0.0.1:8080/git/myproject.git
git pull
git push
```

Open <http://localhost:8080> for the web page. Repos live on the SD card in
`/ext/git/<name>.git` (normal bare repositories).

## How it works

**Path of a request.** Git or the browser talks HTTP to `127.0.0.1:8080`. The
PC bridge (`pc/bridge.py`) copies the bytes, unchanged, into frames on the
Flipper's second USB serial channel. The Gitphin app on the Flipper parses the
HTTP request, does the Git work and answers. One request runs at a time; the
bridge queues the rest.

**Plain git, no helper.**
- *Clone, fetch, pull* use git's "dumb HTTP" protocol: git downloads
  `info/refs`, `objects/info/packs`, pack files and loose objects as plain
  files.
- *Push* uses git's built-in WebDAV fallback (`git http-push`): git uploads
  each object as a file (PUT + MOVE), then updates the branch under a lock.
  The first push to an unknown name creates the repo.
- The PC's git does all heavy Git work (packing, deltas, hashing). The Flipper
  stores and serves files and reads objects for the web page.

**Repository storage.** Each repo is a normal bare repository at
`/ext/git/<name>.git` on the SD card. You can copy it to a PC and use it
there. Extra Gitphin data lives in `<repo>.git/hub/`: issues, pull requests,
wiki and releases (JSON/Markdown documents with ETag conflict checks), the
commit graph (`graph.bin`) and caches. Git ignores this folder.

**Background work (when the Flipper is idle for 10 s).**
- Loose objects from pushes are packed into one pack file, so clones need few
  requests.
- Small packs are merged (from 4 small packs upward), so lookups stay fast.
- A commit graph (`hub/graph.bin`, about 70 bytes per commit) is built, so
  compare, merge and log do not need to unpack every commit.
- Each step runs in slices of about 3 s and keeps its progress.

**Memory design.** Apps from the SD card run from RAM, so code competes with
work memory (about 187 KB heap in total).
- A small resident core (~47 KB code) plus five plugins (4–15 KB each) that
  are loaded only while they are needed, one at a time.
- Every allocation goes through a guarded allocator: it refuses a request that
  does not fit the largest free block plus an 8 KB reserve, and the route
  answers `503 low memory, try again` instead. (The firmware halts the device
  when a normal malloc fails.)
- Answers are streamed in 512-byte pieces (chunked JSON), the zlib window is at
  most 8 KB in RAM (older bytes are read back from the SD card), and big
  delta objects are rebuilt through temp files on the SD card and cached.
- Free heap with Gitphin running: about 80–85 KB.

**Self-protection.**
- Every route checks free memory first and has a deadline (60 s for merge,
  compare and diff; 20 s for search).
- The screen spinner moves only while the worker is alive; "STUCK" appears
  when it is not.
- If the app stops answering for 3 minutes, the bridge reboots the Flipper.
  All writes use a temp file + rename, so a reboot does not corrupt repos.

**Screen.** Header with the Gitphin logo; state (Waiting for PC, Ready, Busy,
Packing, STUCK); during transfers a progress bar, object/file count, KB and
speed; "Done: …" for 2 s afterwards.

**Web page.** Served gzipped (~47 KB) from the SD card; all rendering, diffs
and Markdown run in the browser. The page sends one request at a time and
caches everything addressed by a commit/object SHA, so the Flipper is asked as
little as possible.

## Limitations

- **Local only.** The bridge listens on `127.0.0.1`; other machines cannot
  reach it. There are no users, logins or permissions.
- **Speed.** Plain `git push` costs about 0.3 s per object (SD-card writes);
  a first push of a big repo takes minutes. Downloads run at about 330 KB/s,
  uploads at about 120 KB/s. The optional `flipper://` helper pushes one pack
  instead and is much faster for big pushes.
- **Repo size.** Comfortable up to a few MB and a few thousand commits. Larger
  repos work but get slow (compare/merge history walks are capped at 500
  commits; files over 64 KB are skipped by search).
- **Push progress.** Git never tells the server how many objects a push will
  send, so the screen shows the % of the current file and a running count,
  not a total %. Clones of packed repos show the total %.
- **Merge on the device** is a tree merge only: it works when the two branches
  changed different files. Changes to the same file are a conflict; merge
  those on the PC and push.
- **Web edits** are limited to files up to 64 KB; issues/wiki documents up to
  64 KB each.
- **No hooks, CI, webhooks, SSH, LFS, blame or activity graphs.**
- **One person at a time.** Two simultaneous pushes to the same branch are
  protected by compare-and-swap on the ref, but the WebDAV locks are simple.
- **Objects that did not compress well with the Flipper's small-window
  compressor stay loose**; they are read with the SD-backed window and are
  slower to clone.
- **Linux PC only** for the bridge (systemd, udev); the Flipper side is
  independent.

## What to keep in mind

- **Turn the SD desktop animations off** (they take about 49 KB of RAM):
  rename `/ext/dolphin/manifest.txt` to `manifest.txt.off` with the Flipper CLI
  or qFlipper. With them on, heavy routes answer "low memory" more often.
- **Do not keep qFlipper or the web lab connected** while you use Gitphin:
  they hold the USB port and use RAM.
- **Never close a busy Gitphin with Back or `loader close`**: the loader can
  freeze. Reboot instead (Left + Back for 5 s). Data stays safe.
- **After a firmware update**, rebuild the app with the matching SDK
  (`ufbt update`, check `info device` → `firmware.api`), or it will not start.
- **Deploy with the bridge stopped** (see Install) so only one program starts
  the app; after editing `files/index.html`, run `pc/tools/build_page.sh`, or
  the Flipper serves the old gzipped page.
- **Back up your repos.** They are normal bare repos: `git clone --mirror`
  them to the PC now and then, or copy `/ext/git/` from the SD card.
- **Give big pushes time.** Leave the Flipper connected and idle afterwards
  so it can pack and build the commit graph; the first compare/merge on a new
  history is slower until then.
- **If something hangs**: the bridge log tells what happened
  (`journalctl --user -u flipper-hub.service -f`), and the bridge reboots the
  Flipper itself after 3 minutes.
- **Code size is at its limit.** New features should go into a plugin, not
  the core, and must follow the "Resource limits" rules in
  `docs/git-design.md`.

## Install

```bash
./install.sh            # venv + systemd user units, no sudo
./install.sh --remove   # undo
```

The bridge needs read/write access to the Flipper serial port. Install the udev
rule once (`install.sh` prints the command). It gives the desktop user access at
once, with no group change and no new login:

```bash
sudo install -m 644 systemd/70-flipper-hub.rules /etc/udev/rules.d/70-flipper-hub.rules
sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=tty
```

Membership in the `uucp` group also works, but only after the systemd user
manager restarts (full logout or reboot).

Build and install the Flipper app (needs uFBT with the SDK that matches the firmware):

Deploy (one program may start the app at a time, so stop the bridge first):

```bash
pc/tools/build_page.sh
systemctl --user stop flipper-hub.path flipper-hub.service
cd flipper_app && ufbt launch; cd ..
systemctl --user start flipper-hub.path flipper-hub.service
```

`ufbt launch` reports a "device disconnected" error after the upload. That is
expected: the app switches USB to two ports, so the port drops for a moment.
Never close a busy app with `loader close` or Back: the loader can freeze. To
stop a stuck Flipper, reboot it (Left + Back for 5 s).

## Collector tab

A second header tab, **Collector**, shows data about the PC, not the Flipper.
The bridge answers these routes itself (it does not forward them to the Flipper):

| Method | Path | Returns |
|---|---|---|
| GET | `/pc/info` | hostname, OS, kernel, CPU, memory, uptime, IPs, active connections |
| GET | `/pc/wifi` | SSID, signal, and the saved password of the connected Wi-Fi |

Left column: this PC. Right column: the connected Wi-Fi (name + password).
The password is hidden behind a **Show** button and never written to the log.
Linux / NetworkManager only for now.

### Wi-Fi password

The saved password needs root to read. By default `/pc/wifi` returns the SSID
with `password: null` and a note. To show it, install the narrow sudo helper
(root-owned path, so the rule cannot be abused):

```bash
cd ~/Desktop/Flipper/flipper_hub
sudo install -m 755 -o root -g root pc/wifi_psk.sh /usr/local/sbin/flipper-hub-wifi-psk
echo "$USER ALL=(root) NOPASSWD: /usr/local/sbin/flipper-hub-wifi-psk" | sudo tee /etc/sudoers.d/flipper-hub-wifi
sudo chmod 440 /etc/sudoers.d/flipper-hub-wifi
systemctl --user restart flipper-hub.service
```

This lets the bridge run exactly one command as root: read one connection's PSK.
To undo: `sudo rm /usr/local/sbin/flipper-hub-wifi-psk /etc/sudoers.d/flipper-hub-wifi`.

## API

The Git routes (`/git/...`, `/api/git/...`) are listed in `docs/git-design.md`.
The basic file API:

| Method | Path | Notes |
|---|---|---|
| GET | `/` | the web page |
| GET | `/api/info` | name, firmware, battery, SD space, free RAM, uptime |
| GET | `/api/list?path=/ext/...` | folder listing |
| GET | `/api/file?path=...[&dl=1]` | read a file (`dl=1` = download) |
| PUT | `/api/file?path=...` | write a file (request body = content) |
| POST | `/api/mkdir?path=...` | make a folder |
| POST | `/api/delete?path=...` | delete a file or an empty folder |

Paths must be inside `/ext` (SD card) or `/int`. The bridge listens on
`127.0.0.1` only, so other machines cannot reach it.

Speed (measured): about 330 KB/s download, 120 KB/s upload.

## Logs

```bash
journalctl --user -u flipper-hub.service -f
```

## Frame format (bridge <-> Flipper)

`0xA5 | type | len_lo | len_hi | payload` with types OPEN 0x01, DATA 0x02,
CLOSE 0x03, PING 0x10, PONG 0x11; payload up to 1024 bytes. The bridge sends
OPEN, the whole request as DATA, then reads DATA until CLOSE.
