#!/usr/bin/env python3
"""maps/*.map encounter tables -> nucleo/files/regarea.bin (contract section 75, F3: the Registro's AREA page, "AREA")

Usage: areac.py [build|check|info] build (default) writes regarea.bin; check validates it and verifies it is up to date; info prints every species' areas
       areac.py --dump FILE writes one line per species id: `id solar lunar`, each a list of `area index:how` (or `-`), the host test compares the Registro with it
       areac.py selftest

For every species, the zones (the `@area` banners of the maps, AREA_KEYS) whose wild tables contain it, with HOW it is met there: tall grass / open ground / a cave floor (bit 0), surfing (bit 1:
the map's `water` table) and fishing (bit 2: any of the three rods). A zone is the banner name, not a map: Monte Eco 1 and 2 are one row. The two affinities see different lists (contract 66): the
wild tables list the SOLAR form only and a Lunar player meets its counterpart in the same slot (files/aff.bin, the Lunar view), so the Solar list of a Lunar form is empty (it only comes by trade) and
its Lunar list is the zones of its Solar counterpart. Species that no table names (starters, gifts, trades, fossils, legendaries, evolutions) have no entry: the Registro says "unknown area".
Test maps (no @area) and maps with no tables are not zones.

regarea.bin (little-endian):
  header 8 B "NRGA", u8 version = 1, u8 n (species ids 0 .. n-1), u8 n_areas (AREA_COUNT), u8 0
  table @8 per species id: u16 offset of its SOLAR list, u16 offset of its LUNAR list (from the start of the file; 0 = none)
  lists u8 count (1 .. n_areas), then count x {u8 area index (AREA_KEYS position, the text is TXT_GL_AREA_<first key> + index), u8 how: 1 grass / cave / open ground, 2 surf, 4 fishing}, ordered by area index
"""
import struct
import sys
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT_BIN = ROOT / "nucleo" / "files" / "regarea.bin"
AFF_BIN = ROOT / "nucleo" / "files" / "aff.bin"
HDR = 8
VERSION = 1
HOW_GRASS, HOW_SURF, HOW_FISH = 1, 2, 4


class AreaError(Exception):
    pass


def lunar_view(n):
    """The Lunar view of files/aff.bin: [species id -> the species a Lunar player meets instead] (itself when it has no counterpart)."""
    if not AFF_BIN.is_file():
        raise AreaError("%s is missing: run tools/regc.py first" % AFF_BIN.relative_to(ROOT))
    b = AFF_BIN.read_bytes()
    if b[:4] != b"NAFF" or len(b) < 12 + n:
        raise AreaError("aff.bin: bad header or too short for %d species" % n)
    return [b[12 + i] for i in range(n)]


def collect(world):
    """-> {species id: {area index: how bits}} as the maps' tables list them (the Solar view)."""
    out = {}
    for m in world.maps:
        if not m.area:
            continue
        tabs = []
        if m.enc:
            tabs.append((HOW_GRASS, m.enc))
        for kind, (rate, slots) in m.tables.items():
            tabs.append((HOW_SURF if kind == "water" else HOW_FISH, slots))
        for how, slots in tabs:
            for slot in slots:
                d = out.setdefault(slot[0], {})
                d[m.area - 1] = d.get(m.area - 1, 0) | how
    return out


def views(world):
    """-> (n species ids, solar {sp: {area: how}}, lunar {sp: {area: how}})."""
    n = len(mapc.species_ids()) + 1
    sol = collect(world)
    aff = lunar_view(n)
    lun = {}
    for sp, areas in sol.items():
        r = aff[sp] if sp < n else sp
        for a, how in areas.items():
            d = lun.setdefault(r or sp, {})
            d[a] = d.get(a, 0) | how
    return n, sol, lun


def build(world=None):
    world = world or mapc.load_world()
    n, sol, lun = views(world)
    n_areas = len(mapc.AREA_KEYS)
    lists = bytearray()
    off = HDR + 4 * n
    tab = []
    for sp in range(n):
        offs = []
        for view in (sol, lun):
            a = view.get(sp)
            if not a:
                offs.append(0)
                continue
            offs.append(off + len(lists))
            lists += bytes([len(a)]) + b"".join(bytes([k, a[k]]) for k in sorted(a))
        tab.append(offs)
    if off + len(lists) > 0xFFFF:
        raise AreaError("regarea.bin is too big for 16-bit offsets")
    blob = bytearray(b"NRGA" + bytes([VERSION, n, n_areas, 0]))
    for o in tab:
        blob += struct.pack("<HH", *o)
    blob += lists
    return bytes(blob), (n, sol, lun)


