#!/usr/bin/env python3
"""Elementi pictures: assets/data/species.toml (`art = "<folder>"`) + assets/art/elementis/<folder>/*.png
-> nucleo/files/mon.bin (+ build/preview/mon.png).

Per species id (position in species.toml, 1-based) two pictures, 4 grays + mask, cropped to the mask bbox:
  FRONT = <folder>/south-west.png (the enemy / the popup) scaled to at most FRONT_MAX_H rows
  BACK = <folder>/north-east.png (the player's creature) scaled to at most BACK_MAX_H rows
Colour -> 4 grays with tools/elementi_preview.adaptive_levels (the approved typecast: the sprite's own tones in 3
clusters, very dark pixels + the silhouette edge = black). `art_dark_cut = N` in species.toml (default 70) lowers the
luminance below which a pixel is black: a very dark creature (muncrit, 45) would otherwise turn into a black blob. The art is resampled (BOX) BEFORE the typecast, exactly
like tools/battle_mock.py (creature(..., max_h=...)); final art only changes the folder named in species.toml.

File format = the sprite bank of contract 3.1 (nucleo/bank.h reads it): header 8 B `u8 version = 1, u8 n_spr,
u16 0, u32 size`, `n_spr` directory entries of 16 B (kind 2 ACTOR, ndirs 1, nframes 1, flags 1 = mask, ax = w / 2,
ay = h - 1), then the data (2 bpp pixels + 1 bpp mask, 4-byte aligned). Sprite index of species s (1-based):
2 * (s - 1) = front, 2 * (s - 1) + 1 = back. A species without `art` (or with `art_pending = true` and no folder yet) gets a 1x1 placeholder with flags 0 (the loader,
nucleo/monpic.c, reports "no picture"). Battle task (contract section 10.1): after those 2N entries come, per species,
front medium / back medium / front native / back native (index 2N + 4 * (s - 1) + k), then the trainer pictures
(6N + trainer id - 1: one per trainer of trainers.toml, `art = "<npc folder>"`, else the rival). The header's byte 1 holds the number of trainer pictures (it was n_spr, a u8 that capped the file at 255 entries) and the u16 at bytes 2..3 holds N
(species count, ids 0..N-1 of species.toml order), so a loader finds the battle entries and the end of the directory (6 * N + trainers) without other data.

  python3 -I tools/monc.py build mon.bin + the preview sheet
  python3 -I tools/monc.py check verify mon.bin is up to date (exit 1 otherwise)
  python3 -I tools/monc.py sheet OUT.png flitter muncrit LCD-look sheet of the named species (popup / medium / native, front + back)
  python3 -I tools/monc.py eclipse OUT.png LCD-look sheet of the Eclipse Forms (contract 67: each next to its normal form)

Contract 67 (the Eclipse Form): a species with `eclipse_art = "<folder>"` in species.toml also gets its four battle pictures (front medium, back medium, front native, back native; the same typecast as
the normal ones, `eclipse_dark_cut` = 25 by default because the art is dark) from <folder>/south-west.png / north-east.png. They are the LAST entries of mon.bin, after the trainer pictures, four per form in
species.toml order (the forms are numbered 1, 2, 3...: entry 6 * N + T + 4 * (form - 1) + kind, T = trainer pictures), and header byte 1 counts the trainer pictures PLUS those entries, so the plain loader
(nucleo/monpic.c monpic_load_kind with BP_TRAINER) reads form f's kind k as "trainer picture" T + 4 * (f - 1) + k: the battle scene needs no new loader and the resident app no new byte. The pictures are loaded
only when the form triggers; the file names are the art folders' names, so a replaced PNG only needs `monc.py` again.
Inputs are downloaded PNGs: run with -I (isolated).
"""
import os
import struct
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
import lvlib  # noqa: E402
from elementi_preview import adaptive_levels, DARK_CUT  # noqa: E402

