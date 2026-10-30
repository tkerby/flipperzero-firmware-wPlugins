"""ConceptNet (SPEC 2.3).

NOTE the licence: ConceptNet is CC BY-SA 4.0, and data derived from it inherits
share-alike. files/20q.dat as shipped contains no ConceptNet data. If you build
with --hf-conceptnet you must relicense the resulting .dat accordingly and say
so -- see README, "Data and licensing".
"""

from __future__ import annotations

import re

from . import Matcher
from .hf import rows

DATASET = "peandrew/conceptnet_en_nomalized"  # normalised English subset
MIN_WEIGHT = 1.0

RELATION_PHRASE = {
    "isa": "is a {obj}",
    "hasa": "has {obj}",
    "partof": "part of {obj}",
    "madeof": "made of {obj}",
    "usedfor": "used for {obj}",
    "capableof": "can {obj}",
    "hasproperty": "is {obj}",
    "atlocation": "found in {obj}",
}


def _lemma(term):
    """/c/en/dog/n -> dog"""
    term = re.sub(r"^/c/en/", "", str(term))
    term = term.split("/")[0]
    return term.replace("_", " ").strip().lower()


def load(name_to_index, mapping, questions, limit=200000):
    m = Matcher(mapping)
    out = []
    for row in rows(DATASET, limit=limit):
        rel = str(row.get("rel") or row.get("relation") or "").lower()
        rel = rel.replace("/r/", "").strip()
        start = _lemma(row.get("arg1") or row.get("start") or "")
        end = _lemma(row.get("arg2") or row.get("end") or "")
        try:
            weight = float(row.get("weight") or 1.0)
        except (TypeError, ValueError):
            weight = 0.0

        if weight < MIN_WEIGHT:
            m.drop("conceptnet", "low weight", start, rel, end, weight)
            continue
        if start not in name_to_index:
            m.drop("conceptnet", "start not in vocabulary", start, rel, end)
            continue
        if m.blocked_text(start, rel, end):
            m.drop("conceptnet", "blocklist", start, rel, end)
            continue
        template = RELATION_PHRASE.get(rel)
        if not template:
            m.drop("conceptnet", "relation not used", start, rel, end)
            continue
        hit = m.match(template.format(obj=end))
        if not hit:
            m.drop("conceptnet", "phrase not mapped", start, rel, end)
            continue
        q, polarity, p_override = hit
        p_hat = p_override if p_override is not None else min(1.0, weight / 3.0)
        if polarity == 0:
            p_hat = 1.0 - p_hat
        out.append((name_to_index[start], q, p_hat))
    return out, m.flush()
