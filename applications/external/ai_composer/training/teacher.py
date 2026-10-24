"""Rule-based "music teacher": writes 8-bar melodies in five styles.

The neural network on the Flipper is trained only on melodies from this file,
so the project uses no third-party songs. Every melody is a list of
(pitch, duration) pairs:

  pitch     semitones above the tonic (-5 ... 19) or None for a rest
  duration  in sixteenth notes, one of DURATIONS; notes never cross a barline

All melodies are 4/4, 8 bars = 128 sixteenths, and end on the tonic.
"""
import random

DURATIONS = (1, 2, 3, 4, 6, 8, 12, 16)
PITCH_MIN, PITCH_MAX = -5, 19
BAR = 16
BARS = 8
STYLES = ("chiptune", "blues", "classical", "folk", "game", "phonk", "hiphop", "jazz")

MAJOR = (0, 2, 4, 5, 7, 9, 11)
NATURAL_MINOR = (0, 2, 3, 5, 7, 8, 10)
HARMONIC_MINOR = (0, 2, 3, 5, 7, 8, 11)
DORIAN = (0, 2, 3, 5, 7, 9, 10)
MIXOLYDIAN = (0, 2, 4, 5, 7, 9, 10)
PENTATONIC = (0, 2, 4, 7, 9)
BLUES = (0, 3, 5, 6, 7, 10)
PHRYGIAN = (0, 1, 3, 5, 7, 8, 10)
MINOR_PENTATONIC = (0, 3, 5, 7, 10)

# Chords as pitch classes relative to the tonic.
C = {
    "I": (0, 4, 7), "ii": (2, 5, 9), "iii": (4, 7, 11), "IV": (5, 9, 0), "V": (7, 11, 2),
    "vi": (9, 0, 4), "V7": (7, 11, 2, 5), "I7": (0, 4, 7, 10), "IV7": (5, 9, 0, 3),
    "i": (0, 3, 7), "iv": (5, 8, 0), "v": (7, 10, 2), "III": (3, 7, 10), "VI": (8, 0, 3),
    "VII": (10, 2, 5), "bVII": (10, 2, 5), "Vm": (7, 11, 2),
    "bII": (1, 5, 8),
    # Seventh chords for jazz
    "Imaj7": (0, 4, 7, 11), "ii7": (2, 5, 9, 0), "iii7": (4, 7, 11, 2), "vi7": (9, 0, 4, 7),
    "VI7": (9, 1, 4, 7), "IVmaj7": (5, 9, 0, 4), "i7": (0, 3, 7, 10), "iv7": (5, 8, 0, 3),
}

