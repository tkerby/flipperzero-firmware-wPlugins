#!/usr/bin/env python3
"""Font + language pipeline (contract sections 3.2 and 4).

  assets/lang/langs.toml  +  glyph sources (ASCII-art .txt / BDF)  +  assets/text/<xx>.txt
    -> nucleo/files/lang/<dir>/font.nfnt   (per enabled language, the subset font)
    -> nucleo/gen/sysfont.h                (built-in 5x10 ASCII + UI glyphs: logo, debug overlays)
    -> nucleo/gen/langs_gen.h              (C language table: id, dir, ASCII name)
    -> build/preview/font_<xx>.png         (glyph grid + sample text boxes, 8x / 4x)

Usage: mkfont.py [lang ...]     (default: every enabled language; naming a disabled
                                             language builds it under build/lang/<dir>/ only)
       mkfont.py selftest
       mkfont.py fixture <dir>     synthetic 12x12 font with 600 pseudo-random "hanzi" -> <dir>/lang/zz/
                                   font.nfnt (> FONT_RAM_MAX: exercises the SD glyph cache on the host)

Library: load_cfg, Lang (code_of, charset, build, build_kb), parse_font, parse_bdf, parse_txt_font.

Naming keyboard (task K): assets/lang/keyboard.toml lists the keys of every language; their characters (and the `hidden` ones and the
suggested names') join the font's charset, and `lang/<dir>/kb.bin` (NKBD v1, see Lang.build_kb) carries the grid pages as font codes
plus the (code, Unicode) pairs of the non-ASCII characters: names are stored as Unicode and converted to the codes of the active language.

Glyph source file `font.txt`: `== <char or U+XXXX>` then exactly h rows of w chars ('.' off,
'#' on); lines starting with '#' outside a glyph are comments.

NFNT v1 (little-endian, glyph rows with the left pixel in bit 7 of the first byte of each row):
  0 "NFNT"  4 u8 ver=1  5 u8 flags (bit0 = per-glyph advance table)  6 u8 cell_w  7 u8 cell_h
  8 u8 adv_narrow  9 u8 adv_wide  10 u8 pitch  11 u8 baseline  12 u16 n_glyphs  14 u16 first_code
  16 u8 bpr  17 u8 bpg  18 u16 narrow_below  20 u32 off_adv  24 u32 off_glyphs  28 u32 charset_id
  glyph of code c = off_glyphs + (c - first_code) * bpg (bpg = bpr * cell_h); adv(c) = table[c-first]
  when flag bit0 else (c < narrow_below ? adv_narrow : adv_wide).
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANGS_TOML = os.path.join(ROOT, "assets", "lang", "langs.toml")
OUT_FILES = os.path.join(ROOT, "nucleo", "files", "lang")
OUT_SYSFONT = os.path.join(ROOT, "nucleo", "gen", "sysfont.h")
OUT_LANGS_H = os.path.join(ROOT, "nucleo", "gen", "langs_gen.h")
OUT_LANG_NAMES_H = os.path.join(ROOT, "nucleo", "gen", "lang_names.h")
KEYBOARD_TOML = os.path.join(ROOT, "assets", "lang", "keyboard.toml")
OUT_PREVIEW = os.path.join(ROOT, "build", "preview")
OUT_TEST = os.path.join(ROOT, "build", "lang")

# ---- code space (contract 4) -----------------------------------------------------------------
END, PLAYER, RIVAL, LF, PAGE = 0x0000, 0x0001, 0x0002, 0x000A, 0x000C
VAR0 = 0x0003  # {v0}..{v3} = 0x0003..0x0006 (TX_VAR0..3): runtime variables (text_set_var)
N_VARS = 4
GLYPH_MORE, GLYPH_CURSOR, GLYPH_ELLIPSIS = 0x7F, 0x80, 0x81
FIRST_CODE = 0x20
FIRST_LANG_CODE = 0x90
UI_CHARS = {"▼": GLYPH_MORE, "▶": GLYPH_CURSOR, "…": GLYPH_ELLIPSIS}  # v > ...
TOKENS = {"{player}": PLAYER, "{rival}": RIVAL}
TOKENS.update({"{v%d}" % i: VAR0 + i for i in range(N_VARS)})
LANG_BYTE = {"es": 0, "en": 1, "ja": 2, "zh": 3}  # documented ids; the toml `id` is authoritative
KB_BLANK, KB_SPACE = "¤", "␣"  # keyboard.toml: an empty cell / the space key
PRESET_KEYS = ["gl.name.p%d" % i for i in range(4)] + ["gl.name.r%d" % i for i in range(4)]  # suggested names


class FontError(Exception):
    pass


# ---- glyph sources --------------------------------------------------------------------------
def parse_header(tok):
    tok = tok.strip()
    if tok.startswith("U+") and len(tok) > 2:
        return chr(int(tok[2:], 16))
    if len(tok) == 1:
        return tok
    raise FontError("bad glyph header %r" % tok)


def parse_txt_font(path, w, h):
    """ASCII-art font -> {char: [row int, ...]} (h rows, bit w-1-x = pixel x)."""
    glyphs = {}
    lines = open(path, encoding="utf-8").read().split("\n")
    i = 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("== "):
            ch = parse_header(ln[3:])
            rows = lines[i + 1:i + 1 + h]
            if len(rows) != h:
                raise FontError("%s: glyph %r: fewer than %d rows" % (path, ch, h))
            vals = []
            for k, r in enumerate(rows):
                if len(r) != w or set(r) - set(".#"):
                    raise FontError("%s: glyph %r row %d bad: %r" % (path, ch, k, r))
                vals.append(int(r.replace(".", "0").replace("#", "1"), 2))
            if ch in glyphs:
                raise FontError("%s: duplicate glyph %r" % (path, ch))
            glyphs[ch] = vals
            i += 1 + h
        else:
            i += 1
    return glyphs


def parse_bdf(path):
    """BDF -> {char: dict(w, h, xoff, yoff, dwidth, rows=[int], )}; rows are w bits wide, left = MSB."""
    out = {}
    cur = None
    in_bitmap = False
    for raw in open(path, encoding="latin-1"):
        ln = raw.strip()
        if in_bitmap:
            if ln == "ENDCHAR":
                in_bitmap = False
                if cur is not None and cur["enc"] >= 0 and len(cur["rows"]) == cur["h"]:
                    out[chr(cur["enc"])] = cur
                cur = None
            elif ln:
                nbits = 4 * len(ln)
                v = int(ln, 16)
                cur["rows"].append(v >> (nbits - cur["w"]) if nbits >= cur["w"] else v << (cur["w"] - nbits))
            continue
        parts = ln.split()
        if not parts:
            continue
        k = parts[0]
        if k == "STARTCHAR":
            cur = {"enc": -1, "w": 0, "h": 0, "xoff": 0, "yoff": 0, "dwidth": 0, "rows": []}
        elif cur is not None and k == "ENCODING":
            cur["enc"] = int(parts[1])
        elif cur is not None and k == "DWIDTH":
            cur["dwidth"] = int(parts[1])
        elif cur is not None and k == "BBX":
            cur["w"], cur["h"], cur["xoff"], cur["yoff"] = (int(p) for p in parts[1:5])
        elif cur is not None and k == "BITMAP":
            in_bitmap = True
    return out


def parse_ranges(specs):
    """["3041-3096", "30FC"] -> [(lo, hi)] (hex, inclusive)."""
    out = []
    for s in specs:
        a, _, b = s.partition("-")
        out.append((int(a, 16), int(b or a, 16)))
    return out


def in_ranges(cp, ranges):
    return any(lo <= cp <= hi for lo, hi in ranges)


# ---- config ---------------------------------------------------------------------------------
class Lang:
    def __init__(self, key, d, cfg):
        self.key = key
        self.cfg = cfg
        self.id = int(d["id"])
        self.enabled = bool(d.get("enabled", False))
        self.dir = d.get("dir", key)
        self.name = d.get("name", key)
        self.text_path = os.path.join(ROOT, d.get("text", "assets/text/%s.txt" % key))
        self.cell_w = int(d["cell_w"])
        self.cell_h = int(d["cell_h"])
        self.adv_narrow = int(d["adv_narrow"])
        self.adv_wide = int(d["adv_wide"])
        self.pitch = int(d["pitch"])
        self.baseline = int(d["baseline"])
        self.narrow_below = int(d.get("narrow_below", FIRST_LANG_CODE))
        self.wrap = d.get("wrap", "word")
        self.type_ms = int(d.get("type_ms", 30))
        self.native_name = d.get("native_name", d.get("name", key))
        self.name_px = int(d["name_px"]) if "name_px" in d else int(cfg.box["name_px"])
        # worst-case width reserved for {v0}..{v3} by the lint (convention: v0 creature name, v1 move /
        # stat / type name, v2 number, v3 spare name)
        self.var_px = [int(v) for v in d.get("var_px", cfg.box["var_px"])]
        if len(self.var_px) != N_VARS:
            raise FontError("lang %s: var_px needs %d values" % (key, N_VARS))
        self.no_start = d.get("no_start", "")
        self.no_end = d.get("no_end", "")
        self.ranges = parse_ranges(d.get("ranges", ["0020-007E"]))
        self.include = parse_ranges(d.get("include", []))
        self.extra = d.get("extra", "")
        self.sources = d.get("font", [])
        if not 1 <= self.cell_w <= 16 or not 1 <= self.cell_h <= 16:
            raise FontError("lang %s: cell must be 1..16 px" % key)
        if self.wrap not in ("word", "kana", "char"):
            raise FontError("lang %s: wrap must be word, kana or char" % key)
        if not 1 <= self.type_ms <= 255:
            raise FontError("lang %s: type_ms must be 1..255" % key)
        if not self.sources:
            raise FontError("lang %s: no [[lang.%s.font]] source" % (key, key))
        self._glyphs = None
        self._chars = None
        self._chars_mtime = None

    # -- characters ---
    def used_chars(self):
        """Non-ASCII characters used by this language's text file (UI chars excluded)."""
        import textc
        if not os.path.isfile(self.text_path):
            return set()
        used = set()
        for pages in textc.load(self.text_path).values():
            for page in pages:
                for line in page:
                    for tok in TOKENS:
                        line = line.replace(tok, "")
                    used.update(c for c in line if ord(c) > 0x7E)
        return {c for c in used if c not in UI_CHARS}

    # -- naming keyboard (keyboard.toml, task K) ---
    def kb(self):
        """The keyboard config of this language (assets/lang/keyboard.toml) or None."""
        return load_kb().get(self.key)

    def kb_grid_chars(self):
        """Non-ASCII characters of the keyboard pages, in page order, without repeats."""
        k = self.kb()
        out = []
        if k:
            for pg in k["page"]:
                for row in pg["rows"]:
                    for ch in row:
                        if ord(ch) > 0x7E and ch not in (KB_BLANK, KB_SPACE) and ch not in out:
                            out.append(ch)
        return out

    def kb_table_chars(self):
        """Every non-ASCII character a stored name can hold in this language: the keys, the `hidden` ones and the characters of
        the suggested names (their order = the order of the kb.bin table)."""
        out = self.kb_grid_chars()
        k = self.kb()
        if k:
            out += [c for c in k.get("hidden", "") if c not in out]
            import textc
            if os.path.isfile(self.text_path):
                texts = textc.load(self.text_path)
                for key in PRESET_KEYS:
                    for page in texts.get(key, []):
                        for line in page:
                            out += [c for c in line if ord(c) > 0x7E and c not in out]
        return out

    def extra_chars(self):
        """Characters that get codes 0x90+ (Unicode order)."""
        s = set(self.used_chars()) | set(self.extra) | set(self.native_name) | set(self.kb_table_chars())
        for lo, hi in self.include:
            s.update(chr(c) for c in range(lo, hi + 1))
        s = {c for c in s if ord(c) > 0x7E and c not in UI_CHARS}
        return sorted(s)

    def charset(self):
        """Cached sorted list of 0x90+ characters (code = 0x90 + index)."""
        key = (os.path.getmtime(self.text_path) if os.path.isfile(self.text_path) else None,
               os.path.getmtime(KEYBOARD_TOML) if os.path.isfile(KEYBOARD_TOML) else None)
        if self._chars is None or self._chars_mtime != key:
            self._chars = self.extra_chars()
            self._chars_mtime = key
            self._cmap = None
        if getattr(self, "_cmap", None) is None:
            self._cmap = {c: FIRST_LANG_CODE + i for i, c in enumerate(self._chars)}
        return self._chars

    def charset_id(self):
        s = "%d:%s" % (self.id, "".join(self.charset()))
        return zlib.crc32(s.encode("utf-8")) & 0xFFFFFFFF

    def code_of(self, ch):
        """Unicode char -> font code, or None when the language has no code for it."""
        o = ord(ch)
        if 0x20 <= o <= 0x7E:
            return o
        if ch in UI_CHARS:
            return UI_CHARS[ch]
        self.charset()
        return self._cmap.get(ch)

    def char_of(self, code):
        if 0x20 <= code <= 0x7E:
            return chr(code)
        for ch, c in UI_CHARS.items():
            if c == code:
                return ch
        self.charset()
        i = code - FIRST_LANG_CODE
        return self._chars[i] if 0 <= i < len(self._chars) else None

    def allowed(self, ch):
        cp = ord(ch)
        return (in_ranges(cp, self.ranges) or in_ranges(cp, self.include) or ch in self.extra
                or ch in UI_CHARS)

    # -- glyphs ---
    def _load_sources(self):
        srcs = []
        for s in self.sources:
            kind = s.get("kind", "txt")
            path = os.path.join(ROOT, s["path"])
            if kind == "txt":
                data = parse_txt_font(path, int(s["w"]), int(s["h"]))
                srcs.append(dict(s, kind=kind, data=data))
            elif kind == "bdf":
                srcs.append(dict(s, kind=kind, data=parse_bdf(path)))
            else:
                raise FontError("lang %s: font kind %r" % (self.key, kind))
        return srcs

    @staticmethod
    def _src_accepts(src, ch):
        cp = ord(ch)
        codes = src.get("codes", "all")
        if codes == "all":
            return True
        if codes == "ascii":
            return 0x20 <= cp <= 0x7E
        if codes == "ui":
            return ch in UI_CHARS
        return in_ranges(cp, parse_ranges(codes))

    def glyphs(self):
        """{char: (cell rows [int, cell_w bits wide], adv)} for ASCII + UI + charset; cached.
        Raises nothing for absent glyphs: see missing()."""
        if self._glyphs is None:
            self._glyphs = self._build_glyphs()
        return self._glyphs

    def _default_adv(self, code):
        return self.adv_narrow if code < self.narrow_below else self.adv_wide

    def _build_glyphs(self):
        srcs = self._load_sources()
        want = [chr(c) for c in range(0x20, 0x7F)] + list(UI_CHARS) + self.charset()
        out = {}
        for ch in want:
            for s in srcs:
                if ch not in s["data"] or not self._src_accepts(s, ch):
                    continue
                g = s["data"][ch]
                sx, sy = int(s.get("x", 0)), int(s.get("y", 0))
                if s["kind"] == "txt":
                    w, h = int(s["w"]), int(s["h"])
                    x0, y0, rows = sx, sy, g
                    adv = int(s["adv"]) if "adv" in s else None
                else:  # bdf: origin on the baseline row
                    w, h = g["w"], g["h"]
                    x0 = sx + g["xoff"]
                    y0 = sy + self.baseline - (h + g["yoff"])
                    rows = g["rows"]
                    adv = g["dwidth"] if s.get("adv", "default") == "table" else None
                if x0 < 0 or y0 < 0 or x0 + w > self.cell_w or y0 + h > self.cell_h:
                    raise FontError("lang %s: glyph %r (%dx%d at %d,%d) does not fit the %dx%d cell"
                                    % (self.key, ch, w, h, x0, y0, self.cell_w, self.cell_h))
                cell = [0] * self.cell_h
                for r, v in enumerate(rows):
                    cell[y0 + r] = v << (self.cell_w - x0 - w)
                code = self.code_of(ch)
                out[ch] = (cell, adv if adv is not None else self._default_adv(code))
                break
        return out

    def missing(self):
        """Characters that need a glyph but no source has one."""
        g = self.glyphs()
        want = [chr(c) for c in range(0x20, 0x7F)] + list(UI_CHARS) + self.charset()
        return [c for c in want if c not in g]

    def adv_of(self, ch):
        """Advance in pixels of a character (None if there is no glyph)."""
        g = self.glyphs().get(ch)
        return g[1] if g else None

    def render_text(self, text):
        """Draws `text` with this language's glyph cells and advances -> (w, h, rows) trimmed to the ink
        (rows = h ints, w bits wide, left pixel = MSB). Raises FontError on a missing glyph."""
        g = self.glyphs()
        pix = set()
        x = 0
        for ch in text:
            if ch not in g:
                raise FontError("lang %s: no glyph for %r (native name)" % (self.key, ch))
            cell, adv = g[ch]
            for r, v in enumerate(cell):
                for c in range(self.cell_w):
                    if (v >> (self.cell_w - 1 - c)) & 1:
                        pix.add((x + c, r))
            x += adv
        if not pix:
            return 0, 0, []
        x0, x1 = min(p[0] for p in pix), max(p[0] for p in pix)
        y0, y1 = min(p[1] for p in pix), max(p[1] for p in pix)
        w = x1 - x0 + 1
        rows = [sum(1 << (w - 1 - (px - x0)) for px in range(x0, x1 + 1) if (px, py) in pix)
                for py in range(y0, y1 + 1)]
        return w, y1 - y0 + 1, rows

    # -- NFNT ---
    @property
    def bpr(self):
        return (self.cell_w + 7) // 8

    @property
    def bpg(self):
        return self.bpr * self.cell_h

    def last_code(self):
        return max(GLYPH_ELLIPSIS, FIRST_LANG_CODE + len(self.charset()) - 1)

    def build(self):
        """-> bytes of font.nfnt. Raises FontError when a glyph is missing."""
        miss = self.missing()
        if miss:
            raise FontError("lang %s: no glyph for %s" % (
                self.key, " ".join("%r(U+%04X)" % (c, ord(c)) for c in miss[:20])
                + (" ..." if len(miss) > 20 else "")))
        n = self.last_code() - FIRST_CODE + 1
        bpr, bpg = self.bpr, self.bpg
        glyph = bytearray(n * bpg)
        advs = [self._default_adv(FIRST_CODE + i) for i in range(n)]
        for ch, (cell, adv) in self.glyphs().items():
            idx = self.code_of(ch) - FIRST_CODE
            b = b"".join((v << (bpr * 8 - self.cell_w)).to_bytes(bpr, "big") for v in cell)
            glyph[idx * bpg:(idx + 1) * bpg] = b
            advs[idx] = adv
        table = any(advs[i] != self._default_adv(FIRST_CODE + i) for i in range(n))
        off_adv = 32 if table else 0
        off_glyphs = 32 + (((n + 3) & ~3) if table else 0)
        hdr = b"NFNT" + struct.pack(
            "<BBBBBBBBHHBBHIII", 1, 1 if table else 0, self.cell_w, self.cell_h, self.adv_narrow,
            self.adv_wide, self.pitch, self.baseline, n, FIRST_CODE, bpr, bpg, self.narrow_below,
            off_adv, off_glyphs, self.charset_id())
        assert len(hdr) == 32, len(hdr)
        out = bytearray(hdr)
        if table:
            out += bytes(advs) + b"\0" * (off_glyphs - 32 - n)
        out += glyph
        return bytes(out)


    def build_kb(self):
        """-> bytes of kb.bin (the naming keyboard), or None when keyboard.toml has no section for this language.

        NKBD v1 (little-endian): 0 "NKBD"  4 u8 ver=1  5 u8 lang id  6 u8 n_pages  7 u8 rows  8 u8 cols  9 u8 flags (bit0 command row
        on top)  10 u8 max_name  11 u8 max_nick  12 u16 n_chars  14 u16 0  16 u32 charset_id; 20: n_chars x (u16 font code, u16 Unicode)
        = the non-ASCII characters of the language's names (keys, hidden, suggested names); then n_pages x rows x cols u16 font codes
        (0 = empty cell, 0x20 = the space key)."""
        k = self.kb()
        if k is None:
            return None
        cols, rows, pages = int(k["cols"]), int(k["rows"]), k["page"]
        if not 1 <= len(pages) <= 8 or not 1 <= cols <= 12 or not 1 <= rows <= 8:
            raise FontError("keyboard %s: pages 1..8, cols 1..12, rows 1..8" % self.key)
        cells = []
        for n, pg in enumerate(pages):
            if len(pg["rows"]) != rows or any(len(r) != cols for r in pg["rows"]):
                raise FontError("keyboard %s page %d: needs %d rows of %d characters" % (self.key, n, rows, cols))
            for row in pg["rows"]:
                for ch in row:
                    if ch == KB_BLANK:
                        cells.append(0)
                    elif ch == KB_SPACE:
                        cells.append(0x20)
                    else:
                        code = self.code_of(ch)
                        if code is None or ord(ch) > 0x7E and ord(ch) > 0xFFFF:
                            raise FontError("keyboard %s: %r has no code" % (self.key, ch))
                        cells.append(code)
        mn, mk = int(k["max_name"]), int(k["max_nick"])
        grid = [ch for pg in pages for row in pg["rows"] for ch in row if ch not in (KB_BLANK, KB_SPACE)]
        wide = max(self.adv_of(ch) or 0 for ch in grid + [" "])
        if mn * wide > self.name_px:
            raise FontError("keyboard %s: max_name %d x %d px = %d > name_px %d (langs.toml)" % (self.key, mn, wide, mn * wide, self.name_px))
        if mk * wide > self.var_px[0]:
            raise FontError("keyboard %s: max_nick %d x %d px = %d > var_px[0] %d (langs.toml)" % (self.key, mk, wide, mk * wide, self.var_px[0]))
        table = self.kb_table_chars()
        if len(table) > 255 * 2:
            raise FontError("keyboard %s: too many characters" % self.key)
        flags = 1 if k["cmd"] == "top" else 0
        if k["cmd"] not in ("top", "bottom"):
            raise FontError("keyboard %s: cmd must be top or bottom" % self.key)
        out = bytearray(b"NKBD" + struct.pack("<BBBBBBBBHHI", 1, self.id, len(pages), rows, cols, flags, mn, mk,
                                              len(table), 0, self.charset_id()))
        for ch in table:
            out += struct.pack("<HH", self.code_of(ch), ord(ch))
        for c in cells:
            out += struct.pack("<H", c)
        return bytes(out)


