#!/usr/bin/env python3
"""assets/music/*.txt (+ index.txt) -> nucleo/files/music.bin + nucleo/gen/music_gen.h   (contract section 28)

Usage: musc.py [build|check|info]       build (default) writes the two files; check validates and verifies they are up to date
       musc.py --wav DIR                renders every track / jingle / effect to DIR/<name>.wav (a pulse wave at the device's frequencies, volumes and
                                        lengths, the same 10 ms tick machine as nucleo/sound.c: envelopes, vibrato, arpeggios and drums included)
       musc.py --ticks FILE             writes the (frequency, volume) of every 10 ms tick of every track, as the tick machine produces them: the host test
                                        (tools/host_world/sound_check.c) compares nucleo/sound.c's speaker calls with this file
       musc.py --fixture PREFIX         writes PREFIX.bin + PREFIX_ticks.txt: adversarial tracks for the engine test (every feature at its limits)
       musc.py selftest

music.bin (little-endian, format 3; format 2 had a 48-slot table: maps 0..31, trainers 0..15):
  header 8 B       "NMUS", u8 version = 3, u8 n_tracks, u8 0, u8 0
  table 512 B      @8: byte per slot -> track id (0xFF = none): slots 0..255 = map id (maps/maps.txt), slots 256..511 = 256 + trainer id (trainers.toml position, 1-based; 0 = a wild
                   battle). It stays ON THE CARD: sound_play(SND_PICK + slot) reads the one byte it needs (contract 28.2)
  index 4 B each   @520: u16 offset of the track's record, u16 record size in bytes (music <= 400, effects <= 200: the record is one heap block)
  record           u8 events (<= 255), u8 loop (event index the track returns to, 255 = plays once), u8 volume 1..255, then 2 bytes per event, then the instruments:
    event {a, b}   a = pitch (0 = rest, else (octave - 2) << 4 | semitone: C3 = 0x10 .. B8 = 0x6B) and b = length in ticks (1 tick = 10 ms; 1..255), or a command
                   when (a & 15) >= 12, x = a >> 4:  12 DRUM  instrument x plays its own pitch for b ticks (a hit inside the melody; the melodic instrument is not changed)
                                                     13 INS   the notes that follow use instrument x
                                                     15 REP   x plays in all (2..15) of the last b events (one level; written for a repeated `[ ... ]`)
    instruments    u8 offset per instrument (from the first of these bytes), then each: u8 n steps (1..31) | slow << 7 (a step lasts 2 ticks), u8 loop (the step the
                   table returns to after its last: n - 1 holds the last one), u8 pitch (a drum's note, 0 = a melodic instrument), then n steps
                   {u8 volume 0..255, i8 pitch change in 1/128 of the note's frequency}. The tick machine: the note is on for its length - 1 ticks (a drum for all of them,
                   a note of 1 tick sounds); at tick k of a note step j = k >> slow (j >= n: loop + (j - loop) % (n - loop)); frequency = hz(pitch) + (hz * change >> 7),
                   volume = (track volume (halved for "Bajo") + 1) * step volume >> 8; a frequency under 100 Hz or a volume of 0 is silence.
The engine (nucleo/sound.c) loads one record into a heap block when the track is asked for. Tracks 0 .. FX0-1 are music (loop, or `once`: a fanfare), FX0.. are effects.

Track source (assets/music/<name>.txt), one voice, an MML-like text (every older file is still valid):
  tempo N      quarter notes per minute (also the token `tN` inside the notes)           vol N     1..255 (the engine scales it by the Options volume)
  meter A/B    time signature for the `|` bar checks (default 4/4)                       once      the music plays once and stops (a fanfare)
  title TEXT   free text
  notes:  c d e f g a b  with `+` sharp / `-` flat, then a length (1 2 4 8 16 32 = 1/N of a whole note, `.` dots), `=N` = exactly N ticks (10 ms);
          r4 = rest; `&` ties:  c4&c8  c4&8;  oN sets the octave (3..8, c4 = middle C), `<` `>` one octave down / up, lN the default length (l8, l4.);
          `[ ... ]N` repeats N times (default 2; nesting allowed; may span lines; the innermost repeat is stored once with a REP command when its body leaves the octave, length,
          tempo and instrument as it found them); `|` = bar line (checked against the meter when every length is in beats);
          `@loop` = the point the track returns to (default: the start; none for `once` and for effects); `#` or `;` start a comment.
  instruments:  inst NAME key=value ...   defines one (the built-in `plain` is a full-volume square); `@NAME` makes the notes that follow use it.
          `!NAME` plays the drum NAME (an instrument with drum=PITCH) in a slot of the given length like a rest: `!kick8` is the hit followed by silence up to an
          eighth; `!hat=2` is exactly 2 ticks; `c8!hat` / `r8!hat` is a note or rest whose last ticks are the hit and `!kick:c8` one whose first ticks are (the bar keeps its length). Keys (melodic): att=T ticks to reach full volume, dec=T decay time constant in ticks, sus=L level the decay settles at
          (0..1), lvl=L overall level, vib=A vibrato depth in 1/128 (1 = +-0.8 %), vdelay=T ticks before it starts, vper=T its period, arp=o1,o2,.. semitone offsets the note
          cycles through (0,4,7 = a major arpeggio; -24 .. 11), rate=T ticks per arpeggio step, tone=v,v.. the volume shape of each step, slow (steps of 2 ticks), v=a,b,.. d=a,b,.. explicit steps (volume 0..255, pitch
          change in 1/128; loop=N the step they return to, noise=A seed=S a jitter on d). Drums: drum=PITCH (c5), v=.. d=.. or fall=F (the pitch change drops F per tick), noise=A seed=S (a random jitter of +-A), slow.
index.txt:  music NAME / fx NAME (the id order of the file: all music first), map MAPNAME TRACK (maps/maps.txt names), trainer NAME|wild|* TRACK (wild = a wild battle, * = every other trainer).
"""
import math
import re
import struct
import sys
import tomllib
import wave
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "music"
OUT_BIN = ROOT / "nucleo" / "files" / "music.bin"
OUT_GEN = ROOT / "nucleo" / "gen" / "music_gen.h"
MAPS_TXT = ROOT / "maps" / "maps.txt"
TRAINERS = ROOT / "assets" / "data" / "trainers.toml"
WAV_DIR = ROOT / "build" / "music_wav_v2"

TICK_MS = 10
NOTE7 = [
    2093,
    2217,
    2349,
    2489,
    2637,
    2794,
    2960,
    3136,
    3322,
    3520,
    3729,
    3951,
]  # C7..B7: the device's integer table (nucleo/sound.c)
TAB_OFF, TAB_MAP, TAB_TRAINER, TAB_N = (
    8,
    0,
    256,
    512,
)  # format 3: 256 map slots + 256 trainer slots (format 2: 0, 32, 48)
IDX_OFF = TAB_OFF + TAB_N  # 520
IDX_SIZE = 4
VERSION = 3
MAX_TRACKS = 128  # C6 (contract 49): was 64; the music table / index hold a u8 id (SND_COUNT < SND_PICK 0x100), nothing in the engine sizes anything by it: 0 B of RAM
OCT_MIN, OCT_MAX = 3, 8
MAX_MUSIC, MAX_FX = 400, 200  # record bytes: one heap block each (contract 28.2)
MAX_EVENTS = 255
MAX_INSTS = 16
MAX_STEPS = 31
NO_LOOP = 255
DEFAULT_VOL = 180
SAMPLE_RATE = 44100
CMD_DRUM, CMD_INS, CMD_REP = 12, 13, 15


class MuscError(Exception):
    pass