STYLE_RULES = {
    "chiptune": {
        "scales": [(MAJOR, 0.5), (NATURAL_MINOR, 0.5)],
        "progressions": {
            MAJOR: [["I", "V", "vi", "IV"] * 2, ["vi", "IV", "I", "V"] * 2, ["I", "IV", "V", "I", "vi", "IV", "V", "I"]],
            NATURAL_MINOR: [["i", "VI", "III", "VII"] * 2, ["i", "VII", "VI", "VII"] * 2, ["i", "iv", "VII", "III", "VI", "iv", "v", "i"]],
        },
        "rhythms": [[2] * 8, [1, 1, 2, 2, 2, 4, 4], [2, 2, 4, 2, 2, 4], [4, 2, 2, 4, 4], [1, 1, 1, 1, 2, 2, 4, 4],
                    [2, 2, 2, 2, 4, 4], [3, 3, 2, 3, 3, 2]],
        "range": (-5, 19), "rest": 0.04, "arpeggio": 0.6, "step": 0.3, "octave_jump": 0.1,
        "repeat": [(2, 0), (6, 4)], "end_len": [4, 8],
    },
    "blues": {
        "scales": [(BLUES, 1.0)],
        "progressions": {BLUES: [["I7", "IV7", "I7", "I7", "IV7", "IV7", "I7", "V7"],
                                 ["I7", "V7", "IV7", "IV7", "I7", "V7", "I7", "V7"]]},
        "rhythms": [[3, 1, 3, 1, 8], [3, 1, 3, 1, 4, 4], [4, 3, 1, 8], [2, 2, 3, 1, 8], [8, 3, 1, 4],
                    [3, 1, 4, 3, 1, 4], [6, 2, 8]],
        "range": (-5, 12), "rest": 0.18, "arpeggio": 0.2, "step": 0.75, "octave_jump": 0.0,
        "repeat": [(2, 0), (4, 0)], "end_len": [8, 12],
    },
    "classical": {
        "scales": [(MAJOR, 0.7), (HARMONIC_MINOR, 0.3)],
        "progressions": {
            MAJOR: [["I", "IV", "V", "I", "vi", "ii", "V", "I"], ["I", "V", "I", "IV", "I", "ii", "V", "I"],
                    ["I", "vi", "IV", "V", "I", "IV", "V", "I"]],
            HARMONIC_MINOR: [["i", "iv", "Vm", "i", "VI", "iv", "Vm", "i"], ["i", "Vm", "i", "iv", "i", "iv", "Vm", "i"]],
        },
        "rhythms": [[4, 4, 4, 4], [4, 2, 2, 4, 4], [6, 2, 4, 4], [2, 2, 2, 2, 4, 4], [4, 4, 8], [6, 2, 8], [2, 2, 4, 8]],
        "range": (0, 16), "rest": 0.0, "arpeggio": 0.15, "step": 0.8, "octave_jump": 0.0,
        "repeat": [(4, 0), (5, 1)], "end_len": [8, 16], "half_cadence": True,
    },
    "folk": {
        "scales": [(PENTATONIC, 0.4), (MIXOLYDIAN, 0.3), (DORIAN, 0.3)],
        "progressions": {
            PENTATONIC: [["I", "IV", "I", "V", "I", "IV", "V", "I"], ["I", "I", "IV", "I", "I", "V", "IV", "I"]],
            MIXOLYDIAN: [["I", "bVII", "I", "V", "I", "bVII", "IV", "I"], ["I", "IV", "I", "bVII", "I", "IV", "bVII", "I"]],
            DORIAN: [["i", "IV", "i", "VII", "i", "IV", "VII", "i"], ["i", "VII", "i", "IV", "i", "VII", "IV", "i"]],
        },
        "rhythms": [[2] * 8, [3, 1, 2, 2, 4, 4], [2, 2, 2, 2, 4, 4], [4, 2, 2, 2, 2, 4], [3, 1, 3, 1, 2, 2, 4],
                    [2, 2, 4, 2, 2, 4]],
        "range": (0, 14), "rest": 0.02, "arpeggio": 0.25, "step": 0.7, "octave_jump": 0.0,
        "repeat": [(2, 0), (6, 0), (3, 1)], "end_len": [4, 8],
    },
    "game": {
        "scales": [(NATURAL_MINOR, 0.4), (HARMONIC_MINOR, 0.2), (MAJOR, 0.4)],
        "progressions": {
            NATURAL_MINOR: [["i", "VI", "VII", "i", "i", "VI", "III", "VII"], ["i", "VII", "VI", "VII", "i", "VII", "VI", "v"]],
            HARMONIC_MINOR: [["i", "VI", "iv", "Vm", "i", "VI", "iv", "Vm"]],
            MAJOR: [["I", "V", "vi", "IV", "I", "V", "IV", "V"], ["I", "IV", "V", "I", "vi", "IV", "V", "I"]],
        },
        "rhythms": [[3, 1, 3, 1, 4, 4], [4, 4, 3, 1, 4], [2, 2, 2, 2, 8], [6, 2, 4, 4], [3, 3, 2, 8], [4, 2, 2, 8]],
        "range": (-5, 17), "rest": 0.05, "arpeggio": 0.35, "step": 0.45, "octave_jump": 0.05,
        "repeat": [(2, 0), (4, 0)], "end_len": [8, 16], "fanfare": True, "sequence_up": True,
    },
    # Memphis-style cowbell lines: dark modes, high register, staccato, looped riffs.
    "phonk": {
        "scales": [(PHRYGIAN, 0.5), (HARMONIC_MINOR, 0.3), (NATURAL_MINOR, 0.2)],
        "progressions": {
            PHRYGIAN: [["i", "bII", "i", "bII", "i", "bII", "iv", "i"], ["i", "i", "bII", "i"] * 2],
            HARMONIC_MINOR: [["i", "VI", "iv", "Vm"] * 2, ["i", "iv", "Vm", "i"] * 2],
            NATURAL_MINOR: [["i", "i", "VI", "VII"] * 2, ["i", "VI", "VII", "i"] * 2],
        },
        "rhythms": [[3, 3, 2, 3, 3, 2], [2, 2, 1, 1, 2, 2, 2, 4], [3, 3, 4, 2, 2, 2], [1, 1, 2, 1, 1, 2, 4, 4],
                    [2, 1, 1, 2, 1, 1, 4, 4], [3, 3, 2, 2, 2, 4]],
        "range": (0, 17), "rest": 0.14, "arpeggio": 0.3, "step": 0.5, "octave_jump": 0.15,
        "repeat": [(1, 0), (2, 0), (3, 1), (5, 4), (6, 4), (7, 5)], "end_len": [4, 8],
    },
    # Laid-back loops with space and syncopation.
    "hiphop": {
        "scales": [(MINOR_PENTATONIC, 0.5), (DORIAN, 0.3), (NATURAL_MINOR, 0.2)],
        "progressions": {
            MINOR_PENTATONIC: [["i", "iv", "i", "iv"] * 2, ["i", "VII", "VI", "VII"] * 2],
            DORIAN: [["i", "IV"] * 4, ["i7", "IV", "i7", "IV", "iv7", "IV", "i7", "i7"]],
            NATURAL_MINOR: [["i", "VI", "iv", "v"] * 2, ["i", "VI", "III", "VII"] * 2],
        },
        "rhythms": [[4, 2, 2, 6, 2], [3, 1, 4, 4, 4], [2, 4, 2, 4, 4], [6, 2, 2, 2, 4], [4, 4, 3, 1, 4],
                    [8, 2, 2, 4], [3, 3, 2, 8]],
        "range": (-5, 12), "rest": 0.2, "rest_any": True, "arpeggio": 0.25, "step": 0.6,
        "octave_jump": 0.0, "repeat": [(2, 0), (3, 1), (6, 0), (7, 1)], "end_len": [8, 12],
    },
    # Swing feel (dotted eighth + sixteenth), ii-V-I with sevenths, chromatic passing tones.
    "jazz": {
        "scales": [(MAJOR, 1.0)],
        "progressions": {MAJOR: [["ii7", "V7", "Imaj7", "Imaj7", "ii7", "V7", "Imaj7", "Imaj7"],
                                 ["Imaj7", "vi7", "ii7", "V7", "iii7", "VI7", "ii7", "Imaj7"],
                                 ["Imaj7", "IVmaj7", "iii7", "VI7", "ii7", "V7", "Imaj7", "Imaj7"]]},
        "rhythms": [[3, 1, 3, 1, 3, 1, 3, 1], [3, 1, 3, 1, 4, 4], [4, 3, 1, 3, 1, 4], [6, 2, 3, 1, 4],
                    [2, 2, 3, 1, 3, 1, 4], [8, 3, 1, 4], [3, 1, 3, 1, 8]],
        "range": (-2, 19), "rest": 0.06, "arpeggio": 0.35, "step": 0.45, "octave_jump": 0.0,
        "chromatic": 0.25, "repeat": [(4, 0)], "end_len": [8, 12],
    },
}


