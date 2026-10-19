# Fusion Pixel Font sources (vendored)

The two BDF files `tools/mkfont.py` needs to build the Japanese (kana, 8 px) and Simplified Chinese (12 px) fonts
(`assets/lang/langs.toml`). Upstream: TakWolf/fusion-pixel-font on GitHub (release 2026.09.25), licence SIL OFL 1.1 (`OFL.txt`,
also shipped in `nucleo/files/licenses/fusion-pixel-OFL.txt`). They live here, and not in the local-only `models_dl/`, so that a fresh
clone or CI can run `tools/build_assets.sh`. The other Fusion Pixel variants used by `tools/font_gate.py` stay in `models_dl/fonts/fusion/`.