_kb_cache = {}


def load_kb(path=None):
    """keyboard.toml -> {lang key: section}."""
    path = path or KEYBOARD_TOML
    if not os.path.isfile(path):
        return {}
    st = (path, os.path.getmtime(path))
    if st not in _kb_cache:
        import tomllib
        with open(path, "rb") as f:
            _kb_cache[st] = tomllib.load(f)
    return _kb_cache[st]


class Cfg:
    pass


_cfg_cache = {}


def load_cfg(path=None):
    path = path or os.environ.get("NUCLEO_LANGS") or LANGS_TOML
    st = (path, os.path.getmtime(path))
    if st in _cfg_cache:
        return _cfg_cache[st]
    import tomllib
    with open(path, "rb") as f:
        d = tomllib.load(f)
    c = Cfg()
    c.path = path
    c.master = d.get("master", "es")
    c.box = dict(max_px=120, max_lines=2, max_codes=199, name_px=42, var_px=[60, 72, 24, 60])
    c.box.update(d.get("box", {}))
    c.sysfont = d.get("sysfont", dict(path="assets/font/font.txt", w=5, h=10, adv=6))
    c.limits_px = d.get("limits_px", {})
    c.limits_codes = d.get("limits_codes", {})
    c.langs = {k: Lang(k, v, c) for k, v in d.get("lang", {}).items()}
    if c.master not in c.langs:
        raise FontError("master language %r not defined" % c.master)
    ids = [l.id for l in c.langs.values()]
    if len(set(ids)) != len(ids) or any(not 0 <= i < 256 for i in ids):
        raise FontError("language ids must be unique u8 values")
    dirs = [l.dir for l in c.langs.values()]
    if len(set(dirs)) != len(dirs):
        raise FontError("language dirs must be unique")
    _cfg_cache[st] = c
    return c