def _choose(rng, weighted):
    r = rng.random() * sum(w for _, w in weighted)
    for item, w in weighted:
        r -= w
        if r <= 0:
            return item
    return weighted[-1][0]


def _pitches_in_range(pcs, lo, hi):
    return [p for p in range(lo, hi + 1) if p % 12 in pcs]


def _nearest(candidates, target, rng, spread=2):
    candidates = sorted(candidates, key=lambda p: abs(p - target))
    pool = candidates[:spread] if len(candidates) > 1 else candidates
    return rng.choice(pool)


def _step(scale_pitches, current, direction):
    if current not in scale_pitches:
        return _nearest(scale_pitches, current, random.Random(current), 1)
    i = scale_pitches.index(current) + direction
    i = max(0, min(len(scale_pitches) - 1, i))
    return scale_pitches[i]


def _bar(rng, rules, scale, chord, rhythm, prev, direction, lo, hi):
    """One bar of notes. Strong beats land on chord tones, the rest moves by step or arpeggio."""
    scale_pitches = _pitches_in_range(scale, lo, hi)
    chord_pitches = _pitches_in_range(chord, lo, hi) or scale_pitches
    notes, pos = [], 0
    for dur in rhythm:
        strong = pos in (0, 8) or dur >= 4
        may_rest = not strong or (rules.get("rest_any") and pos != 0)
        if notes and rng.random() < rules["rest"] and may_rest:
            notes.append((None, dur))
        else:
            if prev <= lo + 1:
                direction = 1
            elif prev >= hi - 1:
                direction = -1
            elif rng.random() < 0.25:
                direction = -direction
            r = rng.random()
            if not strong and rng.random() < rules.get("chromatic", 0.0) and lo < prev < hi:
                # Chromatic passing tone: a semitone toward the next chord tone.
                notes.append((prev + direction, dur))
                prev += direction
                pos += dur
                continue
            if strong:
                pitch = _nearest(chord_pitches, prev + direction * 2, rng)
            elif r < rules["arpeggio"]:
                above = [p for p in chord_pitches if (p - prev) * direction > 0]
                pitch = min(above, key=lambda p: abs(p - prev)) if above else _nearest(chord_pitches, prev, rng)
            elif r < rules["arpeggio"] + rules["step"]:
                pitch = _step(scale_pitches, prev, direction)
            else:
                pitch = _nearest(scale_pitches, prev + direction * rng.choice((2, 3, 4)), rng)
            if rules["octave_jump"] and rng.random() < rules["octave_jump"] and lo <= pitch + 12 * direction <= hi:
                pitch += 12 * direction
            notes.append((pitch, dur))
            prev = pitch
        pos += dur
    return notes, prev, direction


