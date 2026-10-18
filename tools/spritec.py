#!/usr/bin/env python3
"""Sprite bank builder (library + CLI) used by tools/mapc.py.

  spritec.py preview <catalog.toml> <out.png> [--scale 4] [names...] contact sheet (LCD look)
  spritec.py bank <catalog.toml> <out.bin> [names...] write a bank (default: all)
  spritec.py dump <bankfile|map_NN.bin> [-v] decode a bank, print directory
  spritec.py selftest

Library:
  cat = load_catalog(path) -> Catalog (cat.entries: dict name -> Entry)
  data, index = build_bank(cat, names) -> (bytes, {name: bank index}); layout = contract 3.1
  decode_bank(data) -> list of sprite dicts (with frames) for round trips
  load_sprite(entry) -> Sprite (levels/mask frames, anchor, has_mask)

Catalog (assets/catalog.toml), one `[[sprite]]` (or `[sprite.<name>]`) per sprite:
  name, kind = "tile"|"object"|"actor", src, fw, fh (footprint tiles, default 1), cflags
  (list of "solid","grass","water","counter","warp","trigger","ledge", or an int), door_tiles
  (object-relative [[i,j],..]), ndirs (actor 1|4|8, default 8), nframes (actor, default = all
  available), fill (tile diamond fill 0..3, default none), text (default text key; unused here).
  Optional: ax, ay (override anchor), mask = "auto"|true|false, skin (actors default true),
  tlight, tmid, mirror_west (actor: build W/NW/SW from E/NE/SE), crop, fold (default true: see below),
  frames (tile / object, animated, contract 22): `frames = ["a_0.lv.png", "a_1.lv.png", ...]` (2..8, same size) instead of `src`;
  the bank entry has nframes = N, ndirs 1; the engine shows frame (clock / ANIM_MS) % N and the map gets the header flag MAPF_ANIM,
  slide (tile, C4-A, contract 35.3): `slide = "S"|"SE"|"E"|"NE"|"N"|"NW"|"W"|"SW"` = an arrow floor: the cflags get C_SLIDE (128) and the directory flags bits 4..6 the push facing,
  slide_bike (tile, C5, contract 42.6): the same with a facing, but the push only happens while the hero rides the bicycle (the directory flags get bit 7 too),
  hop (actor: bank flag SPRF_HOP = 4, the engine bobs its walks, contract 13.5),
  stream (actor, default true: in a MAP bank an actor of >= STREAM_MIN_BYTES with more than one direction is streamed from the card, SPRF_STREAM = 8,
  contract 3.1; `stream = false` keeps it resident).
  levels = "luma" (default) | "adaptive" (actors with raw PNG frames only), dark_cut = N (adaptive, default 40): the
  typecast. luma = lvlib.levels_from_rgba (skin by colour -> white, else luminance thresholds tlight / tmid): right for
  coloured art. adaptive = lvlib.adaptive_levels: for greyscale art, where no skin colour exists, the lighter-than-dark_cut
  tones of each direction (all its frames pooled, so a walk cycle keeps one tone mapping) are k-means
  clustered into levels 0/1/2 (brightest = opaque white: faces, coats), lum <= dark_cut and the silhouette edge -> 3
  (the same typecast as the Elementi pictures, tools/monc.py). skin / tlight / tmid are ignored in this mode.
  adaptive_split = { north = [0.50, 0.85] } (adaptive only, optional, per direction name): for a facing whose k-means puts
  everything in one level (a back view: hair, jacket and trousers all alike), cut that direction's non-dark luminances
  (> dark_cut, pooled over its frames) at these quantiles instead (lvlib.quantile_tones): lum < q(0.50) -> level 2,
  lum < q(0.85) -> level 1, else level 0. Directions not listed keep the k-means. Keys are the ORDER8 / ORDER4 names.

Folding (contract 3.1, SPRF_MIRROR): an actor whose W / NW / SW (4-dir: W) frames are visually identical to the flipped
E / NE / SE ones is stored without them (flags bit 1) and drawn mirrored at run time. Automatic; `fold = false` in the
catalog entry keeps all directions. `mirror_west = true` makes the art identical on purpose, so it always folds.
`src` is resolved against the catalog folder, then the repo root, then the cwd.
 tile/object: one `.lv.png` (stem without extension also accepted).
 actor: a folder with, per direction name (south, south-east, east, north-east, north, north-west,
  west, south-west; 4-dir uses south, east, north, west), one of <d>/f<i>.lv.png, <d>.lv.png,
  <d>/f<i>.png (raw PixelLab), <d>.png. Raw PNGs are converted with lvlib and a shared bbox
  (ax = w//2, ay = h-1); .lv.png frames must all share one size and keep their json anchor.
"""
import argparse
import os
import re
import struct
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lvlib  # noqa: E402
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KINDS = {"tile": 0, "object": 1, "actor": 2}
FACINGS = ["S", "SE", "E", "NE", "N", "NW", "W", "SW"]
CFLAGS = {"solid": 1, "grass": 2, "water": 4, "counter": 8, "warp": 16, "trigger": 32, "ledge": 64}
ORDER8 = ["south", "south-east", "east", "north-east", "north", "north-west", "west", "south-west"]
ORDER4 = ["south", "east", "north", "west"]
MIRROR = {"west": "east", "north-west": "north-east", "south-west": "south-east"}
BANK_VERSION = 1
BANK_VERSION_STREAM = 2  # a bank with at least one SPRF_STREAM actor (contract 3.1: its pixels are not in the resident part)
ADAPTIVE_DARK_CUT = 40  # default dark_cut of levels = "adaptive" actors (greyscale PixelLab v3 art)


