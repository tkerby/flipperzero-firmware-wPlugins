# Credits and third-party licences

## The game

- Design, programming and art: **Daniel Jahziel** (the in-game credits roll, `gl.c9.cr.design` / `gl.c9.cr.art`).
- Music: original.
- Disclosure: the art was made **with PixelLab** (an AI pixel-art tool). PixelLab is NOT credited as an author: the roll carries a separate
  line, "Hecho con PixelLab" / "Made with PixelLab" / "PixelLabでつくりました" / "由 PixelLab 协助制作" (in the art block).

## Fusion Pixel Font (Japanese kana and Simplified Chinese glyphs shipped in the game)

- Fusion Pixel Font, https://github.com/TakWolf/fusion-pixel-font (release 2026.09.25)
- Copyright (c) 2022, TakWolf (https://takwolf.com)
- Licence: SIL Open Font License, Version 1.1 (https://openfontlicense.org); the font declares no
  Reserved Font Name.
- **Shipped as extracted bitmaps** (a subset, not the font file): `nucleo/files/lang/ja/font.nfnt`
  (Fusion Pixel 8 px proportional, `ja` build: every hiragana / katakana, Japanese punctuation, the
  ASCII glyphs) and `nucleo/files/lang/zh/font.nfnt` (Fusion Pixel 12 px proportional, `zh_hans` build:
  the hanzi used by the game texts, Chinese punctuation, the ASCII glyphs), built by `tools/mkfont.py`
  from `assets/font/src/fusion/` (the two BDF files, vendored). The game's own UI glyphs (more-arrow, cursor, ellipsis) in those
  fonts are original work (`assets/font/ui_8.txt`, `ui_12.txt`); the Fusion arrows are not used.
- The licence texts ship with the app in `nucleo/files/licenses/` (copied from the Fusion Pixel
  download, `OFL.txt` and `LICENSES/`):
  - `fusion-pixel-OFL.txt` - Fusion Pixel Font, Copyright (c) 2022, TakWolf (OFL-1.1)
  - `ark-pixel-OFL.txt` - Ark Pixel Font, Copyright (c) 2021, TakWolf (OFL-1.1) - 12 px build
  - `galmuri-OFL.txt` - Galmuri, Copyright (c) 2019-2025 Lee Minseo (quiple@quiple.dev) (OFL-1.1)
  - `boutique-bitmap-OFL.txt` - Boutique Bitmap 7x7 / 9x9, Copyright (C) 2002-2019 Num Kadoma;
    2026 fancy type foundry (OFL-1.1)
  - `misaki.txt` - Misaki font (the 8x8 dot Japanese font), Copyright (C) 2002-2021 Num Kadoma: its own
    free licence (free use incl. commercial, copying and redistribution, no warranty) - 8 px build
  - `miseki-bitmap-OFL.txt` - Miseki Bitmap, Copyright (c) 2023-2024 Mark Li (OFL-1.1) - 8 px build
  - `cubic-11-OFL.txt` - Cubic 11, Copyright (c) 2005 M+ FONTS PROJECT, (C) 2002-2004 COZ (OFL-1.1)
    - 12 px build
- The in-game credits roll (contract 50, `nucleo/scene_ending.c`, texts `gl.c9.cr.*`) lists the copyright lines above and the other six licences of the Fusion Pixel bundle (a block per font). A modified /
  derived font may not be sold on its own; embedding the extracted glyphs in the game is allowed.

## Latin font (Nucleo)

`assets/font/font.txt` (system font: logo, debug overlays), `assets/font/font_v2.txt` (the Spanish /
English dialog font, 5x11) and `assets/font/ui_8.txt` / `ui_12.txt` (UI glyphs of the ja / zh fonts)
are original hand-drawn work with no third-party font data.
