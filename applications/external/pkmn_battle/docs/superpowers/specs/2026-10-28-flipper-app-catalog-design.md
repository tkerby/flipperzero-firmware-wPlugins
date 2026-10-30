# Flipper Apps Catalog submission design

Date: 2026-10-28  
App: `[GBC] Pokemon Battle` (`pkmn_battle`)  
Target catalog: https://github.com/flipperdevices/flipper-application-catalog

## Goal

Publish this app under the **GPIO** category in the Flipper Apps Catalog so users can install it from Flipper mobile apps and Flipper Lab. The catalog hosts only metadata (`manifest.yml`); source stays in this GitHub repository.

## Context

- Source repo: https://github.com/cycalo/Flipper-Zero-Game-Boy-Pokemon-Battle
- License: MIT (permits catalog binary distribution)
- `application.fam` already defines `appid`, name, category GPIO, version `0.1`, author, icon, description
- Related catalog app: `applications/GPIO/pokemon` (KBEmbedded trading tool)
- Verified: FAP builds cleanly against official firmware **1.5.1** (API **88.2**) and passes the local size gate; no app C code changes required for official SDK

## Approach (approved)

**Official-first dual CI:** default builds and docs target official Flipper `release`; keep an optional Momentum matrix job for compatibility.

Rejected alternatives: official-only (drops Momentum docs/CI), minimal catalog patch with phone JPEGs (fails screenshot rules).

## Source repository changes

### 1. CI (`.github/workflows/build.yml`)

| Job | SDK | Notes |
| --- | --- | --- |
| `fap` / official | default ufbt `release` (no custom index) | Primary; size gate; upload artifacts |
| `fap` / Momentum | Momentum index + `release` | Compat only |
| `host` | unchanged | Existing host tests |

### 2. README firmware wording

- Requirements and build instructions default to official firmware / `python -m ufbt update`
- Note that Momentum remains usable when the FAP API matches the device firmware
- Optional Momentum `ufbt update --index-url=...` kept as a short secondary path

### 3. Catalog metadata files

| File | Purpose |
| --- | --- |
| `changelog.md` | Catalog changelog; start with `v0.1` initial release |
| `README_catalog.md` | Short catalog description (features, hardware, link to full README) |
| `.flipcorg/gallery/*.png` | Unmodified qFlipper screenshots (typically 512×256 PNG), e.g. `1.png`… |

Phone photos in `docs/images/` stay for GitHub README only; they are **not** catalog screenshots.

### 4. Commit gate

Commit and push source-repo prep (including gallery PNGs) before the catalog PR so `commit_sha` points at a complete tree.

## Catalog repository changes

- Fork `flipperdevices/flipper-application-catalog`
- Branch: `cycalo/pkmn_battle_0.1`
- Add `applications/GPIO/pkmn_battle/manifest.yml` (optional short README in the same folder)
- Manifest shape:

```yaml
sourcecode:
  type: git
  location:
    origin: https://github.com/cycalo/Flipper-Zero-Game-Boy-Pokemon-Battle.git
    commit_sha: <sha after source-repo push>
description: "@README_catalog.md"
changelog: "@changelog.md"
screenshots:
  - .flipcorg/gallery/1.png
  - .flipcorg/gallery/2.png
  # additional gallery frames as available
```

- Name, id, category, version, author, icon, short_description remain in `application.fam`
- Validate with `python3 tools/bundle.py --nolint applications/GPIO/pkmn_battle/manifest.yml bundle.zip` before opening the PR
- Fill PR template; AI disclosure: partially AI assisted for catalog packaging, docs, and CI; application logic authored separately

## Execution order

1. Dual CI + README + `changelog.md` + `README_catalog.md`
2. User supplies qFlipper gallery PNGs (hard gate)
3. Commit and push source repo (explicit user approval for the commit)
4. Fork catalog, add manifest, validate, open PR

## Out of scope

- Feature work on the battle app
- Renaming `appid` or rebranding away from Pokémon naming
- Device testing on behalf of catalog reviewers (respond on PR if asked)

## Risks

- Moderators may request live official-firmware testing or trademark wording changes
- Catalog CI builds against official SDK; already verified locally for 1.5.1
- Without valid qFlipper screenshots the catalog PR must not be opened
