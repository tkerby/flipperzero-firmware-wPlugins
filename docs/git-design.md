# Flipper Hub Git – design (phases 1 and 2)

Goal: Gitea-like repository hosting on the Flipper. The Flipper stores bare
repositories on the SD card and serves them; all Git logic that needs real
compute (building packs, merges) runs in the PC-side `git` through a helper.

Rules that do not change:
- All HTTP logic lives in the Flipper app. `pc/bridge.py` stays a byte copier.
- The PC helper is a *client* (like the browser), not a server.
- Repos live at `/ext/git/<name>.git` (normal bare layout). Packs only; the
  helper never writes loose objects.

## Names

- Repo name: `[A-Za-z0-9._-]{1,64}`, must not start with `.`, must not end with
  `.git`. URLs use the name without `.git` except under `/git/`.
- Ref names given to the API are full: `refs/heads/<x>` or `refs/tags/<x>`.
  Reject: `..`, `//`, `@{`, a component starting with `.`, ending with `/`,
  `.` or `.lock`, and any of: space, control chars, `~ ^ : ? * [ \`.
- SHA: 40 lowercase hex. `0000000000000000000000000000000000000000` (ZERO)
  means "does not exist".

## Phase 1 – Flipper routes

All JSON answers use the existing `{"ok":true,...}` / `{"ok":false,"error":"..."}`
shape. Parameters are query parameters (percent-encoded).

| Method | Path | Behaviour |
|---|---|---|
| GET | `/git/<name>.git/<rest>` | Raw file from `/ext/git/<name>.git/<rest>` for the git dumb HTTP protocol. Ignore the query string (`?service=...`). Reject `..` and empty segments. 404 when missing. `text/plain` for `info/refs`, `HEAD`, `objects/info/packs`; `application/octet-stream` otherwise. |
| GET | `/api/git/repos` | `{"ok":true,"repos":[{"name":"x","description":"...","head":"main","packs":2}]}` sorted by name. `head` = branch from `HEAD` (without `refs/heads/`). |
| POST | `/api/git/create?name=&branch=main&description=` | Make the bare skeleton (below). 409 if it exists. `branch` and `description` optional. |
| POST | `/api/git/delete?name=` | Remove the repo folder recursively. |
| POST | `/api/git/rename?name=&to=` | Rename the folder. 409 if `to` exists. |
| POST | `/api/git/describe?name=&text=` | Write `description`. |
| GET | `/api/git/refs?name=` | `{"ok":true,"head":"refs/heads/main","refs":[{"name":"refs/heads/main","sha":"..."}]}`. Loose refs under `refs/heads` and `refs/tags` (recursive, names may contain `/`) plus `packed-refs`; loose wins. Sorted by name. |
| POST | `/api/git/ref?name=&ref=&old=&new=` | Compare-and-swap. `old` must equal the current value (ZERO = must not exist), else 409 `"ref changed"`. `new` = ZERO deletes (loose file and `packed-refs` line). Makes parent folders. Writes `<sha>\n`. Then regenerates `info/refs`. |
| POST | `/api/git/head?name=&branch=` | Write `HEAD` = `ref: refs/heads/<branch>\n`. |
| POST | `/api/git/server-info?name=` | Regenerate `info/refs` and `objects/info/packs`. |

Existing routes are reused by the helper: `PUT /api/file` (upload pack/idx),
`GET /api/list` (list `objects/pack`), `POST /api/delete` (old packs).

Skeleton for create:
```
HEAD                 ref: refs/heads/<branch>\n
config               [core]\n\trepositoryformatversion = 0\n\tfilemode = false\n\tbare = true\n
description          <description or "Unnamed repository">\n
refs/heads/  refs/tags/  objects/pack/  objects/info/  info/
info/refs            (empty)
objects/info/packs   \n
```

Generated files:
- `info/refs`: one line per ref, `<sha>\t<refname>\n`, sorted by refname.
- `objects/info/packs`: `P pack-<hash>.pack\n` per `.pack` file in
  `objects/pack`, then one empty line `\n`.

## Phase 1 – PC helper

`pc/git-remote-flipper` (Python 3 stdlib only). Remote URL: `flipper://<name>`.
Server base URL from env `FLIPPER_HUB_URL` (default `http://127.0.0.1:8080`).
`git flipper ...` is the same script installed as `git-flipper`.

