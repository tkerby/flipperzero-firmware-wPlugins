"""Tiny note-by-note melody model, small enough for the Flipper Zero.

Per step the input is the previous note (pitch, duration), where the next note
starts (beat within the bar, bar number) and a style vector. A GRU carries the
musical context; one head predicts the next pitch, a second head predicts its
duration given that pitch. The style vector is a weighted sum of learned style
embeddings, so styles can be mixed at generation time.
"""

import torch
import torch.nn as nn

from teacher import BAR, BARS, DURATIONS, PITCH_MAX, PITCH_MIN, STYLES

PITCHES = PITCH_MAX - PITCH_MIN + 1  # 25 notes
REST = 0  # pitch index 0 = rest, 1..25 = notes
P_VOCAB = PITCHES + 1  # targets
P_BOS = P_VOCAB  # start token (input only)
D_VOCAB = len(DURATIONS)
D_BOS = D_VOCAB
E, H, EP = 32, 80, 16  # embedding, hidden, pitch-for-duration embedding


def pitch_index(pitch):
    return REST if pitch is None else pitch - PITCH_MIN + 1


def index_pitch(i):
    return None if i == REST else i - 1 + PITCH_MIN


class Composer(nn.Module):
    def __init__(self):
        super().__init__()
        self.emb_pitch = nn.Embedding(P_VOCAB + 1, E)
        self.emb_dur = nn.Embedding(D_VOCAB + 1, E)
        self.emb_beat = nn.Embedding(BAR, E)
        self.emb_bar = nn.Embedding(BARS, E)
        self.emb_style = nn.Embedding(len(STYLES), E)
        self.gru = nn.GRU(E, H, batch_first=True)
        self.head_pitch = nn.Linear(H, P_VOCAB)
        self.emb_pitch2 = nn.Embedding(P_VOCAB, EP)
        self.head_dur = nn.Linear(H + EP, D_VOCAB)

    def inputs(self, prev_p, prev_d, beat, bar, style_vec):
        return (
            self.emb_pitch(prev_p)
            + self.emb_dur(prev_d)
            + self.emb_beat(beat)
            + self.emb_bar(bar)
            + style_vec
        )

    def style_vector(self, weights):
        """weights: (batch, n_styles) mixing weights, rows sum to 1."""
        return weights @ self.emb_style.weight

    def forward(self, prev_p, prev_d, beat, bar, style, next_p, h0=None):
        style_vec = self.emb_style(style)[:, None, :]
        x = self.inputs(prev_p, prev_d, beat, bar, style_vec)
        out, h = self.gru(x, h0)
        logits_p = self.head_pitch(out)
        logits_d = self.head_dur(torch.cat([out, self.emb_pitch2(next_p)], dim=-1))
        return logits_p, logits_d, h
