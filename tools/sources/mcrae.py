"""metaeval/semantic-feature-production-norms (SPEC 2.1).

Prod_Freq is how many of 30 participants volunteered the feature, so
Prod_Freq / 30 is a direct estimate of P(yes) -- the reason this is the
gold-standard seed rather than just more triples.
"""

from __future__ import annotations

from . import Matcher
from .hf import rows

DATASET = "metaeval/semantic-feature-production-norms"


def load(name_to_index, mapping, questions):
    m = Matcher(mapping)
    out = []
    for row in rows(DATASET, limit=100000):
        concept = str(row.get("Concept", "")).replace("_", " ").strip().lower()
        feature = str(row.get("Feature", ""))
        if concept not in name_to_index:
            m.drop("mcrae", "concept not in vocabulary", concept, feature)
            continue
        if m.blocked_text(concept, feature):
            m.drop("mcrae", "blocklist", concept, feature)
            continue
        hit = m.match(feature)
        if not hit:
            m.drop("mcrae", "feature not mapped", concept, feature)
            continue
        q, polarity, p_override = hit
        try:
            freq = float(row.get("Prod_Freq") or 0)
        except (TypeError, ValueError):
            freq = 0.0
        p_hat = p_override if p_override is not None else min(1.0, freq / 30.0)
        if polarity == 0:
            p_hat = 1.0 - p_hat
        out.append((name_to_index[concept], q, p_hat))
    return out, m.flush()