FRONT_MAX_H = 30  # rows (battle_mock scene 9: the enemy's creature at ~30 px next to the 26 px text box)
BACK_MAX_H = 36   # the player's creature (back view, bigger, bottom-left)
# Battle images (contract section 10): MEDIUM = the normal view (battle_mock scene 7 "COMPACT"), NATIVE = the focus
# shots (the PixelLab crop at full detail, only shrunk when taller than the screen allows).
MED_FRONT_MAX_H = 42  # the enemy's creature (front view, top-right)
MED_BACK_MAX_H = 46   # the player's creature (back view, bottom-left)
NAT_MAX_H = 60        # focus shot: "about 60 px"; sprouton's 65-row crop is shrunk to this
# Contract 67: the Eclipse Forms' pictures may be a little bigger than the normal ones (the layout has the room: the player's back picture stands on row 48 (PL_BOT) and the focus shot's bottom is row 61)
ECL_MED_FRONT_MAX_H = 44
ECL_MED_BACK_MAX_H = 48
ECL_NAT_MAX_H = 62
ECL_DARK_CUT = 25     # the Eclipse art is dark: the normal 45 / 70 would turn it into a black blob
ECL_LIFT = 0.45       # ... and nearly black: its gray tones are stretched to the full range and lifted by this gamma before the typecast (`eclipse_lift` in species.toml; 1 = off)
ECL_KINDS = ("fmed", "bmed", "fnat", "bnat")   # = BP_FRONT_MED, BP_BACK_MED, BP_FRONT_NAT, BP_BACK_NAT (nucleo/monpic.h)
FRONT_VIEW, BACK_VIEW = "south-west", "north-east"
SPECIES_TOML = os.path.join(ROOT, "assets", "data", "species.toml")
ART_DIR = os.path.join(ROOT, "assets", "art", "elementis")
# Trainer pictures of the battle intro (actor art converted like the creatures, native size). Index = 6 * n_species + k, with
# k = trainer id - 1 (one entry per trainer of assets/data/trainers.toml, in file order). A trainer with `art = "<folder under
# assets/art/npcs/>"` gets that folder's south-west.png; every other trainer gets the rival's picture (the fallback).
# The hero's own picture is the world sprite (render.c hero_view, facing NE), so it costs no data.
NPC_DIR = os.path.join(ROOT, "assets", "art", "npcs")
TRAINERS_TOML = os.path.join(ROOT, "assets", "data", "trainers.toml")
TRAINER_FALLBACK = "rival"
TRAINER_DARK_CUT = 40  # the NPC catalog's levels = "adaptive" default (greyscale PixelLab v3 art)
OUT_BIN = os.path.join(ROOT, "nucleo", "files", "mon.bin")
OUT_PREVIEW = os.path.join(ROOT, "build", "preview", "mon.png")
OUT_ALL = os.path.join(ROOT, "build", "preview", "mon_all.png")
OUT_TRAINERS = os.path.join(ROOT, "build", "preview", "trainers.png")
KIND_ACTOR = 2
ECL_SPECIES = ["pyrewolf", "tidalgard", "grovestag"]   # the forms 1, 2, 3 (nucleo/battle_core.h bc_ecl_form)


def load_view(folder, view, max_h, dark_cut=DARK_CUT, split=None, lift=None):
    """PNG -> (levels, mask) cropped to the mask bbox, scaled to <= max_h rows first."""
    return load_png(os.path.join(ART_DIR, folder, view + ".png"), max_h, dark_cut, split, lift)


def lift_tones(im, gamma):
    """A very dark picture (the Eclipse forms): its luminance, 2 % .. 99.5 % of the opaque pixels, stretched to 0 .. 255 and lifted by `gamma`; gray out, alpha kept."""
    px = im.load()
    lums = sorted(0.299 * r + 0.587 * g + 0.114 * b for y in range(im.height) for x in range(im.width) for r, g, b, a in [px[x, y]] if a >= 128)
    if not lums:
        return im
    lo, hi = lums[int(len(lums) * 0.02)], lums[min(len(lums) - 1, int(len(lums) * 0.995))]
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if a >= 128:
                v = round(255 * min(1.0, max(0.0, (0.299 * r + 0.587 * g + 0.114 * b - lo) / max(1.0, hi - lo))) ** gamma)
                px[x, y] = (v, v, v, a)
    return im