class CatalogError(Exception):
    pass


def _cflags(v):
    if isinstance(v, int):
        return v
    out = 0
    for s in v or []:
        if s not in CFLAGS:
            raise CatalogError(f"unknown cflag {s!r} (known: {', '.join(CFLAGS)})")
        out |= CFLAGS[s]
    return out


class Entry:
    def __init__(self, d, cat):
        self.raw = d
        self.cat = cat
        self.name = d.get("name")
        if not self.name:
            raise CatalogError(f"catalog entry without name: {d}")
        self.kind = d.get("kind")
        if self.kind not in KINDS:
            raise CatalogError(f"{self.name}: kind must be tile|object|actor, got {self.kind!r}")
        self.src = d.get("src")
        self.frames = d.get("frames")   # tile / object: an animated sprite, `frames = ["a_0.lv.png", ...]` instead of src (contract 22)
        if self.frames is not None:
            if self.kind == "actor" or self.src or not isinstance(self.frames, list) or not 2 <= len(self.frames) <= 8 \
                    or not all(isinstance(x, str) for x in self.frames):
                raise CatalogError(f"{self.name}: frames = [2..8 paths] is for tiles / objects and replaces src")
            self.src = self.frames[0]
        if not self.src:
            raise CatalogError(f"{self.name}: missing src")
        self.fw, self.fh = int(d.get("fw", 1)), int(d.get("fh", 1))
        self.cflags = _cflags(d.get("cflags", []))
        self.slide = None       # C4-A (contract 35.3): `slide = "<facing>"` on a TILE = an arrow floor: C_SLIDE in cflags + the facing in the directory flags bits 4..6
        self.slide_bike = False  # C5 (contract 42.6): `slide_bike = "<facing>"` = a bike slope (directory flags bit 7 SPRF_SLIDE_BIKE): pushes only a rider
        for key in ("slide", "slide_bike"):
            if key in d:
                if self.kind != "tile" or d[key] not in FACINGS or ("slide" in d and "slide_bike" in d):
                    raise CatalogError(f"{self.name}: {key} = one of {' '.join(FACINGS)} (tiles only, not both)")
                self.slide = FACINGS.index(d[key])
                self.slide_bike = key == "slide_bike"
                self.cflags |= 128
        self.door_tiles = [tuple(t) for t in d.get("door_tiles", [])]
        self.ndirs = int(d.get("ndirs", 8 if self.kind == "actor" else 1))
        self.nframes = int(d["nframes"]) if "nframes" in d else None
        self.fill = d.get("fill")
        if self.fill is not None and not (isinstance(self.fill, int) and 0 <= self.fill <= 3):
            raise CatalogError(f"{self.name}: fill must be 0..3")
        self.text = d.get("text")
        if self.ndirs not in (1, 4, 8):
            raise CatalogError(f"{self.name}: ndirs must be 1, 4 or 8")
        if self.kind != "actor" and self.ndirs != 1:
            raise CatalogError(f"{self.name}: only actors may have ndirs != 1")
        if not (1 <= self.fw <= 255 and 1 <= self.fh <= 255):
            raise CatalogError(f"{self.name}: bad footprint")

    @property
    def path(self):
        return self.cat.resolve(self.src)


class Catalog:
    def __init__(self, path, extra=None):
        self.path = os.path.abspath(path)
        self.dir = os.path.dirname(self.path)
        with open(path, "rb") as f:
            doc = tomllib.load(f)
        raw = []
        sp = doc.get("sprite", doc.get("sprites", []))
        if isinstance(sp, dict):
            raw = [dict(v, name=k) for k, v in sp.items()]
        else:
            raw = list(sp)
        for ex in ([extra] if isinstance(extra, (str, os.PathLike)) else list(extra or [])):   # more files of [[sprite]] entries (maps/test_catalog.toml: the sprites of the test maps; assets/catalog_roam.toml, contract 55), paths as in the main one
            with open(ex, "rb") as f:
                raw += list(tomllib.load(f).get("sprite", []))
        self.entries = {}
        for d in raw:
            e = Entry(d, self)
            if e.name in self.entries:
                raise CatalogError(f"duplicate sprite name {e.name!r}")
            self.entries[e.name] = e
        self._sprites = {}

    def resolve(self, src):
        if os.path.isabs(src):
            return src
        for base in (self.dir, REPO, os.getcwd()):
            p = os.path.join(base, src)
            if os.path.exists(p) or os.path.exists(p + ".lv.png"):
                return p
        return os.path.join(self.dir, src)

    def entry(self, name):
        try:
            return self.entries[name]
        except KeyError:
            raise CatalogError(f"sprite {name!r} not in catalog {self.path}") from None

    def sprite(self, name):
        if name not in self._sprites:
            self._sprites[name] = load_sprite(self.entry(name))
        return self._sprites[name]


def load_catalog(path, extra=None):
    return Catalog(path, extra)


