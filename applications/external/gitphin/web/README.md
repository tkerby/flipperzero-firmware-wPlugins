# Gitphin web page: sources

The Flipper serves one page (`flipper_app/files/index.html`, and its `.gz`). These are its sources.
`pc/tools/build_page.sh` joins them. Do not edit the files in `flipper_app/files/` by hand.

Build: `pc/tools/build_page.sh` (needs python3 and gzip). Then commit `flipper_app/files/index.html` and `index.html.gz`.
`pc/tools/build_page.sh DIR` writes the two files to `DIR` instead (CI uses this to check the committed files).
Test: `pc/tests/test_page.sh`.

## Map

- `index.html`: head and body markup. `@@CSS@@` and `@@JS@@` mark where the bundle puts the CSS and the JS.
- `order.txt`: the one list of all CSS and JS files, in the order they are joined. The page is a classic
  script that uses globals, so the order matters. A new file must be added there.
- `css/`: `theme` and `base` (tokens, reset), `shell` (top bar, side bar), `toasts`, `controls`, `components`,
  `repos`, `code` (code box, diff), `repo_parts`, `markdown`, `discussion`, `dialogs`, `hubstat`, `late`,
  `highlight` (token colors), `avatar`.
- `js/core/`: fetch queue and cache (`core`, `cache`), Flipper status (`hubstat`), toasts and theme, shell, router, boot.
- `js/pixel/`: pixel font data, text, icon sprite, `pixelize`.
- `js/files/`: Files tab (`api`, `files_tab`, `guard` for the system folders), Collector tab.
- `js/repo/`: repo page (view, code tree is in `code_view`, commits, diff, markdown, edit, search, settings,
  `avatar`, `zip`, repo list) and shared helpers and dialogs.
- `js/hub/`: issues, labels, pull requests, releases, wiki, and the doc store they use.
- `js/code/`: syntax highlighter (`hl_langs`, `hl_lines`, `highlight`).

Rules: at most 500 lines per file; one responsibility per file.

## Features ported from gitphinDroid

- Syntax highlighting in the code view: no library, files up to 200 KB, lines up to 2000 characters.
- The Files tab does not delete `/ext/apps_data/gitphin`, `/ext/apps_data/gitphin/git`, `/ext/apps_assets/gitphin`
  (note "Needed by Gitphin") and their parents `/ext`, `/ext/apps_data`, `/ext/apps_assets`. `api()` refuses the call too.
- Repo pictures: file `hub/avatar.png` in the repo folder, moved with `/api/file`, `/api/mkdir`, `/api/delete`.
  The browser shrinks the picture to at most 128x128 and 16 KB.
- Download ZIP on the repo page: `GET /pc/zip?name=<repo>&ref=<ref>`, answered by the PC bridge.
