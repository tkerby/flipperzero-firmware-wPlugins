#!/usr/bin/env python3
"""T1 translation self-check. Run: python build/t1/check.py"""
import re, sys, os

D = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "text"
)
LANGS = ["es", "en", "ja", "zh"]
LIM = {"es": 20, "en": 20, "ja": 15, "zh": 10}
errs = []
# Variable tokens ({player} {rival} {v0}..{v3}, see tools/textc.py) count as this many glyph cells in
# the per-line limit (the exact pixel check is `tools/textc.py lint`).
TOK = re.compile(r"\{(player|rival|v[0-3])\}")
TOK_CELLS = {  # token -> cells per language (name_px / var_px of langs.toml divided by the advance)
    "es": dict(player=7, rival=7, v0=10, v1=12, v2=4, v3=10),
    "en": dict(player=7, rival=7, v0=10, v1=12, v2=4, v3=10),
    "ja": dict(player=5, rival=5, v0=6, v1=7, v2=3, v3=6),
    "zh": dict(player=3, rival=3, v0=4, v1=4, v2=2, v3=4),
}


def cells(lang, ln):
    return len(TOK.sub("", ln)) + sum(TOK_CELLS[lang][t] for t in TOK.findall(ln))


def toks(pages):
    return sorted(t for pg in pages for ln in pg for t in TOK.findall(ln))


def parse(lang):
    path = os.path.join(D, lang + ".txt")
    ents, order, cur, page = {}, [], None, None
    for n, raw in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        line = raw.rstrip("\r")
        if line.startswith("#"):
            continue
        m = re.fullmatch(r"\[([^\]]+)\]", line)
        if m:
            cur = m.group(1)
            if cur in ents:
                errs.append(f"{lang}:{n} duplicate key {cur}")
            ents[cur] = [[]]
            order.append(cur)
            continue
        if line == "":
            cur = None
            continue
        if cur is None:
            errs.append(f"{lang}:{n} text outside entry: {line!r}")
            continue
        if line == "---":
            ents[cur].append([])
        else:
            ents[cur][-1].append(line)
    return ents, order


def is_hira(c):
    return "ぁ" <= c <= "ゟ"


def is_kata(c):
    return "゠" <= c <= "ヿ"


JA_PUNCT = set("、。！？「」ー…・ ")
FORBID_START = set("，。！？、…；：）」")
# common traditional-only chars (not exhaustive)
TRAD = set(
    "們這個來為說時間開關點對還與當麼從見讓專體後妳電腦機學習話語長門車東書華師實驗現發場處爲麵務線過進輸導護們鎮隊園"
)

data = {l: parse(l) for l in LANGS}
base_order = data["es"][1]
for l in LANGS:
    if data[l][1] != base_order:
        errs.append(f"{l}: key set/order differs from es")
    for k in base_order:
        if k not in data[l][0]:
            continue
        e, b = data[l][0][k], data["es"][0][k]
        if len(e) != len(b):
            errs.append(f"{l}:{k} pages {len(e)} != es {len(b)}")
        if toks(e) != toks(b):
            errs.append(f"{l}:{k} tokens {toks(e)} != es {toks(b)}")
        for pi, pg in enumerate(e):
            if not 1 <= len(pg) <= 2:
                errs.append(f"{l}:{k} page {pi} has {len(pg)} lines")
            for ln in pg:
                same_ascii = ln.isascii() and all(
                    any(ln in q for q in data[x][0].get(k, [])) for x in LANGS
                )  # C9 / merge: a name or licence line that is the same ASCII in every language (the credits roll measures it in pixels: <= 16 glyphs of the 6-px font fit the 128-px screen)
                if cells(l, ln) > LIM[l] and not (
                    same_ascii and len(ln) <= (19 if k.startswith("gl.c9.cr.") else 16)
                ):
                    errs.append(
                        f"{l}:{k} p{pi} too long ({cells(l, ln)}>{LIM[l]}): {ln}"
                    )
                if ln != ln.strip():
                    errs.append(f"{l}:{k} stray space: {ln!r}")
                raw, ln = ln, TOK.sub("", ln)
                if ln.isascii() and k.startswith("gl.c9.cr.") and "/" in ln:
                    continue  # contract 58: the Ko-fi address of the credits (ko-fi.com/ + the handle): the same ASCII in every language, measured in pixels by the credits roll
                if ln.isascii() and all(
                    data[x][0].get(k) == data["es"][0][k] for x in LANGS
                ):
                    continue  # C9 (contract 50): the font licences of the credits are the same ASCII text in every language
                if "{" in ln or "}" in ln:
                    errs.append(f"{l}:{k} unknown {{token}}: {ln!r}")
                if not ln:
                    continue
                if l == "ja":
                    for c in ln:
                        if re.match(r"[㐀-鿿豈-﫿々]", c):
                            errs.append(f"ja:{k} kanji {c} in {ln}")
                        elif "｡" <= c <= "ﾟ":
                            errs.append(f"ja:{k} half-width kana {c}")
                        elif not (
                            is_hira(c)
                            or is_kata(c)
                            or c in JA_PUNCT
                            or re.match(r"[A-Za-z0-9]", c)
                        ):
                            errs.append(
                                f"ja:{k} unexpected char {c!r} U+{ord(c):04X} in {ln}"
                            )
                if l == "zh":
                    if raw[0] in FORBID_START:
                        errs.append(f"zh:{k} line starts with {raw[0]}: {raw}")
                    for c in ln:
                        if c in TRAD:
                            errs.append(f"zh:{k} traditional char {c}: {ln}")
                        if c in ",.!?":
                            errs.append(f"zh:{k} half-width punct {c}: {ln}")
                    if "..." in ln:
                        errs.append(f"zh:{k} use … not ...: {ln}")
                if l == "en" and "..." in ln:
                    errs.append(f"en:{k} use … not ...: {ln}")
                if any(ord(c) < 0x20 for c in ln):
                    errs.append(f"{l}:{k} control char")

# old generic creature words must be gone (official term: Elementi / Elementis / エレメンティ / 元灵)
OLD = {"es": ["criatura"], "en": ["creature"], "ja": ["クリーチャー"], "zh": ["生物"]}
for l, words in OLD.items():
    txt = open(os.path.join(D, l + ".txt"), encoding="utf-8").read().lower()
    for w in words:
        if w in txt:
            errs.append(f"{l}: old word {w!r} still present")

# table
out = []
for k in base_order:
    out.append(f"[{k}]")
    for l in LANGS:
        pages = data[l][0].get(k, [])
        out.append(f"  {l}: " + "  ||  ".join(" / ".join(p) for p in pages))
    out.append("")
open(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "table.txt"),
    "w",
    encoding="utf-8",
).write("\n".join(out))
if errs:
    print("\n".join(errs))
    print(f"FAIL: {len(errs)} problem(s)")
    sys.exit(1)
print(f"OK: {len(base_order)} keys x {len(LANGS)} languages")
