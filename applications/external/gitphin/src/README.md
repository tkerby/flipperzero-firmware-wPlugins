# Gitphin C sources

One folder per responsibility. `application.fam` lists what each build (the
resident core and the five `.fal` plugins) compiles. No file is over 500 lines.

| Folder  | What                                                         | Built into          |
|---------|--------------------------------------------------------------|---------------------|
| `app`   | `gitphin.c`: the screen, the worker, start and exit           | core                |
| `http`  | HTTP server (`hub_http.c`), chunked answers, USB transport, guarded allocator, plugin loader and its API table (`hub_api_table.inc`) | core |
| `git`   | repo routes (`hub_git.c`: names, refs, info files, dumb HTTP, idle packer), browse routes (`hub_gitview.c`) | core |
| `obj`   | object reader (`hub_gitobj.c`: idx, loose, pack, delta, spill and cache, refs, parsing, commit graph), storage I/O (`hub_gitio*`), tree cursor | core |
| `util`  | inflate, SHA-1                                               | core / plugins      |
| `pack`  | packer and pack merge (`hub_gitpack.c`), commit graph writer, deflate | `hub_pl_pack`  |
| `write` | object writer, commit-file and tree merge, diff               | `hub_pl_write` (diff also `hub_pl_query`) |
| `ext`   | the plugin git routes (`hub_gitext.c`), search                | `hub_pl_query`, `hub_pl_write` |
| `doc`   | issues, wiki and other documents; their helpers               | `hub_pl_doc` (helpers also `hub_pl_pack`) |
| `dav`   | WebDAV for `git http-push`                                    | `hub_pl_dav`        |

## Includes

Quoted includes are relative to the including file (`"hub_req.h"` in the same
folder, `"../obj/hub_gitobj.h"` from another one), so no `-I` flag is needed:
uFBT, the host tests (`pc/tests`) and `tools/screenshots` build the files as
they are. `hub_mem.h` stays the last include of every built `.c` file.

## Big files: one unit, several parts

A big module is still one translation unit. Its unit file keeps the original
name (the one in `application.fam`, e.g. `obj/hub_gitobj.c`) and `#include`s
its parts in order:

- `<unit>_int.h`: the limits and types the parts share (internal, only the
  unit and its parts include it);
- `<unit>_<part>.c`: the code, each part one responsibility. A part starts
  with a comment `Part of <unit>.c: included there, in this order, not built
  alone.` Never list a part in `application.fam` or a test script.

Why: uFBT links a `.fap`/`.fal` with `ld -r`, which does not merge equal
strings of different objects, and a function in another object cannot be
inlined. Separate objects made the core 900 bytes bigger after two files.
As one unit, the preprocessed code is token for token the old file, and the
built `.text`, `.rodata` and `.data` are byte for byte the same.

`hub_gitext.c` is built twice: `HUB_EXT_QUERY` (query plugin: `_diff.c`,
`_log.c`) and `HUB_EXT_WRITE` (write plugin: `_write.c`); `_common.c` and
`_dispatch.c` go into both. The `#ifdef`s stay in the unit file.

## Rules

- A new file goes into the folder of its responsibility; add it to the right
  `sources` list (a function that two plugins use must be in a file that both
  lists build, or in the core and in `hub_api_table.inc`).
- A part grows too big: split it into two parts of the same unit.
- Check: `find flipper_app -name '*.[ch]' | xargs wc -l | sort -n | tail`.
