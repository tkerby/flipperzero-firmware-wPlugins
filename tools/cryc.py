#!/usr/bin/env python3
"""assets/data/species.toml (+ optional assets/data/cries.toml) -> nucleo/files/cry.bin (contract section 75, F3)

Usage: cryc.py [build|check|info] build (default) writes cry.bin; check validates it and verifies it is up to date; info prints every species' three parameters
       cryc.py --wav DIR renders every cry to DIR/<NNN>_<species>.wav (a pulse wave at the device's frequencies and lengths: the same 10 ms tick machine as nucleo/sound.c,
                                    tools/musc.py's Machine) so the cries can be LISTENED TO on the host; `--wav DIR name1 name2 ..` renders only those species
       cryc.py --ticks FILE writes the (frequency:volume) of every 10 ms tick of every cry: the host test (tools/host_world/f3_check.c) compares nucleo/sound.c's speaker
                                    calls with this file
       cryc.py selftest

cries are three bytes per species (a base cry out of ~38 waveforms, a pitch and a length). Ours work the same way in spirit: BASES below is a small table of base patterns
(what the voice does: a bark, a howl, a chirp ...), and every species has three numbers (base, pitch, length), derived at build time from its types, weight and height (so a heavy
Fire species barks low and a light Flying one chirps high) and optionally overridden in assets/data/cries.toml (`[species_name]` with `base = N`, `pitch = N` (the root note, 0 = C3 ..
71 = B8) and/or `length = N` (the length scale in 1/8, 8 = as written)). The result is compiled into ONE sound-engine record per species (the format of music.bin's effects, see
tools/musc.py: events + instruments), so the resident engine needs no new code to play it: sound_play(SND_CRY + species) reads the 4-byte index entry and the record from cry.bin.

cry.bin (little-endian):
  header 8 B "NCRY", u8 version = 1, u8 n (the species ids 0 .. n-1, id 0 = none), u8 0, u8 0
  index 4 B each @8: per species id: u16 offset of its record, u16 record size (0 = no cry)
  record the sound engine's record (musc.py module doc): u8 events, u8 255 (plays once), u8 volume, 2 bytes per event, the instruments
"""
import math
import random
import struct
import sys
import tomllib
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import musc  # noqa: E402  (the record format, the instruments and the tick machine)

ROOT = Path(__file__).resolve().parent.parent
SPECIES = ROOT / "assets" / "data" / "species.toml"
OVERRIDES = ROOT / "assets" / "data" / "cries.toml"
OUT_BIN = ROOT / "nucleo" / "files" / "cry.bin"
HDR = 8
IDX = 4
VERSION = 1
VOL = 150                 # the record's volume (the Options volume scales it): below the jingles (200) so a cry is never the loudest thing
MIN_TICKS, MAX_TICKS = 8, 48    # a cry lasts 0.08 .. 0.48 s
LO_ABS, HI_ABS = 17, 52   # the lowest / highest absolute semitone (0 = C3) any note of a cry may have, glides included: 175 Hz .. 2.7 kHz (the piezo is weak under 150 Hz and shrill above 3 kHz)
SCALES = (6, 7, 8, 9, 10, 11, 12)  # length scales in 1/8


def lin(a, b, n):
    return [round(a + (b - a) * i / (n - 1)) if n > 1 else a for i in range(n)]


def rnd(seed, n, lo, hi):
    r = random.Random(seed)
    return [r.randint(lo, hi) for _ in range(n)]


def inst(name, vols, ds, loop=None, slow=False):
    return musc.Inst(name, list(zip(vols, ds)), loop=loop, slow=slow)


# the instruments: a volume envelope and a pitch glide per 10 ms step (the engine's table; a note longer than the table holds its last step, or returns to `loop`)
INSTS = {
    "bark": inst("bark", lin(255, 70, 8), [0, -8, -14, -20, -24, -28, -30, -32]),
    "yip": inst("yip", lin(255, 150, 6), lin(0, 44, 6)),
    "wail": inst("wail", lin(255, 200, 20), lin(0, 40, 8) + lin(40, -24, 12)),
    "growl": inst("growl", [255, 240, 230, 220, 255, 190], [0, -8, -16, -20, -24, -8], loop=4),
    "trill": inst("trill", [255, 255], [0, 22], loop=0),
    "wob": inst("wob", [255, 255, 255, 255], [0, 14, 0, -14], loop=0, slow=True),
    "peep": inst("peep", [255, 255, 230, 180, 120], lin(0, 30, 5)),
    "chirp": inst("chirp", [255, 255, 200, 100], [0, 30, 60, 80]),
    "dive": inst("dive", lin(255, 120, 10), lin(0, -60, 10)),
    "hiss": inst("hiss", rnd(5, 24, 150, 255), rnd(6, 24, -30, 30), loop=12),
    "flat": inst("flat", [255, 255, 255, 230, 200, 160, 120, 80], [0] * 8),
    "bell": inst("bell", [255, 255, 230, 200, 170, 140, 110, 90, 70, 60], [0] * 10),
    "rise": inst("rise", lin(210, 255, 12), lin(0, 70, 12)),
    "zap": inst("zap", lin(255, 180, 8), lin(0, -70, 8)),
}