def enabled_langs(cfg=None):
    cfg = cfg or load_cfg()
    return sorted((l for l in cfg.langs.values() if l.enabled), key=lambda l: l.id)


def get_lang(key=None, cfg=None):
    cfg = cfg or load_cfg()
    key = key or cfg.master
    if key not in cfg.langs:
        raise FontError("unknown language %r (have: %s)" % (key, ", ".join(cfg.langs)))
    return cfg.langs[key]


# ---- reading a font file back (tests, previews) ----------------------------------------------
class FontFile:
    def __init__(self, data):
        if data[:4] != b"NFNT":
            raise FontError("bad magic")
        (self.ver, self.flags, self.cell_w, self.cell_h, self.adv_narrow, self.adv_wide, self.pitch,
         self.baseline, self.n, self.first, self.bpr, self.bpg, self.narrow_below, self.off_adv,
         self.off_glyphs, self.charset_id) = struct.unpack_from("<BBBBBBBBHHBBHIII", data, 4)
        self.data = data

    def adv(self, code):
        i = code - self.first
        if self.flags & 1:
            return self.data[self.off_adv + i]
        return self.adv_narrow if code < self.narrow_below else self.adv_wide

    def rows(self, code):
        """Pixel rows of a glyph as lists of 0/1."""
        o = self.off_glyphs + (code - self.first) * self.bpg
        out = []
        for r in range(self.cell_h):
            row = self.data[o + r * self.bpr:o + (r + 1) * self.bpr]
            out.append([(row[x >> 3] >> (7 - (x & 7))) & 1 for x in range(self.cell_w)])
        return out