# ------------------------------------------------------------------ sprites
class Sprite:
    def __init__(self, entry, w, h, ax, ay, frames, has_mask):
        self.entry, self.w, self.h, self.ax, self.ay = entry, w, h, ax, ay
        self.frames = frames  # [dir][frame] = (levels, mask)
        self.has_mask = has_mask
        self.ndirs, self.nframes = len(frames), len(frames[0])


def _num_key(p):
    m = re.findall(r"\d+", os.path.basename(p))
    return (int(m[-1]) if m else 0, p)


def _find_frames(folder, name):
    """-> (mode, [paths]) mode 'lv' or 'raw'; or (None, [])."""
    sub = os.path.join(folder, name)
    if os.path.isdir(sub):
        fs = [os.path.join(sub, f) for f in os.listdir(sub) if f.endswith(".lv.png")]
        if fs:
            return "lv", sorted(fs, key=_num_key)
    if os.path.exists(os.path.join(folder, name + ".lv.png")):
        return "lv", [os.path.join(folder, name + ".lv.png")]
    if os.path.isdir(sub):
        fs = [os.path.join(sub, f) for f in os.listdir(sub) if f.endswith(".png") and not f.endswith(".lv.png")]
        if fs:
            return "raw", sorted(fs, key=_num_key)
    if os.path.exists(os.path.join(folder, name + ".png")):
        return "raw", [os.path.join(folder, name + ".png")]
    return None, []


def _adaptive_cut(entry):
    """dark_cut when the actor uses `levels = "adaptive"`, else None."""
    lv = entry.raw.get("levels", "luma")
    if lv == "luma":
        return None
    if lv != "adaptive":
        raise CatalogError(f"{entry.name}: levels must be \"luma\" or \"adaptive\", got {lv!r}")
    if entry.kind != "actor":
        raise CatalogError(f"{entry.name}: levels = \"adaptive\" is for actors only")
    return int(entry.raw.get("dark_cut", ADAPTIVE_DARK_CUT))


def _adaptive_splits(entry):
    """{direction: [p_dark, p_mid]} from the optional `adaptive_split` of an adaptive actor ({} when absent)."""
    sp = entry.raw.get("adaptive_split", {})
    if not sp:
        return {}
    if entry.raw.get("levels", "luma") != "adaptive":
        raise CatalogError(f"{entry.name}: adaptive_split needs levels = \"adaptive\"")
    out = {}
    for k, v in sp.items():
        if k not in ORDER8:
            raise CatalogError(f"{entry.name}: adaptive_split: unknown direction {k!r}")
        if not (isinstance(v, list) and len(v) == 2 and all(isinstance(x, (int, float)) for x in v)
                and 0 < v[0] < v[1] < 1):
            raise CatalogError(f"{entry.name}: adaptive_split.{k} must be [p1, p2] with 0 < p1 < p2 < 1, got {v!r}")
        out[k] = [float(x) for x in v]
    return out


def _finish_mask(entry, frames):
    mode = entry.raw.get("mask", "auto")
    need = any(m and not l for g in frames for lv, mk in g for lr, mr in zip(lv, mk) for l, m in zip(lr, mr))
    if mode == "auto":
        return need
    return bool(mode)