Remote helper protocol (see `man gitremote-helpers`), capabilities
`list`, `fetch`, `push`, `option`:
- `list` / `list for-push`: `<sha> <ref>` per remote ref, `@refs/heads/<b> HEAD`
  when that branch exists, then a blank line. Unknown repo: empty list
  (so the first push can create it).
- `fetch`: for every remote `objects/pack/*.pack` not already present by name in
  the local `$GIT_DIR/objects/pack`, download the `.pack`, build the index
  locally with `git index-pack` (do not trust the remote `.idx`), move both into
  `objects/pack`.
- `push` batch: one `git pack-objects --revs` for the whole batch (new tips, and
  `^<sha>` for every remote ref tip that exists locally). Skip upload when the
  pack is empty. Upload `.pack` then `.idx` with `PUT /api/file`. Then each ref
  by `POST /api/git/ref` (CAS with the remote value read by `list`). Rules:
  branches must fast-forward unless `+`; existing tags need `+`; `:dst` deletes.
  Answers `ok <dst>` / `error <dst> <reason>` (`non-fast-forward`,
  `fetch first`, `already exists`, `stale info`). Then `server-info`.
  Repo missing → create it first (push-to-create). If `HEAD` points to a branch
  that does not exist after the push, set `HEAD` to the first pushed branch.
- Long uploads: no short timeouts (upload is about 120 KB/s); progress on
  stderr unless `option progress false`.

`git flipper` subcommands: `list`, `create <name> [-d text] [-b branch]`,
`delete <name> --yes`, `rename <old> <new>`, `describe <name> <text>`,
`url <name>` (prints both URLs), `gc <name>`.
`gc`: download all packs into a temp bare repo, set the remote refs, `git repack
-a -d`, upload the one new pack + idx, `server-info`, delete the old packs,
`server-info` again.

Clone without the helper (any git, also a Gitea mirror):
`git clone http://127.0.0.1:8080/git/<name>.git`.

## Phase 1b – zero-install push (plain `git push http://...`)

Goal: plug in the Flipper, then normal git works with no helper:
```
git clone http://127.0.0.1:8080/git/<name>.git
git push  http://127.0.0.1:8080/git/<name>.git main
```
How: when `info/refs` is not a smart-HTTP answer, git fetches with the dumb
protocol and pushes with `git http-push`, which uses **WebDAV**. The PC git
does all object work; the Flipper only stores files. So the Flipper implements
the WebDAV subset that `git http-push` uses, under `/git/<name>.git/`:

- `PROPFIND` (Depth 0 and 1): 207 Multi-Status XML; for a folder list its
  children (`D:href`, `D:resourcetype` with `D:collection` for folders,
  `D:getcontentlength`). Must work for `objects/xx/` and the repo root, and
  answer the `supportedlock`/`lockdiscovery` props http-push asks for.
- `MKCOL` (201; 405 if it exists), `PUT` (201/204, streamed to `.part` then
  renamed, same as `/api/file`), `MOVE` (`Destination` header is a full URL;
  `Overwrite`), `DELETE`, `LOCK` (returns `Lock-Token:
  <opaquelocktoken:...>` header and the XML `activelock` body with the token
  and `Timeout: Second-600`), `UNLOCK` (204). One user: locks are tracked
  simply (path + token, in RAM) and expire.
- `OPTIONS` answers `DAV: 1, 2` and `Allow`.
- Push-to-create: `GET /git/<name>.git/info/refs?service=git-receive-pack`
  for a repo that does not exist creates it (default branch `main`), like
  Gitea's push-to-create. A plain GET of a missing repo is still 404.
- After any write under `refs/` or `objects/pack/`, regenerate `info/refs` and
  `objects/info/packs` so the next clone sees the new state.
- http-push uploads **loose objects**. Phase 2's object reader must handle
  loose objects as well as packs. The optional `git flipper gc` (PC helper)
  joins them into one pack later; it is never required.
- Raise the request head limit to 4 KB (MOVE/LOCK headers are long).
- The bridge serialises parallel connections; that is fine.

The PC helper (`flipper://`) stays as an optional extra for faster pack
pushes and `gc`; nothing depends on it.

## Phase 2 – browse on the Flipper

The SDK has no zlib (only heatshrink) and does not export SHA-1. Reading needs
no SHA-1, so phase 2 only adds an inflate decoder of our own.

### Modules (Flipper)