def parse_font(data):
    return FontFile(data)


# ---- sysfont.h ------------------------------------------------------------------------------
def pack_glyph(rows, w):
    """rows: h values of w bits (leftmost pixel = the top bit) -> ceil(w * h / 8) bytes, row r at bit r * w (LSB first)."""
    n = 0
    for r, v in enumerate(rows):
        n |= (v & ((1 << w) - 1)) << (r * w)
    return list(n.to_bytes((w * len(rows) + 7) // 8, "little"))


def emit_sysfont_h(cfg):
    sf = cfg.sysfont
    w, h = int(sf["w"]), int(sf["h"])
    if w > 8:
        raise FontError("sysfont must be <= 8 px wide")
    glyphs = parse_txt_font(os.path.join(ROOT, sf["path"]), w, h)
    last = GLYPH_ELLIPSIS
    out = ["// Generated by tools/mkfont.py from %s - do not edit." % sf["path"],
           "#pragma once", "#include <stdint.h>", "",
           "#define SYSFONT_W %d" % w, "#define SYSFONT_H %d" % h, "#define SYSFONT_ADV %d" % int(sf["adv"]),
           "#define SYSFONT_FIRST 0x20", "#define SYSFONT_LAST 0x%02X" % last, "",
           "#define SYSFONT_BYTES ((SYSFONT_W * SYSFONT_H + 7) / 8)", "",
           "// Built-in font: 0x20-0x7E ASCII, 0x7F more-arrow, 0x80 cursor, 0x81 ellipsis.",
           "// Packed (post-M5 memory reserve, contract 27): SYSFONT_BYTES bytes per glyph; row r is the SYSFONT_W bits at bit r * SYSFONT_W (LSB first) of the",
           "// glyph's bytes, its leftmost pixel in the row's top bit. One spare byte at the end (the unpacker reads a byte past a row).",
           "static const uint8_t sysfont_bits[(SYSFONT_LAST - SYSFONT_FIRST + 1) * SYSFONT_BYTES + 1] = {"]
    for code in range(FIRST_CODE, last + 1):
        ch = chr(code) if code < 0x7F else next(c for c, v in UI_CHARS.items() if v == code)
        if ch not in glyphs:
            raise FontError("sysfont source lacks %r" % ch)
        label = {0x5C: "backslash", 0x7F: "more", 0x80: "cursor", 0x81: "ellipsis"}.get(code, ch)
        out.append("    %s, /* 0x%02X %s */" % (",".join("0x%02X" % b for b in pack_glyph(glyphs[ch], w)), code, label))
    out += ["    0x00,", "};", ""]
    return "\n".join(out)


def emit_langs_h(cfg):
    ls = enabled_langs(cfg)
    if not ls:
        raise FontError("no language is enabled")
    out = ["// Generated by tools/mkfont.py from assets/lang/langs.toml - do not edit.",
           "#pragma once", "#include <stdint.h>", "",
           "// Enabled languages, ascending id. A language index is its position here; the file",
           "// header byte (txt_*.bin `lang`) is LANG_ID[index].",
           "#define LANG_COUNT %d" % len(ls)]
    for i, l in enumerate(ls):
        out.append("#define LANG_IDX_%s %d" % (l.key.upper(), i))
    out += ["static const uint8_t LANG_ID[LANG_COUNT] = {%s};" % ", ".join(str(l.id) for l in ls),
            "static const char* const LANG_DIR[LANG_COUNT] = {%s};" % ", ".join('"%s"' % l.dir for l in ls),
            "// ASCII names for the language picker (system font)",
            "static const char* const LANG_NAME[LANG_COUNT] = {%s};" % ", ".join('"%s"' % l.name for l in ls),
            "// Typewriter speed of the dialog box: ms per glyph (before the player's text-speed setting)",
            "static const uint8_t LANG_TYPE_MS[LANG_COUNT] = {%s};" % ", ".join(str(l.type_ms) for l in ls),
            "// Longest player / rival name and creature nickname, in characters (assets/lang/keyboard.toml)",
            "static const uint8_t LANG_MAX_NAME[LANG_COUNT] = {%s};" % ", ".join(str(int((l.kb() or {}).get("max_name", 7))) for l in ls),
            "static const uint8_t LANG_MAX_NICK[LANG_COUNT] = {%s};" % ", ".join(str(int((l.kb() or {}).get("max_nick", 10))) for l in ls),
            ""]
    return "\n".join(out)


def emit_lang_names_h(cfg):
    """Pre-rendered native names for the language picker / options screen (1 bpp, MSB = left pixel,
    trimmed to the ink, drawn with each language's own font)."""
    ls = enabled_langs(cfg)
    out = ["// Generated by tools/mkfont.py from assets/lang/langs.toml - do not edit.",
           "// Native language names, each drawn with its own language font (language picker, options).",
           "#pragma once", "#include <stdint.h>", "#include \"langs_gen.h\"", "",
           "typedef struct {",
           "    uint8_t w, h;         // ink size in px",
           "    uint8_t bpr;          // bytes per row = (w + 7) / 8",
           "    const uint8_t* rows;  // h rows of bpr bytes, left pixel in bit 7 (ov_bitmap format)",
           "} LangNameBmp;", ""]
    out += ["// ONE definition per module (contract 38.5): the source file that defines LANG_NAMES_DEFINE before including this header holds the data (scene_lang.c); the others read the extern.",
            "extern const LangNameBmp LANG_NAME_BMP[LANG_COUNT];", "#ifdef LANG_NAMES_DEFINE", ""]
    for i, l in enumerate(ls):
        w, h, rows = l.render_text(l.native_name)
        bpr = (w + 7) // 8
        data = b"".join((v << (bpr * 8 - w)).to_bytes(bpr, "big") for v in rows)
        out.append("// %s: %r (%dx%d)" % (l.key, l.native_name, w, h))
        out.append("static const uint8_t LANG_NAME_ROWS_%d[] = {%s};" % (i, ",".join("0x%02X" % b for b in data)))
    out.append("const LangNameBmp LANG_NAME_BMP[LANG_COUNT] = {")
    for i, l in enumerate(ls):
        w, h, rows = l.render_text(l.native_name)
        out.append("    {%d, %d, %d, LANG_NAME_ROWS_%d}," % (w, h, (w + 7) // 8, i))
    out += ["};", "#endif", ""]
    return "\n".join(out)


# ---- preview --------------------------------------------------------------------------------
def render_png(lang, font, path):
    from PIL import Image, ImageDraw
    import textc
    S = 8 if lang.cell_w <= 8 else 5
    cols = 16
    codes = [c for c in range(FIRST_CODE, lang.last_code() + 1) if lang.char_of(c) is not None]
    cw, ch_ = lang.cell_w * S + 16, lang.cell_h * S + 22
    nrows = (len(codes) + cols - 1) // cols
    LCD, INK = (255, 139, 41), (24, 14, 6)
    ps = 4
    boxw, boxh = (lang.cfg.box["max_px"] + 4) * ps, (lang.cfg.box["max_lines"] * lang.pitch + 4) * ps
    texts = textc.load(lang.text_path) if os.path.isfile(lang.text_path) else {}
    boxes = []
    for key in list(texts)[:6]:
        boxes.append(texts[key][0][:2])
    gridw = cols * cw
    width = max(gridw, 2 * boxw + 60)
    nb = (len(boxes) + 1) // 2
    height = nrows * ch_ + 30 + nb * (boxh + 20) + 10
    im = Image.new("RGB", (width, height), (60, 60, 70))
    d = ImageDraw.Draw(im)
    px = im.load()

    def put(code, x0, y0, scale, ink):
        for ry, row in enumerate(font.rows(code)):
            for rx, c in enumerate(row):
                if c:
                    for dy in range(scale):
                        for dx in range(scale):
                            px[x0 + rx * scale + dx, y0 + ry * scale + dy] = ink

    for n, code in enumerate(codes):
        cx, cy = (n % cols) * cw + 8, (n // cols) * ch_ + 4
        d.rectangle([cx - 1, cy - 1, cx + lang.cell_w * S, cy + lang.cell_h * S], fill=(230, 230, 220))
        d.line([cx - 1, cy + lang.baseline * S, cx + lang.cell_w * S, cy + lang.baseline * S], fill=(200, 215, 240))
        put(code, cx, cy, S, (20, 20, 30))
        d.text((cx, cy + lang.cell_h * S + 4), "%02X" % code if code < 0x100 else "%04X" % code, fill=(255, 255, 255))
    y = nrows * ch_ + 30
    for k, lines in enumerate(boxes):
        bx = 20 + (k % 2) * (boxw + 20)
        by = y + (k // 2) * (boxh + 20)
        d.rectangle([bx, by, bx + boxw - 1, by + boxh - 1], fill=LCD)
        d.rectangle([bx, by, bx + boxw - 1, by + boxh - 1], outline=INK, width=ps)
        for li, line in enumerate(lines):
            x = bx + 4 * ps
            for chr_ in textc_tokens(line):
                code = lang.code_of(chr_)
                if code is None or code - font.first >= font.n:
                    continue
                put(code, x, by + (2 + li * lang.pitch) * ps, ps, INK)
                x += font.adv(code) * ps
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path)


def textc_tokens(line):
    for tok in TOKENS:
        line = line.replace(tok, "NNNNNNN")
    return line


# ---- driver ---------------------------------------------------------------------------------
def build_lang(lang, out_dir, preview=True):
    data = lang.build()
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "font.nfnt")
    with open(path, "wb") as f:
        f.write(data)
    ff = parse_font(data)
    kb = lang.build_kb()
    kb_path = os.path.join(out_dir, "kb.bin")
    if kb is not None:
        with open(kb_path, "wb") as f:
            f.write(kb)
    elif os.path.exists(kb_path):
        os.remove(kb_path)
    if preview:
        render_png(lang, ff, os.path.join(OUT_PREVIEW, "font_%s.png" % lang.key))
    return path, ff


def build_fixture(out_dir, n=600):
    """Synthetic big font for the host cache test (random bitmaps, deterministic)."""
    import random
    import tempfile
    d = tempfile.mkdtemp()
    rnd = random.Random(1234)
    bdf = os.path.join(d, "big.bdf")
    with open(bdf, "w") as f:
        f.write("STARTFONT 2.1\nFONT big\nSIZE 12 75 75\nFONTBOUNDINGBOX 12 12 0 -2\nCHARS %d\n" % n)
        for i in range(n):
            f.write("STARTCHAR g%d\nENCODING %d\nDWIDTH 12 0\nBBX 12 12 0 -2\nBITMAP\n" % (i, 0x4E00 + i))
            for _ in range(12):
                f.write("%04X\n" % (rnd.getrandbits(12) << 4))
            f.write("ENDCHAR\n")
        f.write("ENDFONT\n")
    toml = os.path.join(d, "l.toml")
    with open(toml, "w") as f:
        f.write('master = "zz"\n[lang.zz]\nid = 9\nenabled = true\ndir = "zz"\nname = "Fixture"\n'
                'text = "%s/none.txt"\ncell_w = 12\ncell_h = 12\nadv_narrow = 6\nadv_wide = 12\n'
                'pitch = 12\nbaseline = 10\ninclude = ["4E00-%04X"]\n'
                '[[lang.zz.font]]\nkind = "txt"\npath = "assets/font/font.txt"\nw = 5\nh = 10\ny = 1\n'
                '[[lang.zz.font]]\nkind = "bdf"\npath = "%s"\n' % (d, 0x4E00 + n - 1, bdf))
    lang = load_cfg(toml).langs["zz"]
    data = lang.build()
    os.makedirs(os.path.join(out_dir, "lang", "zz"), exist_ok=True)
    with open(os.path.join(out_dir, "lang", "zz", "font.nfnt"), "wb") as f:
        f.write(data)
    return len(data)


def main(argv):
    if len(argv) >= 2 and argv[1] == "selftest":
        return selftest()
    if len(argv) >= 3 and argv[1] == "fixture":
        print("mkfont: fixture font %d B -> %s/lang/zz/font.nfnt" % (build_fixture(argv[2]), argv[2]))
        return 0
    args = [a for a in argv[1:] if not a.startswith("--")]
    cfg = load_cfg()
    langs = [get_lang(a, cfg) for a in args] if args else enabled_langs(cfg)
    try:
        if not args:
            for path, text in ((OUT_SYSFONT, emit_sysfont_h(cfg)), (OUT_LANGS_H, emit_langs_h(cfg)),
                               (OUT_LANG_NAMES_H, emit_lang_names_h(cfg))):
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w") as f:
                    f.write(text)
                print("mkfont: wrote %s" % os.path.relpath(path, ROOT))
        for l in langs:
            out_dir = os.path.join(OUT_FILES if l.enabled else OUT_TEST, l.dir)
            path, ff = build_lang(l, out_dir)
            print("mkfont: %s: %d glyph slots (%d chars + ASCII/UI), cell %dx%d, adv %d/%d, %d B -> %s"
                  % (l.key, ff.n, len(l.charset()), ff.cell_w, ff.cell_h, ff.adv_narrow, ff.adv_wide,
                     os.path.getsize(path), os.path.relpath(path, ROOT)))
    except FontError as e:
        print("mkfont ERROR:", e)
        return 1
    return 0


def selftest():
    import tempfile
    cfg = load_cfg()
    es = get_lang("es", cfg)
    # code space
    assert es.code_of("a") == 0x61 and es.code_of("▼") == 0x7F and es.code_of("▶") == 0x80
    assert es.code_of("…") == 0x81
    cs = es.charset()
    assert cs == sorted(cs) and es.code_of(cs[0]) == 0x90 and es.code_of("ı") is None
    assert all(es.char_of(es.code_of(c)) == c for c in cs)
    # NFNT roundtrip of the es font vs. the ASCII-art source
    ff = parse_font(es.build())
    src = parse_txt_font(os.path.join(ROOT, "assets/font/font_v2.txt"), 5, 11)
    assert (ff.cell_w, ff.cell_h, ff.pitch, ff.baseline) == (5, 11, 11, 9)
    for ch in [chr(c) for c in range(0x20, 0x7F)] + list(UI_CHARS) + cs:
        got = ff.rows(es.code_of(ch))
        want = [[(v >> (4 - x)) & 1 for x in range(5)] for v in src[ch]]
        assert got == want, ch
        assert ff.adv(es.code_of(ch)) == 6
    assert ff.flags == 0 and ff.off_glyphs == 32 and ff.charset_id == es.charset_id()
    # ja / zh: Fusion Pixel glyphs + our own UI glyphs, advance tables, subset charsets
    ja, zh = get_lang("ja", cfg), get_lang("zh", cfg)
    fja, fzh = parse_font(ja.build()), parse_font(zh.build())
    assert (fja.cell_w, fja.cell_h, fja.pitch) == (8, 9, 10) and (fzh.cell_w, fzh.cell_h, fzh.pitch) == (12, 12, 12)
    assert fja.flags & 1 and fzh.flags & 1
    ui8 = parse_txt_font(os.path.join(ROOT, "assets/font/ui_8.txt"), 8, 9)
    ui12 = parse_txt_font(os.path.join(ROOT, "assets/font/ui_12.txt"), 12, 12)
    for ch in UI_CHARS:  # the UI glyphs are ours in every language, not Fusion's
        assert fja.rows(UI_CHARS[ch]) == [[(v >> (7 - x)) & 1 for x in range(8)] for v in ui8[ch]], ch
        assert fzh.rows(UI_CHARS[ch]) == [[(v >> (11 - x)) & 1 for x in range(12)] for v in ui12[ch]], ch
    assert fja.adv(UI_CHARS["▼"]) == 8 and fzh.adv(UI_CHARS["▶"]) == 12
    assert all(ja.code_of(chr(c)) for c in range(0x3041, 0x3094)) and all(ja.code_of(chr(c)) for c in range(0x30A1, 0x30F7))
    assert ja.code_of("日") is None and not ja.allowed("日") and zh.allowed("日")  # ja rejects kanji
    assert fja.adv(ja.code_of("あ")) == 8 and fja.adv(ord("A")) == 5 and fzh.adv(zh.code_of("你")) == 12
    assert fzh.adv(ord("A")) == 8
    assert "中" in zh.charset() and "文" in zh.charset() and zh.code_of("中") >= 0x90
    w, h, rows = es.render_text("Español")
    assert w > 30 and h >= 9 and len(rows) == h and all(0 <= r < (1 << w) for r in rows)
    assert es.render_text("")[0] == 0
    # BDF reader + placement + advance table, on a synthetic 8x8 font
    d = tempfile.mkdtemp()
    bdf = os.path.join(d, "t.bdf")
    open(bdf, "w").write(
        "STARTFONT 2.1\nFONT t\nSIZE 8 75 75\nFONTBOUNDINGBOX 8 8 0 -1\nCHARS 2\n"
        "STARTCHAR a\nENCODING 12354\nSWIDTH 1000 0\nDWIDTH 8 0\nBBX 8 8 0 -1\nBITMAP\n"
        "80\n40\n20\n10\n08\n04\n02\n01\nENDCHAR\n"
        "STARTCHAR b\nENCODING 12356\nSWIDTH 1000 0\nDWIDTH 6 0\nBBX 5 3 1 1\nBITMAP\n"
        "F8\n88\nF8\nENDCHAR\nENDFONT\n")
    g = parse_bdf(bdf)
    assert g["あ"]["rows"] == [0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01]
    assert g["い"]["w"] == 5 and g["い"]["rows"] == [0x1F, 0x11, 0x1F]
    toml = os.path.join(d, "l.toml")
    open(toml, "w").write(
        'master = "xx"\n[lang.xx]\nid = 7\nenabled = true\ndir = "xx"\nname = "Test"\ntext = "%s/none.txt"\n'
        'cell_w = 8\ncell_h = 10\nadv_narrow = 5\nadv_wide = 8\npitch = 10\nbaseline = 8\n'
        'ranges = ["0020-007E"]\ninclude = ["3042-3042", "3044-3044"]\n'
        '[[lang.xx.font]]\nkind = "txt"\npath = "assets/font/font.txt"\nw = 5\nh = 10\n'
        '[[lang.xx.font]]\nkind = "bdf"\npath = "%s"\nadv = "table"\n' % (d, bdf))
    xx = load_cfg(toml).langs["xx"]
    f2 = parse_font(xx.build())
    assert f2.flags == 1 and f2.bpr == 1 and f2.bpg == 10 and f2.cell_w == 8
    ca, cb = xx.code_of("\u3042"), xx.code_of("\u3044")
    assert (ca, cb) == (0x90, 0x91) and f2.adv(ca) == 8 and f2.adv(cb) == 6 and f2.adv(0x41) == 5
    ra = f2.rows(ca)  # bbx 8x8 yoff -1, baseline 8: top row 1
    assert ra[0] == [0] * 8 and ra[1] == [1, 0, 0, 0, 0, 0, 0, 0] and ra[8] == [0, 0, 0, 0, 0, 0, 0, 1]
    rb = f2.rows(cb)  # bbx 5x3 at x 1, yoff 1: rows 4..6
    assert rb[4] == [0, 1, 1, 1, 1, 1, 0, 0] and rb[5] == [0, 1, 0, 0, 0, 1, 0, 0] and rb[3] == [0] * 8
    # a glyph that does not fit the cell is rejected
    bad = load_cfg(toml).langs["xx"]
    bad.cell_h = 9
    bad._glyphs = None
    try:
        bad.build()
        raise AssertionError("oversized glyph accepted")
    except FontError:
        pass
    # the naming keyboard (task K): kb.bin of every language is consistent with its font, es <-> en lose no character, limits fit the lint's tokens
    en = get_lang("en", cfg)
    for l in (es, en, ja, zh):
        kb = l.build_kb()
        assert kb[:4] == b"NKBD" and kb[4] == 1 and kb[5] == l.id
        pages, rows, cols, flags, mn, mk, nch, _, cid = struct.unpack_from("<BBBBBBHHI", kb, 6)
        assert cid == l.charset_id() and len(kb) == 20 + 4 * nch + 2 * pages * rows * cols
        table = {struct.unpack_from("<HH", kb, 20 + 4 * i)[0]: struct.unpack_from("<HH", kb, 20 + 4 * i)[1] for i in range(nch)}
        assert all(l.code_of(chr(u)) == c for c, u in table.items())          # every pair is (the font's code, the character)
        cells = struct.unpack_from("<%dH" % (pages * rows * cols), kb, 20 + 4 * nch)
        assert all(c == 0 or c == 0x20 or 0x21 <= c <= 0x7E or c in table for c in cells)  # every key is ASCII or in the table
        assert all(c <= l.last_code() for c in cells)
        assert mn * 1 <= l.cfg.box["name_px"] and (mn, mk) == (int(l.kb()["max_name"]), int(l.kb()["max_nick"]))
        for key in PRESET_KEYS:                                                  # the suggested names can be stored
            for page in __import__("textc").load(l.text_path).get(key, []):
                assert all(ord(ch) < 0x7F or l.code_of(ch) in table for line in page for ch in line), (l.key, key)
    assert set(es.kb_grid_chars()) <= set(en.kb_table_chars()) and set(en.kb_grid_chars()) <= set(es.kb_table_chars())
    assert [l.kb()["max_name"] for l in (es, en, ja, zh)] == [7, 7, 5, 3] and [l.kb()["max_nick"] for l in (es, en, ja, zh)] == [10, 10, 6, 4]
    print("mkfont selftest: OK (es charset %d chars, font %d B)" % (len(cs), len(es.build())))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
