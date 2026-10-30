# OnkyoRemote

A Flipper Zero `.fap` that controls an Onkyo AV receiver over infrared.

Build and tooling details live in `.claude/skills/flipper-app-dev/SKILL.md` —
read it before touching `application.fam` or calling SDK functions.

## Quick reference

```bash
ufbt            # build -> dist/onkyo_remote.fap
ufbt launch     # build + upload + run over USB
ufbt lint       # must pass before committing
```

Build after every source change; `ufbt` is fast and APPCHK catches API drift.

## Conventions

- Firmware C style, enforced by `ufbt format` / `ufbt lint` and the checked-in
  `.clang-format`. Do not hand-format against it.
- IR codes belong in `onkyo_commands.c`, never inline in the UI code.
- Onkyo command bytes are model-specific. Do not present any code in that table
  as confirmed unless it has been verified against real hardware — say so
  plainly when it has not been.

## Pinned versions

Builds against Flipper SDK `1.4.3`, API `87.1`. `ufbt update` moves these; if it
is run, rebuild and update this note.
