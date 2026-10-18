#!/usr/bin/env python3
"""assets/text/<xx>.txt -> per-language u16 text tables (contract sections 3.2, 4, 6).

Usage: textc.py lint [lang ...] | dump <bin> | selftest
       (lint: every enabled language of assets/lang/langs.toml by default; a disabled one can be
        named explicitly)
Library: load, encode, decode, width_px, lint, warnings, build_table, parse_table, is_global.

Text source: `[key]`, then lines; `---` starts a new page; `#` lines are comments. Inside a line
`{player}` / `{rival}` are the name substitutions (codes 0x0001 / 0x0002) and `{v0}`..`{v3}` the
runtime variables (codes 0x0003..0x0006, nucleo/text.c text_set_var / text_set_var_num). Lint width of
a token: `box.name_px` for the names, `var_px[n]` (per language, default [box].var_px) for {vn}.

NTXT v2 (little-endian): "NTXT", u16 n, u8 version = 2, u8 lang (the toml `id`), u32 charset_id
(must equal the font's), u16 offsets[n+1] (BYTES, relative to the strings area), strings = u16 codes
ending 0x0000; 0x000A line break, 0x000C page break. Header = 12 bytes; strings start at
12 + 2 * (n + 1).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkfont  # noqa: E402  (language config, code space, fonts)

ROOT = mkfont.ROOT
END, LF, PAGE = mkfont.END, mkfont.LF, mkfont.PAGE
HDR = 12
TEXT_X = 4          # nucleo/config.h TB_TEXT_X: left margin of the text
MORE_RIGHT_X = 123  # TB_W - TB_MORE_RIGHT: the more-arrow's cell ends here (config.h TB_MORE_RIGHT = 5)


class TextError(Exception):
    pass


def master_path():
    return mkfont.get_lang().text_path


# ---- source file -----------------------------------------------------------------------------
def load(path=None):
    """-> {key: [page, ...]}, page = [line, ...]; insertion-ordered. Default: the master text."""
    path = path or master_path()
    texts = {}
    key = None
    pages = None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            ln = raw.rstrip("\n").rstrip("\r")
            if ln.startswith("#"):
                continue
            if ln.startswith("[") and ln.endswith("]"):
                key = ln[1:-1].strip()
                if not key:
                    raise TextError("%s:%d: empty key" % (path, n))
                if key in texts:
                    raise TextError("%s:%d: duplicate key [%s]" % (path, n, key))
                pages = [[]]
                texts[key] = pages
            elif ln.strip() == "":
                key = None
            elif key is None:
                raise TextError("%s:%d: text outside an entry: %r" % (path, n, ln))
            elif ln.strip() == "---":
                pages.append([])
            else:
                pages[-1].append(ln)
    return texts


def is_global(key):
    return key.startswith("gl.")


# ---- encoding --------------------------------------------------------------------------------
def tokenize(line):
    """-> list of (kind, value): ('c', char) or ('t', control code)."""
    out = []
    i = 0
    while i < len(line):
        if line[i] == "{":
            for tok, code in mkfont.TOKENS.items():
                if line.startswith(tok, i):
                    out.append(("t", code))
                    i += len(tok)
                    break
            else:
                out.append(("c", "{"))
                i += 1
        else:
            out.append(("c", line[i]))
            i += 1
    return out


def encode(s, lang):
    """Unicode line (no page/line breaks) -> list of u16 codes for `lang`."""
    out = []
    for kind, v in tokenize(s):
        if kind == "t":
            out.append(v)
            continue
        code = lang.code_of(v)
        if code is None:
            raise TextError("character %r (U+%04X) has no code in language %s" % (v, ord(v), lang.key))
        out.append(code)
    return out


def decode(codes, lang):
    """u16 codes -> string; LF -> '\\n', page break -> '\\f', names -> {player}/{rival}."""
    names = {c: t for t, c in mkfont.TOKENS.items()}  # 1 {player} 2 {rival} 3..6 {v0}..{v3}
    out = []
    for c in codes:
        if c == LF:
            out.append("\n")
        elif c == PAGE:
            out.append("\f")
        elif c == END:
            break
        elif c in names:
            out.append(names[c])
        else:
            out.append(lang.char_of(c) or "�")
    return "".join(out)


def token_px(code, lang, v3=None):
    """Worst-case pixel width reserved for a name / variable token (`v3`: a wider reserve for {v3})."""
    if v3 is not None and code == mkfont.VAR0 + 3:
        return v3
    if mkfont.VAR0 <= code < mkfont.VAR0 + mkfont.N_VARS:
        return lang.var_px[code - mkfont.VAR0]
    return lang.name_px


def width_px(line, lang, v3=None):
    """Pixel width of a line (name tokens count `box.name_px`, {vn} `var_px[n]`). Raises TextError on unknown chars."""
    w = 0
    for kind, v in tokenize(line):
        if kind == "t":
            w += token_px(v, lang, v3)
        else:
            a = lang.adv_of(v)
            if a is None:
                raise TextError("no glyph for %r (U+%04X)" % (v, ord(v)))
            w += a
    return w


def _names(pages):
    n = {tok: 0 for tok in mkfont.TOKENS}
    for page in pages:
        for line in page:
            for tok in n:
                n[tok] += line.count(tok)
    return {t: c for t, c in n.items() if c}


# ---- lint ------------------------------------------------------------------------------------
# Texts that get substituted into a {vn} / {player} / {rival} slot must fit the width the lint reserves.
# The battle names a trainer through {v3} (class + name, the gl.tr.* texts: <= TR_NAME_CODES codes, nucleo/text.h TEXT_VAR_LEN - 1); the lines
# that name him reserve the widest of them (or a rival name) instead of var_px[3].
TR_NAME_KEYS = ("gl.bt.challenge", "gl.bt.rival_sendout", "gl.bt.win")
TR_NAME_CODES = 15
SLOT_PREFIXES = (("gl.sp.", 0), ("gl.mv.", 1), ("gl.ty.", 1), ("gl.bt.st.", 1), ("gl.name.", None))


def lint(texts, lang, master=None):
    """Per-language lint -> list of error strings (empty when OK).
    Checks: <= max_lines lines per page, every line <= max_px pixels, every char allowed and present
    in the font, entry fits the dialog buffer, limits_px prefixes, key set and name tokens equal the
    master's (when `master` is given)."""
    cfg = lang.cfg
    box = cfg.box
    errs = []
    try:
        missing = set(lang.missing())
    except mkfont.FontError as e:
        return ["font: %s" % e]
    used = lang.used_chars()
    arrow_room = box["max_px"]
    try:  # the blinking more-arrow (U+25BC) sits at the right end of the last line
        g = lang.glyphs().get("\u25bc")
        if g:
            ink = [c for v in g[0] for c in range(lang.cell_w) if (v >> (lang.cell_w - 1 - c)) & 1]
            arrow_room = min(arrow_room, MORE_RIGHT_X - lang.cell_w + min(ink) - TEXT_X)
    except mkfont.FontError:
        pass
    for ch in sorted(missing - used):
        errs.append("font %s has no glyph for %r (U+%04X)" % (lang.key, ch, ord(ch)))
    tr_px = lang.name_px
    for key, pages in texts.items():  # the widest trainer name (and the cap on its length)
        if key.startswith("gl.tr."):
            try:
                tr_px = max([tr_px] + [width_px(l, lang) for page in pages for l in page])
            except TextError:
                pass  # reported below
            n = sum(len(l) for page in pages for l in page)
            if n > TR_NAME_CODES or len(pages) != 1 or len(pages[0]) != 1:
                errs.append("[%s]: a trainer name is one line of at most %d glyphs" % (key, TR_NAME_CODES))
    for key, pages in texts.items():
        ncodes = 0
        for p, page in enumerate(pages, 1):
            where = "[%s] page %d" % (key, p)
            if not page:
                errs.append("%s: empty page" % where)
            if len(page) > box["max_lines"]:
                errs.append("%s: %d lines (max %d)" % (where, len(page), box["max_lines"]))
            ncodes += (1 if p > 1 else 0) + max(0, len(page) - 1)
            for a, b in zip(page, page[1:]):  # kinsoku: hand-broken lines of the same page
                if a and a[-1] in lang.no_end:
                    errs.append("%s: line ends with %r (kinsoku: must not end a line): %r" % (where, a[-1], a))
                if b and b[0] in lang.no_start:
                    errs.append("%s: line starts with %r (kinsoku: must not start a line): %r" % (where, b[0], b))
            for l, line in enumerate(page, 1):
                bad = None
                for kind, v in tokenize(line):
                    if kind == "c" and (ord(v) < 0x20 or ord(v) == 0x7F or not lang.allowed(v)):
                        bad = v
                        break
                    if kind == "c" and v in missing:
                        bad = v
                        break
                if bad is not None:
                    why = "not allowed in %s" % lang.key if not lang.allowed(bad) else "not in the font"
                    errs.append("%s line %d: character %r (U+%04X) %s" % (where, l, bad, ord(bad), why))
                    continue
                ncodes += len(tokenize(line))
                try:
                    w = width_px(line, lang, tr_px if key in TR_NAME_KEYS else None)
                except TextError as e:
                    errs.append("%s line %d: %s" % (where, l, e))
                    continue
                if w > box["max_px"]:
                    errs.append("%s line %d: %d px (max %d): %r" % (where, l, w, box["max_px"], line))
                elif l == 2 and p < len(pages) and w > arrow_room:
                    errs.append("%s line %d: %d px (max %d on a page with a following page: the more-arrow "
                                "sits at the end of the last line): %r" % (where, l, w, arrow_room, line))
                for pre, lim in cfg.limits_px.items():
                    if key.startswith(pre) and w > lim:
                        errs.append("%s line %d: %d px (max %d for %s*): %r" % (where, l, w, lim, pre, line))
        for pre, lim in cfg.limits_codes.items():  # contract 72: a name that travels in a fixed code buffer (the zone banner, an item name in {v0})
            if key.startswith(pre):
                nc = max(len(tokenize(l)) for page in pages for l in page)
                if nc > lim:
                    errs.append("[%s]: %d codes (max %d for %s*)" % (key, nc, lim, pre))
        if ncodes > box["max_codes"]:
            errs.append("[%s]: %d codes (max %d, dialog buffer)" % (key, ncodes, box["max_codes"]))
        for pre, slot in SLOT_PREFIXES:  # substitution sources must fit their slot
            if key.startswith(pre) and not key.startswith("gl.name.ask"):
                lim = lang.name_px if slot is None else lang.var_px[slot]
                try:
                    w = max(width_px(l, lang) for page in pages for l in page)
                except TextError:
                    continue  # reported above
                if w > lim:
                    errs.append("[%s]: %d px does not fit its %s slot (%d px)"
                                % (key, w, "name" if slot is None else "{v%d}" % slot, lim))
    if master is not None:
        for k in master:
            if k not in texts:
                errs.append("missing key [%s] (present in the master %s)" % (k, cfg.master))
        for k in texts:
            if k not in master:
                errs.append("extra key [%s] (not in the master %s)" % (k, cfg.master))
        for k in texts:
            if k in master and _names(texts[k]) != _names(master[k]):
                errs.append("[%s]: name / variable tokens differ from the master (%s vs %s)"
                            % (k, _names(texts[k]), _names(master[k])))
    return errs