def load_sprite(entry):
    d = entry.raw
    tl, tm = d.get("tlight", lvlib.T_LIGHT), d.get("tmid", lvlib.T_MID)
    skin = d.get("skin", entry.kind == "actor")
    path = entry.path
    if entry.kind in ("tile", "object") and entry.frames:
        fr = []   # animated: one dir, N frames (nframes = N in the directory); all frames the same size, the anchor is the first one's
        for k, src in enumerate(entry.frames):
            fp = entry.cat.resolve(src)
            p = fp if os.path.isfile(fp) else fp + ".lv.png"
            if not os.path.isfile(p) or not p.endswith(".lv.png"):
                raise CatalogError(f"{entry.name}: frame {k} not found (an animated sprite needs .lv.png frames): {fp}")
            lv, mk, meta = lvlib.read_lv_png(p)
            if k == 0:
                ax, ay = meta["ax"], meta["ay"]
            elif (len(lv[0]), len(lv)) != (len(fr[0][0][0]), len(fr[0][0])):
                raise CatalogError(f"{entry.name}: frame {k} has another size than frame 0")
            fr.append((lv, mk))
        frames = [fr]
    elif entry.kind in ("tile", "object"):
        p = path if os.path.isfile(path) else path + ".lv.png"
        if not os.path.isfile(p):
            raise CatalogError(f"{entry.name}: source not found: {path}")
        if p.endswith(".lv.png"):
            lv, mk, meta = lvlib.read_lv_png(p)
            ax, ay = meta["ax"], meta["ay"]
        else:  # raw RGBA png: convert on the fly (no skin rule unless asked)
            lv, mk = lvlib.levels_from_rgba(Image.open(p), tl, tm, d.get("skin", False))
            if d.get("crop", False):
                b = lvlib.shared_bbox([mk])
                lv, mk = lvlib.crop(lv, *b), lvlib.crop(mk, *b)
            ax, ay = len(lv[0]) // 2, len(lv) - 1
        frames = [[(lv, mk)]]
    else:
        if not os.path.isdir(path):
            raise CatalogError(f"{entry.name}: actor src must be a folder: {path}")
        order = {8: ORDER8, 4: ORDER4, 1: ["south"]}[entry.ndirs]
        mirror = d.get("mirror_west", False)
        found, grid, modes = {}, [], set()
        for n in order:  # pass 1: locate the frames of every stored direction
            srcname = MIRROR[n] if mirror and n in MIRROR else n
            if srcname not in found:
                mode, fs = _find_frames(path, srcname)
                if not fs:
                    raise CatalogError(f"{entry.name}: no frames for direction {srcname!r} in {path}")
                modes.add(mode)
                found[srcname] = (mode, fs)
        adaptive = _adaptive_cut(entry)
        splits = _adaptive_splits(entry)
        conv = {}
        for srcname, (mode, fs) in found.items():
            if mode == "lv":
                conv[srcname] = [lvlib.read_lv_png(f) for f in fs]
            elif adaptive is not None:
                # one tone mapping per direction, pooled over its frames: a walk cycle must not change tone from frame to
                # frame, while each facing (face / back of the head ...) gets its own spread over levels 0/1/2
                ims = [Image.open(f) for f in fs]
                tones = (lvlib.quantile_tones(ims, splits[srcname], adaptive) if srcname in splits
                         else lvlib.adaptive_tones(ims, adaptive))
                conv[srcname] = [lvlib.adaptive_levels(im, adaptive, tones) + (None,) for im in ims]
            else:
                conv[srcname] = [lvlib.levels_from_rgba(Image.open(f), tl, tm, skin) + (None,) for f in fs]
        for n in order:  # pass 2: the direction grid (mirrored where asked)
            srcname = MIRROR[n] if mirror and n in MIRROR else n
            g = conv[srcname]
            if srcname != n:
                g = [(lvlib.flip_h(lv), lvlib.flip_h(mk), meta and dict(meta, ax=meta["w"] - 1 - meta["ax"]))
                     for lv, mk, meta in g]
            grid.append(g)
        nf = min(len(g) for g in grid)
        if entry.nframes:
            if entry.nframes > nf:
                raise CatalogError(f"{entry.name}: nframes={entry.nframes} but only {nf} available")
            nf = entry.nframes
        grid = [g[:nf] for g in grid]
        sizes = {(len(lv[0]), len(lv)) for g in grid for lv, _, _ in g}
        if len(sizes) != 1:
            raise CatalogError(f"{entry.name}: frames have different sizes {sorted(sizes)}")
        if modes == {"lv"}:
            ax, ay = grid[0][0][2]["ax"], grid[0][0][2]["ay"]
            frames = [[(lv, mk) for lv, mk, _ in g] for g in grid]
        else:
            if "lv" in modes:
                raise CatalogError(f"{entry.name}: mixing .lv.png and raw png directions is not supported")
            bb = lvlib.shared_bbox([mk for g in grid for _, mk, _ in g])
            if mirror:  # crop symmetric about the canvas centre, so flip(crop(E)) == crop(flip(E)) (folding needs it)
                cw = len(grid[0][0][0][0])
                bb = (min(bb[0], cw - 1 - bb[2]), bb[1], max(bb[2], cw - 1 - bb[0]), bb[3])
            frames = [[(lvlib.crop(lv, *bb), lvlib.crop(mk, *bb)) for lv, mk, _ in g] for g in grid]
            ax, ay = (bb[2] - bb[0] + 1) // 2, bb[3] - bb[1]
    h, w = len(frames[0][0][0]), len(frames[0][0][0][0])
    ax, ay = int(d.get("ax", ax)), int(d.get("ay", ay))
    if not (1 <= w <= 255 and 1 <= h <= 255 and 0 <= ax <= 255 and 0 <= ay <= 255):
        raise CatalogError(f"{entry.name}: size {w}x{h} / anchor ({ax},{ay}) out of u8 range")
    return Sprite(entry, w, h, ax, ay, frames, _finish_mask(entry, frames))


# ------------------------------------------------------------------ bank
# folded directions: logical dir -> the stored direction it mirrors (by ndirs)
FOLD = {8: {5: 3, 6: 2, 7: 1}, 4: {3: 1}}
SPRF_MASK, SPRF_MIRROR, SPRF_HOP, SPRF_STREAM = 1, 2, 4, 8
# An actor whose pixel data is at least this many bytes (and that has more than one direction) is STREAMED when the bank is built for a map
# (build_bank(stream=True)): its data lies after the resident part of the bank and the engine reads one facing at a time from the card.
# Smaller actors (props, the wandering bird: < 1 KB) stay resident: they would read at every turn and save next to nothing.
STREAM_MIN_BYTES = 1024


def _flip(grid):
    return [row[::-1] for row in grid]


def can_fold(sp):
    """True when every mirrored direction equals the flip of its twin where it is visible (mask set; no mask: all
    levels), for every frame."""
    if sp.entry.kind != "actor" or sp.ndirs not in FOLD or not sp.entry.raw.get("fold", True):
        return False
    for d, twin in FOLD[sp.ndirs].items():
        for (la, ma), (lb, mb) in zip(sp.frames[d], sp.frames[twin]):
            lb, mb = _flip(lb), _flip(mb)
            for y in range(sp.h):
                for x in range(sp.w):
                    if sp.has_mask:
                        if ma[y][x] != mb[y][x] or (ma[y][x] and la[y][x] != lb[y][x]):
                            return False
                    elif la[y][x] != lb[y][x]:
                        return False
    return True


