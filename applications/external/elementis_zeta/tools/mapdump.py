#!/usr/bin/env python3
"""mapdump.py - decode nucleo/files/map_NN.bin (+ txt_NN.bin / txt_gl.bin) and print it.

  mapdump.py <map_NN.bin|NN|name> [...]   header, counts, top-down ASCII grids, objects/NPCs/warps/
                                          interacts/spawns with decoded text, bank directory
  mapdump.py --verify [NN|name ...]       re-parse maps/*.map through mapc and check every field of
                                          the .bin (header, grids, all defs, scripts, text, bank) matches;
                                          scripts are also disassembled and re-encoded byte for byte, and
                                          the opcode numbers are compared with nucleo/script.h;
                                          no argument = every map in maps/maps.txt.  Exit 1 on diff.

Library: parse_map_bin(data) -> dict (all sections decoded, text ids unresolved).
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapc  # noqa: E402
import spritec  # noqa: E402
import textc  # noqa: E402

ROOT = mapc.ROOT
FILES = ROOT / "nucleo" / "files"
FNAME = mapc.FACINGS
CF = [(1, "solid"), (2, "grass"), (4, "water"), (8, "counter"), (16, "warp"), (32, "trigger"), (64, "ledge")]


def cf_str(c):
    return "|".join(n for b, n in CF if c & b) or "-"


def parse_map_bin(d):
    """Decode a map file into plain dicts/lists (inverse of mapc.emit_v1)."""
    if d[:4] != b"NMAP":
        raise ValueError("bad magic")
    (ver, mid, w, h, bg, flags, area, _r, n_objs, n_npcs, n_warps, n_int, n_spw, _r2) = \
        struct.unpack_from("<BBBBBBBBHBBBBH", d, 4)
    offs = struct.unpack_from("<8I", d, 20)
    (fsize,) = struct.unpack_from("<I", d, 52)
    off_scr, n_scr, x4 = struct.unpack_from("<IHH", d, 56)   # x4 = extras offset / 4 (0 = none)
    names = ("ground", "cflags", "objs", "npcs", "warps", "interacts", "spawns", "bank")
    off = {}
    for k, v in zip(names, offs):
        if k == "bank":
            if x4:
                off["extras"] = x4 * 4      # encounter table + ledges: between spawns and the scripts
            if n_scr:
                off["scripts"] = off_scr    # between spawns / extras and bank
        off[k] = v
    r = dict(version=ver, id=mid, w=w, h=h, bg=bg & 3, nofollow=bg >> 2, flags=flags, area=area, n_objs=n_objs,
             n_npcs=n_npcs, n_warps=n_warps, n_interacts=n_int, n_spawns=n_spw, off=off,
             n_scripts=n_scr, file_size=fsize,
             bank_res=_r2 * 4,   # v2: bytes of the resident part of the bank (0 in v1 files: the whole bank)
             fly=_r,   # C6 (contract 46.1): header byte 11 = 1 + the Planeador slot (0 = not a Planeador town)
             reserved_ok=((off_scr == 0) == (n_scr == 0) and (ver >= 2 or _r2 == 0)))
    r["ground"] = list(d[off["ground"]:off["ground"] + w * h])
    r["cflags"] = list(d[off["cflags"]:off["cflags"] + w * h])
    r["objs"] = [struct.unpack_from("<4B", d, off["objs"] + 4 * k) for k in range(n_objs)]
    # flag ids are 10 bits (contract 40.2): the file keeps bits 8..9 in spare bits, decoded here so every consumer sees the logical id (NPC flags byte keeps bit0 / bit1 only)
    r["npcs"] = []
    for k in range(n_npcs):
        (s_, i_, j_, f_, beh_, fl_, t_, sf_, hf_, par_, tf_) = struct.unpack_from("<6BH4B", d, off["npcs"] + 12 * k)
        r["npcs"].append((s_, i_, j_, f_, beh_, fl_ & 3, t_, sf_ | (fl_ >> 2 & 3) << 8, hf_ | (fl_ >> 4 & 3) << 8, par_, tf_ | (fl_ >> 6 & 3) << 8))
    r["warps"] = [struct.unpack_from("<8B", d, off["warps"] + 8 * k) for k in range(n_warps)]
    r["interacts"] = []
    for k in range(n_int):
        (i_, j_, dm_, mo_, kd_, t_, fl_) = struct.unpack_from("<5BHB", d, off["interacts"] + 8 * k)
        r["interacts"].append((i_, j_, dm_, mo_, kd_ & 0x3F, t_, fl_ | (kd_ >> 6) << 8))
    r["spawns"] = [struct.unpack_from("<4B", d, off["spawns"] + 4 * k) for k in range(n_spw)]
    r["enc_rate"], r["enc"], r["ledges"], r["tables"], r["open_rate"] = 0, [], [], {}, 0
    r["roam_mask"], r["roam_spr"] = b"", []
    if x4:
        o = off["extras"]
        r["enc_rate"], ne, nl, tail, r["open_rate"] = struct.unpack_from("<5B", d, o)   # (contract 63: byte 4 = the open-ground rate)
        r["enc"] = [struct.unpack_from("<4B", d, o + 5 + 4 * k) for k in range(ne)]
        r["ledges"] = [struct.unpack_from("<3B", d, o + 5 + 4 * ne + 3 * k) for k in range(nl)]
        t = o + 5 + 4 * ne + 3 * nl
        if tail >> 1:   # contract 55: the roam block (its length / 8 is in bits 1..7): the area mask (ceil(w * h / 8) bytes), the bank index of each grass slot's roam actor (n_enc bytes), zero padding; between the ledges and the C5 tail
            mb = (w * h + 7) // 8
            r["roam_mask"], r["roam_spr"] = bytes(d[t:t + mb]), list(d[t + mb:t + mb + ne])
            t += (tail >> 1) * 8
        if tail & 1:   # C5 (contract 42.1): rate[4], n[4], then the slots of the water / fish_old / fish_good / fish_super tables
            rates, ns, q = d[t:t + 4], d[t + 4:t + 8], t + 8
            for kind, rate, n in zip(mapc.WILD_TABLES, rates, ns):
                if n:
                    r["tables"][kind] = (rate, [struct.unpack_from("<4B", d, q + 4 * k) for k in range(n)])
                q += 4 * n
    r["scripts"], r["code"] = [], b""
    if n_scr:
        o = off["scripts"]
        r["scripts"] = []   # (offset, length, trigger, cond flag (10 bits), cond value, reserved = 0): the file's last byte is cond_hi
        for k in range(n_scr):
            (o_, l_, t_, c_, v_, hi_) = struct.unpack_from("<HHBBBB", d, o + 8 * k)
            r["scripts"].append((o_, l_, t_, c_ | (hi_ & 3) << 8, v_, hi_ >> 2))
        r["code"] = d[o + 8 * n_scr:off["bank"]]
    r["bank_bytes"] = d[off["bank"]:fsize]
    r["bank"] = spritec.decode_bank(r["bank_bytes"])
    return r


class Texts:
    def __init__(self, nn):
        self.local = self.gl = None
        ld = FILES / "lang" / textc.mkfont.get_lang().dir  # master language tables
        p = ld / f"txt_{nn:02d}.bin"
        if p.is_file():
            self.local = textc.parse_table(p.read_bytes())
        g = ld / "txt_gl.bin"
        if g.is_file():
            self.gl = textc.parse_table(g.read_bytes())

    def get(self, tid):
        if tid == 0xFFFF:
            return None
        tbl = self.gl if tid & 0x8000 else self.local
        i = tid & 0x7FFF
        if tbl is None or i >= len(tbl):
            return [["<missing text %04X>" % tid]]
        return tbl[i]

    def s(self, tid):
        pages = self.get(tid)
        if pages is None:
            return "-"
        return "[%04X] " % tid + "  //  ".join(" / ".join(p) for p in pages)


def ascii_grids(r, out):
    w, h = r["w"], r["h"]
    npc = {(n[1], n[2]): "N" for n in r["npcs"]}
    spw = {(s[1], s[2]): chr(ord("0") + s[0]) if s[0] < 10 else "S" for s in r["spawns"]}
    wrp = {(x[0], x[1]) for x in r["warps"]}
    itx = {(x[0], x[1]): ("!" if x[3] == 0 else "T") for x in r["interacts"]}

    def head():
        out("    " + "".join(str(i % 10) for i in range(w)))

    out("cflags (#solid ~water ,grass c counter > warp T trigger ^ ledge $ arrow floor . free; "
        "overlay: N npc, digit spawn id, ! look, T step):")
    head()
    for j in range(h):
        row = ""
        for i in range(w):
            c = r["cflags"][j * w + i]
            ch = "."
            if c & 1: ch = "#"
            elif c & 16: ch = ">"
            elif c & 32: ch = "T"
            elif c & 4: ch = "~"
            elif c & 8: ch = "c"
            elif c & 64: ch = "^"
            elif c & 128: ch = "$"   # arrow floor (C_SLIDE)
            elif c & 2: ch = ","
            if (i, j) in itx and ch not in "#": ch = itx[(i, j)]
            if (i, j) in npc: ch = "N"
            if (i, j) in spw: ch = spw[(i, j)]
            row += ch
        out(f"{j:3d} {row}")
    out("ground (bank index in base 36, ' ' = nothing drawn):")
    head()
    for j in range(h):
        row = ""
        for i in range(w):
            g = r["ground"][j * w + i]
            row += " " if g == 0xFF else "0123456789abcdefghijklmnopqrstuvwxyz"[g % 36]
        out(f"{j:3d} {row}")


NATIVE_NAMES = {v: k for k, v in mapc.NATIVE_IDS.items()}


def fmt_ins(name, vals, tx=None, end=None):
    """One instruction as assembler-like text."""
    kinds = mapc.OPS[name][1]
    if name == "call_native":
        nid, a, b = vals
        nm = NATIVE_NAMES.get(nid)
        nk = mapc.NATIVE_KINDS.get(nid)
        if nm and nk is not None and all(v == 0 for v in (a, b)[len(nk):]):
            return " ".join([nm] + [str(v * 10 if kd == "money" else v) for kd, v in zip(nk, (a, b))])
        return f"native {nid} {a} {b}"
    parts = []
    for kind, v in zip(kinds, vals):
        if kind == "label":
            parts.append(f"->{end + v:04d}" if end is not None else f"{v:+d}")
        elif kind == "dir":
            parts.append(FNAME[v] if v < 8 else str(v))
        elif kind == "text":
            parts.append(f"{v:#06x}" + (f' "{tx.s(v)[7:60]}"' if tx else ""))
        else:
            parts.append(str(v))
    return " ".join([name] + parts)


TRIG = {0: "called", 1: "on_enter", 2: "interact", 3: "npc", 4: "step"}


def dump_scripts(r, tx, out):
    out(f"scripts ({r['n_scripts']}):")
    for k, (o, ln, trig, cf, cv, _res) in enumerate(r["scripts"]):
        cond = f" if {'!' if not cv else ''}flag {cf}" if cf else ""
        out(f"  script {k}: {TRIG.get(trig, trig)}{cond}  code [{o}..{o + ln})")
        try:
            ins = mapc.decode_script(r["code"][o:o + ln])
        except ValueError as e:
            out(f"      !! cannot decode: {e}")
            continue
        targets = set()
        for a, name, vals in ins:
            for kind, v in zip(mapc.OPS[name][1], vals):
                if kind == "label":
                    targets.add(a + mapc.ins_size(name) + v)
        for a, name, vals in ins:
            if a in targets:
                out(f"      {a:04d}:")
            out(f"      {a:04d}    " + fmt_ins(name, vals, tx, a + mapc.ins_size(name)))
        if ln in targets:
            out(f"      {ln:04d}:")


def dump(data, nn, out=print):
    r = parse_map_bin(data)
    tx = Texts(r["id"] if nn is None else nn)
    names = tuple(r["off"])
    out(f"NMAP v{r['version']}  id {r['id']}  {r['w']}x{r['h']}  bg {r['bg']}  flags 0x{r['flags']:02X}"
        f"{' (interior)' if r['flags'] & 1 else ''}{' (balloon: last bank sprite)' if r['flags'] & 2 else ''}{' (anim: animated tiles / objects)' if r['flags'] & 4 else ''}{' (cave: Exit Wisp works)' if r['flags'] & 8 else ''}{' (dark: a diamond of light without the Linterna)' if r['flags'] & 16 else ''}{' (ghosts)' if r['flags'] & 32 else ''}{' (fog: drifting mist)' if r['flags'] & 64 else ''}{' (pits: a boulder is checked against the pit NPCs)' if r['flags'] & 128 else ''}  area {r['area']}{'  fly slot ' + str(r['fly'] - 1) if r['fly'] else ''}  file_size {r['file_size']} "
        f"(actual {len(data)})")
    if r["version"] >= 2:
        out(f"bank: {r['bank_res']} B resident + {len(r['bank_bytes']) - r['bank_res']} B streamed from the card")
    out(f"counts: objs {r['n_objs']}  npcs {r['n_npcs']}  warps {r['n_warps']}  "
        f"interacts {r['n_interacts']}  spawns {r['n_spawns']}  scripts {r['n_scripts']}  "
        f"bank sprites {len(r['bank'])}")
    out("offsets: " + "  ".join(f"{k}={r['off'][k]}" for k in names))
    ascii_grids(r, out)
    bank = r["bank"]
    nm = lambda i: f"{i}:{bank[i]['kind']}" if i < len(bank) else f"{i}:??"
    out(f"objects ({len(r['objs'])}) [spr i0 j0 flags], footprint from bank:")
    for k, (s, i0, j0, fl) in enumerate(r["objs"]):
        b = bank[s]
        out(f"  {k:3d} spr {nm(s):<9} ({i0:2d},{j0:2d}) {b['fw']}x{b['fh']} front-sum "
            f"{i0 + b['fw'] - 1 + j0 + b['fh'] - 1:3d} flags {fl}")
    out(f"npcs ({len(r['npcs'])}):")
    for (s, i, j, f, beh, fl, t, sf, hf, par, tf) in r["npcs"]:
        bname = {0: "static", 1: "turn", 2: "wander", 3: "trainer"}.get(beh & 15, "?") + (f" r{beh >> 4}" if beh >> 4 else "")
        out(f"  spr {nm(s)} at ({i},{j}) face {FNAME[f]} beh {bname} flags {fl} show {sf} hide {hf} "
            f"param {par} {'beaten_flag' if beh & 15 == mapc.NPCB_TRAINER else 'talk_flag'} {tf}")
        if fl & mapc.NPCF_SCRIPT:
            out(f"      runs script {t}")
            continue
        out(f"      text {tx.s(t)}")
        if tf:
            out(f"      then {tx.s(t + 1)}")
    out(f"warps ({len(r['warps'])}):")
    for (i, j, tm, ti, tj, tf, fl, rd) in r["warps"]:
        dirs = "any" if rd == 0xFF else ",".join(FNAME[k] for k in range(8) if rd >> k & 1)
        out(f"  ({i},{j}) -> map {tm} ({ti},{tj}) face {FNAME[tf]} flags {fl} req_dirs {dirs}")
    out(f"interacts ({len(r['interacts'])}):")
    for (i, j, dm, mode, kind, t, fl) in r["interacts"]:
        dirs = "any" if dm == 0xFF else ",".join(FNAME[k] for k in range(8) if dm >> k & 1)
        out(f"  ({i},{j}) {'look' if mode == 0 else 'step'} kind {kind} dirs {dirs} flag {fl}")
        out(f"      " + (f"runs script {t}" if kind == mapc.IK_SCRIPT else f"text {tx.s(t)}"))
    if r["enc_rate"] or r["enc"] or r["open_rate"]:
        tot = sum(e[3] for e in r["enc"]) or 1
        names = {v: k for k, v in mapc.species_ids().items()}
        out(f"encounters (grass, rate {r['enc_rate']}/256 = {100 * r['enc_rate'] / 256:.1f}% per step; open ground {r['open_rate']}/256 = {100 * r['open_rate'] / 256:.1f}%):")
        for (sp, lo, hi, wt) in r["enc"]:
            out(f"  species {sp} ({names.get(sp, '?')}) lv {lo}-{hi} weight {wt} ({100 * wt / tot:.0f}%)")
    for kind, (rate, slots) in r["tables"].items():
        tot = sum(e[3] for e in slots) or 1
        names = {v: k for k, v in mapc.species_ids().items()}
        out(f"encounters ({kind}, {'rate' if kind == 'water' else 'bite'} {rate}/256 = {100 * rate / 256:.1f}%):")
        for (sp, lo, hi, wt) in slots:
            out(f"  species {sp} ({names.get(sp, '?')}) lv {lo}-{hi} weight {wt} ({100 * wt / tot:.0f}%)")
    if r["ledges"]:
        out(f"ledges ({len(r['ledges'])}): " + "  ".join(f"({i},{j}){FNAME[d]}" for i, j, d in r["ledges"]))
    out(f"spawns ({len(r['spawns'])}):")
    for (sid, i, j, f) in r["spawns"]:
        out(f"  id {sid}: ({i},{j}) face {FNAME[f]}")
    dump_scripts(r, tx, out)
    out("bank:")
    spritec.dump(r["bank_bytes"], out=lambda s: out("  " + s))
    return r


# ---- verify ---------------------------------------------------------------------------------------
def script_h_opcodes():
    """{mnemonic: number} parsed from the OP_* enum of nucleo/script.h."""
    import re
    txt = (mapc.ROOT / "nucleo" / "script.h").read_text(encoding="utf-8")
    return {mm.group(1).lower(): int(mm.group(2), 16)
            for mm in re.finditer(r"^\s*OP_([A-Z_]+)\s*=\s*(0x[0-9A-Fa-f]+)", txt, re.M) if mm.group(1) != "_COUNT"}


def c_native_ids():
    """{mnemonic: id} parsed from the NAT_* enums of script.h, natives.h and natives_shop.h."""
    import re
    out = {}
    for f in ("script.h", "natives.h", "natives_shop.h"):
        p = mapc.ROOT / "nucleo" / f
        if not p.is_file():
            continue
        for mm in re.finditer(r"\bNAT_([A-Z_]+)\s*=\s*(\d+)", p.read_text(encoding="utf-8")):
            if mm.group(1) not in ("NONE", "MAX"):
                out[mm.group(1).lower()] = int(mm.group(2))
    return out


def verify_scripts(world, m, r, plan, e, eq):
    """Script section vs the source: header fields, table and code bytes, disassembly round trip."""
    eq("n_scripts", r["n_scripts"], len(m.scripts))
    if not m.scripts:
        return
    rep2 = mapc.Report()
    want = mapc.assemble_scripts(world, m, rep2, plan)
    for x in rep2.errors:
        e(f"script assembly: {x}")
    if want is None:
        return
    table = b"".join(mapc.pack_script_def(t[0], t[1], t[2], t[3], t[4]) for t in r["scripts"])
    if any(t[5] for t in r["scripts"]):
        e("script table: cond_hi has bits above the flag id")
    eq("script table", table, want[0])
    eq("script code", r["code"][:len(want[1])], want[1])
    if r["code"][len(want[1]):].strip(b"\0"):
        e("script code has non-zero padding")
    # disassemble each script from the FILE and re-encode: must give the same bytes
    for k, (o, ln, trig, cf, cv, res) in enumerate(r["scripts"]):
        raw = r["code"][o:o + ln]
        try:
            again = mapc.encode_script(mapc.decode_script(raw))
        except ValueError as ex:
            e(f"script {k}: cannot disassemble: {ex}")
            continue
        if again != raw:
            e(f"script {k}: disassemble/re-encode differs")
        sc = m.scripts[k]
        if (trig, res) != (sc.trigger, 0):
            e(f"script {k} ({sc.name}): trigger {trig} != {sc.trigger}")


def verify_map(world, m, data=None):
    """List of mismatch strings (empty = OK)."""
    errs = []
    nn = m.id
    p = FILES / f"map_{nn:02d}.bin"
    if data is None:
        if not p.is_file():
            return [f"{p} not found (run mapc.py build)"]
        data = p.read_bytes()
    try:
        r = parse_map_bin(data)
    except Exception as e:  # noqa
        return [f"cannot decode: {e}"]
    plan = mapc.get_plan(world)
    e = errs.append

    def eq(what, got, want):
        if got != want:
            e(f"{what}: file {got!r} != source {want!r}")

    h_ops = script_h_opcodes()
    src_ops = {k: v[0] for k, v in mapc.OPS.items() if k != "call_native"}
    src_ops["call_native"] = mapc.OPS["call_native"][0]
    if h_ops != src_ops:
        e(f"opcode numbers differ between nucleo/script.h and mapc.OPS: "
          f"{sorted(set(h_ops.items()) ^ set(src_ops.items()))}")
    c_nat = c_native_ids()
    if c_nat != mapc.NATIVE_IDS:
        e(f"native ids differ between the C headers and mapc.NATIVE_IDS: {sorted(set(c_nat.items()) ^ set(mapc.NATIVE_IDS.items()))}")
    eq("version", r["version"], mapc.MAP_VERSION)
    # streaming (contract 3.1): the header's bank_res is the bank header's size; every SPRF_STREAM sprite lies behind it, up to the end of
    # the file, in directory order without gaps; nothing else does; and exactly the actors that spritec.is_streamed() picks are streamed
    bb = r["bank_bytes"]
    bank_size = struct.unpack_from("<I", bb, 4)[0]
    eq("bank_res", r["bank_res"], bank_size)
    streamed = [b for b in r["bank"] if b["flags"] & spritec.SPRF_STREAM]
    pos = bank_size
    for b in streamed:
        if b["data_off"] != pos or b["kind"] != "actor":
            e(f"streamed sprite {b['index']}: data_off {b['data_off']} (expected {pos}) / kind {b['kind']}")
        pos += b["size"] + (-b["size"]) % 4
    eq("stream part ends at the file end", pos, len(bb))
    for b in r["bank"]:
        if not b["flags"] & spritec.SPRF_STREAM and b["data_off"] + b["size"] > bank_size:
            e(f"resident sprite {b['index']} reaches into the stream part")
    eq("map_id", r["id"], m.id)
    eq("w", r["w"], m.w)
    eq("h", r["h"], m.h)
    eq("bg", r["bg"], m.bg)
    eq("nofollow (@nofollow, header bg byte bit 2)", r["nofollow"], int(bool(m.nofollow)))
    eq("flags", r["flags"], mapc.map_flags(m, bb))
    if m.balloon and (not r["bank"] or m.bank[-1] != mapc.BALLOON_SPRITE):
        e("the balloon sprite is not the last bank sprite")
    eq("area (@area) / reserved zero", (r["area"], r["reserved_ok"]), (m.area, True))
    eq("fly byte (@fly slot + 1)", r["fly"], (m.fly[0] + 1) if getattr(m, "fly", None) else 0)
    eq("file_size field", r["file_size"], len(data))
    for k in ("n_objs", "n_npcs", "n_warps", "n_interacts", "n_spawns"):
        eq(k, r[k], len(getattr(m, {"n_objs": "objects", "n_npcs": "npcs", "n_warps": "warps",
                                    "n_interacts": "interacts", "n_spawns": "spawns"}[k])))
    for k, v in r["off"].items():
        if v % 4 or not (mapc.HEADER_SIZE <= v <= len(data)):
            e(f"section {k} offset {v} not 4-aligned / out of range")
    offs = list(r["off"].values())
    if offs != sorted(offs):
        e("section offsets not ascending")
    eq("ground", r["ground"], m.ground)
    eq("cflags", r["cflags"], m.cflags)
    eq("objects", [tuple(x) for x in r["objs"]], [(o.spr, o.i0, o.j0, o.flags) for o in m.objects])
    tid = lambda k: plan.id_of(m, k) if k else 0xFFFF
    sid = lambda name: next(k for k, x in enumerate(m.scripts) if x.name == name)
    eq("npcs", [tuple(x) for x in r["npcs"]],
       [(n.spr, n.i, n.j, n.facing, n.behavior, n.flags, sid(n.script) if n.script else tid(n.text_key),
         n.show_flag, n.hide_flag, n.param, n.talk_flag) for n in m.npcs])
    eq("warps", [tuple(x) for x in r["warps"]],
       [(w.i, w.j, w.target_map_id, w.target_i, w.target_j, w.target_facing, w.flags, w.req_dirs)
        for w in m.warps])
    eq("interacts", [tuple(x) for x in r["interacts"]],
       [(x.i, x.j, x.dirmask, x.mode, x.kind, sid(x.script) if x.script else tid(x.text_key), x.flag)
        for x in m.interacts])
    eq("enc_rate", r["enc_rate"], m.enc_rate)
    eq("open_rate (contract 63)", r["open_rate"], m.open_rate)
    eq("encounters", [tuple(x) for x in r["enc"]], [(sid, lo, hi, wt) for sid, lo, hi, wt, _n in m.enc])
    eq("ledges", [tuple(x) for x in r["ledges"]], [tuple(x) for x in m.ledges])
    eq("wild tables (C5)", {k: (v[0], [tuple(x) for x in v[1]]) for k, v in r["tables"].items()},
       {k: (v[0], [(sid, lo, hi, wt) for sid, lo, hi, wt, _n in v[1]]) for k, v in m.tables.items()})
    eq("roam mask / actors (contract 55)", (r["roam_mask"], r["roam_spr"]), (m.roam_mask, list(m.roam_spr)))
    for i_, j_, d_ in m.ledges:
        if not r["cflags"][j_ * m.w + i_] & mapc.C_LEDGE:
            e(f"ledge ({i_},{j_}) tile lacks C_LEDGE")
    verify_scripts(world, m, r, plan, e, eq)
    eq("spawns", [tuple(x) for x in r["spawns"]], [(s.id, s.i, s.j, s.facing) for s in m.spawns])
    if [s.id for s in m.spawns] != list(range(len(m.spawns))):
        e("spawn ids are not 0..n-1")
    # sorted objects
    sums = [(o[1] + r["bank"][o[0]]["fw"] - 1) + (o[2] + r["bank"][o[0]]["fh"] - 1) for o in r["objs"]]
    if sums != sorted(sums):
        e("objects not sorted by front-tile sum")
    # bank: directory vs catalog + byte-exact rebuild (placeholders substituted when art is missing)
    cat = world.catalog
    if len(r["bank"]) != len(m.bank):
        e(f"bank has {len(r['bank'])} sprites, map uses {len(m.bank)}")
    else:
        for k, (b, name) in enumerate(zip(r["bank"], m.bank)):
            s = cat.sprites[name]
            fill = s.get("fill")
            want = (spritec.KINDS[s["kind"]], s["fw"], s["fh"], s["_cf"], 0xFF if fill is None else fill,
                    s["ndirs"])
            got = (spritec.KINDS[b["kind"]], b["fw"], b["fh"], b["cflags"], b["fill"], b["ndirs"])
            if got != want:
                e(f"bank[{k}] {name}: directory {got} != catalog {want}")
        try:
            rebuilt = mapc.build_map_bank(world, m, placeholder=True)[0]
            if rebuilt != r["bank_bytes"]:
                e("bank bytes differ from a fresh rebuild (art changed since build? run mapc.py build)")
            # streaming must not change a single pixel: every frame of the streamed bank equals the all-resident build's
            flat = spritec.decode_bank(mapc.build_map_bank(world, m, placeholder=True, stream=False)[0])
            for k, (a, b) in enumerate(zip(r["bank"], flat)):
                if [a[x] for x in ("kind", "w", "h", "ndirs", "nframes", "ax", "ay", "frames", "size")] != \
                   [b[x] for x in ("kind", "w", "h", "ndirs", "nframes", "ax", "ay", "frames", "size")] or \
                   a["flags"] & ~spritec.SPRF_STREAM != b["flags"]:
                    e(f"bank[{k}]: the streamed bank decodes to different pixels / flags than the all-resident one")
        except Exception as ex:  # noqa
            e(f"cannot rebuild bank: {ex}")
    # cflag sanity: doors/warps
    for w in m.warps:
        if not r["cflags"][w.j * m.w + w.i] & mapc.C_WARP:
            e(f"warp ({w.i},{w.j}) tile lacks C_WARP")
    # text
    texts = textc.load()
    tx = Texts(nn)
    if tx.local is None:
        e("txt_%02d.bin missing" % nn)
    else:
        keys = plan.local[m.name]
        if len(tx.local) != len(keys):
            e(f"txt_{nn:02d}.bin has {len(tx.local)} entries, expected {len(keys)}")
        else:
            for i, k in enumerate(keys):
                if tx.local[i] != texts[k]:
                    e(f"txt_{nn:02d}[{i}] [{k}] differs from es.txt")
    if tx.gl is None:
        e("txt_gl.bin missing")
    else:
        if len(tx.gl) != len(plan.gl):
            e(f"txt_gl.bin has {len(tx.gl)} entries, expected {len(plan.gl)}")
        else:
            for i, k in enumerate(plan.gl):
                if tx.gl[i] != texts[k]:
                    e(f"txt_gl[{i}] [{k}] differs from es.txt")
    # resolved text of every reference + talk-flag pairing
    for n in m.npcs:
        if n.talk_flag and not n.script:
            a, b = tid(n.text_key), tid(n.text_key2)
            if b != a + 1:
                e(f"npc {n.name}: .b id {b:#06x} != .a id {a:#06x} + 1")
    for it in m.interacts:
        if it.script:
            continue
        got = tx.get(tid(it.text_key))
        if got != texts.get(it.text_key):
            e(f"interact ({it.i},{it.j}) text [{it.text_key}] does not decode to es.txt")
    return errs


def main(argv):
    args = argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return 2
    verify = args[0] == "--verify"
    if verify:
        args = args[1:]
    w = mapc.load_world()
    if w.report.errors:
        mapc.print_report(w)
        return 1
    if verify:
        sel = []
        for a in args:
            m = next((x for x in w.maps if a in (x.name, str(x.id), f"{x.id:02d}")), None)
            if m is None:
                print(f"unknown map {a!r}")
                return 2
            sel.append(m)
        sel = sel or w.maps
        bad = 0
        for m in sel:
            errs = verify_map(w, m)
            print(f"map {m.id:02d} {m.name}: " + ("OK (round trip matches)" if not errs else f"{len(errs)} MISMATCH(ES)"))
            for x in errs:
                print("   ", x)
            bad += bool(errs)
        return 1 if bad else 0
    rc = 0
    for a in args:
        p = Path(a)
        nn = None
        if a.isdigit():
            p, nn = FILES / f"map_{int(a):02d}.bin", int(a)
        elif not p.exists():
            m = w.by_name(a)
            if m is None:
                print(f"cannot find {a!r}")
                rc = 2
                continue
            p, nn = FILES / f"map_{m.id:02d}.bin", m.id
        else:
            import re
            mm = re.match(r"map_(\d+)\.bin$", p.name)
            nn = int(mm.group(1)) if mm else None
            if nn is not None:
                FILES_ = p.parent
        dump(p.read_bytes(), nn)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