def warnings(texts, lang, master):
    """Non-fatal differences against the master (page counts)."""
    out = []
    for k, pages in texts.items():
        if k in master and len(pages) != len(master[k]):
            out.append("[%s]: %d pages, the master has %d" % (k, len(pages), len(master[k])))
    return out


# ---- tables ----------------------------------------------------------------------------------
def encode_entry(pages, lang):
    codes = []
    for p, page in enumerate(pages):
        if p:
            codes.append(PAGE)
        for l, line in enumerate(page):
            if l:
                codes.append(LF)
            codes += encode(line, lang)
    codes.append(END)
    return codes


def build_table(texts, keys, lang):
    """Table for `keys` in order (text id = index). See the module docstring."""
    strings = [encode_entry(texts[k], lang) for k in keys]
    offs, o = [], 0
    for s in strings:
        offs.append(o)
        o += 2 * len(s)
    offs.append(o)
    if o > 0xFFFF:
        raise TextError("text table too large (%d bytes)" % o)
    hdr = b"NTXT" + struct.pack("<HBBI", len(keys), 2, lang.id, lang.charset_id())
    body = b"".join(struct.pack("<%dH" % len(s), *s) for s in strings)
    return hdr + struct.pack("<%dH" % (len(keys) + 1), *offs) + body