def stored_dirs(sp):
    """Logical directions whose frames are written to the bank."""
    if can_fold(sp):
        return [d for d in range(sp.ndirs) if d not in FOLD[sp.ndirs]]
    return list(range(sp.ndirs))


def sprite_data(sp):
    out = bytearray()
    for d in stored_dirs(sp):
        g = sp.frames[d]
        for lv, mk in g:
            out += lvlib.pack_2bpp(lv, 0, 0, sp.w - 1, sp.h - 1)
            if sp.has_mask:
                out += lvlib.pack_mask(mk, 0, 0, sp.w - 1, sp.h - 1)
    return bytes(out)


def is_streamed(sp, nbytes):
    """True when the actor sprite `sp` (data of `nbytes`) goes to the stream part of a map bank (contract 3.1, SPRF_STREAM)."""
    st = sp.entry.raw.get("stream", True)   # `stream = "always"` (the roaming creatures' actors, contract 55): streamed whatever the size
    return sp.entry.kind == "actor" and sp.ndirs > 1 and (nbytes >= STREAM_MIN_BYTES or st == "always") and bool(st)


def build_bank(catalog, names, stream=False):
    """Bank bytes (contract 3.1: 8 B header, 16 B directory entries, 4-aligned data) for `names`
    (duplicates collapse, order kept).  Returns (bytes, {name: index}).
    stream=True (maps): actors that `is_streamed()` get SPRF_STREAM and their data is written AFTER the resident part; the header's
    `size` is the resident size (header + directory + the other sprites), data_off of a streamed sprite is still counted from the
    bank start (>= size), so the bytes after `size` are read from the card on demand."""
    names = list(dict.fromkeys(names))
    if len(names) > 255:
        raise CatalogError("more than 255 sprites in a bank")
    index = {n: i for i, n in enumerate(names)}
    sprites = [catalog.sprite(n) for n in names]
    off = 8 + 16 * len(names)
    blobs_of = []
    for sp in sprites:
        blob = sprite_data(sp)
        blobs_of.append((blob + b"\0" * ((-len(blob)) % 4), stream and is_streamed(sp, len(blob))))
    res_len = sum(len(b) for b, st in blobs_of if not st)
    directory, resident, tail = bytearray(), bytearray(), bytearray()
    for sp, (blob, st) in zip(sprites, blobs_of):
        e = sp.entry
        flags = (SPRF_MASK if sp.has_mask else 0) | (SPRF_MIRROR if can_fold(sp) else 0) | (SPRF_HOP if e.raw.get("hop") else 0) | ((e.slide << 4) if e.slide is not None else 0) | (128 if e.slide_bike else 0)
        if st:
            flags |= SPRF_STREAM
            data_off = off + res_len + len(tail)
            tail += blob
        else:
            data_off = off + len(resident)
            resident += blob
        directory += struct.pack("<12BI", KINDS[e.kind], sp.w, sp.h, sp.ndirs, sp.nframes,
                                 flags, e.fw, e.fh, sp.ax, sp.ay, e.cflags,
                                 0xFF if e.fill is None else e.fill, data_off)
    total = off + len(resident)
    ver = BANK_VERSION_STREAM if tail else BANK_VERSION
    data = struct.pack("<BBHI", ver, len(names), 0, total) + bytes(directory) + bytes(resident) + bytes(tail)
    assert len(data) == total + len(tail)
    return data, index


def decode_bank(data):
    """Inverse of build_bank -> list of dicts (kind,w,h,ndirs,nframes,flags,has_mask,fw,fh,ax,ay,
    cflags,fill,data_off,frames[dir][frame]=(levels, mask|None))."""
    ver, n, _, size = struct.unpack_from("<BBHI", data, 0)
    if ver not in (BANK_VERSION, BANK_VERSION_STREAM):
        raise ValueError(f"bank version {ver}")
    if size > len(data):
        raise ValueError(f"bank size {size} > data {len(data)}")
    names = {v: k for k, v in KINDS.items()}
    out = []
    for i in range(n):
        k, w, h, nd, nf, fl, fw, fh, ax, ay, cf, fill, off = struct.unpack_from("<12BI", data, 8 + 16 * i)
        if off % 4:
            raise ValueError(f"sprite {i}: data_off {off} not 4-aligned")
        bpr, mbpr = (w + 3) // 4, (w + 7) // 8
        p, frames = off, []
        nstored = nd if not fl & SPRF_MIRROR else nd - len(FOLD[nd])
        for d in range(nstored):
            row = []
            for f in range(nf):
                lv = lvlib.unpack_2bpp(data[p:p + bpr * h], w, h)
                p += bpr * h
                mk = None
                if fl & 1:
                    mk = lvlib.unpack_mask(data[p:p + mbpr * h], w, h)
                    p += mbpr * h
                row.append((lv, mk))
            frames.append(row)
        if fl & SPRF_MIRROR:  # rebuild the folded directions as the flips of their twins (callers see all `nd`)
            full = list(frames)
            for d in range(nstored, nd):
                twin = FOLD[nd][d]
                full.append([(_flip(l), _flip(m) if m is not None else None) for l, m in frames[twin]])
            # directions 5..7 / 3 sit after the stored ones in index order: keep logical order
            frames = full
        out.append(dict(index=i, kind=names.get(k, k), w=w, h=h, ndirs=nd, nframes=nf, flags=fl,
                        has_mask=bool(fl & SPRF_MASK), folded=bool(fl & SPRF_MIRROR), fw=fw, fh=fh, ax=ax, ay=ay, cflags=cf, fill=fill,
                        data_off=off, size=p - off, frames=frames))
    return out


