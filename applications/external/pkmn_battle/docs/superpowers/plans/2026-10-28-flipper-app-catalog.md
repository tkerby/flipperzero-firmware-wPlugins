# Flipper Apps Catalog Submission Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare this repository for the Flipper Apps Catalog and open a PR that publishes `pkmn_battle` under GPIO.

**Architecture:** Source repo gains official-first dual CI, catalog metadata files, and qFlipper gallery screenshots. Catalog fork gets only `applications/GPIO/pkmn_battle/manifest.yml` pointing at a fixed source commit SHA. No Flipper C app code changes.

**Tech Stack:** GitHub Actions + `flipperdevices/flipperzero-ufbt-action`, ufbt (official + Momentum SDK indexes), YAML manifest, Markdown catalog docs, `gh` for fork/PR.

## Global Constraints

- App id remains `pkmn_battle`; category remains `GPIO`; version remains `0.1` / `fap_version=(0, 1)`.
- Catalog screenshots must be unmodified qFlipper captures (typically 512×256 PNG); do not use `docs/images/*.jpg`.
- Do not open the catalog PR until `.flipcorg/gallery/` has at least one valid PNG and the source commit containing them is pushed.
- Do not commit unless the user explicitly asks (except where a task step says to ask first).
- Never force-push; never skip git hooks.
- AI PR disclosure: partially AI assisted (catalog packaging, docs, CI); app logic not generated in this plan.

**Spec:** `docs/superpowers/specs/2026-10-28-flipper-app-catalog-design.md`

## File map

| Path | Role |
| --- | --- |
| `.github/workflows/build.yml` | Dual FAP matrix (official + Momentum) + host job |
| `README.md` | Official-first firmware / build docs |
| `changelog.md` | Catalog changelog (`v0.1`) |
| `README_catalog.md` | Short description loaded by catalog manifest |
| `.flipcorg/gallery/*.png` | qFlipper screenshots for catalog |
| `applications/GPIO/pkmn_battle/manifest.yml` | Lives only in catalog fork, not this repo |
| `applications/GPIO/pkmn_battle/README.md` | Optional one-liner in catalog fork |

---

### Task 1: Dual CI for official + Momentum

**Files:**
- Modify: `.github/workflows/build.yml`
- Test: workflow YAML review + (after push) Actions run

**Interfaces:**
- Consumes: existing `tools/size_gate.py`, `dist/` FAP output from ufbt action
- Produces: CI job matrix with `official` (size-gated) and `momentum` (compat)

- [ ] **Step 1: Replace the FAP job with a matrix**

Overwrite `.github/workflows/build.yml` with:

```yaml
name: Build

on:
  push:
  pull_request:
  workflow_dispatch:

jobs:
  fap:
    strategy:
      fail-fast: false
      matrix:
        include:
          - name: official
            sdk-channel: release
            sdk-index-url: ""
            size-gate: true
          - name: momentum
            sdk-channel: release
            sdk-index-url: https://up.momentum-fw.dev/firmware/directory.json
            size-gate: false
    name: FAP (${{ matrix.name }})
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v6
      - name: Fetch gblink submodule
        run: git submodule update --init --depth 1 lib/flipper-gblink
      - name: Build with ufbt (official)
        if: matrix.sdk-index-url == ''
        uses: flipperdevices/flipperzero-ufbt-action@v0.1
        id: build-official
        with:
          sdk-channel: ${{ matrix.sdk-channel }}
      - name: Build with ufbt (Momentum)
        if: matrix.sdk-index-url != ''
        uses: flipperdevices/flipperzero-ufbt-action@v0.1
        id: build-momentum
        with:
          sdk-channel: ${{ matrix.sdk-channel }}
          sdk-index-url: ${{ matrix.sdk-index-url }}
      - name: Size gate (FAP code + data <= 100 KiB)
        if: matrix.size-gate
        run: python3 tools/size_gate.py
      - name: Upload FAP
        uses: actions/upload-artifact@v6
        with:
          name: pkmn_battle-${{ matrix.name }}-${{ steps.build-official.outputs.suffix || steps.build-momentum.outputs.suffix }}
          path: ${{ steps.build-official.outputs.fap-artifacts || steps.build-momentum.outputs.fap-artifacts }}

  host:
    name: Host tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v6
      - name: Fetch SameBoy submodule
        run: git submodule update --init --depth 1 host/third_party/SameBoy
      - name: Install toolchain
        run: sudo apt-get update && sudo apt-get install -y ninja-build clang
      - name: Configure
        run: cmake -G Ninja -B build-host -DCMAKE_C_COMPILER=clang
      - name: Build
        run: ninja -C build-host
      - name: Test
        run: ctest --test-dir build-host --output-on-failure
```

Note: GitHub Actions expression `A || B` for outputs may not work as written. Prefer computing suffix/path in a shell step:

```yaml
      - name: Export build outputs
        id: outs
        run: |
          if [ "${{ matrix.name }}" = "official" ]; then
            echo "suffix=${{ steps.build-official.outputs.suffix }}" >> "$GITHUB_OUTPUT"
            echo "artifacts=${{ steps.build-official.outputs.fap-artifacts }}" >> "$GITHUB_OUTPUT"
          else
            echo "suffix=${{ steps.build-momentum.outputs.suffix }}" >> "$GITHUB_OUTPUT"
            echo "artifacts=${{ steps.build-momentum.outputs.fap-artifacts }}" >> "$GITHUB_OUTPUT"
          fi
      - name: Upload FAP
        uses: actions/upload-artifact@v6
        with:
          name: pkmn_battle-${{ matrix.name }}-${{ steps.outs.outputs.suffix }}
          path: ${{ steps.outs.outputs.artifacts }}
```

Use the shell export pattern in the real file (not the fragile `||` form).

- [ ] **Step 2: Sanity-check YAML locally**

Run: `python -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml'))"`  
Expected: no exception (install `pyyaml` if missing: `pip install pyyaml`).

- [ ] **Step 3: Ask user before committing**

Do not commit yet unless the user asks. Continue to Task 2 so docs land in the same source-prep commit if preferred.

---

### Task 2: Official-first README + catalog docs

**Files:**
- Modify: `README.md` (What you need + Building the Flipper app)
- Create: `changelog.md`
- Create: `README_catalog.md`

**Interfaces:**
- Consumes: existing feature list and hardware notes from `README.md`
- Produces: files referenced by catalog manifest as `@changelog.md` and `@README_catalog.md`

- [ ] **Step 1: Update "What you need" firmware bullet**

In `README.md`, replace the Momentum-only firmware bullet with:

```markdown
- A Flipper Zero on [official firmware](https://flipperzero.one/update) **Release** (or matching
  API). This tree builds against official `release` (verified on 1.5.1 / API 88.2). Custom
  firmware such as [Momentum](https://momentum-fw.dev/) also works when its API matches the FAP.
```

- [ ] **Step 2: Update "Building the Flipper app"**

Replace the build section body so official is default. Final README section should read:

### Building the Flipper app