def pitch_byte(abs_semi):
    """Absolute semitone (0 = C3 .. 71 = B8) -> the pitch byte of music.bin."""
    return ((abs_semi // 12 + 1) << 4) | (abs_semi % 12)


def hz(pitch):
    """Frequency in Hz of a pitch byte, exactly as the device computes it (integers)."""
    return (NOTE7[pitch & 15] << (pitch >> 4)) >> 5


# ------------------------------------------------------------------------------------------------ instruments
class Inst:
    """One instrument: a table of steps {volume 0..255, pitch change in 1/128} (see the module doc)."""

    def __init__(self, name, steps, loop=None, pitch=0, slow=False, ticks=None):
        self.name = name
        self.steps = [(int(v), int(d)) for v, d in steps]
        self.loop = len(self.steps) - 1 if loop is None else loop
        self.pitch = pitch
        self.slow = bool(slow)
        self.ticks = (
            ticks if ticks is not None else len(self.steps) * (2 if slow else 1)
        )  # a drum's natural length
        n = len(self.steps)
        if not 1 <= n <= MAX_STEPS:
            raise MuscError(
                "instrument %s: %d steps (1..%d): shorten the envelope or add `slow`"
                % (name, n, MAX_STEPS)
            )
        if not 0 <= self.loop < n:
            raise MuscError("instrument %s: bad loop point" % name)
        for v, d in self.steps:
            if not 0 <= v <= 255 or not -128 <= d <= 127:
                raise MuscError(
                    "instrument %s: step (%d, %d) is out of range (volume 0..255, pitch change -128..127)"
                    % (name, v, d)
                )

    def blob(self):
        return bytes(
            [len(self.steps) | (0x80 if self.slow else 0), self.loop, self.pitch]
        ) + b"".join(struct.pack("<Bb", v, d) for v, d in self.steps)


PLAIN = Inst("plain", [(255, 0)])


def semi_ratio_units(o):
    return round(128 * (2 ** (o / 12.0) - 1))


def parse_note_name(s, where):
    m = re.fullmatch(r"([a-g])([+-]?)([3-8])", s)
    if not m:
        raise MuscError("%sbad pitch %r (like c5, f+4)" % (where, s))
    semi = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}[m.group(1)] + {
        "": 0,
        "+": 1,
        "-": -1,
    }[m.group(2)]
    a = (int(m.group(3)) - OCT_MIN) * 12 + semi
    if not 0 <= a < 72:
        raise MuscError("%spitch %r is outside C3..B8" % (where, s))
    return pitch_byte(a)


def make_inst(name, kv, flags, where=""):
    """kv: key -> string, flags: set of bare words. -> Inst."""
    known = {
        "att",
        "dec",
        "sus",
        "lvl",
        "vib",
        "vdelay",
        "vper",
        "arp",
        "rate",
        "tone",
        "loop",
        "v",
        "d",
        "drum",
        "fall",
        "noise",
        "seed",
        "ticks",
    }
    for k in kv:
        if k not in known:
            raise MuscError("%sunknown instrument key %r" % (where, k))
    for f in flags:
        if f != "slow":
            raise MuscError("%sunknown instrument flag %r" % (where, f))
    slow = "slow" in flags

    def num(key, default, conv=float):
        try:
            return conv(kv[key]) if key in kv else default
        except ValueError:
            raise MuscError("%sbad number in %s=%s" % (where, key, kv[key]))

    def lst(key):
        try:
            return [int(x) for x in kv[key].split(",")]
        except ValueError:
            raise MuscError("%sbad list in %s=%s" % (where, key, kv[key]))

    pitch = parse_note_name(kv["drum"], where) if "drum" in kv else 0
    ticks = int(num("ticks", 0)) or None
    if "v" in kv:  # explicit steps
        vols = lst("v")
        ds = lst("d") if "d" in kv else [0] * len(vols)
        if len(ds) != len(vols):
            raise MuscError("%sv= and d= need the same number of values" % where)
        noise, x = int(num("noise", 0)), int(num("seed", 1)) * 2654435761 & 0xFFFFFFFF
        if noise:
            for i in range(len(ds)):
                x = (x * 1103515245 + 12345) & 0x7FFFFFFF
                ds[i] = max(-110, min(127, ds[i] + (x >> 16) % (2 * noise + 1) - noise))
        return Inst(
            name,
            list(zip(vols, ds)),
            int(num("loop", len(vols) - 1)) if "loop" in kv else None,
            pitch,
            slow,
            ticks,
        )
    if pitch:  # a drum without explicit steps: a short fall
        vols = [255, 190, 120]
        fall = num("fall", 30.0)
        noise = int(num("noise", 0))
        seed = int(num("seed", 1))
        ds = []
        x = seed * 2654435761 & 0xFFFFFFFF
        for k in range(len(vols)):
            x = (x * 1103515245 + 12345) & 0x7FFFFFFF
            jit = ((x >> 16) % (2 * noise + 1) - noise) if noise else 0
            ds.append(max(-110, round(-fall * k) + jit))
        return Inst(name, list(zip(vols, ds)), None, pitch, slow, ticks)
    att, dec, lvl = num("att", 0.0), num("dec", 0.0), num("lvl", 1.0)
    sus = num("sus", 1.0 if dec == 0 else 0.0)
    vib, vdelay, vper = num("vib", 0.0), int(num("vdelay", 8)), int(num("vper", 8))
    arp = lst("arp") if "arp" in kv else None
    rate = int(num("rate", 2))
    tone = (
        lst("tone") if "tone" in kv else None
    )  # the volume shape of each arpeggio tone (0..255, spread over `rate` ticks)
    if tone and (not arp or not all(0 <= x <= 255 for x in tone)):
        raise MuscError("%stone=v,v,.. (0..255) goes with arp=" % where)
    if arp and vib:
        raise MuscError("%san instrument is either an arpeggio or a vibrato" % where)
    if arp:
        for o in arp:
            if not -24 <= o <= 11:
                raise MuscError(
                    "%sarp offset %d: -24..11 semitones (an octave up does not fit: write the note an octave higher and use -12)"
                    % (where, o)
                )
    if not 0 < lvl <= 1 or not 0 <= sus <= 1:
        raise MuscError("%slvl / sus must be in 0..1" % where)

    def level(t):
        a = min(1.0, (t + 1) / att) if att > 0 else 1.0
        dd = sus + (1 - sus) * math.exp(-max(0.0, t - att) / dec) if dec > 0 else 1.0
        return max(0, min(255, round(255 * lvl * a * dd)))

    lv = [level(t) for t in range(600)]
    final = lv[-1]
    last_off = max(
        [t for t in range(600) if abs(lv[t] - final) > 3] or [-1]
    )  # (the tail within 3 of the final level is cut: it is inaudible)
    for res in (2,) if slow else (1, 2):
        per_t = len(arp) * rate if arp else (vper if vib else 0)  # the period in ticks
        if per_t and per_t % res:
            continue
        settle = -(-(last_off + 1) // res)  # the first step at the final level
        if per_t:
            n0 = max(settle, -(-vdelay // res)) if vib else settle
            n, lp = n0 + per_t // res, n0
        else:
            n, lp = settle + 1, settle
        if n > MAX_STEPS:
            continue
        steps = []
        for k in range(n):
            t = k * res
            if arp:
                d = semi_ratio_units(arp[(t // rate) % len(arp)])
            elif vib and t >= vdelay:
                d = round(vib * math.sin(2 * math.pi * (t - vdelay) / vper))
            else:
                d = 0
            vv = lv[t] if k < settle else final
            if tone:
                vv = vv * tone[(t % rate) * len(tone) // rate] // 255
            steps.append((vv, d))
        return Inst(name, steps, lp, 0, res == 2, None)
    raise MuscError(
        "%sinstrument %s needs more than %d steps even with `slow`: use a shorter decay or period"
        % (where, name, MAX_STEPS)
    )


# ------------------------------------------------------------------------------------------------ parsing
class Track:
    def __init__(self, name):
        self.name = name
        self.vol = DEFAULT_VOL
        self.once = False
        self.title = name
        self.events = []  # the compiled events [(a, b)]
        self.insts = []  # [Inst], index = the instrument number
        self.loop = NO_LOOP  # event index or NO_LOOP
        self.notes = 0  # sounding notes, for info


NOTE_RE = re.compile(r"^([a-g])([+-]?)(\d*)(\.*)(?:=(\d+))?$")
REST_RE = re.compile(r"^r(\d*)(\.*)(?:=(\d+))?$")
DRUM_RE = re.compile(r"^!([a-z_]+)(\d*)(\.*)(?:=(\d+))?$")
LEN_RE = re.compile(r"^(\d*)(\.*)(?:=(\d+))?$")
SEMI = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def strip_comment(line):
    toks = line.replace(";", " ; ").replace("<", " < ").replace(">", " > ").split()
    out = []
    for t in toks:
        if t == ";" or t.startswith("#"):
            break
        out.append(t)
    return out


class Parser:
    def __init__(self, name, text):
        self.t = Track(name)
        self.tempo = Fraction(120)
        self.meter = Fraction(1)
        self.octave = 4
        self.deflen = 4
        self.defdots = ""
        self.pos = Fraction(0)  # position in whole notes (for the bar checks)
        self.exact = True  # False once a `=N` length is used: the bar checks are off
        self.inst = "plain"  # the instrument the next notes use
        self.insts = {"plain": PLAIN}
        self.ev = (
            []
        )  # ['n', pitch, ms, inst, head drum, tail drum] ['r', ms, head, tail] ['d', inst, ms] ['loop'] ['rb'] ['re', n]
        self.lines = []
        self.where = ""
        pending, first_no, depth = (
            [],
            0,
            0,
        )  # a `[ ... ]` may span lines: the lines are joined up to the closing bracket
        for no, raw in enumerate(text.splitlines(), 1):
            toks = strip_comment(raw)
            if not toks:
                continue
            if not pending:
                first_no = no
            pending += toks
            depth += sum(t == "[" for t in toks) - sum(
                bool(re.fullmatch(r"\](\d*)", t)) for t in toks
            )
            if depth <= 0:
                self.lines.append((first_no, pending))
                pending, depth = [], 0
        if pending:
            self.lines.append(
                (first_no, pending)
            )  # (run_tokens reports the missing bracket)

    def err(self, msg):
        raise MuscError("%s%s" % (self.where, msg))

    def length(self, digits, dots, ticks):
        """-> (whole-note fraction or None, ticks or None)"""
        if ticks:
            if digits or dots:
                self.err("a length is either 1/N or =ticks, not both")
            n = int(ticks)
            if not 1 <= n <= 255:
                self.err("=%d: ticks must be 1..255" % n)
            return None, n
        n = int(digits) if digits else self.deflen
        if not digits and not dots:
            dots = self.defdots
        if n not in (1, 2, 4, 8, 16, 32, 64):
            self.err("length 1/%d is not one of 1 2 4 8 16 32 64" % n)
        f = Fraction(1, n)
        add = f
        for _ in dots:
            add /= 2
            f += add
        return f, None

    def span(self, whole, ticks):
        """-> milliseconds of a length, advancing the bar position."""
        if ticks is not None:
            self.exact = False
            return Fraction(ticks * TICK_MS)
        self.pos += whole
        return whole * 240000 / self.tempo

    def add(self, pitch, whole, ticks, tie=False):
        ms = self.span(whole, ticks)
        if tie:
            if (
                not self.ev
                or self.ev[-1][0] != "n"
                or self.ev[-1][1] != pitch
                or self.ev[-1][4]
                or self.ev[-1][5]
            ):
                self.err(
                    "a tie needs the same note right before it (without a drum hit)"
                )
            self.ev[-1][2] += ms
        elif pitch == 0:
            self.ev.append(["r", ms, None, None])
        else:
            self.ev.append(["n", pitch, ms, self.inst, None, None])

    def note_piece(self, tok, tie):
        m = NOTE_RE.match(tok)
        if not m:
            self.err("bad note %r" % tok)
        letter, acc, digits, dots, ticks = m.groups()
        semi = SEMI[letter] + {"": 0, "+": 1, "-": -1}[acc]
        s = (self.octave - OCT_MIN) * 12 + semi
        if not 0 <= s < 72:
            self.err("note %r is outside C3..B8" % tok)
        whole, tk = self.length(digits, dots, ticks)
        self.add(pitch_byte(s), whole, tk, tie)

    def is_drum(self, name):
        if name not in self.insts or not self.insts[name].pitch:
            self.err(
                "unknown drum %r (define it with: inst %s drum=c5 ...)" % (name, name)
            )
        return True

    def last_pitch(self):
        e = self.ev[-1]
        return e[1] if e[0] == "n" else None

    def token(self, tok):
        if tok == "|":
            if self.exact and self.pos % self.meter:
                self.err(
                    "bar line in the middle of a bar: %s of a %s bar are played"
                    % (self.pos % self.meter, self.meter)
                )
            return
        if tok == "@loop":
            if self.t.once:
                self.err("@loop in a `once` track")
            if any(e[0] == "loop" for e in self.ev):
                self.err("two @loop markers")
            self.ev.append(["loop"])
            return
        if tok.startswith("@"):
            name = tok[1:]
            if name not in self.insts:
                self.err(
                    "unknown instrument %r (define it with: inst %s ...)" % (name, name)
                )
            if self.insts[name].pitch:
                self.err("%r is a drum: play it with !%s" % (name, name))
            self.inst = name
            return
        if (
            tok[0] in "abcdefgr" and "!" in tok
        ):  # a note or rest with a drum hit at its end: its last ticks are the hit
            base, name = tok.split("!", 1)
            if "&" in base or not self.is_drum(name):
                self.err("bad drum tail %r (a drum name, no tie)" % tok)
            self.token(base)
            self.ev[-1][-1] = name
            return
        if tok[0] == "!" and ":" in tok:  # a drum hit at the start of a note or rest
            name, base = tok[1:].split(":", 1)
            if (
                not base
                or base[0] not in "abcdefgr"
                or "&" in base
                or "!" in base
                or not self.is_drum(name)
            ):
                self.err("bad drum head %r (!drum:note)" % tok)
            self.token(base)
            self.ev[-1][-2] = name
            return
        if tok.startswith("!"):
            m = DRUM_RE.match(tok)
            if not m:
                self.err("bad drum %r" % tok)
            name, digits, dots, ticks = m.groups()
            if name not in self.insts or not self.insts[name].pitch:
                self.err(
                    "unknown drum %r (define it with: inst %s drum=c5 ...)"
                    % (name, name)
                )
            whole, tk = self.length(digits, dots, ticks)
            ms = self.span(whole, tk)
            self.ev.append(["d", name, ms])
            return
        if tok == "<":
            self.octave -= 1
        elif tok == ">":
            self.octave += 1
        elif re.fullmatch(r"o\d+", tok):
            self.octave = int(tok[1:])
        elif re.fullmatch(r"l\d+\.*", tok):
            m = re.fullmatch(r"l(\d+)(\.*)", tok)
            n = int(m.group(1))
            if n not in (1, 2, 4, 8, 16, 32, 64):
                self.err("default length 1/%d" % n)
            self.deflen = n
            self.defdots = m.group(2)
        elif re.fullmatch(r"t\d+", tok):
            v = int(tok[1:])
            if not 30 <= v <= 400:
                self.err("tempo %d: 30..400" % v)
            self.tempo = Fraction(v)
        elif tok[0] in "abcdefg":
            parts = tok.split("&")
            self.note_piece(parts[0], False)
            pitch = self.last_pitch()
            for p in parts[1:]:
                if re.match(r"^[a-g]", p):
                    self.note_piece(p, True)
                else:
                    m = LEN_RE.match(p)
                    if not m or not (m.group(1) or m.group(3)):
                        self.err("bad tie %r" % tok)
                    whole, tk = self.length(*m.groups())
                    self.add(pitch, whole, tk, True)
        elif tok[0] == "r":
            m = REST_RE.match(tok)
            if not m:
                self.err("bad rest %r" % tok)
            whole, tk = self.length(*m.groups())
            self.add(0, whole, tk)
        else:
            self.err("unknown token %r" % tok)

    def snapshot(self):
        return (self.octave, self.deflen, self.defdots, self.tempo, self.inst)

    def run_tokens(self, toks):
        i = 0
        while i < len(toks):
            tok = toks[i]
            if tok == "[":
                depth, j = 1, i + 1
                while j < len(toks) and depth:
                    if toks[j] == "[":
                        depth += 1
                    elif re.fullmatch(r"\](\d*)", toks[j]):
                        depth -= 1
                    j += 1
                if depth:
                    self.err("[ without ]")
                body = toks[i + 1 : j - 1]
                n = int(toks[j - 1][1:] or 2)
                if not 1 <= n <= 64:
                    self.err("repeat count %d" % n)
                if "@loop" in body:
                    self.err("@loop inside a repeat")
                self.repeat(body, n)
                i = j
                continue
            if re.fullmatch(r"\](\d*)", tok):
                self.err("] without [")
            self.token(tok)
            i += 1

    def repeat(self, body, n):
        """Plays the body n times: once plus a REP command when the body ends as it began, else n copies."""
        start = len(self.ev)
        snap0, pos0 = self.snapshot(), self.pos
        self.ev.append(["rb"])
        self.run_tokens(body)
        inner = self.ev[start + 1 :]
        notes = [e[3] for e in inner if e[0] == "n"]
        n_ev = (
            len(inner) + len(notes) + 1
        )  # (an upper bound of the events: an instrument change may add one per note)
        ok = (
            n >= 2
            and n <= 15
            and snap0 == self.snapshot()
            and not any(e[0] in ("rb", "re") for e in inner)
            and (not notes or notes[0] == notes[-1])
            and n_ev <= 250
            and (n - 1) * len(inner) * 2 > 2
        )
        if ok:
            self.ev.append(["re", n])
            self.pos += (n - 1) * (self.pos - pos0)
        else:
            del self.ev[start]
            for _ in range(n - 1):
                self.run_tokens(body)

    def parse(self):
        for no, toks in self.lines:
            self.where = "%s.txt:%d: " % (self.t.name, no)
            key = toks[0]
            if key == "tempo":
                if len(toks) != 2 or not toks[1].isdigit():
                    self.err("tempo N")
                self.token("t" + toks[1])
            elif key == "vol":
                if (
                    len(toks) != 2
                    or not toks[1].isdigit()
                    or not 1 <= int(toks[1]) <= 255
                ):
                    self.err("vol N (1..255)")
                self.t.vol = int(toks[1])
            elif key == "meter":
                m = re.fullmatch(r"(\d+)/(\d+)", toks[1]) if len(toks) == 2 else None
                if not m or int(m.group(2)) not in (2, 4, 8, 16):
                    self.err("meter A/B")
                self.meter = Fraction(int(m.group(1)), int(m.group(2)))
            elif key == "once":
                self.t.once = True
            elif key == "title":
                self.t.title = " ".join(toks[1:])
            elif key == "inst":
                self.define_inst(toks[1:])
            else:
                self.run_tokens(toks)
        self.where = "%s.txt: " % self.t.name
        if not any(e[0] in ("n", "d") for e in self.ev):
            self.err("no notes")
        return self.finish()

    def define_inst(self, toks):
        if (
            not toks
            or not re.fullmatch(r"[a-z][a-z0-9_]*", toks[0])
            or toks[0] in ("loop", "plain")
        ):
            self.err("inst NAME key=value ... (NAME lower-case, not `plain` or `loop`)")
        if toks[0] in self.insts:
            self.err("instrument %s defined twice" % toks[0])
        kv, flags = {}, set()
        for t in toks[1:]:
            if "=" in t:
                k, v = t.split("=", 1)
                kv[k] = v
            else:
                flags.add(t)
        self.insts[toks[0]] = make_inst(toks[0], kv, flags, self.where)

    # ---- ticks and events
    def finish(self):
        t = self.t
        # 1. milliseconds -> whole ticks (the rounding error does not accumulate: boundaries are rounded, not lengths); a REP body is converted once
        total, prev = Fraction(0), 0
        flat = (
            []
        )  # ['n', pitch, ticks, inst] ['r', ticks] ['d', inst, ticks] ['loop'] ['rb'] ['re', n]
        stack = []
        for e in self.ev:
            k = e[0]
            if k in ("loop", "rb"):
                flat.append(list(e))
                if k == "rb":
                    stack.append((total, prev, len(flat)))
                continue
            if k == "re":
                t0, p0, i0 = stack.pop()
                ms_body, tb = total - t0, prev - p0
                total += (e[1] - 1) * ms_body
                prev += (e[1] - 1) * tb
                flat.append(list(e))
                continue
            total += e[1] if k == "r" else e[2]
            cur = round(total / TICK_MS)
            ticks = cur - prev
            prev = cur
            if ticks < 1:
                self.err(
                    "a note is shorter than one tick (10 ms): slow the tempo or use longer notes"
                )
            if (
                k == "d"
            ):  # a drum in a slot: the hit (its natural length), then silence up to the slot
                nat = self.insts[e[1]].ticks
                flat.append(["d", e[1], min(ticks, nat)])
                if ticks > nat:
                    flat.append(["r", ticks - nat])
                continue
            head, tail = (e[4], e[5]) if k == "n" else (e[2], e[3])
            nh = self.insts[head].ticks if head else 0
            nt = self.insts[tail].ticks if tail else 0
            main = ticks - nh - nt
            if main < (1 if k == "n" else 0):
                self.err(
                    "a note of %d ticks cannot also hold a drum hit of %d ticks"
                    % (ticks, nh + nt)
                )
            if head:
                flat.append(["d", head, nh])
            if k == "r":
                while main > 255:
                    flat.append(["r", 255])
                    main -= 255
                if main:
                    flat.append(["r", main])
            else:
                if main > 255:
                    self.err(
                        "a note is longer than 2.55 s (%d ticks): split it with a rest or a new note"
                        % main
                    )
                flat.append(["n", e[1], main, e[3]])
            if tail:
                flat.append(["d", tail, nt])
        # 2. instruments, in the order of first use; the first melodic note's is number 0 (the engine starts with it)
        order = []
        for e in flat:
            if e[0] == "n" and e[3] not in order:
                order.append(e[3])
                break
        for e in flat:
            nm = e[3] if e[0] == "n" else (e[1] if e[0] == "d" else None)
            if nm and nm not in order:
                order.append(nm)
        if not order:
            order = ["plain"]
        if len(order) > MAX_INSTS:
            self.err(
                "%d instruments in one track (at most %d)" % (len(order), MAX_INSTS)
            )
        idx = {nm: i for i, nm in enumerate(order)}
        melodic = [e[3] for e in flat if e[0] == "n"]
        looping = not t.once and not self.is_fx
        # 3. events. The loop point: a track that returns to a note must find the instrument it was written for: if the instrument at the end is not the
        # one of the first note after the loop point, that note selects it again (an INS right after the marker; for a loop at the start, at event 0)
        loop_pos = next((i for i, e in enumerate(flat) if e[0] == "loop"), None)
        after = [
            e[3]
            for e in (flat[loop_pos:] if loop_pos is not None else flat)
            if e[0] == "n"
        ]
        force = looping and bool(after) and melodic[-1] != after[0]
        out = []
        cur = 0 if melodic else None
        loop_idx = 0
        rb = []
        notes = 0
        if force and loop_pos is None:
            cur = None
        for fi, e in enumerate(flat):
            k = e[0]
            if k == "loop":
                loop_idx = len(out)
                if force:
                    cur = None
                continue
            if k == "rb":
                # the body is played several times by one REP: the instrument its first note needs is selected before the body, not inside it
                depth, body_notes = 1, []
                for e2 in flat[fi + 1 :]:
                    depth += (e2[0] == "rb") - (e2[0] == "re")
                    if not depth:
                        break
                    if e2[0] == "n":
                        body_notes.append(e2[3])
                if body_notes and cur != idx[body_notes[0]]:
                    out.append((CMD_INS | idx[body_notes[0]] << 4, 0))
                    cur = idx[body_notes[0]]
                rb.append(len(out))
                continue
            if k == "re":
                body = len(out) - rb.pop()
                if not 1 <= body <= 255 or not 2 <= e[1] <= 15:
                    self.err(
                        "a repeated section of %d events x %d does not fit a REP command"
                        % (body, e[1])
                    )
                out.append((CMD_REP | e[1] << 4, body))
                continue
            if k == "n":
                ix = idx[e[3]]
                if cur != ix:
                    out.append((CMD_INS | ix << 4, 0))
                    cur = ix
                notes += 1
                out.append((e[1], e[2]))
            elif k == "d":
                out.append((CMD_DRUM | idx[e[1]] << 4, e[2]))
                notes += 1
            else:
                out.append((0, e[1]))
        if len(out) > MAX_EVENTS:
            self.err(
                "%d events do not fit a track (at most %d): shorten it or repeat a section"
                % (len(out), MAX_EVENTS)
            )
        if loop_idx >= len(out):
            self.err("@loop at the very end: nothing to loop")
        t.events = out
        t.insts = [self.insts[nm] for nm in order]
        t.loop = loop_idx if looping else NO_LOOP
        t.notes = notes
        return t

    is_fx = False


def parse_track(name, text, fx=False):
    p = Parser(name, text)
    p.is_fx = fx
    t = p.parse()
    if fx and any(e[0] == "loop" for e in p.ev):
        raise MuscError("%s: effects and `once` tracks cannot loop" % name)
    return t


# ------------------------------------------------------------------------------------------------ compiling
def record(t, fx):
    """-> the track's record bytes."""
    loop = NO_LOOP if (fx or t.once) else t.loop
    body = b"".join(bytes(e) for e in t.events)
    blobs = [i.blob() for i in t.insts]
    offs, o = [], len(blobs)
    for b in blobs:
        offs.append(o)
        o += len(b)
    area = bytes(offs) + b"".join(blobs)
    if len(area) > 255:
        raise MuscError(
            "%s: the instruments need %d bytes (at most 255)" % (t.name, len(area))
        )
    rec = bytes([len(t.events), loop, t.vol]) + body + area
    limit = MAX_FX if fx else MAX_MUSIC
    if len(rec) > limit:
        raise MuscError(
            "%s: the record is %d bytes (at most %d for %s): shorten it"
            % (t.name, len(rec), limit, "an effect" if fx else "music")
        )
    return rec


def load_index(index_text=None):
    maps = {}
    for line in MAPS_TXT.read_text().splitlines():
        m = re.match(r"^(\d+)\s+(\w+)\s*$", line)
        if m:
            maps[m.group(2)] = int(m.group(1))
    with open(TRAINERS, "rb") as f:
        trs = {name: i + 1 for i, name in enumerate(tomllib.load(f))}
    trs["wild"] = 0  # a wild battle opens with trainer slot 0 (battle_open.c)
    tracks, fx, maptab, trtab, default_tr = [], [], {}, {}, None
    text = (SRC / "index.txt").read_text() if index_text is None else index_text
    for no, raw in enumerate(text.splitlines(), 1):
        toks = strip_comment(raw)
        if not toks:
            continue
        where = "index.txt:%d: " % no
        k = toks[0]
        if k in ("music", "fx") and len(toks) == 2:
            if not re.fullmatch(r"[a-z][a-z0-9_]*", toks[1]):
                raise MuscError(
                    where + "track name %r: lower-case identifier expected" % toks[1]
                )
            if k == "music" and fx:
                raise MuscError(
                    where
                    + "music tracks come before the effects (the engine tells them apart by id)"
                )
            if toks[1] in tracks + fx:
                raise MuscError(where + "track %s listed twice" % toks[1])
            (tracks if k == "music" else fx).append(toks[1])
        elif k == "map" and len(toks) == 3:
            if toks[1] not in maps:
                raise MuscError(where + "unknown map %r (maps/maps.txt)" % toks[1])
            mid = maps[toks[1]]
            if (
                mid >= TAB_TRAINER
            ):  # (u8 map ids always fit; kept as a guard for the format)
                raise MuscError(
                    where
                    + "map %s has id %d: the table holds ids 0..%d"
                    % (toks[1], mid, TAB_TRAINER - 1)
                )
            maptab[mid] = toks[2]
        elif k == "trainer" and len(toks) == 3:
            if toks[1] == "*":
                default_tr = toks[2]
            elif toks[1] in trs:
                if trs[toks[1]] >= TAB_N - TAB_TRAINER:
                    raise MuscError(
                        where
                        + "trainer %s has id %d: the table holds %d"
                        % (toks[1], trs[toks[1]], TAB_N - TAB_TRAINER)
                    )
                trtab[trs[toks[1]]] = toks[2]
            else:
                raise MuscError(
                    where + "unknown trainer %r (assets/data/trainers.toml)" % toks[1]
                )
        else:
            raise MuscError(where + "bad line %r" % raw.strip())
    return tracks, fx, maptab, trtab, default_tr


def compile_all(index_text=None, extra=None):
    """-> dict(bin=bytes, header=str, info=[...]). index_text / extra (name -> track source): for the selftest."""
    music, fx, maptab, trtab, default_tr = load_index(index_text)
    names = music + fx
    if not names or len(names) > MAX_TRACKS:
        raise MuscError(
            "index.txt: 1..%d tracks expected, got %d" % (MAX_TRACKS, len(names))
        )
    ids = {n: i for i, n in enumerate(names)}
    for what, nm in (
        list(("map %d" % m, n) for m, n in maptab.items())
        + list(("trainer %d" % m, n) for m, n in trtab.items())
        + ([("trainer *", default_tr)] if default_tr else [])
    ):
        if nm not in ids:
            raise MuscError("index.txt: %s uses the unknown track %r" % (what, nm))
        if ids[nm] >= len(music):
            raise MuscError(
                "index.txt: %s uses the effect %r (a table slot names a music track)"
                % (what, nm)
            )
    tab = bytearray([0xFF] * TAB_N)
    for m, n in maptab.items():
        tab[TAB_MAP + m] = ids[n]
    if default_tr:
        for t in range(TAB_TRAINER + 1, TAB_N):
            tab[t] = ids[default_tr]
    for t, n in trtab.items():
        tab[TAB_TRAINER + t] = ids[n]
    body = bytearray()
    index = bytearray()
    info = []
    base = IDX_OFF + IDX_SIZE * len(names)
    for i, name in enumerate(names):
        path = SRC / (name + ".txt")
        is_fx = i >= len(music)
        if extra and name in extra:
            t = parse_track(name, extra[name], is_fx)
        elif not path.is_file():
            raise MuscError(
                "%s: track source assets/music/%s.txt is missing" % (name, name)
            )
        else:
            t = parse_track(name, path.read_text(encoding="utf-8"), is_fx)
        rec = record(t, is_fx)
        start = base + len(body)
        if start > 0xFFFF:
            raise MuscError("music.bin is over 64 KB")
        body += rec
        index += struct.pack("<HH", start, len(rec))
        looped = rec[1] != NO_LOOP
        first, lp = _passes(Rec(rec))
        info.append(
            dict(
                id=i,
                name=name,
                fx=is_fx,
                bytes=len(rec),
                events=len(t.events),
                notes=t.notes,
                insts=len(t.insts),
                intro_s=(first - lp) / 100 if looped else 0,
                loop_s=lp / 100 if looped else first / 100,
                loops=looped,
                once=not looped,
                vol=t.vol,
                title=t.title,
            )
        )
    out = (
        b"NMUS"
        + bytes([VERSION, len(names), 0, 0])
        + bytes(tab)
        + bytes(index)
        + bytes(body)
    )
    h = [
        "// generated by tools/musc.py from assets/music/ - do not edit (contract section 28)",
        "#pragma once",
    ]
    for i, n in enumerate(music):
        h.append("#define SND_%s %d" % (n.upper(), i))
    h.append(
        "#define SND_FX0 %d /* the first effect: ids below are music */" % len(music)
    )
    for i, n in enumerate(fx):
        h.append("#define SND_FX_%s %d" % (n.upper(), len(music) + i))
    h += [
        "#define SND_COUNT %d" % len(names),
        "#define SND_TAB_OFF %d /* the map / trainer table in music.bin */" % TAB_OFF,
        "#define SND_TAB_MAP %d /* + map id */" % TAB_MAP,
        "#define SND_TAB_TRAINER %d /* + trainer id */" % TAB_TRAINER,
        "#define SND_TAB_N %d /* slots in the table */" % TAB_N,
        "#define SND_IDX_OFF %d /* the track index (4 bytes per track) */" % IDX_OFF,
    ]
    return dict(
        bin=out, header="\n".join(h) + "\n", info=info, tab=bytes(tab), names=names
    )


# ------------------------------------------------------------------------------------------------ reading a compiled file back
class Rec:
    """A decoded record: what the engine reads from its heap block."""

    def __init__(self, raw):
        self.raw = bytes(raw)
        self.ne, self.loop, self.vol = raw[0], raw[1], raw[2]
        self.ev = [(raw[3 + 2 * i], raw[4 + 2 * i]) for i in range(self.ne)]
        ia = 3 + 2 * self.ne
        self.insts = []
        for i in range(raw[ia]):  # (the first offset is also the number of offsets)
            q = ia + raw[ia + i]
            n, lp, pitch = raw[q] & 31, raw[q + 1], raw[q + 2]
            steps = [
                (raw[q + 3 + 2 * j], struct.unpack("b", bytes([raw[q + 4 + 2 * j]]))[0])
                for j in range(n)
            ]
            self.insts.append(
                dict(
                    n=n,
                    slow=raw[q] >> 7,
                    lp=lp,
                    pitch=pitch,
                    steps=steps,
                    end=q + 3 + 2 * n,
                )
            )
        self.area_end = max(i["end"] for i in self.insts)


def decode(blob):
    """music.bin -> list of dict(start, size, loop, vol, rec=Rec); validates the layout."""
    if blob[:4] != b"NMUS" or blob[4] != VERSION:
        raise MuscError("music.bin: bad magic / version")
    n = blob[5]
    tracks = []
    for i in range(n):
        start, size = struct.unpack_from("<HH", blob, IDX_OFF + IDX_SIZE * i)
        if (
            start < IDX_OFF + IDX_SIZE * n
            or start + size > len(blob)
            or size < 3 + 2 + 1 + 5
        ):
            raise MuscError("music.bin: track %d: bad index entry" % i)
        raw = blob[start : start + size]
        r = Rec(raw)
        if (
            r.ne == 0
            or r.vol == 0
            or (r.loop != NO_LOOP and r.loop >= r.ne)
            or r.area_end != size
        ):
            raise MuscError("music.bin: track %d: bad record header" % i)
        for j, (a, b) in enumerate(r.ev):
            nib = a & 15
            if nib < 12:
                if a and ((a >> 4) < 1 or (a >> 4) > 6):
                    raise MuscError("music.bin: track %d: bad note %02x" % (i, a))
                if b < 1:
                    raise MuscError("music.bin: track %d: a note of 0 ticks" % i)
            elif nib == 12:
                if (a >> 4) >= len(r.insts) or not r.insts[a >> 4]["pitch"] or b < 1:
                    raise MuscError("music.bin: track %d: bad drum" % i)
            elif nib == 13:
                if (a >> 4) >= len(r.insts) or r.insts[a >> 4]["pitch"]:
                    raise MuscError("music.bin: track %d: bad INS" % i)
            elif nib == 15:
                if not 2 <= (a >> 4) <= 15 or not 1 <= b <= j:
                    raise MuscError("music.bin: track %d: bad REP" % i)
            else:
                raise MuscError("music.bin: track %d: unknown command" % i)
        for ins in r.insts:
            if not 1 <= ins["n"] <= MAX_STEPS or ins["lp"] >= ins["n"]:
                raise MuscError("music.bin: track %d: bad instrument" % i)
        tracks.append(
            dict(
                start=start,
                size=size,
                loop=None if r.loop == NO_LOOP else r.loop,
                vol=r.vol,
                rec=r,
            )
        )
    return tracks


# ---- the tick machine: a line-by-line port of tick_body() in nucleo/sound.c (one voice)
class Machine:
    def __init__(self, rec, low=False):
        self.r = rec
        self.low = low
        self.i = self.k = self.len = self.p = self.cur = self.ins = self.rep = 0
        self.done = False

    def tick(self):
        """-> (frequency in Hz or 0, volume 0..255) the speaker has during this 10 ms tick."""
        r = self.r
        if self.done:
            return 0, 0
        self.k = (self.k + 1) & 255
        if self.k >= self.len:
            while True:
                if self.i >= r.ne:
                    if r.loop == NO_LOOP:
                        self.done = True
                        return 0, 0
                    self.i = r.loop
                a, b = r.ev[self.i]
                self.i += 1
                x, n = a >> 4, a & 15
                if n <= 12:
                    if n == 12:
                        self.p, self.cur = r.insts[x]["pitch"], x
                    else:
                        self.p, self.cur = a, self.ins
                    self.len, self.k = b, 0
                    break
                if n == 13:
                    self.ins = x
                else:
                    if not self.rep:
                        self.rep = x
                    self.rep = (self.rep - 1) & 255
                    if self.rep:
                        self.i -= b + 1
        ins = r.insts[self.cur]
        k, p = self.k, self.p
        j = k >> ins["slow"]
        f = e = 0
        if p and (ins["pitch"] or k + 1 < self.len or self.len == 1):
            n, lp = ins["n"], ins["lp"]
            if j >= n:
                j = lp + (j - lp) % (n - lp)
            sv, sd = ins["steps"][j]
            f = (NOTE7[p & 15] << (p >> 4)) >> 5
            f += (f * sd) >> 7
            e = (((r.vol >> 1) if self.low else r.vol) + 1) * sv >> 8
        if f < 100 or not e:
            f = 0
        return f, e


def _passes(r):
    """Walks the event stream (REP included) and returns (first pass ticks, loop pass ticks)."""
    i, rep, ticks, passes = 0, 0, 0, []
    guard = 0
    while guard < 100000:
        guard += 1
        if i >= r.ne:
            passes.append(ticks)
            ticks = 0
            if r.loop == NO_LOOP or len(passes) == 2:
                break
            i = r.loop
        a, b = r.ev[i]
        i += 1
        x, n = a >> 4, a & 15
        if n <= 12:
            ticks += b
        elif n == 15:
            if not rep:
                rep = x
            rep -= 1
            if rep:
                i -= b + 1
    if len(passes) == 1:
        return passes[0], passes[0]
    return passes[0], passes[1]


def device_ticks(raw, n, low=False):
    """The first n ticks of the tick machine: [(frequency, volume)]."""
    m = Machine(Rec(raw), low)
    return [m.tick() for _ in range(n)]


def render(tr, max_s=75, loops=2):
    """-> (samples as bytes of int16, seconds): the intro once, the loop body `loops` times (a looping track stops at max_s).

    A tick of (f Hz, volume v) is a pulse train of duty v / 255 / 2, like the piezo driver's PWM (a quiet note is a thin pulse: thinner and fainter).
    """
    raw = tr["rec"].raw
    intro_loop, loop_pass = _passes(tr["rec"])
    looped = tr["loop"] is not None
    if looped:
        # the first pass is intro + loop body; the loop body alone is loop_pass
        n = min(intro_loop - loop_pass + loops * loop_pass, max_s * 100)
    else:
        n = intro_loop + 5
    ticks = device_ticks(raw, n)
    out = bytearray()
    amp = 0.35 * 32767
    per_tick = SAMPLE_RATE * TICK_MS // 1000
    phase = 0
    prev = (0, 0)
    for f, v in ticks:
        if (f, v) != prev and f != prev[0]:
            phase = 0
        prev = (f, v)
        if not f:
            out += b"\x00\x00" * per_tick
            continue
        duty = v / 255.0 / 2
        hi, lo = struct.pack("<h", int(amp * (1 - duty))), struct.pack(
            "<h", int(-amp * duty)
        )
        thr = int(duty * SAMPLE_RATE)
        out += b"".join(
            [
                hi if ((phase + i) * f) % SAMPLE_RATE < thr else lo
                for i in range(per_tick)
            ]
        )
        phase += per_tick
    return bytes(out), len(ticks) * TICK_MS / 1000


def write_wavs(res, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    tracks = decode(res["bin"])
    for name, tr in zip(res["names"], tracks):
        data, secs = render(tr)
        with wave.open(str(out_dir / (name + ".wav")), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(SAMPLE_RATE)
            w.writeframes(data)
        print(
            "  %-16s %6.1f s  %s"
            % (
                name + ".wav",
                secs,
                "(intro + 2 loops)" if tr["loop"] is not None else "",
            )
        )


def write_ticks(res, path):
    """One line per track: `id name n`, then a line of n `frequency:volume` pairs (the ticks of the first pass plus two loop passes)."""
    tracks = decode(res["bin"])
    lines = []
    for i, (name, tr) in enumerate(zip(res["names"], tracks)):
        r = tr["rec"]
        first, lp = _passes(r)
        n = first + (2 * lp if tr["loop"] is not None else 0) + 8
        t = device_ticks(r.raw, n)
        lines.append("%d %s %d" % (i, name, n))
        lines.append(" ".join("%d:%d" % x for x in t))
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(lines) + "\n")


# ------------------------------------------------------------------------------------------------ commands
def print_info(res):
    print(
        "%2s  %-16s %6s %5s %5s %5s %7s %7s  %s"
        % (
            "id",
            "track",
            "events",
            "insts",
            "notes",
            "bytes",
            "intro s",
            "loop s",
            "kind",
        )
    )
    for r in res["info"]:
        kind = "fx" if r["fx"] else ("loop" if r["loops"] else "once")
        print(
            "%2d  %-16s %6d %5d %5d %5d %7.2f %7.2f  %s  vol %d"
            % (
                r["id"],
                r["name"],
                r["events"],
                r["insts"],
                r["notes"],
                r["bytes"],
                r["intro_s"],
                r["loop_s"],
                kind,
                r["vol"],
            )
        )
    print("music.bin: %d bytes, %d tracks" % (len(res["bin"]), len(res["info"])))


def cmd_build():
    res = compile_all()
    OUT_BIN.write_bytes(res["bin"])
    OUT_GEN.write_text(res["header"])
    print_info(res)
    return 0


def cmd_check():
    res = compile_all()
    decode(res["bin"])
    bad = 0
    for path, want in ((OUT_BIN, res["bin"]), (OUT_GEN, res["header"].encode())):
        if not path.is_file() or path.read_bytes() != want:
            print(
                "ERROR: %s is missing or out of date (run tools/musc.py)"
                % path.relative_to(ROOT)
            )
            bad += 1
    if not bad:
        print(
            "musc check: %d tracks, %d bytes OK" % (len(res["info"]), len(res["bin"]))
        )
    return 1 if bad else 0


def cmd_selftest():
    pb = pitch_byte
    # frequencies of the integer table: C3 130, A3 220, C4 261, A4 440, A5 880, B8 7902; octaves double (+- the integer rounding)
    assert [hz(pb(a)) for a in (0, 9, 12, 21, 33, 71)] == [
        130,
        220,
        261,
        440,
        880,
        7902,
    ]
    assert all(hz(pb(a + 12)) in (2 * hz(pb(a)), 2 * hz(pb(a)) + 1) for a in range(60))
    assert (
        pb(0) == 0x10
        and pb(71) == 0x6B
        and len({pb(a) for a in range(72)}) == 72
        and 0 not in {pb(a) for a in range(72)}
    )

    def events(src, fx=False):
        return list(parse_track("x", src, fx).events)

    def notes(src):  # the sounding events only, as (pitch, ticks), the REPs played out
        ev, out, i, rep = events(src), [], 0, 0
        while i < len(ev):
            a, b = ev[i]
            i += 1
            if (a & 15) < 12:
                out.append((a, b))
            elif (a & 15) == 15:
                rep = rep or a >> 4
                rep -= 1
                if rep:
                    i -= b + 1
        return out

    C4, D4, B3, C5 = pb(12), pb(14), pb(11), pb(24)
    # lengths: tempo 120 -> a quarter is 500 ms = 50 ticks; dots, ties, rests, tick lengths
    assert notes("tempo 120\no4 c4") == [(C4, 50)]
    assert notes("tempo 120\no4 c4. r8 d=7") == [(C4, 75), (0, 25), (D4, 7)]
    assert notes("tempo 120\no4 c4&c8 c4&8") == [(C4, 75), (C4, 75)]
    assert notes("tempo 120\nl8 o3 b o4 c <b >>c") == [
        (B3, 25),
        (C4, 25),
        (B3, 25),
        (C5, 25),
    ]
    assert notes("tempo 120\no4 [ c8 d8 ]3") == [(C4, 25), (D4, 25)] * 3
    nested = notes("tempo 120\no4 [ c8 [ d16 ]2 ]2")
    assert (
        len(nested) == 6 and sum(n[1] for n in nested) == 100
    )  # (boundary rounding: 12 / 13 ticks, the sum is exact)
    assert sum(
        n[1] for n in notes("tempo 140\no4 l16 " + "c d e f g a b >c< " * 3)
    ) in range(250, 265)
    assert notes("tempo 120\nl4. o4 c d") == [(C4, 75), (D4, 75)]
    # sharps / flats (also across an octave: b+ is c of the next octave, c- is b of the one below) / the bar check
    assert notes("o4 c+8 d-8")[0][0] == notes("o4 c+8 d-8")[1][0] == pb(13)
    assert [n[0] for n in notes("o4 b+8 c-8")] == [pb(24), pb(11)]
    parse_track("x", "o4 c4 d4 e4 f4 | g1 |")
    # rounding does not accumulate: 16 sixteenths at an awkward tempo sum to the exact duration
    t = parse_track("x", "tempo 133\no4 " + "c16 " * 16)
    assert abs(sum(e[1] for e in t.events) * 10 - 16 * 240000 / 133 / 16) <= 5, t.events
    # loop point
    t = parse_track("x", "tempo 120\no4 l8 c d e f @loop g a b >c< d")
    assert t.loop == 4 and len(t.events) == 9
    assert (
        parse_track("x", "o4 c4 d4").loop == 0
        and parse_track("x", "once\no4 c4 d4").loop == NO_LOOP
    )
    assert (
        record(parse_track("x", "o4 c4 d4", True), True)[1] == NO_LOOP
    )  # an effect never loops
    # a plain old track: no commands, one instrument (the built-in plain)
    t = parse_track("x", "o4 c4 d4")
    assert (
        len(t.insts) == 1
        and t.insts[0].name == "plain"
        and t.insts[0].blob() == bytes([1, 0, 0, 255, 0])
    )
    # instruments: INS only where the instrument changes, the first melodic note's is number 0, a drum is a command and does not change it
    src = "inst pl att=0 dec=6 sus=0.2\ninst kk drum=c5 v=255,200 d=0,-40\ntempo 120\no5 l8 c d @pl e f !kk8 g a @plain b"
    t = parse_track("x", src)
    assert [i.name for i in t.insts] == ["plain", "pl", "kk"]
    cmds = [(a & 15, a >> 4, b) for a, b in t.events if (a & 15) >= 12]
    assert cmds == [(13, 1, 0), (12, 2, 2), (13, 0, 0)], cmds
    assert (
        sum(b for a, b in t.events if (a & 15) != 13) == 8 * 25
    ), t.events  # (the drum slot adds up: the hit and the silence)
    # a drum hit at the end / start of a note or rest steals its last / first ticks; the note keeps its place in the bar
    t = parse_track(
        "x",
        "inst h drum=c8 v=100,60 d=0,-10\ntempo 120\no5 c8!h d8 !h:e8 r8!h r8 !h:r8",
    )
    cmds = [(a & 15, b) for a, b in t.events]
    assert cmds == [
        (0, 23),
        (12, 2),
        (2, 25),
        (12, 2),
        (4, 23),
        (0, 23),
        (12, 2),
        (0, 25),
        (12, 2),
        (0, 23),
    ], cmds
    assert sum(b for a, b in t.events) == 6 * 25
    for src in (
        "inst h drum=c8\ntempo 400\no5 c64!h",
        "o5 c4&c4!h",
        "inst h drum=c8\no5 c4&c4!h",
        "inst h drum=c8\no5 !h:",
        "o5 c4!nosuch",
        "inst h drum=c8\no5 !h:!h",
        "inst h drum=c8\no5 c4 &!h",
    ):
        try:
            parse_track("x", src)
        except MuscError:
            continue
        raise AssertionError("accepted bad input: %r" % src)
    # a track that begins with another instrument starts on it (number 0, no INS), and a looping track that ends on another one re-selects at the loop
    t = parse_track("x", "inst a att=1\ntempo 120\n@a o4 c4 d4")
    assert t.insts[0].name == "a" and not any((e[0] & 15) == 13 for e in t.events)
    t = parse_track("x", "inst a att=1\nonce\ntempo 120\no4 c4 @a d4")
    assert (
        [(e[0] & 15) for e in t.events] == [0, 13, 2]
        and t.events[0][0] == pb(12)
        and (t.events[1][0] >> 4) == 1
    )
    t = parse_track(
        "x", "inst a att=1\ntempo 120\no4 c4 @loop d4 @a e4"
    )  # loops back to d, written for plain: it is selected again
    assert (t.events[t.loop][0] & 15, t.events[t.loop][0] >> 4) == (13, 0), t.events
    t = parse_track(
        "x", "inst a att=1\ntempo 120\no4 c4 @a d4"
    )  # a loop at the start of a track that ends on `a`
    assert (t.events[0][0] & 15, t.events[0][0] >> 4) == (13, 0), t.events
    # REP: a repeated section that ends as it began is stored once
    t = parse_track("x", "tempo 120\no4 [ c8 d8 e8 ]4 f4")
    assert len(t.events) == 5 and t.events[3] == (15 | 4 << 4, 3), t.events
    t2 = parse_track(
        "x", "tempo 120\no4 [ c8 > d8 e8 ]3 f4"
    )  # leaves the octave changed: unrolled (and each copy is an octave higher)
    assert len(t2.events) == 10 and [e[0] >> 4 for e in t2.events[::3]][:3] == [2, 3, 4]
    t3 = parse_track(
        "x", "tempo 120\no4 [ c8 [ d16 e16 ]2 ]2"
    )  # nested: the inner one is a REP
    assert sum(1 for e in t3.events if (e[0] & 15) == 15) == 2 and all(
        e[1] > 0 for e in t3.events
    )
    # the machine plays a REP like the unrolled text (tempo 150: every length here is whole ticks)
    for src in (
        "tempo 150\no4 [ c8 d8 e8 ]4 f4",
        "tempo 150\no4 [ c8 [ d16 e16 ]3 f8 ]2 g4",
        "inst k drum=c5 v=255,200 d=0,-30\ntempo 150\no4 [ c8 !k8 e8 ]3 f4",
    ):
        a = device_ticks(record(parse_track("x", src), False), 600)
        b = device_ticks(_unrolled(src), 600)
        assert a == b, src
    # the instrument generator: a pluck decays, a swell rises, a vibrato loops, an arpeggio cycles the offsets, a plain note is steady
    pl = make_inst("p", dict(att="0", dec="5", sus="0.2"), set())
    assert (
        pl.steps[0][0] == 255
        and pl.steps[-1][0] == round(255 * 0.2)
        and pl.loop == len(pl.steps) - 1
        and all(a[0] >= b[0] for a, b in zip(pl.steps, pl.steps[1:]))
    )
    sw = make_inst("s", dict(att="12"), set())
    assert sw.steps[0][0] < sw.steps[5][0] < sw.steps[-1][0] == 255
    vb = make_inst("v", dict(vib="2", vdelay="6", vper="8"), set())
    assert (
        len(vb.steps) == 14
        and vb.loop == 6
        and vb.steps[0][1] == 0
        and any(d > 0 for _, d in vb.steps)
        and any(d < 0 for _, d in vb.steps)
    )
    ar = make_inst("a", dict(arp="0,4,7", rate="2"), set())
    assert (
        [d for _, d in ar.steps] == [0, 0, 33, 33, 64, 64]
        and ar.loop == 0
        and len(ar.steps) == 6
    )
    assert make_inst("a", dict(arp="0,-12"), set()).steps[-1][1] == -64
    dr = make_inst("d", dict(drum="c5", fall="40", noise="10", seed="3"), set())
    assert (
        dr.pitch == pb(24) and len(dr.steps) == 3 and dr.steps[0][1] != dr.steps[1][1]
    )
    # bad input -> MuscError, never a traceback
    bad = [
        "o4 h4",
        "o4 c4 |  d8 |",
        "o9 c4",
        "o2 c4",
        "o4 c4&d4",
        "o4 [ c4",
        "o4 c4 ]",
        "o4 c3",
        "tempo 5\nc4",
        "tempo 120\nvol 0",
        "vol 300",
        "meter 5/3",
        "o4 c=300",
        "o4 c4=5",
        "tempo 30\no4 c1&c1&c1",
        "",
        "tempo 120",
        "o4 c4 @loop @loop d4",
        "o4 [ @loop c4 ]",
        "o4 c4 @loop",
        "once\no4 c4 @loop d4",
        "o4 &c4",
        "o4 c4 zz",
        "o4 r4&c4",
        "o8 b+4",
        "o3 c-4",
        "o4 c4 @nosuch",
        "o4 c4 !nosuch",
        "inst a att=x\no4 c4",
        "inst a bogus=1\no4 c4",
        "inst a slow2\no4 c4",
        "inst plain att=1\no4 c4",
        "inst a\ninst a\no4 c4",
        "inst d drum=c5\no4 c4 @d",
        "inst d drum=c9\no4 c4",
        "inst a v=1,2 d=1\no4 c4",
        "inst a v=300\no4 c4",
        "inst a arp=0,12\no4 c4",
        "inst a arp=0,4 vib=1\no4 c4",
        "inst a dec=200\no4 c4",
        "inst a v=" + ",".join(["255"] * 40) + "\no4 c4",
        "inst d drum=c5\no4 !d0",
        "tempo 120\no4 " + "c32 d32 " * 140,  # too many events
    ]
    for src in bad:
        try:
            parse_track("x", src)
        except MuscError:
            continue
        raise AssertionError("accepted bad input: %r" % src)
    # a full compile round trip on the real sources: the decoded events are the compiled events, loops and sizes agree
    res = compile_all()
    dec = decode(res["bin"])
    music, fx, *_ = load_index()
    assert len(dec) == len(music) + len(fx) and res["bin"][5] == len(dec)
    for i, name in enumerate(music + fx):
        t = parse_track(
            name, (SRC / (name + ".txt")).read_text(encoding="utf-8"), i >= len(music)
        )
        r = dec[i]["rec"]
        assert r.ev == [tuple(e) for e in t.events], name
        assert dec[i]["loop"] == (None if (i >= len(music) or t.once) else t.loop), name
        assert (
            dec[i]["size"] <= (MAX_FX if i >= len(music) else MAX_MUSIC)
            and dec[i]["vol"] == t.vol
        ), name
        assert i < len(music) or r.loop == NO_LOOP, name  # effects play once
        assert [(i["steps"], i["lp"], i["pitch"]) for i in r.insts] == [
            (i.steps, i.loop, i.pitch) for i in t.insts
        ], name
    # format 3 table: 256 map slots + 256 trainer slots, in the file and in the compile result; map ids up to 255 work; the default trainer fills every slot but the wild one
    assert (
        res["bin"][4] == VERSION == 3
        and len(res["tab"]) == TAB_N == 512
        and TAB_TRAINER == 256
        and IDX_OFF == TAB_OFF + TAB_N == 520
    )
    assert res["bin"][TAB_OFF : TAB_OFF + TAB_N] == res["tab"]
    tnames = res["names"]
    assert (
        res["tab"][TAB_MAP + 13] == tnames.index("ruta3")
        and res["tab"][TAB_MAP + 255] == 0xFF
        and res["tab"][TAB_MAP + 199] == 0xFF
    )
    assert res["tab"][TAB_TRAINER] == tnames.index("battle_wild") and all(
        res["tab"][TAB_TRAINER + t] != 0xFF for t in range(256)
    )
    global MAPS_TXT
    import tempfile

    old_maps = MAPS_TXT
    with tempfile.TemporaryDirectory() as td:
        MAPS_TXT = Path(td) / "maps.txt"
        MAPS_TXT.write_text("0 a_map\n130 b_map\n255 c_map\n")
        try:
            r2 = compile_all(
                index_text="music title\nmap a_map title\nmap b_map title\nmap c_map title\ntrainer * title\n"
            )
            assert (
                r2["tab"][0] == r2["tab"][130] == r2["tab"][255] == 0
                and r2["tab"][1] == 0xFF
                and r2["tab"][TAB_TRAINER + 255] == 0
            )
            assert (
                r2["tab"][TAB_TRAINER] == 0xFF
            )  # (slot 0 of the trainers is the wild battle: only named by `trainer wild`)
        finally:
            MAPS_TXT = old_maps
    # the tick machine: a note of 3 ticks sounds 2 ticks, a note of 1 tick sounds 1, a rest is silent; the volume is (vol + 1) * step >> 8
    rec = record(parse_track("x", "vol 200\no4 c=3 r=2 d=1"), True)
    assert device_ticks(rec, 8) == [
        (261, 200),
        (261, 200),
        (0, 0),
        (0, 0),
        (0, 0),
        (293, 200),
        (0, 0),
        (0, 0),
    ]
    assert device_ticks(rec, 1, low=True) == [(261, 100)]
    # a drum is sounding for all its ticks and falls in pitch; a pluck fades; the arpeggio changes the pitch every `rate` ticks
    rec = record(
        parse_track("x", "inst k drum=c5 v=255,200,100 d=0,-32,-64\nvol 255\n!k=3 r=2"),
        True,
    )
    t = device_ticks(rec, 5)
    assert (
        [f for f, v in t[:3]] == [523, 392, 261]
        and [v for f, v in t[:3]] == [255, 200, 100]
        and t[3:] == [(0, 0)] * 2
    ), t
    rec = record(
        parse_track("x", "inst a arp=0,4,7 rate=2\nvol 255\n@a o4 c=12 r=1"), True
    )
    assert [f for f, v in device_ticks(rec, 12)[:11]] == [
        261,
        261,
        328,
        328,
        391,
        391,
        261,
        261,
        328,
        328,
        391,
    ], device_ticks(rec, 12)
    # WAV: 44100 Hz, a tick is 441 samples, the file length matches
    data, secs = render(
        dict(
            rec=Rec(record(parse_track("x", "o4 a=10 r=5"), True)),
            loop=None,
            start=0,
            vol=255,
        )
    )
    assert len(data) == 2 * 441 * 20 and abs(secs - 0.20) < 1e-9, (len(data), secs)
    # too long a track, a missing track source, bad index lines
    try:
        compile_all(
            index_text=(SRC / "index.txt").read_text() + "music " + "x" * 3 + "\n",
            extra={"x" * 3: "tempo 600\no4 " + "c16 " * 400},
        )
        raise AssertionError("accepted a huge track")
    except MuscError:
        pass
    for bad_idx in (
        "music nosuch_track_zz\n",
        "music a\nmusic a\n",
        "fx a\nmusic b\n",
        "map nowhere x\n",
        "bogus\n",
        "music title\nmap villa_aliso nosuch\n",
        "music title\nfx heal\nmap villa_aliso heal\n",
    ):
        try:
            compile_all(index_text=bad_idx)
        except MuscError:
            continue
        raise AssertionError("accepted index %r" % bad_idx)
    print("musc selftest: OK (%d tracks, %d bytes)" % (len(dec), len(res["bin"])))
    return 0


# ------------------------------------------------------------------------------------------------ the engine test fixture
# Adversarial tracks for tools/host_world/sound_check.c: every feature at its limits (the engine's speaker calls must equal this machine's ticks).
FIXTURE = {
    # long notes, vibrato with a delay, a slow swell, a pluck, ties, 1-tick notes, a note of 255 ticks
    "f0": """title vibrato, swell, pluck, long notes
tempo 150
inst vib att=2 dec=10 sus=0.7 vib=2 vdelay=6 vper=8
inst swl att=40 slow
inst plk dec=4 sus=0.1
vol 200
o5 @vib c2 d4&d4 e=1 f=1 g=1 @swl a1 @plk b8 b8 b8 r=3 b=2 >c=255 r=255 r=45 <@plain c4 @loop g4 e8 c8 g2
""",
    # arpeggios at the three rates, up and down offsets, ending in another instrument than the one it begins with
    "f1": """title arpeggios
tempo 180
inst maj arp=0,4,7 rate=1
inst min arp=0,3,7,3 rate=2
inst low arp=0,-12,-5 rate=4
inst slw arp=0,5,9 rate=2 slow
vol 150
o5 @maj c4 e4 @min a4 @low >c4 @slw <g2 @maj c8 d8 @loop @min e4 g4 @low a2
""",
    # drums: slots, exact ticks, noise, a REP of 15, a nested REP, a REP of one event, REP inside a loop body
    "f2": """title drums and repeats
tempo 150
inst kick drum=c5 v=255,200,120 d=0,-40,-80
inst snr drum=f6 noise=30 seed=5 fall=10
inst hat drum=c8 v=120,60 d=0,-20 slow
inst bass dec=3 sus=0.3
vol 220
o5 @bass [ !kick8 !hat16 !hat16 !snr8 !hat8 ]15 [ [ c16 !hat=2 e16 ]3 !kick4 ]2 [ g8 ]5 !snr=1 !kick=7 @loop [ c8 !hat=1 d8 !snr8 ]4 e4
""",
    # a 31-step instrument that loops in the middle, a slow one, a 31 step drum
    "f3": """title long instruments
tempo 120
inst big v=255,250,245,240,235,230,225,220,215,210,205,200,195,190,185,180,175,170,165,160,155,150,145,140,135,130,125,120,115,110,105 d=0,1,2,3,2,1,0,-1,-2,-3,-2,-1,0,1,2,3,2,1,0,-1,-2,-3,-2,-1,0,1,2,3,2,1,0
inst slw v=255,100,255,100,255,100,255 d=0,5,0,-5,0,5,0 slow
inst sweep drum=c6 v=255,230,200,170,140,110,80,50,20 d=0,-10,-20,-30,-40,-50,-60,-70,-80
vol 255
o4 @big c1 @slw e2 !sweep4 @big g=7 !sweep=40 @plain a4
""",
    # an instrument change on every note, a loop in the middle of a track that ends on another instrument
    "f4": """title switching
tempo 200
inst a dec=2 sus=0.2
inst b att=3
inst c vib=1 vdelay=2 vper=4
vol 180
o5 @a c8 @b d8 @c e8 @plain f8 @loop @b g8 @a a8 @c b8 @b >c8 @a <b8 @c a8
""",
    # a once track (a fanfare) with a rest longer than 2.55 s, and a drum first
    "f5": """title once
once
inst kick drum=a5 v=255,200 d=0,-50
vol 255
tempo 120
!kick8 r1 r1 r2 o5 a8 c8 e8 >a4 @plain <r=255 r=45
""",
    # a track with only drums, looping
    "f6": """title only drums
tempo 150
inst k drum=c5 v=255,150 d=0,-60
inst h drum=c8 v=100
inst thud drum=c4 v=255,200,120 d=0,-50,-80
vol 120
[ !k4 !h8 !h8 ]3 !k4 !thud4 !k4
""",
}


def fixture_sources():
    """-> (music names, fx names, sources): the fixture padded to the real number of music tracks (so the engine's SND_FX0 split fits) plus three effects."""
    n_music = len(load_index()[0])
    src = dict(FIXTURE)
    music = list(FIXTURE)
    pad = list(FIXTURE.values())
    while len(music) < n_music:
        name = "g%d" % len(music)
        src[name] = pad[len(music) % len(pad)]
        music.append(name)
    fx = ["e0", "e1", "e2"]
    src["e0"] = (
        FIXTURE["f2"]
        .replace(
            "[ !kick8 !hat16 !hat16 !snr8 !hat8 ]15",
            "[ !kick8 !hat16 !hat16 !snr8 !hat8 ]3",
        )
        .split("@loop")[0]
    )
    src["e1"] = (
        "tempo 180\ninst a arp=0,4,7 rate=1\ninst v vib=2 vdelay=3 vper=4\nvol 255\no5 @a c8 e8 @v g4 >c2"
    )
    src["e2"] = (
        "tempo 240\ninst k drum=c5 v=255,200,100 d=0,-30,-60\nvol 255\n!k=3 r=2 !k=3 o6 g=1 r=1 g=1"
    )
    return music, fx, src


def cmd_fixture(prefix):
    music, fx, src = fixture_sources()
    res = compile_all(
        index_text="".join("music %s\n" % n for n in music)
        + "".join("fx %s\n" % n for n in fx),
        extra=src,
    )
    Path(prefix + ".bin").parent.mkdir(parents=True, exist_ok=True)
    Path(prefix + ".bin").write_bytes(res["bin"])
    write_ticks(res, prefix + "_ticks.txt")
    print(
        "musc fixture: %d music + %d effects, %d bytes"
        % (len(music), len(fx), len(res["bin"]))
    )
    return 0


def _unrolled(src):
    """The record of the source with every repeat written out (REP never used): the machine must play it like the REP version."""

    def unroll(toks):
        out, i = [], 0
        while i < len(toks):
            if toks[i] == "[":
                depth, j = 1, i + 1
                while depth:
                    depth += (toks[j] == "[") - bool(re.fullmatch(r"\](\d*)", toks[j]))
                    j += 1
                n = int(toks[j - 1][1:] or 2)
                out += unroll(toks[i + 1 : j - 1]) * n
                i = j
            else:
                out.append(toks[i])
                i += 1
        return out

    lines = []
    for ln in src.splitlines():
        toks = strip_comment(ln)
        lines.append(
            " ".join(unroll(toks))
            if toks
            and toks[0] not in ("inst", "tempo", "vol", "meter", "once", "title")
            else ln
        )
    return record(parse_track("x", "\n".join(lines)), False)


def main(argv):
    try:
        if argv[:1] == ["selftest"]:
            return cmd_selftest()
        if argv[:1] == ["--wav"]:
            res = compile_all()
            out = Path(argv[1]) if len(argv) > 1 else WAV_DIR
            print(
                "rendering %d tracks to %s (44.1 kHz mono; looping music = intro + 2 loops)"
                % (len(res["names"]), out)
            )
            write_wavs(res, out)
            return 0
        if argv[:1] == ["--fixture"] and len(argv) == 2:
            return cmd_fixture(argv[1])
        if argv[:1] == ["--ticks"] and len(argv) == 2:
            write_ticks(compile_all(), argv[1])
            return 0
        if argv[:1] == ["check"]:
            return cmd_check()
        if argv[:1] == ["info"]:
            print_info(compile_all())
            return 0
        if not argv or argv == ["build"]:
            return cmd_build()
        print(__doc__)
        return 2
    except MuscError as e:
        print("musc: ERROR: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