- `hub_inflate.c/.h` – our own RFC 1950/1951 decoder (zlib header, stored,
  fixed and dynamic Huffman blocks; Adler-32 may be skipped). Input comes from a
  read callback (File, 512-byte buffer). Two output modes:
  - into a caller buffer of known size (the buffer is the window), and
  - streamed to a write callback with a 32 KB malloc'd window, for large
    non-delta blobs.
  All tables and state are malloc'd (worker stack is 4 KB). No recursion.
- `hub_gitobj.c/.h` – object reader for one repo:
  - lookup by 20-byte id: loose `objects/xx/<38>` first, then each
    `objects/pack/*.idx` (v2 only: magic `\377tOc`, fanout, binary search
    reading 20 bytes per probe from the file, 4-byte offsets, 8-byte large
    offset table when the MSB is set).
  - pack entry header (type + size varint), `OFS_DELTA` and `REF_DELTA`.
    Resolve delta chains **iteratively**: collect the chain (max depth 64),
    inflate the base, apply each delta into a new buffer, free the old one.
  - Size limit `HUB_GIT_MAX_OBJ` for anything held in RAM (start at 32 KB,
    tune by measured free heap; every malloc failure is a clean error, never a
    crash). Non-delta blobs larger than the limit can still be **streamed**.
  - Parse helpers: commit (tree, parents, author, committer, message), tree
    entries (`<mode> <name>\0<20 bytes>`), annotated tag (object, type, tag,
    tagger, message).
- `hub_git.c` – the routes below.

### Ref resolution (`ref` parameter)

40-hex SHA → as is. Else, in order: `HEAD`, `refs/...` as given,
`refs/heads/<x>`, `refs/tags/<x>`. Annotated tags are peeled to the commit.

### Routes

| Method | Path | Answer |
|---|---|---|
| GET | `/api/git/tree?name=&ref=&path=` | `{"ok":true,"commit":"<sha>","path":"src","entries":[{"name":"a.c","type":"blob","mode":"100644","sha":"..."}]}`. `type`: `tree`, `blob`, `link` (120000), `commit` (submodule 160000). Sorted: trees first, then by name. |
| GET | `/api/git/blob?name=&ref=&path=` or `?name=&sha=` | Raw bytes. `Content-Type` from the extension (text types as `text/plain; charset=utf-8`), `Content-Length`, header `X-Git-Sha`. `&dl=1` adds `Content-Disposition`. 413 `"object too large"` when it is a delta object over the limit. |
| GET | `/api/git/log?name=&ref=&skip=0&n=30` | First-parent walk (like `git log --first-parent`). `{"ok":true,"commits":[C...],"more":true}`; `n` max 50. |
| GET | `/api/git/commit?name=&sha=` | One commit `C` with the full message. |
| GET | `/api/git/refs` (extended) | Each ref also gets `"peeled":"<sha>"` when it is an annotated tag. |

`C` = `{"sha","tree","parents":[...],"author":{"name","email","time","tz"},
"committer":{...},"message"}`. In `log`, `message` is capped at 1 KB; in
`commit` at 16 KB. `time` is Unix seconds, `tz` the raw `+0200` string.

### Web page (Repos tab)

- Ref selector (branches and tags) on Code and Commits.
- Code: breadcrumb path, tree listing (folders first), click a file to view it
  with line numbers, Raw and Download links, binary files (NUL in the first
  8000 bytes) show only the download link. README (`README.md`, `README`,
  `readme.md`...) of the current folder rendered under the listing with a small
  built-in Markdown renderer (headings, lists, code blocks, inline code, links,
  emphasis; all text escaped first).
- Commits: list with subject, author, relative date, short sha, paging with
  "Older". Commit page: full message, author/committer, parents (links), tree
  link ("Browse files").
- "Too large" errors show a clear note, not a broken view.

### Test data

A generated repo (script in `pc/tests/`) with: 200+ commits, nested folders, a
README.md, a binary file, a 200 KB text file edited many times (so it is
stored as deltas), an annotated tag. Measure tree, blob and log times on the
real device and record them in the README.

## Phase 3 – collaboration

Principle: the Flipper does the Git object work and stores documents; the
browser does presentation logic (line diffs, issue lists). Heavy compute that
the Flipper cannot do (true 3-way content merge) is not offered; the page says
"merge locally and push".

### Git routes (Flipper)

