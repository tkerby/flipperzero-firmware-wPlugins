# Changelog

## 1.2

- **Syntax highlighting** in the code view for C, Java/Kotlin, JS/TS, JSON, Python, shell, Go,
  Rust, HTML/XML, CSS, Markdown, YAML/TOML/INI, Makefile and diffs. No libraries, light and dark
  colors with readable contrast.
- **Repo avatars.** Upload a picture for a repo on its page ("Avatar"); it shows in the repo list
  and the header. The browser shrinks it to 128 px first. Stored in the repo folder
  (`hub/avatar.png`), so it moves and goes away with the repo.
- **Download ZIP** of any branch, tag or commit from the repo page (`/pc/zip`, made by the PC
  bridge from the Flipper's data, like `git archive`).
- **System folders are protected** in the Files tab: the repos folder and the app's own folders
  cannot be deleted by mistake.
- **PC bridge extras** (answers to "isn't it a single point of failure?"):
  - **Backup on plug-in:** every repo is mirrored to `~/GitphinBackup/` when the Flipper connects
    (`git flipper backup now|status|on|off|dir`).
  - **Push to both:** `git flipper mirror-add` adds the Flipper as a second push URL, so one
    `git push` updates GitHub and the Flipper.
  - **Webhooks:** a JSON `POST` after each push or repo change, signed with HMAC-SHA256 when a
    secret is set (`git flipper webhook add|list|remove|secret|test`).
- **Code layout:** every source file is now under 500 lines and grouped by area
  (`flipper_app/src/`, `flipper_app/web/`, `pc/gitphin_bridge/`, `pc/gitphin_git/`). The built
  app is byte for byte the same code as 1.1.
- FAQ in the README: what Gitphin is for, and why it is not a single point of failure.
- README: a short demo video and GIF (`tools/promo/make_promo.sh`), and a link to
  [Gitphin Droid](https://github.com/Adhamura/gitphinDroid), the same server and web UI on Android.

## 1.1

- **Repos move to the app's own data folder**, `/ext/apps_data/gitphin/git/<name>.git`, as the
  Flipper Apps Catalog asks. On the first start, Gitphin moves an existing `/ext/git` folder there
  by itself (only when the new folder does not exist yet). The Git URLs do not change.
- **The web file manager works on the SD card only** (`/ext`). The internal flash (`/int`) is no
  longer reachable.
- Author and contact in the app description.

## 1.0

The first public release.

- **Git server on the Flipper Zero.** Repositories are normal bare repos on the SD card
  (`/ext/git/<name>.git`). `git clone`, `fetch` and `pull` use Git's dumb HTTP protocol, `git push`
  its WebDAV fallback, with plain `git` and no helper. The first push to a new name creates the
  repo.
- **Web UI at `http://localhost:8080`.** Repositories, code browser with Markdown READMEs, commit
  history, diffs, compare, branches, tags, search, file edits and uploads, issues with labels and
  milestones, pull requests with fast-forward and tree merge, releases with assets, wiki, a file
  manager for the SD card, light and dark theme.
- **Built for the Flipper's RAM.** A resident core plus five plugins loaded on demand, a guarded
  allocator (`503 low memory, try again` instead of a halt), streamed answers, an SD-backed zlib
  window, temp file + rename for every write.
- **Idle housekeeping.** Packs loose objects, merges small packs and builds a commit graph while
  the Flipper is idle, in slices.
- **PC bridge** (`pc/bridge.py`, Linux): starts the app on plug-in, maps `127.0.0.1:8080` onto the
  USB channel, reboots a Flipper that stopped answering. Optional `git-remote-flipper` helper for
  `flipper://` remotes with one-pack pushes and the `git flipper` CLI.
- **One file to install.** The plugins and the web page are embedded in `gitphin.fap` and unpacked
  on the first start (to `/ext/apps_assets/gitphin/`), so the `.fap` alone is the whole app, ready
  for the Flipper Apps Catalog.
- **New name everywhere: Gitphin.** App ID `gitphin` (was `flipper_hub`): the app is
  `/ext/apps/Tools/gitphin.fap`; systemd units `gitphin.path` / `gitphin.service` and udev rule
  `70-gitphin.rules` (were `flipper-hub.*`); `GITPHIN_URL` for the git helper (`FLIPPER_HUB_URL`
  still works). `install.sh` removes the old units. Upgrading from a pre-release build: delete
  `/ext/apps/Tools/flipper_hub.fap`, `/ext/apps_data/flipper_hub/` and
  `/ext/apps_assets/flipper_hub/` from the SD card; your repos in `/ext/git/` stay as they are.
- **The service works from any folder.** `install.sh` writes the folder it runs from into
  `gitphin.service` (it was fixed to `~/Desktop/Flipper/flipper_hub`). `ufbt_options.py` picks
  the attached Flipper's port instead of one fixed device (`GITPHIN_FLIP_PORT` overrides it).
- **Releases.** MIT license; GitHub Actions builds the app for the official release and dev
  firmware, packs the PC bridge and publishes both with `SHA256SUMS`. README screenshots are made
  by `tools/screenshots/capture.sh` from the app's real route code.