# ------------------------------------------------------------------ preview
def _font(size=11):
    try:
        return ImageFont.load_default(size)
    except TypeError:
        return ImageFont.load_default()


def _view(levels, mask, scale, ax, ay):
    im = lvlib.lcd_image(levels, mask, scale)
    dr = ImageDraw.Draw(im)
    cx, cy = ax * scale + scale // 2, ay * scale + scale // 2
    dr.line([(cx - 3, cy), (cx + 3, cy)], fill=(0, 200, 255))
    dr.line([(cx, cy - 3), (cx, cy + 3)], fill=(0, 200, 255))
    return im


def contact_sheet(items, scale=4, width=1400):
    """items = [(label, [[(levels, mask, ax, ay), ...] rows])] -> PIL image."""
    font, pad = _font(11), 8
    blocks = []
    for label, rows in items:
        views = [[_view(lv, mk, scale, ax, ay) for lv, mk, ax, ay in r] for r in rows]
        bw = max(sum(v.width for v in r) + 4 * (len(r) - 1) for r in views)
        bh = sum(max(v.height for v in r) + 4 for r in views)
        bw = max(bw, int(font.getlength(label)) + 2)
        blocks.append((label, views, bw, bh + 14))
    x = y = pad
    shelf = 0
    pos = []
    for b in blocks:
        if x + b[2] + pad > width and x > pad:
            x, y, shelf = pad, y + shelf + pad, 0
        pos.append((x, y))
        x += b[2] + pad
        shelf = max(shelf, b[3])
    sheet = Image.new("RGB", (width, y + shelf + pad), (34, 34, 38))
    dr = ImageDraw.Draw(sheet)
    for (label, views, bw, bh), (px, py) in zip(blocks, pos):
        dr.text((px, py), label, fill=(230, 230, 230), font=font)
        cy = py + 14
        for r in views:
            cx = px
            for v in r:
                sheet.paste(v, (cx, cy))
                cx += v.width + 4
            cy += max(v.height for v in r) + 4
    return sheet


def preview(catalog, out, scale=4, names=None):
    items = []
    for n in names or list(catalog.entries):
        sp = catalog.sprite(n)
        e = sp.entry
        rows = [[(g[0][0], g[0][1], sp.ax, sp.ay) for g in sp.frames]]
        if sp.nframes > 1:
            rows.append([(lv, mk, sp.ax, sp.ay) for lv, mk in sp.frames[0]])
        label = f"{n} [{e.kind}] {sp.w}x{sp.h}" + (f" {sp.ndirs}d x{sp.nframes}f" if e.kind == "actor" else "")
        items.append((label, rows))
    sheet = contact_sheet(items, scale)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    sheet.save(out)
    return sheet


# ------------------------------------------------------------------ dump
def dump(data, verbose=False, out=print):
    if data[:4] == b"NMAP":
        off = struct.unpack_from("<I", data, 20 + 4 * 7)[0]
        data = data[off:]
    spr = decode_bank(data)
    ver, n, _, size = struct.unpack_from("<BBHI", data, 0)
    out(f"bank v{ver}: {n} sprites, {size} bytes (file {len(data)})")
    out("idx kind    w  h dirs frm mask  fw fh  ax ay  cfl fill data_off  size")
    for s in spr:
        out(f"{s['index']:3d} {s['kind']:6s} {s['w']:3d} {s['h']:2d} {s['ndirs']:4d} {s['nframes']:3d} "
            f"{'yes' if s['has_mask'] else 'no ':4s} {s['fw']:3d}{s['fh']:3d} {s['ax']:3d}{s['ay']:3d} "
            f"{s['cflags']:4d} {'none' if s['fill'] == 255 else s['fill']:>4} {s['data_off']:8d} {s['size']:5d}"
            + (" stream" if s['flags'] & SPRF_STREAM else ""))
        if verbose:
            lv, mk = s["frames"][0][0]
            for y in range(s["h"]):
                out("      " + "".join("_" if mk and not mk[y][x] else " .+#"[lv[y][x]]
                                        for x in range(s["w"])))
    return spr