| Method | Path | Answer |
|---|---|---|
| GET | `/api/git/diff?name=&sha=` | Commit against its first parent (root commit: against empty tree). |
| GET | `/api/git/diff?name=&base=&head=` | Two refs/commits. |
| | | Both: `{"ok":true,"base":"<sha>","head":"<sha>","files":[{"path":"a/b.c","status":"A\|D\|M\|T","old_sha","new_sha","old_mode","new_mode"}],"truncated":false}`. Tree compare is iterative (explicit stack of path + two tree ids), unchanged subtrees (same id) are skipped. Max 300 files, then `truncated`. No rename detection. |
| GET | `/api/git/compare?name=&base=&head=` | `{"ok":true,"base":"<sha>","head":"<sha>","merge_base":"<sha or null>","ahead":[C...max 50],"ahead_count":N,"behind_count":N,"ff":bool,"capped":bool}`. Walks all parents breadth-first; marks ancestors of `base` (max 2000 ids, sorted array of 20-byte ids, binary search) then walks `head`. `ff` = merge_base equals base tip. Over the cap → `capped:true`, counts are lower bounds. |

Line diffs are computed in the browser (Myers, JS): the page fetches both blobs
with `/api/git/blob?sha=`. Blobs over the size limit or binary show "file
changed (too large / binary)".

### Document store (Flipper)

Issues, pull requests, labels, milestones, releases and wiki pages are JSON or
Markdown documents written by the page. The Flipper stores them with
optimistic concurrency; it does not interpret them.

- Files live in `<repo>.git/hub/<key>` (git ignores this folder; dumb clients
  never request it).
- Key: segments of `[A-Za-z0-9._-]`, separated by `/`, no `..`, no leading
  `.`, max 160 chars, e.g. `issues/12.json`, `wiki/Home.md`.
- ETag = `"<crc32 hex of content>"`.

| Method | Path | Answer |
|---|---|---|
| GET | `/api/git/doc?name=&key=` | Raw content + `ETag`. 404 if missing. |
| PUT | `/api/git/doc?name=&key=` | Body max 64 KB. Needs `If-Match: "<etag>"` (replace) or `If-None-Match: *` (create). Mismatch → 412 `{"ok":false,"error":"changed"}`. Success → `{"ok":true,"etag":"..."}`. Written via `.part` + rename. Makes parent folders. |
| POST | `/api/git/doc-delete?name=&key=` | Needs `If-Match`. |
| GET | `/api/git/docs?name=&prefix=` | `{"ok":true,"keys":[{"key","size"}]}` for files under the prefix folder (one level). |

Document shapes (owned by the page, listed here so all builders agree):
- `meta.json`: `{"next":1,"labels":[{"name","color"}],"milestones":[{"id","title","due","state"}]}`.
  `next` is the shared number for issues and pull requests (like Gitea).
  Allocate a number by CAS on `meta.json`.
- `issues/<n>.json`: `{"n","kind":"issue|pull","title","body","state":"open|closed|merged","labels":[],"milestone":null,"author","created","updated","comments":[{"id","author","body","created","edited"}],"pull":{"base":"main","head":"feature","merged_sha":null,"merged_at":null}}`.
- `issues/index.json`: `{"items":[{"n","kind","title","state","labels","milestone","updated","comments"}]}`,
  updated by CAS after every issue write (the list view reads only this).
- Times are ISO 8601 UTC strings from the browser. `author` is a name the
  user sets once in the page (stored in localStorage; default "me").

### Pull requests

- Open: pick base and head branch, title, body. The page shows commits
  (`compare.ahead`), files and line diffs, and comments.
- Merge: only when `compare.ff` is true (fast-forward). The page updates the
  base ref with `/api/git/ref` CAS (old = base tip, new = head tip), then sets
  `state:"merged"`, `merged_sha`. Optional "delete branch" afterwards.
  When not fast-forward, phase 4's tree merge is tried (if built); otherwise
  the page shows the commands to merge locally and push.
- A closed PR can be reopened. A merged one cannot.

### Page (Repos tab)

Sub-tabs: Code, Commits, Branches, Tags, Issues, Pull requests (with open
counts). Commit page gets the file list and line diffs. Issues: list with
open/closed filter and label filter, new issue, issue page with comments,
labels, milestone, close/reopen, edit. Labels and milestones manager.
Markdown rendering for bodies and comments (same renderer as README).

## Phase 4 – extras

### Writing objects on the Flipper