# the base patterns: (instrument, semitones above the root note, ticks at length scale 8/8); a rest has instrument None. Total <= 36 ticks at scale 8 (the scales go to 12/8, MAX_TICKS 48)
BASES = [
    ("bark", [("bark", 0, 6), (None, 0, 2), ("bark", 0, 6)]),
    ("yip", [("peep", 0, 4), (None, 0, 1), ("peep", 4, 4), ("bark", 0, 8)]),
    ("howl", [("wail", 0, 26)]),
    ("growl", [("growl", -5, 24)]),
    ("chirp", [("chirp", 0, 4), (None, 0, 1), ("chirp", 3, 4), (None, 0, 1), ("chirp", 7, 6)]),
    ("squeal", [("rise", 0, 12), ("dive", 4, 10)]),
    ("roar", [("zap", 2, 8), ("growl", -5, 16)]),
    ("buzz", [("trill", 0, 22)]),
    ("croak", [("dive", -2, 8), (None, 0, 2), ("dive", 0, 10)]),
    ("mew", [("rise", 0, 10), ("dive", 3, 14)]),
    ("hiss", [("hiss", 0, 22)]),
    ("tweet", [("chirp", 4, 3), ("chirp", 7, 3), ("chirp", 4, 3), ("chirp", 9, 6)]),
    ("moo", [("wob", -3, 26)]),
    ("rumble", [("growl", -10, 28)]),
    ("zap", [("zap", 12, 6), ("hiss", 0, 3), ("rise", -5, 8)]),
    ("bell", [("flat", 0, 4), ("flat", 7, 4), ("bell", 12, 14)]),
]
NB = len(BASES)
# which base patterns suit a type (indexes into BASES); the species picks one by its id
TYPE_BASES = {
    "normal": [0, 9, 1, 12, 5], "fighting": [0, 6, 14, 1], "flying": [4, 11, 5, 9], "poison": [10, 7, 3, 8], "ground": [13, 6, 3, 12], "rock": [13, 0, 3, 15],
    "bug": [7, 4, 11, 10], "ghost": [2, 8, 10, 15], "fire": [6, 2, 0, 3], "water": [8, 12, 9, 5], "grass": [9, 15, 1, 4], "electric": [14, 7, 1, 5],
    "psychic": [15, 2, 9, 14], "ice": [15, 5, 11, 2], "dragon": [6, 2, 13, 14],
}


class CryError(Exception):
    pass


def species_list():
    """-> [(id, name, types, height m, weight kg)] in species.toml order (id = position, 1-based)."""
    data = tomllib.loads(SPECIES.read_text(encoding="utf-8"))
    out = []
    for i, (name, v) in enumerate(data.items(), 1):
        if not isinstance(v, dict):
            continue
        out.append((i, name, list(v.get("types", ["normal"])), float(v.get("height", 1.0)), float(v.get("weight", 10.0))))
    return out


def overrides():
    if not OVERRIDES.is_file():
        return {}
    return tomllib.loads(OVERRIDES.read_text(encoding="utf-8"))


def span(base):
    """(lowest, highest) rel semitone of a base's notes."""
    rels = [r for ins, r, t in BASES[base][1] if ins]
    return min(rels), max(rels)


def clamp_root(base, root):
    lo, hi = span(base)
    return max(LO_ABS - lo, min(HI_ABS - hi, root))


def derive(sp, ov):
    """-> (base, root pitch, length scale) of a species: from its first type, weight and height; cries.toml overrides any of the three."""
    i, name, types, h, w = sp
    cands = TYPE_BASES.get(types[0], [0])
    base = cands[(i * 7 + sum(map(ord, name))) % len(cands)]
    root = round(48 - 3.0 * (math.log2(max(w, 0.3)) + 1.7))  # a 0.3 kg creature: C7; 10 kg: ~F5; 300 kg: ~A3
    s = round(8 + 2.0 * math.log2(max(h, 0.2) / 1.0))        # 0.2 m: 5.7/8 .. 1 m: 8/8 .. 3 m: 11/8
    scale = min(SCALES, key=lambda x: abs(x - s))
    o = ov.get(name, {})
    base = o.get("base", base)
    root = o.get("pitch", root)
    scale = o.get("length", scale)
    if not 0 <= base < NB:
        raise CryError("%s: base %d out of range 0..%d" % (name, base, NB - 1))
    return base, clamp_root(base, root), scale