def load_png(path, max_h, dark_cut=DARK_CUT, split=None, lift=None):
    im = Image.open(path).convert("RGBA")
    if lift is not None and lift != 1:
        im = lift_tones(im, lift)
    bb = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    if bb is None:
        raise ValueError(f"{path} is empty")
    im = im.crop(bb)
    if im.height > max_h:
        s = max_h / im.height
        im = im.resize((max(1, round(im.width * s)), max_h), Image.BOX)
    pad = Image.new("RGBA", (im.width + 2, im.height + 2), (0, 0, 0, 0))  # adaptive_levels wants a border
    pad.paste(im, (1, 1))
    # split = [p1, p2] (species.toml art_split): cut the non-dark tones at their quantiles instead of the k-means (a very dark picture whose
    # k-means lumps face / cap / body into one level, dozelf); computed on THIS picture (each size is converted on its own)
    tones = lvlib.quantile_tones(pad, split, dark_cut) if split else None
    lv, mk = adaptive_levels(pad, dark_cut, tones)
    b = lvlib.mask_bbox(mk)
    return lvlib.crop(lv, *b), lvlib.crop(mk, *b)


def species_list():
    with open(SPECIES_TOML, "rb") as f:
        doc = tomllib.load(f)
    out = []
    for name, d in doc.items():
        art = d.get("art")
        # `art_pending = true` (C8, contract 51): the folder is still being generated; while its south-west.png is missing the species gets the 1x1 "no picture" placeholder
        # (a warning here; the loader reports it and the battle / Registro stay in the normal view). A missing folder WITHOUT the key is still an error.
        if art and d.get("art_pending") and not os.path.exists(os.path.join(ART_DIR, art, FRONT_VIEW + ".png")):
            print(f"monc: WARN {name}: art_pending and {art}/ has no {FRONT_VIEW}.png yet: placeholder (no picture)", file=sys.stderr)
            art = None
        out.append((name, art, int(d.get("art_dark_cut", DARK_CUT)), d.get("art_split")))
    return out


def eclipse_list():
    """-> [(species name, art folder, dark cut, split, lift)] of the species with `eclipse_art` (contract 67), in species.toml order = the form number - 1."""
    with open(SPECIES_TOML, "rb") as f:
        doc = tomllib.load(f)
    out = []
    for name, d in doc.items():
        art = d.get("eclipse_art")
        if not art:
            continue
        for v in (FRONT_VIEW, BACK_VIEW):
            if not os.path.exists(os.path.join(ART_DIR, art, v + ".png")):
                raise ValueError(f"{name}: eclipse_art {art}/{v}.png does not exist")
        out.append((name, art, int(d.get("eclipse_dark_cut", ECL_DARK_CUT)), d.get("eclipse_split"), float(d.get("eclipse_lift", ECL_LIFT))))
    if [o[0] for o in out] != ECL_SPECIES[:len(out)]:
        raise ValueError(f"eclipse_art species {[o[0] for o in out]} must be {ECL_SPECIES} in that order (nucleo/battle_core.h bc_ecl_form numbers the forms in this order)")
    return out


def trainer_list():
    """-> [(trainer name, art folder used)] in trainers.toml order (art falls back to the rival)."""
    with open(TRAINERS_TOML, "rb") as f:
        doc = tomllib.load(f)
    out = []
    for name, d in doc.items():
        art = d.get("art", TRAINER_FALLBACK)
        if not os.path.exists(os.path.join(NPC_DIR, art, "south-west.png")):
            raise ValueError(f"trainer {name}: assets/art/npcs/{art}/south-west.png does not exist")
        out.append((name, art))
    return out