- Own SHA-1 (the SDK does not export one) and own zlib *writer* using stored
  (uncompressed) deflate blocks plus a correct Adler-32. Objects are written as
  loose objects (`objects/xx/<38>`), via `.part` + rename; skip when present.
- `POST /api/git/commit-file?name=&branch=&old=<tip sha>&path=&message=&author=&email=&action=put|delete[&new_branch=]`
  Body = new file content (max 64 KB for web edits). The Flipper loads the
  tip tree, rebuilds each tree on the path (each tree within the RAM limit),
  writes blob + trees + commit, then CAS-updates the branch (or creates
  `new_branch`). Answers `{"ok":true,"commit":"<sha>"}`; 409 when the tip
  moved. Committer = author; time from the Flipper RTC with `+0000`.
- Optional tree merge for pull requests: `POST /api/git/merge?name=&base=&head=&message=&author=&email=`.
  Entry-level 3-way merge of trees (merge base, base, head): when no path is
  changed on both sides differently, write the merged trees and a merge commit
  with two parents and CAS-update base. Any path changed on both sides → 409
  `{"ok":false,"error":"conflict","paths":[...]}`.

### Page features

- Code view: Edit (textarea), New file, Upload file (≤ 64 KB), Delete file;
  commit message box; "commit directly to <branch>" or "create a new branch".
- Releases: `releases/<id>.json` documents `{"id","tag","title","body","draft","prerelease","created","assets":[{"name","size"}]}`
  plus `releases/index.json`; asset files uploaded with `PUT /api/file` to
  `<repo>.git/hub/releases/<id>/<asset>` and downloaded with `/api/file?dl=1`.
  Creating a release can create a lightweight tag from a branch.
- Wiki: `wiki/<Page>.md` documents plus `wiki/index.json`; page list, view
  (Markdown), edit, new, delete; links `[[Page]]`.
- Search: `GET /api/git/search?name=&ref=&q=&max=50` – iterative tree walk,
  file name match and case-insensitive substring match inside non-binary blobs
  within the size limit, `{"ok":true,"hits":[{"path","line","text"}],"truncated":bool,"scanned":N}`,
  time budget 20 s. The page has a search box in the repo header.
- Repo settings page: description, default branch, loose object count with
  the hint `git flipper gc <name>` when it is high, delete repo.

## Resource limits (rule for all app code)

The Flipper must never hang or crash, whatever the client sends. Every part of
the app follows these limits.

### Time

- **Slices:** the worker never does more than about 3 s of work without
  returning to the accept loop. Long jobs (pack, merge, search, storage scan)
  run in slices and keep the work of each slice.
- **Per request:** each route has a deadline. Over the deadline it stops and
  answers with what it has (`truncated` / `remaining`) or 503 "busy, try again".
- **Background work** (auto-pack) runs only when idle for 10 s, one slice at a
  time, never while a push holds a DAV lock, and pauses for 30 s after a write.
- **No unbounded waits:** every wait (flags, mutex, transport read) has a
  timeout. No `FuriWaitForever` in the worker.

### Memory

- **Heap guard:** every heavy route checks free heap and the largest free
  block first, and answers 503 "low memory" instead of allocating.
- **Guarded allocator:** the firmware halts the device when malloc fails, so all app allocations go through the guarded allocator (`hub_mem.c`), which refuses a request that does not fit the largest free block plus the reserve.
- **Reserve:** the app keeps at least 8 KB of heap free for the system (GUI,
  storage, USB). The guard needs include this reserve.
- **Fixed caps:** object RAM limit 16 KB (diff and commit-file 12 KB, merge 8 KB), compare cap 500,
  request head 4 KB, request body for docs/commits 64 KB, JSON answers built in
  RAM max 24 KB, otherwise streamed.
- **Every malloc result is checked.** Failure gives a clean error, never a
  crash.

### Heap budget per route (measured on device, repo "demo")

"Drop" is the biggest fall of free heap during the route, including the
firmware's own allocations (FuriString, File). Measured on the Flipper with a
tracker build (unlshd-090e). `need` is what `hub_heap_ok` asks for, on top of
the 8 KB reserve. Object-size-dependent routes keep room for a 16 KB object.