# ------------------------------------------------------------------ selftest
def selftest():
    import tempfile
    import json
    ok = []

    def check(c, msg):
        if not c:
            raise AssertionError("selftest FAILED: " + msg)
        ok.append(msg)

    with tempfile.TemporaryDirectory() as td:
        # --- 1. tiny 3x2 object, hand-checked bytes
        lv = [[0, 1, 2], [3, 0, 0]]
        mk = [[1, 1, 1], [1, 1, 0]]
        lvlib.write_lv_png(os.path.join(td, "tiny.lv.png"), lv, mk, 1, 1)
        rlv, rmk, meta = lvlib.read_lv_png(os.path.join(td, "tiny.lv.png"))
        check((rlv, rmk) == (lv, mk) and meta == {"w": 3, "h": 2, "ax": 1, "ay": 1}, "lv.png round trip")
        check(lvlib.pack_2bpp(lv, 0, 0, 2, 1) == bytes([0x24, 0x03]), "pack_2bpp bytes")
        check(lvlib.pack_mask(mk, 0, 0, 2, 1) == bytes([7, 3]), "pack_mask bytes")
        # --- 2. 8-dir raw actor (2 frames), 4-dir lv actor with mirror, folder layouts
        act = os.path.join(td, "act")
        for di, name in enumerate(ORDER8):
            for f in range(2):
                im = Image.new("RGBA", (12, 14), (0, 0, 0, 0))
                px = im.load()
                for y in range(2 + f, 10 + di % 3):          # sizes vary per dir/frame
                    for x in range(3 + di % 4, 8 + di % 4):
                        px[x, y] = (200 - 20 * di, 120, 80 + 10 * f, 255)
                px[4 + di % 4, 3 + f] = (240, 170, 120, 255)  # a skin pixel
                os.makedirs(os.path.join(act, name), exist_ok=True)
                im.save(os.path.join(act, name, f"f{f}.png"))
        cat_path = os.path.join(td, "cat.toml")
        with open(cat_path, "w") as f:
            f.write('''
[[sprite]]
name = "tiny"
kind = "object"
src = "tiny"
fw = 1
fh = 1
cflags = ["solid", "counter"]
[[sprite]]
name = "grass"
kind = "tile"
src = "tiny.lv.png"
fill = 1
cflags = ["grass"]
[[sprite]]
name = "hero8"
kind = "actor"
src = "act"
ndirs = 8
nframes = 2
[[sprite]]
name = "npc4"
kind = "actor"
src = "act"
ndirs = 4
nframes = 1
mirror_west = true
[[sprite]]
name = "anim"
kind = "tile"
frames = ["tiny.lv.png", "tiny2.lv.png", "tiny3.lv.png"]
''')
        lvlib.write_lv_png(os.path.join(td, "tiny2.lv.png"), [[2, 2, 2], [1, 1, 0]], mk, 1, 1)
        lvlib.write_lv_png(os.path.join(td, "tiny3.lv.png"), [[3, 2, 1], [0, 0, 0]], mk, 1, 1)
        cat = load_catalog(cat_path)
        an, _ = build_bank(cat, ["anim", "tiny"])
        ad = decode_bank(an)[0]
        check((ad["kind"], ad["ndirs"], ad["nframes"]) == ("tile", 1, 3) and ad["frames"][0][1][0] == [[2, 2, 2], [1, 1, 0]]
              and ad["frames"][0][2][0] == [[3, 2, 1], [0, 0, 0]] and ad["frames"][0][0][0] == lv, "animated tile: nframes 3, one direction, frames in order")
        data, index = build_bank(cat, ["tiny", "hero8", "grass", "npc4", "tiny"])
        check(index == {"tiny": 0, "hero8": 1, "grass": 2, "npc4": 3}, "index_by_name / dedupe")
        ver, n, res, size = struct.unpack_from("<BBHI", data, 0)
        check((ver, n, res, size) == (1, 4, 0, len(data)), "bank header")
        e0 = struct.unpack_from("<12BI", data, 8)
        check(e0 == (1, 3, 2, 1, 1, 1, 1, 1, 1, 1, 9, 0xFF, 8 + 64), f"directory entry 0 {e0}")
        check(data[e0[12]:e0[12] + 4] == bytes([0x24, 0x03, 7, 3]), "sprite 0 data bytes")
        check(all(struct.unpack_from("<12BI", data, 8 + 16 * i)[12] % 4 == 0 for i in range(4)), "data_off 4-aligned")
        dec = decode_bank(data)
        check(dec[0]["frames"][0][0] == (lv, mk), "decode tiny")
        check(dec[2]["fill"] == 1 and dec[2]["kind"] == "tile" and dec[2]["cflags"] == 2, "tile fill/cflags")
        # actor round trip vs direct lvlib computation with shared bbox
        grid = [[lvlib.levels_from_rgba(Image.open(os.path.join(act, n, f"f{f}.png"))) for f in range(2)]
                for n in ORDER8]
        bb = lvlib.shared_bbox([m for g in grid for _, m in g])
        a = dec[1]
        check((a["ndirs"], a["nframes"], a["kind"]) == (8, 2, "actor"), "actor dirs/frames")
        check((a["w"], a["h"]) == (bb[2] - bb[0] + 1, bb[3] - bb[1] + 1), "actor shared bbox size")
        check((a["ax"], a["ay"]) == (a["w"] // 2, a["h"] - 1), "actor default anchor")
        good = True
        for d in range(8):
            for f in range(2):
                elv, emk = grid[d][f]
                good &= a["frames"][d][f] == (lvlib.crop(elv, *bb), lvlib.crop(emk, *bb)) or \
                    (a["frames"][d][f][1] is None and a["frames"][d][f][0] == lvlib.crop(elv, *bb))
        check(good, "8-dir actor pixels round trip")
        # 4-dir mirror: W (index 3) = flip of E (index 1) in the same cropped box
        b = dec[3]
        e_lv, e_mk = b["frames"][1][0]
        w_lv, w_mk = b["frames"][3][0]
        check(w_lv == lvlib.flip_h(e_lv) and b["ndirs"] == 4, "mirror_west flips east into west")
        # folding: the mirrored 4-dir actor stores S E N only (flags bit 1); the distinct 8-dir actor keeps every direction
        e3 = struct.unpack_from("<12BI", data, 8 + 16 * 3)
        e1 = struct.unpack_from("<12BI", data, 8 + 16 * 1)
        check(e3[5] == SPRF_MASK | SPRF_MIRROR if b["has_mask"] else e3[5] == SPRF_MIRROR, "mirror_west actor is folded")
        check(not e1[5] & SPRF_MIRROR and not a["folded"] and b["folded"], "distinct directions are not folded")
        per_dir = (b["w"] + 3) // 4 * b["h"] + ((b["w"] + 7) // 8 * b["h"] if b["has_mask"] else 0)
        check(b["size"] == 3 * per_dir, "folded actor stores 3 of 4 directions")
        open(cat_path, "a").write('[[sprite]]\nname = "npc4_full"\nkind = "actor"\nsrc = "act"\nndirs = 4\nnframes = 1\nmirror_west = true\nfold = false\n')
        d_nf, _ = build_bank(load_catalog(cat_path), ["npc4_full"])
        r_nf = decode_bank(d_nf)[0]
        check(not r_nf["folded"] and r_nf["size"] == 4 * per_dir and r_nf["frames"] == b["frames"], "fold = false stores all directions")
        # --- 3. .lv.png actor folder (layout <dir>.lv.png) written by lvlib
        lva = os.path.join(td, "lvact")
        for n in ORDER4:
            lvlib.write_lv_png(os.path.join(lva, n + ".lv.png"), lv, mk, 2, 1)
        with open(cat_path, "a") as f:
            f.write('[[sprite]]\nname = "lvact"\nkind = "actor"\nsrc = "lvact"\nndirs = 4\n')
        cat2 = load_catalog(cat_path)
        d2, i2 = build_bank(cat2, ["lvact"])
        r = decode_bank(d2)[0]
        check(r["ndirs"] == 4 and r["nframes"] == 1 and (r["ax"], r["ay"]) == (2, 1)
              and r["frames"][3][0] == (lv, mk), ".lv.png actor folder")
        # --- 3b. streaming (maps): an actor of >= STREAM_MIN_BYTES with several directions gets SPRF_STREAM, its data lies after the resident part
        global STREAM_MIN_BYTES
        old_min = STREAM_MIN_BYTES
        STREAM_MIN_BYTES = 1  # the tiny test sprites are far below the real threshold
        try:
            ds, _i = build_bank(load_catalog(cat_path), ["tiny", "hero8", "npc4", "grass"], stream=True)
        finally:
            STREAM_MIN_BYTES = old_min
        dn, _i = build_bank(load_catalog(cat_path), ["tiny", "hero8", "npc4", "grass"])
        v_s, n_s, _r, tot_s = struct.unpack_from("<BBHI", ds, 0)
        ents = [struct.unpack_from("<12BI", ds, 8 + 16 * i) for i in range(4)]
        check(v_s == BANK_VERSION_STREAM and [bool(e[5] & SPRF_STREAM) for e in ents] == [False, True, True, False], "stream flags: actors only")
        check(tot_s < len(ds) and ents[1][12] >= tot_s and ents[2][12] > ents[1][12] and all(e[12] % 4 == 0 for e in ents), "stream data lies after the resident size, 4-aligned")
        check(decode_bank(ds) == [dict(d, flags=d["flags"] | (SPRF_STREAM if i in (1, 2) else 0), data_off=e[12])
                                  for i, (d, e) in enumerate(zip(decode_bank(dn), ents))], "streamed bank decodes to the same pixels")
        check(len(ds) == len(dn) and build_bank(load_catalog(cat_path), ["tiny", "hero8"])[0][0] == BANK_VERSION, "no stream: version 1, same size")
        # --- 4. locked files are not overwritten
        open(os.path.join(td, "tiny.locked"), "w").close()
        check(lvlib.write_lv_png(os.path.join(td, "tiny.lv.png"), lv, mk, 0, 0) is False, ".locked respected")
        # dump runs
        lines = []
        dump(data, out=lines.append)
        check(len(lines) == 2 + 4, "dump prints directory")
    print(f"selftest OK ({len(ok)} checks)")


def main():
    ap = argparse.ArgumentParser(description="sprite bank builder")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("preview")
    p.add_argument("catalog"); p.add_argument("out"); p.add_argument("names", nargs="*")
    p.add_argument("--scale", type=int, default=4)
    p = sub.add_parser("bank")
    p.add_argument("catalog"); p.add_argument("out"); p.add_argument("names", nargs="*")
    p = sub.add_parser("dump")
    p.add_argument("bankfile"); p.add_argument("-v", action="store_true")
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        selftest()
    elif a.cmd == "dump":
        with open(a.bankfile, "rb") as f:
            dump(f.read(), a.v)
    else:
        cat = load_catalog(a.catalog)
        if a.cmd == "preview":
            preview(cat, a.out, a.scale, a.names or None)
            print(f"wrote {a.out}")
        else:
            data, idx = build_bank(cat, a.names or list(cat.entries))
            with open(a.out, "wb") as f:
                f.write(data)
            print(f"wrote {a.out}: {len(idx)} sprites, {len(data)} bytes")


if __name__ == "__main__":
    try:
        main()
    except CatalogError as e:
        sys.exit(f"spritec: {e}")