def decode(blob):
    """-> (solar, lunar): lists of {area: how} per species id; validates the layout."""
    if blob[:4] != b"NRGA" or blob[4] != VERSION or len(blob) < HDR:
        raise AreaError("regarea.bin: bad magic / version")
    n, n_areas = blob[5], blob[6]
    if len(blob) < HDR + 4 * n:
        raise AreaError("regarea.bin: truncated table")
    out = ([], [])
    for sp in range(n):
        offs = struct.unpack_from("<HH", blob, HDR + 4 * sp)
        for v in (0, 1):
            d = {}
            if offs[v]:
                o = offs[v]
                if o < HDR + 4 * n or o >= len(blob):
                    raise AreaError("regarea.bin: species %d: bad offset" % sp)
                cnt = blob[o]
                if not 1 <= cnt <= n_areas or o + 1 + 2 * cnt > len(blob):
                    raise AreaError("regarea.bin: species %d: bad count" % sp)
                last = -1
                for k in range(cnt):
                    a, how = blob[o + 1 + 2 * k], blob[o + 2 + 2 * k]
                    if a >= n_areas or a <= last or not 1 <= how <= 7:
                        raise AreaError("regarea.bin: species %d: bad entry" % sp)
                    d[a] = how
                    last = a
            out[v].append(d)
    return out


def cmd_build():
    blob, (n, sol, lun) = build()
    decode(blob)
    OUT_BIN.write_bytes(blob)
    print("areac: %d species with an area (Solar) / %d (Lunar), %d bytes (%s)" % (len(sol), len(lun), len(blob), OUT_BIN.relative_to(ROOT)))
    return 0


def cmd_check():
    blob, (n, sol, lun) = build()
    s, l = decode(blob)
    bad = 0
    names = {v: k for k, v in mapc.species_ids().items()}
    aff = lunar_view(n)
    for sp in sol:  # every species a table names has a Solar list (unless it is a Lunar form: tables never list those) and the Lunar player has its counterpart's
        if not s[sp]:
            print("ERROR: %s is in a table but has no Solar list" % names[sp])
            bad += 1
        if not l[aff[sp] or sp]:
            print("ERROR: %s's counterpart has no Lunar list" % names[sp])
            bad += 1
    for sp in range(n):
        if s[sp] and (sp == 0 or sp not in sol):
            print("ERROR: species %d has an area it is not listed in" % sp)
            bad += 1
    most = max(max(len(x) for x in s), max(len(x) for x in l))
    if not OUT_BIN.is_file() or OUT_BIN.read_bytes() != blob:
        print("ERROR: %s is missing or out of date (run tools/areac.py)" % OUT_BIN.relative_to(ROOT))
        bad += 1
    if not bad:
        print("areac check: %d Solar / %d Lunar species with an area, at most %d areas for one, %d bytes OK" % (sum(1 for x in s if x), sum(1 for x in l if x), most, len(blob)))
    return 1 if bad else 0


def fmt(d):
    keys = mapc.AREA_KEYS
    return ",".join("%s:%d" % (keys[a], d[a]) for a in sorted(d)) or "-"


def cmd_info():
    blob, (n, sol, lun) = build()
    s, l = decode(blob)
    names = {v: k for k, v in mapc.species_ids().items()}
    for sp in range(1, n):
        print("%3d %-12s solar %-60s lunar %s" % (sp, names.get(sp, "?"), fmt(s[sp]), fmt(l[sp])))
    return 0


def fmt_num(d):
    return ",".join("%d:%d" % (a, d[a]) for a in sorted(d)) or "-"


def cmd_dump(path):
    blob, (n, sol, lun) = build()
    s, l = decode(blob)
    lines = ["%d %s %s" % (sp, fmt_num(s[sp]), fmt_num(l[sp])) for sp in range(n)]  # area INDEX (AREA_<KEY> - 1) : how bits
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(lines) + "\n")
    return 0


def cmd_selftest():
    blob, (n, sol, lun) = build()
    assert build()[0] == blob, "not deterministic"
    s, l = decode(blob)
    sp = mapc.species_ids()
    assert not s[0] and not l[0]
    assert s[sp["coilisp"]] and l[sp["dunedil"]] and not s[sp["dunedil"]], "the affinity views"
    assert set(l[sp["dunedil"]]) == set(s[sp["coilisp"]]), "the Lunar counterpart lives where the Solar form does"
    assert not s[sp["embercub"]] and not l[sp["embercub"]], "a starter has no wild area"
    bad = bytearray(blob)
    bad[HDR + 4 * 3] = 0xFF
    bad[HDR + 4 * 3 + 1] = 0xFF
    try:
        decode(bytes(bad))
        raise SystemExit("a bad offset was accepted")
    except AreaError:
        pass
    print("areac selftest OK (%d bytes)" % len(blob))
    return 0


def main(argv):
    try:
        if len(argv) >= 3 and argv[1] == "--dump":
            return cmd_dump(argv[2])
        cmd = argv[1] if len(argv) > 1 else "build"
        return {"build": cmd_build, "check": cmd_check, "info": cmd_info, "selftest": cmd_selftest}[cmd]()
    except AreaError as e:
        print("areac: ERROR: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