| Route | Drop (KB) | need (KB) | Note |
|---|---|---|---|
| `tree` root / `src/core` | 7.0 | 28 | worst case: 16 KB object + list |
| `blob` data/big.txt | 39.5 | 50 | 32 KB inflate window (only for objects > 16 KB) |
| `log` n=30 | 22.0 | 28 | JSON built in RAM |
| `commit` | 5.7 | 40 | message up to 16 KB |
| `diff` of main tip | 8.9 | 32 | two trees at once |
| `compare` main..dev | 21.6 | 28 | |
| `refs` | 26.8 (old: 256-entry array) | 34 | now grows by doubling, starts at 16 entries |
| pack slice (idle, 50 objects) | 46.0 | 58 | + the pack plugin |
| `search` | not measured (crashed the device before the allocator guard) | 45 | |
| `commit-file`, `merge` | not measured | 42, 35 | estimates |

### Plugins

The core (`flipper_hub.fap`, about 43 KB of code) holds the HTTP server, the
transport, the object reader, the repo/ref routes and the browse routes. The
rest is loaded on demand from `/ext/apps_data/flipper_hub/plugins/*.fal`, one
at a time (`hub_plugin.c`): `hub_pl_dav` (WebDAV push), `hub_pl_pack` (pack,
merge), `hub_pl_write` (commit-file, merge), `hub_pl_query` (diff, compare,
search), `hub_pl_doc` (documents). A plugin stays loaded until another is
needed or 5 s pass without use. The load is guarded: the largest free block
must hold the plugin and its job plus the reserve, else 503. WebDAV locks live
in the core so they survive an unload. The core API for plugins is the list in
`hub_api_table.inc`.

### Storage

- **Free space:** writes are refused with 507 "SD card full" when less than
  16 MB would be free afterwards.
- **Caches** (`hub/cache`) are capped (8 MB) and can be deleted at any time.
- **Temp files** (`.part`, `hub/tmp`, `tmp_pack_*`) are removed on error and
  cleaned at app start.
- **Every opened File/dir is closed on every path**, including early exits.

### Deploying and recovery (lessons from two freezes, 2026-10-11)

- **One app launcher at a time.** Stop the bridge (`systemctl --user stop
  flipper-hub.path flipper-hub.service`) before `ufbt launch`, start it again
  after. The bridge's auto-start and `ufbt launch` racing each other froze the
  loader.
- **Never `loader close` (or Back) an app whose worker may be busy.** The app
  joins its worker without a timeout, so the loader waits forever and the whole
  Flipper freezes. Recovery is `power reboot` only.
- **Never repeat a request that froze the device** to debug it. Fix the guard
  or the allocation first, on the host.

### Supervision (self-healing)

- **Heartbeat:** the worker updates a heartbeat tick at least every 3 s (also
  inside slices). The GUI thread checks it. If the worker has not beaten for
  60 s, the screen shows "Worker stuck" and the app logs it.
- **Ping:** the transport answers PING frames between slices, so the bridge
  can tell "busy" from "dead".
- **Bridge recovery:** if a working link has been down for 3 minutes while the
  app port still exists, the bridge reboots the Flipper (`power reboot`). It
  never sends `loader close` (see above). It logs the step. All writes use temp + rename, so a reboot does not
  corrupt repositories.

## Speed layer (commit graph, one open pack, chunked answers)

- **Commit graph** `<repo>/hub/graph.bin` (`hub_gitgraph.c`, pack plugin; reader `hub_git_commit_meta` in the core): 68-byte records (id, committer time, parent1, parent2, flags) sorted by id, 256-entry fanout, pending ids at the end. Compare, merge base, diff?sha and the log skip use it; a miss reads the object and marks the repo stale. Built in idle slices (200 records or the slice clock), any ref change or stale read marks the repo. Missing or damaged file: ignored.
- **Packer**: idle slices append whole `objects/xx` folders to one open `tmp_pack_p` (offset/CRC table in `tmp_pack_t`); the pack is finished (count, SHA-1, idx, renames, loose deletes, each step resumable) when no loose objects are left or it holds 384+ objects. Consolidation starts at 4 small packs.
- **Answers** for log, commit, tree, refs, compare, diff and search are `Transfer-Encoding: chunked` (512 byte chunks, `hub_chunk_*`); an error after the head makes the stream end without the final chunk. Answers addressed by a full 40-hex id carry `Cache-Control: public, max-age=31536000, immutable`, others `no-cache`.
- `log` returns `next_skip` and `more`. Tree listing: folders first, then the rest, each in git order.
- Inflate allocates the stream window from CINFO (2^(CINFO+8)); our deflate writes CINFO 3 (2 KB), stored blocks CINFO 0.