Requires Python 3 and [ufbt](https://github.com/flipperdevices/flipperzero-ufbt). The SDK is kept
project-local, so a globally installed one is left alone:

    git submodule update --init lib/flipper-gblink
    python -m ufbt dotenv_create
    python -m ufbt update
    python -m ufbt

For Momentum firmware, point ufbt at Momentum's index instead:

    python -m ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
    python -m ufbt

Output: `dist/pkmn_battle.fap`. `python -m ufbt launch` installs and starts it over USB.

Use fenced `bash` code blocks in the real README (same style as the current file), not indented blocks.

- [ ] **Step 3: Create `changelog.md`**

```markdown
v0.1:
Initial release: Cable Club Colosseum link battles for Pokemon Crystal on Flipper Zero.
Random teams, Gym Leaders, Elite Four, Lance, Rival, Red, and Prof. Oak; battle AI;
music/picture overrides; pinout picker; SD battle logs.
```

- [ ] **Step 4: Create `README_catalog.md`**

```markdown
# [GBC] Pokemon Battle

Battle your Pokemon Crystal team against a Flipper Zero. The Flipper acts as the second Game Boy
in a Cable Club Colosseum link battle: connect a Game Boy Color over a link cable, pick an
opponent, and fight in the real game while the Flipper's battle AI chooses moves.

## Features

- Link battles against an unmodified Pokemon Crystal cartridge (US/Europe)
- Random teams in a chosen level range, or Crystal boss teams (Gym Leaders, Elite Four, Lance,
  Rival, Red, Prof. Oak) with held items and optional level scaling
- Battle AI (damage formula, KOs, status, boosts, recovery, switches)
- Battle music and trainer picture follow the opponent, or can be set manually
- View Team with sprites, levels, items, and moves
- Pinout picker for common link-cable adapters
- Battle logs on the SD card for PC replay

## Hardware

- Flipper Zero (official Release firmware recommended; API must match the FAP)
- Game Boy Color with Pokemon Crystal; save must reach a Pokemon Center 2F Cable Club
- Game Boy link cable to Flipper GPIO (for example KBEmbedded Game Link GPIO Module, Original
  pinout)

Full instructions: https://github.com/cycalo/Flipper-Zero-Game-Boy-Pokemon-Battle
```

- [ ] **Step 5: Ask user to capture screenshots**

Tell the user: install the official-build FAP, open qFlipper, capture at least main menu, opponent
select, battle, and view team; save unmodified PNGs as:

- `.flipcorg/gallery/1.png`
- `.flipcorg/gallery/2.png`
- `.flipcorg/gallery/3.png`
- `.flipcorg/gallery/4.png`

Do not resize or recompress. Stop Task 3 until files exist.

---

### Task 3: Gallery screenshots gate

**Files:**
- Create: `.flipcorg/gallery/1.png` (and additional frames)
- Test: inspect dimensions/format

**Interfaces:**
- Consumes: user-provided qFlipper PNGs
- Produces: screenshot paths listed in catalog `manifest.yml`

- [ ] **Step 1: Verify gallery files**

Run:

```bash
mkdir -p .flipcorg/gallery
ls -la .flipcorg/gallery/
python - <<'PY'
from pathlib import Path
from PIL import Image
gallery = Path('.flipcorg/gallery')
pngs = sorted(gallery.glob('*.png'))
assert pngs, 'no PNGs in .flipcorg/gallery'
for p in pngs:
    im = Image.open(p)
    print(p, im.size, im.mode, im.format)
    assert im.format == 'PNG', p
PY
```

Expected: one or more PNG lines; typically size `(512, 256)`. If missing, stop and ask the user.

- [ ] **Step 2: Confirm with user before source commit/push**

Ask: commit CI + docs + gallery and push to `origin/main`? Only proceed if they say yes.

- [ ] **Step 3: Commit and push (after approval)**

```bash
git add .github/workflows/build.yml README.md changelog.md README_catalog.md .flipcorg/gallery
git status
git commit -m "$(cat <<'EOF'
Prepare official-firmware catalog submission metadata.

EOF
)"
git push -u origin HEAD
git rev-parse HEAD
```

Record the printed SHA for Task 4 `commit_sha`.

---

### Task 4: Catalog fork, manifest, validate, PR

**Files:**
- Create (in catalog fork): `applications/GPIO/pkmn_battle/manifest.yml`
- Create (in catalog fork): `applications/GPIO/pkmn_battle/README.md`
- Test: `python3 tools/bundle.py --nolint ...`

**Interfaces:**
- Consumes: pushed source `commit_sha`, `README_catalog.md`, `changelog.md`, gallery paths
- Produces: open GitHub PR against `flipperdevices/flipper-application-catalog`

- [ ] **Step 1: Fork and clone catalog**

```bash
gh repo fork flipperdevices/flipper-application-catalog --clone=true --remote=true
cd flipper-application-catalog
git fetch origin
git checkout -b cycalo/pkmn_battle_0.1 origin/main
```

Work in a sibling directory (for example `e:/Programming/flipper-application-catalog`), not inside the app repo.

- [ ] **Step 2: Add app folder files**

Create `applications/GPIO/pkmn_battle/README.md`:

```markdown
# [GBC] Pokemon Battle

Catalog entry for https://github.com/cycalo/Flipper-Zero-Game-Boy-Pokemon-Battle
```

Create `applications/GPIO/pkmn_battle/manifest.yml` with the **real** SHA from Task 3:

```yaml
sourcecode:
  type: git
  location:
    origin: https://github.com/cycalo/Flipper-Zero-Game-Boy-Pokemon-Battle.git
    commit_sha: REPLACE_WITH_TASK3_SHA
description: "@README_catalog.md"
changelog: "@changelog.md"
screenshots:
  - .flipcorg/gallery/1.png
  - .flipcorg/gallery/2.png
  - .flipcorg/gallery/3.png
  - .flipcorg/gallery/4.png
```

If fewer than four PNGs exist, list only the files that exist. Do not invent paths.

- [ ] **Step 3: Validate manifest**

From the catalog repo root:

```bash
python3 -m venv venv
source venv/bin/activate   # Windows Git Bash: source venv/Scripts/activate
pip install -r tools/requirements.txt
export UFBT_HOME="$PWD/venv/ufbt"
ufbt update
python3 tools/bundle.py --nolint applications/GPIO/pkmn_battle/manifest.yml bundle.zip
```

Expected: exit code 0 and `bundle.zip` created. Fix any reported errors before continuing.

- [ ] **Step 4: Commit, push, open PR**

```bash
git add applications/GPIO/pkmn_battle
git commit -m "$(cat <<'EOF'
Add GPIO pkmn_battle Pokemon Crystal battle app.

EOF
)"
git push -u origin HEAD
gh pr create --repo flipperdevices/flipper-application-catalog --title "Add [GBC] Pokemon Battle (pkmn_battle)" --body "$(cat <<'EOF'
# Application Submission

Cable Club Colosseum link-battle opponent for Pokemon Crystal. The Flipper plays the second Game Boy with a battle AI, random or boss teams, pinout picker, and SD battle logs.

# Extra Requirements

- Game Boy Color + Pokemon Crystal (US/Europe) with Cable Club access
- Game Boy link cable adapter to Flipper GPIO (for example KBEmbedded Game Link module)

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

# AI usage disclosure (Fill this out)

- Partially AI assisted - clarify below which parts were AI assisted and briefly explain what they do.

- Catalog packaging (manifest, changelog, README_catalog), CI dual-SDK workflow updates, and this PR text were AI assisted. Application battle/link/engine code was not generated as part of this submission work.

# Reviewer Checklist (Don't fill this out, and don't remove it from the template)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
EOF
)"
```

- [ ] **Step 5: Return the PR URL to the user**

---

## Plan self-review

1. **Spec coverage:** Dual CI, README wording, changelog, README_catalog, gallery gate, commit/push, catalog fork/manifest/validate/PR, AI disclosure, no C code changes — all have tasks.
2. **Placeholders:** `REPLACE_WITH_TASK3_SHA` is an explicit fill-at-runtime token after Task 3; screenshot list must be trimmed to real files.
3. **Consistency:** App id `pkmn_battle`, category `GPIO`, branch `cycalo/pkmn_battle_0.1`, origin URL match the spec.
