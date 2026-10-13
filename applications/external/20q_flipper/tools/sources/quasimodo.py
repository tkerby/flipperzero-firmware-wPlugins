"""Aunsiels/Quasimodo (SPEC 2.2). Web-mined, so filter hard (SPEC 4.3)."""

from __future__ import annotations

from . import Matcher
from .hf import rows

DATASET = "Aunsiels/Quasimodo"
MIN_SCORE = 0.6
MIN_TYPICALITY = 0.5


def load(name_to_index, mapping, questions, limit=200000):
    m = Matcher(mapping)
    out = []
    for row in rows(DATASET, limit=limit):
        subject = str(row.get("subject", "")).strip().lower()
        predicate = str(row.get("predicate", "")).strip().lower()
        obj = str(row.get("object", "")).strip().lower()
        try:
            score = float(row.get("score") or 0.0)
            typicality = float(row.get("typicality") or 0.0)
        except (TypeError, ValueError):
            score = typicality = 0.0
        negative = int(row.get("is_negative") or 0)

        if score < MIN_SCORE or typicality < MIN_TYPICALITY:
            m.drop("quasimodo", "below threshold", subject, predicate, obj, score, typicality)
            continue
        if subject not in name_to_index:
            m.drop("quasimodo", "subject not in vocabulary", subject, predicate, obj)
            continue
        if m.blocked_text(subject, predicate, obj):
            m.drop("quasimodo", "blocklist", subject, predicate, obj)
            continue
        hit = m.match(f"{predicate} {obj}")
        if not hit:
            m.drop("quasimodo", "predicate not mapped", subject, predicate, obj)
            continue
        q, polarity, p_override = hit
        p_hat = p_override if p_override is not None else (score if not negative else 1.0 - score)
        if polarity == 0:
            p_hat = 1.0 - p_hat
        out.append((name_to_index[subject], q, p_hat))
    return out, m.flush()