def _transpose_bar(bar, shift, scale, lo, hi):
    """Repeat a bar on another chord: shift by the chord root, snap to the scale."""
    out = []
    for pitch, dur in bar:
        if pitch is None:
            out.append((None, dur))
            continue
        p = pitch + shift
        while p > hi:
            p -= 12
        while p < lo:
            p += 12
        if p % 12 not in scale:
            p = min(_pitches_in_range(scale, lo, hi), key=lambda q: abs(q - p))
        out.append((p, dur))
    return out


def melody(style, rng):
    rules = STYLE_RULES[style]
    scale = _choose(rng, rules["scales"])
    progression = [C[name] for name in rng.choice(rules["progressions"][scale])]
    lo, hi = rules["range"]
    repeats = dict(rules["repeat"])
    bars, prev, direction = [], rng.choice(_pitches_in_range((0, 4, 7) if 4 in scale else (0, 3, 7), lo, min(hi, 12))), 1
    motif_rhythm = rng.choice(rules["rhythms"])
    for b in range(BARS):
        chord = progression[b]
        if b in repeats and rng.random() < 0.85:
            src = repeats[b]
            shift = (chord[0] - progression[src][0]) % 12
            if shift > 6:
                shift -= 12
            if rules.get("sequence_up") and b == 2:
                shift = 2
            bar = _transpose_bar(bars[src], shift, scale, lo, hi)
            prev = next((p for p, _ in reversed(bar) if p is not None), prev)
        else:
            rhythm = motif_rhythm if b in (0, 4) else rng.choice(rules["rhythms"])
            if b == BARS - 1:
                end = rng.choice(rules["end_len"])
                head = [d for d in rng.choice([r for r in rules["rhythms"] if sum(r) == BAR])]
                # Trim the pattern so the final tonic fills the rest of the bar.
                kept, total = [], 0
                for d in head:
                    if total + d > BAR - end:
                        break
                    kept.append(d)
                    total += d
                rest = BAR - end - total
                for d in sorted(DURATIONS, reverse=True):
                    while rest >= d:
                        kept.append(d)
                        rest -= d
                rhythm = kept + [end]
            bar, prev, direction = _bar(rng, rules, scale, chord, rhythm, prev, direction, lo, hi)
            if b == 0 and rules.get("fanfare"):
                # Heroic opening: a rising fourth or fifth from the tonic.
                leap = rng.choice((5, 7))
                first = [p for p, _ in bar]
                if len(first) >= 2:
                    bar[0] = (0, bar[0][1])
                    bar[1] = (leap if leap <= hi else leap - 12, bar[1][1])
        if b == 3 and rules.get("half_cadence"):
            # Half cadence: end the first phrase on a long dominant tone.
            v_tones = _pitches_in_range((7, 11, 2), lo, hi)
            last_p = next((p for p, _ in reversed(bar) if p is not None), 7)
            bar[-1] = (_nearest(v_tones, last_p, rng, 1), bar[-1][1])
        bars.append(bar)
    # Final note: tonic, nearest octave.
    last = bars[-1]
    prev_p = next((p for p, _ in reversed(last[:-1]) if p is not None), 0)
    tonic = min((t for t in (0, 12) if lo <= t <= hi), key=lambda t: abs(t - prev_p))
    last[-1] = (tonic, last[-1][1])
    notes = [n for bar in bars for n in bar]
    _validate(notes)
    return notes


def _validate(notes):
    total = 0
    for pitch, dur in notes:
        assert dur in DURATIONS, dur
        assert pitch is None or PITCH_MIN <= pitch <= PITCH_MAX, pitch
        assert (total % BAR) + dur <= BAR, "note crosses barline"
        total += dur
    assert total == BAR * BARS, total


if __name__ == "__main__":
    rng = random.Random(1)
    for s in STYLES:
        m = melody(s, rng)
        print(s, len(m), m[:12])