def pack_entries(pics, off0):
    """pics = list of (levels, mask) or None -> (directory bytes, blobs bytes). Placeholder 1x1 for None."""
    directory, blobs = bytearray(), bytearray()
    for pic in pics:
        if pic is None:
            blob, w, h, flags = b"\0\0\0\0", 1, 1, 0
        else:
            lv, mk = pic
            h, w = len(lv), len(lv[0])
            blob = lvlib.pack_2bpp(lv, 0, 0, w - 1, h - 1) + lvlib.pack_mask(mk, 0, 0, w - 1, h - 1)
            blob += b"\0" * ((-len(blob)) % 4)
            flags = 1
        directory += struct.pack("<12BI", KIND_ACTOR, w, h, 1, 1, flags, 1, 1, w // 2, h - 1, 0, 0xFF,
                                 off0 + len(blobs))
        blobs += blob
    return directory, blobs


def build():
    """-> (bank bytes, [(species name, art, front, back, extra)]) with front / back = (levels, mask) or None;
    extra = dict of the battle pictures {fmed, bmed, fnat, bnat}.  Entry layout (contract 9.4 + 10.1):
      0 .. 2N-1        popup front / back (FRONT_MAX_H / BACK_MAX_H)           2 * (s - 1) + back
      2N .. 6N-1       battle: front medium, back medium, front native, back native   2N + 4 * (s - 1) + k
      6N ..            trainer pictures, one per trainer (trainer id - 1)       6N + id - 1
    """
    sp = species_list()
    n_sp = len(sp)
    tr_list = trainer_list()
    ecl = eclipse_list()
    n_extra = len(tr_list) + 4 * len(ecl)  # header byte 1: the trainer pictures + the Eclipse Forms' (contract 67)
    n = 6 * n_sp + n_extra
    if n_extra > 255 or n > 65535:
        raise ValueError("mon.bin: %d species + %d trainers + %d Eclipse pictures is too many (header byte 1 = trainer + Eclipse pictures <= 255, 6 * species + those <= 65535)" % (n_sp, len(tr_list), 4 * len(ecl)))
    pics, entries, battle, trainers = [], [], [], []
    for name, art, dc, split in sp:
        fb = None
        ex = None
        if art:
            if not 0 <= dc <= 254:
                raise ValueError(f"{name}: art_dark_cut {dc} out of range 0..254")
            if split is not None and not (isinstance(split, list) and len(split) == 2 and 0 < split[0] < split[1] < 1):
                raise ValueError(f"{name}: art_split must be [p1, p2] with 0 < p1 < p2 < 1")
            fb = (load_view(art, FRONT_VIEW, FRONT_MAX_H, dc, split), load_view(art, BACK_VIEW, BACK_MAX_H, dc, split))
            ex = {
                "fmed": load_view(art, FRONT_VIEW, MED_FRONT_MAX_H, dc, split),
                "bmed": load_view(art, BACK_VIEW, MED_BACK_MAX_H, dc, split),
                "fnat": load_view(art, FRONT_VIEW, NAT_MAX_H, dc, split),
                "bnat": load_view(art, BACK_VIEW, NAT_MAX_H, dc, split),
            }
        pics.append((name, art, fb[0] if fb else None, fb[1] if fb else None, ex))
    cache = {}
    for name, art in tr_list:
        if art not in cache:
            cache[art] = load_png(os.path.join(NPC_DIR, art, "south-west.png"), NAT_MAX_H, TRAINER_DARK_CUT)
        trainers.append((f"{name} ({art})", cache[art]))
    seq = []
    for name, art, front, back, ex in pics:
        seq += [front, back]
    for name, art, front, back, ex in pics:
        seq += [ex[k] if ex else None for k in ("fmed", "bmed", "fnat", "bnat")]
    seq += [t[1] for t in trainers]
    eclp = []
    for name, art, dc, split, lift in ecl:
        ex = {"fmed": load_view(art, FRONT_VIEW, ECL_MED_FRONT_MAX_H, dc, split, lift), "bmed": load_view(art, BACK_VIEW, ECL_MED_BACK_MAX_H, dc, split, lift),
              "fnat": load_view(art, FRONT_VIEW, ECL_NAT_MAX_H, dc, split, lift), "bnat": load_view(art, BACK_VIEW, ECL_NAT_MAX_H, dc, split, lift)}
        eclp.append((name, art, ex))
        seq += [ex[k] for k in ECL_KINDS]
    off = 8 + 16 * n
    directory, blobs = pack_entries(seq, off)
    total = off + len(blobs)
    data = struct.pack("<BBHI", 1, n_extra, n_sp, total) + bytes(directory) + bytes(blobs)  # header byte 1 = trainer + Eclipse pictures, u16 = species count N
    assert len(data) == total
    return data, pics, trainers, eclp


