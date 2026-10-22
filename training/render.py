"""Render melodies as square-wave WAV files, close to the Flipper's piezo sound."""
import math
import struct
import wave

RATE = 22050
TEMPO = {"chiptune": 150, "blues": 96, "classical": 100, "folk": 120, "game": 132}
TONIC_MIDI = 72  # C5, the range where the Flipper speaker sounds best


def _square(freq, seconds, volume=0.35):
    n = int(RATE * seconds)
    out = []
    gap = int(RATE * min(0.012, seconds * 0.15))  # short articulation gap between notes
    for i in range(n):
        if freq is None or i >= n - gap:
            out.append(0)
            continue
        env = min(1.0, i / (RATE * 0.004))  # 4 ms attack against clicks
        phase = (i * freq / RATE) % 1.0
        out.append(int((1 if phase < 0.5 else -1) * volume * env * 32767))
    return out


def samples(notes, bpm, tonic=TONIC_MIDI):
    sixteenth = 60.0 / bpm / 4
    out = []
    for pitch, dur in notes:
        freq = None if pitch is None else 440.0 * 2 ** ((tonic + pitch - 69) / 12)
        out.extend(_square(freq, dur * sixteenth))
    return out


def write_wav(path, pcm):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(struct.pack(f"<{len(pcm)}h", *pcm))


def silence(seconds):
    return [0] * int(RATE * seconds)
