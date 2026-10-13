"""Optional Hugging Face evidence loaders (SPEC 2).

The shipped files/20q.dat is built from the hand-authored ontology alone, which
keeps it licence-clean (see README, "Data and licensing"). These loaders exist
so external commonsense data can be folded in as *additional* evidence:

    python3 tools/build_dataset.py --hf-mcrae

Every loader returns (rows, dropped) where rows is a list of
(entity_index, question_id, p_hat) and dropped is the count filtered out.
Everything dropped is logged to build/dropped.tsv so it can be audited.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parent
DROP_LOG = REPO / "build" / "dropped.tsv"


def _blocklist():
    terms = []
    for line in (ROOT / "blocklist.txt").read_text().splitlines():
        line = line.strip().lower()
        if line and not line.startswith("#"):
            terms.append(line)
    return terms


class Matcher:
    """Turns a normalised 'predicate object' phrase into question hits."""

    def __init__(self, mapping):
        self.rules = [(re.compile(r["re"]), r["q"], r.get("polarity", 1), r.get("p"))
                      for r in mapping["rules"]]
        self.blocked = _blocklist()
        self.drops = []

    def blocked_text(self, *parts):
        text = " ".join(p.lower() for p in parts if p)
        return any(term in text for term in self.blocked)

    def match(self, phrase):
        phrase = re.sub(r"[_\-]+", " ", phrase.strip().lower())
        phrase = re.sub(r"\s+", " ", phrase)
        for rx, q, polarity, p_override in self.rules:
            if rx.search(phrase):
                return q, polarity, p_override
        return None

    def drop(self, source, why, *parts):
        self.drops.append((source, why, " | ".join(str(p) for p in parts)))

    def flush(self):
        DROP_LOG.parent.mkdir(parents=True, exist_ok=True)
        with open(DROP_LOG, "a") as f:
            for row in self.drops:
                f.write("\t".join(row) + "\n")
        return len(self.drops)


def load(name, name_to_index, mapping, questions):
    if name == "mcrae":
        from . import mcrae
        return mcrae.load(name_to_index, mapping, questions)
    if name == "quasimodo":
        from . import quasimodo
        return quasimodo.load(name_to_index, mapping, questions)
    if name == "conceptnet":
        from . import conceptnet
        return conceptnet.load(name_to_index, mapping, questions)
    raise SystemExit(f"unknown source {name}")