def preview(pics, trainers, path, scale=4):
    from PIL import ImageDraw
    cells = []
    for name, art, front, back, ex in pics:
        for tag, pic in (("front", front), ("back", back)):
            if pic:
                cells.append((f"{name} {tag} {len(pic[0][0])}x{len(pic[0])}", lvlib.lcd_image(pic[0], pic[1], scale=scale, ground=0)))
        if ex:
            for tag in ("fmed", "bmed", "fnat", "bnat"):
                pic = ex[tag]
                cells.append((f"{name} {tag} {len(pic[0][0])}x{len(pic[0])}", lvlib.lcd_image(pic[0], pic[1], scale=scale, ground=0)))
    for name, pic in trainers:
        cells.append((f"trainer {name} {len(pic[0][0])}x{len(pic[0])}", lvlib.lcd_image(pic[0], pic[1], scale=scale, ground=0)))
    if not cells:
        return
    W = max(c[1].width for c in cells) + 12
    H = max(c[1].height for c in cells) + 24
    cols = 4
    rows = (len(cells) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * W + 12, rows * H + 12), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    for k, (label, im) in enumerate(cells):
        x, y = 12 + (k % cols) * W, 12 + (k // cols) * H
        d.text((x, y), label, fill=(255, 255, 255))
        sheet.paste(im, (x, y + 14))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sheet.save(path)


def sheet(pics, names, path, scale=4, cols=1):
    """Species sheet (LCD look): one block per species with the popup, medium and native pictures, front then back
    (`cols` blocks side by side)."""
    from PIL import ImageDraw
    rows = []
    for name, art, front, back, ex in pics:
        if name in names and ex:
            rows.append((name, [("popup front", front), ("popup back", back), ("medium front", ex["fmed"]), ("medium back", ex["bmed"]),
                                ("native front", ex["fnat"]), ("native back", ex["bnat"])]))
    if not rows:
        return False
    ims = [[(t, lvlib.lcd_image(p[0], p[1], scale=scale, ground=0)) for t, p in cells] for _, cells in rows]
    colw = [max(r[c][1].width for r in ims) + 10 for c in range(6)]
    blockw = sum(colw)
    nrows = (len(rows) + cols - 1) // cols
    rowh = [max(i.height for k in range(r * cols, min(len(rows), (r + 1) * cols)) for _, i in ims[k]) + 22 for r in range(nrows)]
    sh = Image.new("RGB", (blockw * cols + 10, sum(rowh) + 10), (40, 40, 40))
    d = ImageDraw.Draw(sh)
    for k, ((name, _), r) in enumerate(zip(rows, ims)):
        x = 8 + (k % cols) * blockw
        y = 8 + sum(rowh[:k // cols])
        for c, (t, im) in enumerate(r):
            d.text((x, y), f"{name} {t} {im.width // scale}x{im.height // scale}", fill=(255, 255, 255))
            sh.paste(im, (x, y + 14))
            x += colw[c]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sh.save(path)
    return True


def trainer_sheet(trainers, path, scale=4):
    """Trainer pictures (LCD look), one cell per trainer in trainers.toml order."""
    from PIL import ImageDraw
    cells = [(f"{i + 1} {name} {len(pic[0][0])}x{len(pic[0])}", lvlib.lcd_image(pic[0], pic[1], scale=scale, ground=0))
             for i, (name, pic) in enumerate(trainers)]
    if not cells:
        return
    W = max(c[1].width for c in cells) + 12
    H = max(c[1].height for c in cells) + 24
    cols = min(6, len(cells))
    rows = (len(cells) + cols - 1) // cols
    sh = Image.new("RGB", (cols * W + 12, rows * H + 12), (40, 40, 40))
    d = ImageDraw.Draw(sh)
    for k, (label, im) in enumerate(cells):
        x, y = 12 + (k % cols) * W, 12 + (k // cols) * H
        d.text((x, y), label, fill=(255, 255, 255))
        sh.paste(im, (x, y + 14))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sh.save(path)


def eclipse_sheet(pics, eclp, path, scale=4):
    """The Eclipse Forms (contract 67), LCD look: per form the normal form's medium back / native back and the Eclipse Form's, then the front pictures."""
    from PIL import ImageDraw
    byname = {p[0]: p for p in pics}
    cells = []
    for name, art, ex in eclp:
        base = byname[name][4]
        for tag, pic in (("normal bmed", base["bmed"]), ("ECLIPSE bmed", ex["bmed"]), ("normal bnat", base["bnat"]), ("ECLIPSE bnat", ex["bnat"]),
                         ("normal fmed", base["fmed"]), ("ECLIPSE fmed", ex["fmed"]), ("normal fnat", base["fnat"]), ("ECLIPSE fnat", ex["fnat"])):
            cells.append((f"{name} {tag} {len(pic[0][0])}x{len(pic[0])}", lvlib.lcd_image(pic[0], pic[1], scale=scale, ground=0)))
    if not cells:
        return False
    W = max(c[1].width for c in cells) + 12
    H = max(c[1].height for c in cells) + 24
    cols = 8
    rows = (len(cells) + cols - 1) // cols
    sh = Image.new("RGB", (cols * W + 12, rows * H + 12), (40, 40, 40))
    d = ImageDraw.Draw(sh)
    for k, (label, im) in enumerate(cells):
        x, y = 12 + (k % cols) * W, 12 + (k // cols) * H
        d.text((x, y), label, fill=(255, 255, 255))
        sh.paste(im, (x, y + 14))
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    sh.save(path)
    return True


def main():
    data, pics, trainers, eclp = build()
    if len(sys.argv) > 2 and sys.argv[1] == "eclipse":   # monc.py eclipse <out.png>
        ok = eclipse_sheet(pics, eclp, sys.argv[2])
        print(f"{sys.argv[2]}: {'written' if ok else 'no species has eclipse_art'}")
        return 0 if ok else 1
    if len(sys.argv) > 3 and sys.argv[1] == "sheet":   # monc.py sheet <out.png> <species...>
        ok = sheet(pics, sys.argv[3:], sys.argv[2])
        print(f"{sys.argv[2]}: {'written' if ok else 'no such species with art'}")
        return 0 if ok else 1
    if len(sys.argv) > 1 and sys.argv[1] == "check":
        cur = open(OUT_BIN, "rb").read() if os.path.exists(OUT_BIN) else b""
        if cur != data:
            print("monc: mon.bin is out of date (run tools/monc.py)")
            return 1
        print("monc: up to date")
        return 0
    with open(OUT_BIN, "wb") as f:
        f.write(data)
    preview(pics, trainers, OUT_PREVIEW)
    sheet(pics, [p[0] for p in pics], OUT_ALL, scale=3, cols=2)
    trainer_sheet(trainers, OUT_TRAINERS)
    arts = sum(1 for p in pics if p[1])
    print(f"{OUT_BIN}: {len(data)} bytes, {len(pics)} species ({arts} with art), preview {OUT_PREVIEW}")
    dims = lambda pic: f"{len(pic[0][0])}x{len(pic[0])}"
    for name, art, front, back, ex in pics:
        if art:
            print(f"  {name:<10} popup {dims(front)} / {dims(back)}  medium {dims(ex['fmed'])} / {dims(ex['bmed'])}  "
                  f"native {dims(ex['fnat'])} / {dims(ex['bnat'])}")
    for name, pic in trainers:
        print(f"  trainer {name}: {dims(pic)}")
    for k, (name, art, ex) in enumerate(eclp):
        print(f"  ECLIPSE form {k + 1} {name} ({art}): medium {dims(ex['fmed'])} / {dims(ex['bmed'])}  native {dims(ex['fnat'])} / {dims(ex['bnat'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