def record(base, root, scale):
    """The sound-engine record of one cry (an effect: it plays once)."""
    segs = BASES[base][1]
    names = []
    for ins, rel, t in segs:
        if ins and ins not in names:
            names.append(ins)
    ev, cur = [], 0
    for ins, rel, t in segs:
        ticks = max(1, round(t * scale / 8))
        if not ins:
            ev.append((0, ticks))
            continue
        ix = names.index(ins)
        if ix != cur:
            ev.append((musc.CMD_INS | ix << 4, 0))
            cur = ix
        ev.append((musc.pitch_byte(max(0, min(71, root + rel))), ticks))
    blobs = [INSTS[n].blob() for n in names]
    offs, o = [], len(blobs)
    for b in blobs:
        offs.append(o)
        o += len(b)
    area = bytes(offs) + b"".join(blobs)
    if len(ev) > 255 or len(area) > 255:
        raise CryError("a cry record is too big")
    return bytes([len(ev), musc.NO_LOOP, VOL]) + b"".join(bytes(e) for e in ev) + area


def build_all():
    """-> (the cry.bin bytes, [(id, name, base, root, scale, record)])."""
    sps = species_list()
    ov = overrides()
    for k in ov:
        if k not in [s[1] for s in sps]:
            raise CryError("cries.toml: unknown species %r" % k)
    n = len(sps) + 1
    seen, rows = {}, []
    for sp in sps:
        base, root, scale = derive(sp, ov)
        rec = record(base, root, scale)
        tries = 0
        while rec in seen and sp[1] not in ov:  # no two species share a cry: nudge the root note, then the length
            tries += 1
            if tries > 60:
                raise CryError("%s: cannot find a free cry" % sp[1])
            root2 = clamp_root(base, root + (1 if tries % 2 else -1) * ((tries + 1) // 2))
            scale2 = SCALES[(SCALES.index(scale) + tries // 7) % len(SCALES)] if scale in SCALES else scale
            rec = record(base, root2, scale2)
            if rec not in seen:
                root, scale = root2, scale2
        seen[rec] = sp[1]
        rows.append((sp[0], sp[1], base, root, scale, rec))
    out = bytearray(b"NCRY" + bytes([VERSION, n, 0, 0]))
    off = HDR + IDX * n
    index = [(0, 0)]
    blob = bytearray()
    for r in rows:
        index.append((off + len(blob), len(r[5])))
        blob += r[5]
    for o, s in index:
        out += struct.pack("<HH", o, s)
    out += blob
    return bytes(out), rows


def decode(blob):
    """cry.bin -> {species id: record bytes}; validates the layout."""
    if blob[:4] != b"NCRY" or blob[4] != VERSION or len(blob) < HDR:
        raise CryError("cry.bin: bad magic / version")
    n = blob[5]
    if len(blob) < HDR + IDX * n:
        raise CryError("cry.bin: truncated index")
    out = {}
    for i in range(n):
        off, size = struct.unpack_from("<HH", blob, HDR + IDX * i)
        if not size:
            continue
        if off < HDR + IDX * n or off + size > len(blob):
            raise CryError("cry.bin: species %d: bad index entry" % i)
        out[i] = bytes(blob[off : off + size])
    return out


def check_record(sid, raw):
    """The same rules the sound engine would need; -> the (frequency, volume) ticks."""
    r = musc.Rec(raw)
    if r.ne == 0 or r.vol == 0 or r.loop != musc.NO_LOOP or r.area_end != len(raw):
        raise CryError("species %d: bad record header" % sid)
    ticks = musc.device_ticks(raw, 200)
    n = 0
    while n < len(ticks) and (ticks[n] != (0, 0) or n < 2):
        n += 1
    total = sum(b for a, b in r.ev if (a & 15) < 12 or (a & 15) == 12)
    if not MIN_TICKS <= total <= MAX_TICKS:
        raise CryError("species %d: %d ticks (%d .. %d)" % (sid, total, MIN_TICKS, MAX_TICKS))
    loud = [f for f, v in ticks[:total] if f]
    if len(loud) < total * 0.5:
        raise CryError("species %d: more than half of the cry is silent (%d of %d ticks sound)" % (sid, len(loud), total))
    if min(loud) < 110 or max(loud) > 3300:
        raise CryError("species %d: frequency %d .. %d Hz is outside 110 .. 3300" % (sid, min(loud), max(loud)))
    return total


def cmd_build():
    blob, rows = build_all()
    decode(blob)
    for sid, raw in decode(blob).items():
        check_record(sid, raw)
    OUT_BIN.write_bytes(blob)
    print("cryc: %d cries, %d bytes (%s)" % (len(rows), len(blob), OUT_BIN.relative_to(ROOT)))
    return 0


def cmd_check():
    blob, rows = build_all()
    recs = decode(blob)
    bad = 0
    total_ms = 0
    for sid, raw in recs.items():
        total_ms += check_record(sid, raw) * 10
    if len(set(recs.values())) != len(recs):
        print("ERROR: two species share a cry")
        bad += 1
    if not OUT_BIN.is_file() or OUT_BIN.read_bytes() != blob:
        print("ERROR: %s is missing or out of date (run tools/cryc.py)" % OUT_BIN.relative_to(ROOT))
        bad += 1
    if not bad:
        print("cryc check: %d cries, %d bytes, %d ms on average OK" % (len(recs), len(blob), total_ms // max(1, len(recs))))
    return 1 if bad else 0


def cmd_info():
    names = {r[0]: r for r in build_all()[1]}
    for sid in sorted(names):
        i, name, base, root, scale, rec = names[sid]
        total = sum(b for a, b in musc.Rec(rec).ev if (a & 15) <= 12)
        print("%3d %-12s base %2d %-7s root %2d (%4d Hz) length %2d/8  %2d ticks  %3d B" % (i, name, base, BASES[base][0], root, musc.hz(musc.pitch_byte(root)), scale, total, len(rec)))
    return 0


def render(raw):
    """-> int16 PCM bytes of one cry (a pulse train per tick, like musc.render)."""
    ticks = musc.device_ticks(raw, sum(b for a, b in musc.Rec(raw).ev if (a & 15) <= 12) + 4)
    out = bytearray()
    amp = 0.35 * 32767
    per_tick = musc.SAMPLE_RATE * musc.TICK_MS // 1000
    phase = 0
    for f, v in ticks:
        if not f:
            out += b"\x00\x00" * per_tick
            continue
        duty = v / 255.0 / 2
        hi, lo = struct.pack("<h", int(amp * (1 - duty))), struct.pack("<h", int(-amp * duty))
        thr = int(duty * musc.SAMPLE_RATE)
        out += b"".join([hi if ((phase + i) * f) % musc.SAMPLE_RATE < thr else lo for i in range(per_tick)])
        phase += per_tick
    return bytes(out)


def cmd_wav(out_dir, only):
    blob, rows = build_all()
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    n = 0
    for sid, name, base, root, scale, rec in rows:
        if only and name not in only:
            continue
        with wave.open(str(Path(out_dir) / ("%03d_%s.wav" % (sid, name))), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(musc.SAMPLE_RATE)
            w.writeframes(render(rec))
        n += 1
    print("cryc: %d wav files in %s" % (n, out_dir))
    return 0


def cmd_ticks(path):
    blob, rows = build_all()
    lines = []
    for sid, name, base, root, scale, rec in rows:
        n = sum(b for a, b in musc.Rec(rec).ev if (a & 15) <= 12) + 4
        t = musc.device_ticks(rec, n)
        lines.append("%d %s %d" % (sid, name, n))
        lines.append(" ".join("%d:%d" % x for x in t))
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(lines) + "\n")
    return 0


def cmd_selftest():
    # two builds are identical; every record decodes; the parameters are inside their ranges; an override changes exactly its species; a bad override is refused
    a, rows = build_all()
    b, _ = build_all()
    assert a == b, "build is not deterministic"
    recs = decode(a)
    assert len(recs) == len(rows) and len(set(recs.values())) == len(recs)
    for r in rows:
        assert 0 <= r[2] < NB and LO_ABS <= r[3] <= HI_ABS and r[4] in SCALES or r[1] in overrides()
    r0 = record(0, 30, 8)
    assert musc.Rec(r0).ne == 3 and r0[1] == musc.NO_LOOP
    assert record(0, 30, 12) != record(0, 30, 8) and record(0, 31, 8) != record(0, 30, 8)
    assert clamp_root(2, 0) >= LO_ABS and clamp_root(2, 99) + span(2)[1] <= HI_ABS
    print("cryc selftest OK (%d cries, %d bases, %d instruments)" % (len(rows), NB, len(INSTS)))
    return 0


def main(argv):
    try:
        if len(argv) >= 2 and argv[1] == "--wav":
            return cmd_wav(argv[2] if len(argv) > 2 else str(ROOT / "build" / "cry_wav"), set(argv[3:]))
        if len(argv) >= 3 and argv[1] == "--ticks":
            return cmd_ticks(argv[2])
        cmd = argv[1] if len(argv) > 1 else "build"
        return {"build": cmd_build, "check": cmd_check, "info": cmd_info, "selftest": cmd_selftest}[cmd]()
    except (CryError, musc.MuscError) as e:
        print("cryc: ERROR: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
