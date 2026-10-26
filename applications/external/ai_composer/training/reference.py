"""Python twin of composer_model.c, using the quantised weights (quantised.npz).

It mirrors the C code step by step (float32, same xorshift32 random numbers),
so tests can compare both, and it renders previews of what the Flipper plays.
"""

import numpy as np

E, H, EP = 32, 80, 16  # must match model.py and composer_model.h
from teacher import BAR, BARS, DURATIONS, PITCH_MIN

P_COUNT, D_COUNT = 26, 8
f32 = np.float32


class Xorshift:
    def __init__(self, seed):
        self.s = np.uint32(seed or 0x9E3779B9)

    def __call__(self):
        x = int(self.s)
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.s = np.uint32(x)
        return f32(x >> 8) * f32(1.0 / 16777216.0)


class Reference:
    def __init__(self, npz="quantised.npz"):
        z = np.load(npz)
        self.w = {k: z[k] for k in z.files}

    def deq(self, name):
        return self.w[name].astype(f32) * self.w[name + "_scale"][:, None]

    def matvec(self, name, bias, x):
        q, s = self.w[name].astype(f32), self.w[name + "_scale"]
        with np.errstate(
            all="ignore"
        ):  # spurious FP flags from Accelerate's matmul on Apple silicon
            out = (q @ x.astype(f32)).astype(f32) * s
        return out + self.w[bias] if bias else out

    def reset(self, weights):
        w = np.maximum(np.asarray(weights, dtype=f32), 0)
        w = w / (w.sum() or 1)
        self.style = (w @ self.deq("cm_emb_style")).astype(f32)
        self.h = np.zeros(H, f32)
        self.prev_p, self.prev_d, self.pos = P_COUNT, D_COUNT, 0

    @staticmethod
    def sample(logits, allowed, temperature, rand):
        logits = np.asarray(logits, f32)
        ok = np.ones(len(logits), bool) if allowed is None else np.asarray(allowed)
        mx = logits[ok].max()
        probs = np.where(ok, np.exp((logits - mx) / f32(temperature)), 0).astype(f32)
        r = rand() * probs.sum(dtype=f32)
        last = 0
        for i, p in enumerate(probs):
            if p <= 0:
                continue
            last = i
            r -= p
            if r < 0:
                return i
        return last

    def next(self, temperature, rand):
        if self.pos >= BAR * BARS:
            return None
        temperature = max(temperature, 0.05)
        beat, bar = self.pos % BAR, self.pos // BAR
        x = (
            self.style
            + self.deq("cm_emb_pitch")[self.prev_p]
            + self.deq("cm_emb_dur")[self.prev_d]
            + self.deq("cm_emb_beat")[beat]
            + self.deq("cm_emb_bar")[bar]
        ).astype(f32)
        gi = self.matvec("cm_gru_w_ih", "cm_gru_b_ih", x)
        gh = self.matvec("cm_gru_w_hh", "cm_gru_b_hh", self.h)
        r = 1 / (1 + np.exp(-(gi[:H] + gh[:H])))
        z = 1 / (1 + np.exp(-(gi[H : 2 * H] + gh[H : 2 * H])))
        n = np.tanh(gi[2 * H :] + r * gh[2 * H :])
        self.h = ((1 - z) * n + z * self.h).astype(f32)
        self.pitch_logits = self.matvec("cm_head_pitch_w", "cm_head_pitch_b", self.h)
        p = self.sample(self.pitch_logits, None, temperature, rand)
        hd = np.concatenate([self.h, self.deq("cm_emb_pitch2")[p]]).astype(f32)
        self.dur_logits = self.matvec("cm_head_dur_w", "cm_head_dur_b", hd)
        fits = [beat + d <= BAR for d in DURATIONS]
        dt = 1 + (temperature - 1) * 0.5 if temperature > 1 else temperature
        d = self.sample(self.dur_logits, fits, dt, rand)
        if (
            self.pos + DURATIONS[d] >= BAR * BARS
        ):  # last note resolves to the nearest tonic
            p = 18 if 12 <= self.prev_p < P_COUNT else 6
        self.prev_p, self.prev_d = p, d
        self.pos += DURATIONS[d]
        return p, d

    def melody(self, weights, temperature, seed):
        self.reset(weights)
        rand = Xorshift(seed)
        notes = []
        while (step := self.next(temperature, rand)) is not None:
            p, d = step
            notes.append((None if p == 0 else p - 1 + PITCH_MIN, DURATIONS[d]))
        return notes