def parse_table(data, lang=None):
    """-> list of entries, each a list of pages of lines (decoded strings). The language comes from
    the header byte (or `lang`)."""
    if data[:4] != b"NTXT":
        raise TextError("bad magic")
    n, ver, lid, cs = struct.unpack_from("<HBBI", data, 4)
    if ver != 2:
        raise TextError("unsupported text table version %d" % ver)
    if lang is None:
        cfg = mkfont.load_cfg()
        found = [l for l in cfg.langs.values() if l.id == lid]
        if not found:
            raise TextError("unknown language id %d" % lid)
        lang = found[0]
    if lang.id != lid or lang.charset_id() != cs:
        raise TextError("table does not match language %s (id %d, charset %08x vs %08x)"
                        % (lang.key, lid, cs, lang.charset_id()))
    offs = struct.unpack_from("<%dH" % (n + 1), data, HDR)
    base = HDR + 2 * (n + 1)
    out = []
    for i in range(n):
        raw = data[base + offs[i]:base + offs[i + 1]]
        codes = struct.unpack("<%dH" % (len(raw) // 2), raw)
        if not codes or codes[-1] != END:
            raise TextError("entry %d not terminated" % i)
        s = decode(codes[:-1], lang)
        out.append([p.split("\n") for p in s.split("\f")])
    return out


# ---- CLI -------------------------------------------------------------------------------------
def cmd_lint(names):
    cfg = mkfont.load_cfg()
    langs = [mkfont.get_lang(n, cfg) for n in names] if names else mkfont.enabled_langs(cfg)
    master_l = mkfont.get_lang(None, cfg)
    master = load(master_l.text_path)
    bad = 0
    for l in langs:
        if not os.path.isfile(l.text_path):
            print("ERROR %s: text file %s not found" % (l.key, os.path.relpath(l.text_path, ROOT)))
            bad += 1
            continue
        texts = master if l is master_l else load(l.text_path)
        errs = lint(texts, l, None if l is master_l else master)
        for e in errs:
            print("ERROR %s:" % l.key, e)
        for w in ([] if l is master_l else warnings(texts, l, master)):
            print("WARNING %s:" % l.key, w)
        if errs:
            bad += 1
        else:
            print("textc lint %s: %d entries OK" % (l.key, len(texts)))
    return 1 if bad else 0


def cmd_dump(path):
    for i, pages in enumerate(parse_table(open(path, "rb").read())):
        print("%3d: %s" % (i, "  ---  ".join(" / ".join(p) for p in pages)))
    return 0


def cmd_selftest():
    cfg = mkfont.load_cfg()
    es = mkfont.get_lang("es", cfg)
    # every charset char round-trips through encode/decode
    allch = "".join(es.char_of(c) for c in range(0x20, 0x7F)) + "".join(es.charset()) + "…▼▶"
    assert decode(encode(allch, es), es) == allch
    assert encode("…", es) == [0x81] and encode("▼▶", es) == [0x7F, 0x80]
    assert encode("{player}x{rival}", es) == [1, 0x78, 2] and encode("{x}", es) == [0x7B, 0x78, 0x7D]
    ac = encode("áÑ¿", es)
    assert all(c >= 0x90 for c in ac) and decode(ac, es) == "áÑ¿"
    texts = load()
    assert not lint(texts, es), lint(texts, es)
    keys = [k for k in texts if not is_global(k)]
    gkeys = [k for k in texts if is_global(k)]
    assert keys and gkeys
    for ks in (keys, gkeys):
        tbl = build_table(texts, ks, es)
        assert parse_table(tbl) == [texts[k] for k in ks]
    # exact layout check
    tiny = {"a": [["Hi", "ñ"], ["Z"]]}
    nc = es.code_of("ñ")
    exp = (b"NTXT\x01\x00\x02\x00" + struct.pack("<I", es.charset_id()) + b"\x00\x00\x0e\x00"
           + struct.pack("<7H", 0x48, 0x69, LF, nc, PAGE, 0x5A, 0))
    assert build_table(tiny, ["a"], es) == exp, build_table(tiny, ["a"], es)
    # pixel-width lint
    assert lint({"x": [["a" * 21]]}, es) and lint({"x": [["a", "b", "c"]]}, es) and lint({"x": [["ß"]]}, es)
    assert not lint({"x": [["a" * 20, "b" * 20]]}, es)
    assert lint({"x": [["{player}" + "a" * 14]]}, es)       # 42 + 84 = 126 px
    assert not lint({"x": [["{player}" + "a" * 13]]}, es)   # 120 px
    assert lint({"gl.menu.x": [["a" * 10]]}, es)            # 60 px > 56 (limits_px)
    assert lint({"gl.area.x": [["a" * 20]]}, es) and not lint({"gl.area.x": [["a" * 19]]}, es)   # 121 px / 19 codes (contract 72)
    assert lint({"gl.it.x": [["a" * 16]]}, es) and not lint({"gl.it.x": [["a" * 15]]}, es)       # limits_codes: {v0} holds 15 codes
    # more-arrow room: the last line of a page that has a following page stops short of the arrow
    assert not lint({"x": [["a", "b" * 19], ["c"]]}, es)   # 114 px: ends exactly where the arrow starts
    assert lint({"x": [["a", "b" * 20], ["c"]]}, es) and not lint({"x": [["a", "b" * 20]]}, es)
    # kinsoku (ja / zh): no closing mark at the start of a line, no opening bracket at its end
    ja, zh = mkfont.get_lang("ja", cfg), mkfont.get_lang("zh", cfg)
    assert lint({"x": [["あい", "。あ"]]}, ja) and lint({"x": [["あい「", "あ"]]}, ja) and not lint({"x": [["あい。", "あ"]]}, ja)
    assert lint({"x": [["你好", "，你"]]}, zh) and not lint({"x": [["你好，", "你"]]}, zh)
    assert any("not allowed" in e for e in lint({"x": [["日"]]}, ja))   # ja rejects kanji
    assert width_px("你好A", zh) == 12 + 12 + 8 and width_px("あA", ja) == 8 + 5
    assert lint({"x": [["tab\there"]]}, es)
    # same key set / name tokens as the master
    m = {"a": [["hi {player}"]], "b": [["x"]]}
    assert not lint(m, es, m)
    assert any("missing key" in e for e in lint({"a": [["hi {player}"]]}, es, m))
    assert any("extra key" in e for e in lint({"a": [["hi {player}"]], "b": [["x"]], "c": [["x"]]}, es, m))
    assert any("tokens differ" in e for e in lint({"a": [["hi"]], "b": [["x"]]}, es, m))
    assert any("tokens differ" in e for e in lint({"a": [["hi {v0}"]], "b": [["x"]]}, es, m))
    # variables {v0}..{v3}: codes 3..6, lint width = var_px[n]
    assert encode("{v0}{v1}{v2}{v3}", es) == [3, 4, 5, 6] and decode([3, 4, 5, 6], es) == "{v0}{v1}{v2}{v3}"
    assert width_px("{v0}", es) == 60 and width_px("{v1}", es) == 72 and width_px("{v2}", es) == 24
    assert width_px("{v0}", zh) == 48 and width_px("{v1}", ja) == 56 and width_px("{v2}", ja) == 20
    assert not lint({"x": [["{v1}" + "a" * 8]]}, es) and lint({"x": [["{v1}" + "a" * 9]]}, es)   # 72 + 48 / 54
    assert lint({"gl.sp.x": [["a" * 11]]}, es) and not lint({"gl.sp.x": [["a" * 10]]}, es)         # slot fit
    assert lint({"gl.mv.x": [["あいうえおかきく"]]}, ja) and not lint({"gl.mv.x": [["あいうえおかき"]]}, ja)
    assert not lint({"gl.name.ask_player": [["a" * 20]]}, es)
    print("textc selftest: OK (%d entries, %d local, %d global)" % (len(texts), len(keys), len(gkeys)))
    return 0


def main(argv):
    if len(argv) >= 2 and argv[1] == "lint":
        return cmd_lint(argv[2:])
    if len(argv) >= 3 and argv[1] == "dump":
        return cmd_dump(argv[2])
    if len(argv) >= 2 and argv[1] == "selftest":
        return cmd_selftest()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
