#!/usr/bin/env python3
"""Builds files/20q.dat (SPEC 4).

Stages:
  1. load the curated vocabulary and expand tags through the ontology
  2. turn tag rules into per-cell evidence
  3. optionally fold in evidence from Hugging Face sources
  4. aggregate with a confidence-weighted Beta posterior (SPEC 4.4)
  5. rank questions by expected Gini reduction (SPEC 5.5)
  6. quantise to 4 bits and pack (SPEC 4.6 / 5)

Runs offline with no network unless a --hf-* flag is given.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import yaml  # noqa: E402

from pack import CATEGORIES, Entity, Question, quantise, write_dataset  # noqa: E402

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent

# SPEC 4.4 evidence strength.
K = 8.0
PRIOR_STRENGTH = 2.0

# Confidence and p_hat for each tag-rule class. `y`/`n` are hand-authored
# assertions, so they carry the "hand override" confidence from SPEC 4.4.
RULES = {
    "y": (0.95, 2.0),
    "n": (0.03, 2.0),
    "l": (0.78, 0.9),
    "u": (0.15, 0.9),
}
RULE_ORDER = ("y", "n", "l", "u")

SOURCE_CONFIDENCE = {"mcrae": 1.0, "quasimodo": 0.45, "conceptnet": 0.6, "llm": 0.35}


def load_yaml(name):
    with open(ROOT / name) as f:
        return yaml.safe_load(f)


def expand_tags(tags, onto):
    """Transitive closure over the implication graph. Cycle-safe."""
    seen = set()
    stack = list(tags)
    while stack:
        t = stack.pop()
        if t in seen:
            continue
        seen.add(t)
        stack.extend(onto.get(t, ()))
    return seen


def validate(entities, questions, onto):
    problems = []
    names = [e["n"] for e in entities]
    for n in names:
        if len(n) > 22:
            problems.append(f"entity name too long: {n}")
        if not n.isascii() or n != n.lower():
            problems.append(f"entity name must be lower-case ASCII: {n}")
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        problems.append(f"duplicate entity names: {sorted(dupes)}")
    for e in entities:
        if e["c"] not in (0, 1, 2, 3):
            problems.append(f"{e['n']}: bad category {e['c']}")
        for t in e["t"]:
            if t not in onto:
                problems.append(f"{e['n']}: unknown tag {t}")
    for q in questions:
        if len(q["text"]) > 72:
            problems.append(f"q{q['id']}: text too long ({len(q['text'])})")
        if not q["text"].isascii():
            problems.append(f"q{q['id']}: non-ASCII text")
        if len(q["def"]) != 4:
            problems.append(f"q{q['id']}: def must have 4 entries")
        for key in RULE_ORDER:
            for t in q.get(key, ()):
                if t not in onto:
                    problems.append(f"q{q['id']}: unknown tag {t}")
    if [q["id"] for q in questions] != list(range(len(questions))):
        problems.append("question ids must be 0..Q-1 in order")
    return problems


def rule_evidence(question, tagset):
    """First matching rule class wins, in the order y > n > l > u."""
    for key in RULE_ORDER:
        tags = question.get(key)
        if tags and tagset.intersection(tags):
            p_hat, conf = RULES[key]
            return p_hat, conf, key
    return None


def aggregate(prior_p, evidence):
    """Confidence-weighted Beta posterior (SPEC 4.4)."""
    alpha = PRIOR_STRENGTH * prior_p
    beta = PRIOR_STRENGTH * (1.0 - prior_p)
    for p_hat, conf in evidence:
        alpha += conf * p_hat * K
        beta += conf * (1.0 - p_hat) * K
    return alpha / (alpha + beta)


def rank_questions(matrix, priors, n):
    """Expected Gini reduction against the prior belief (SPEC 5.5)."""
    total = sum(priors) or 1.0
    b = [p / total for p in priors]
    scores = []
    for j, col in enumerate(matrix):
        s_yes = a = bb = 0.0
        for i in range(n):
            y = b[i] * col[i]
            no = b[i] - y
            s_yes += y
            a += y * y
            bb += no * no
        s_no = 1.0 - s_yes
        scores.append((a / max(s_yes, 1e-6) + bb / max(s_no, 1e-6), j))
    # purity is 1 - E[Gini after asking j]: higher is better (SPEC 6.4)
    scores.sort(key=lambda t: (-t[0], t[1]))
    return [j for _, j in scores]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", default=str(REPO / "files" / "20q.dat"))
    ap.add_argument("--dataset-id", default=None)
    ap.add_argument("--hf-mcrae", action="store_true", help="fold in McRae norms")
    ap.add_argument("--hf-conceptnet", action="store_true")
    ap.add_argument("--hf-quasimodo", action="store_true")
    ap.add_argument("--report", default=str(REPO / "build" / "build_report.json"))
    ap.add_argument(
        "--epoch",
        type=int,
        default=None,
        help="build timestamp; defaults to SOURCE_DATE_EPOCH or now. "
        "Pin it for byte-reproducible builds.",
    )
    args = ap.parse_args()

    onto = load_yaml("ontology.yaml")["tags"]
    entities = load_yaml("entities.yaml")["entities"]
    questions = load_yaml("questions.yaml")["questions"]

    problems = validate(entities, questions, onto)
    if problems:
        for p in problems[:40]:
            print("ERROR:", p)
        raise SystemExit(f"{len(problems)} validation problems")

    entities.sort(key=lambda e: e["n"])
    n, q = len(entities), len(questions)
    tagsets = [expand_tags(e["t"], onto) for e in entities]

    # --- stage 3: optional external evidence -----------------------------
    external = {}  # (i, j) -> list of (p_hat, confidence)
    used_sources = []
    if args.hf_mcrae or args.hf_conceptnet or args.hf_quasimodo:
        import sources

        wanted = []
        if args.hf_mcrae:
            wanted.append("mcrae")
        if args.hf_conceptnet:
            wanted.append("conceptnet")
        if args.hf_quasimodo:
            wanted.append("quasimodo")
        name_to_i = {e["n"]: i for i, e in enumerate(entities)}
        mapping = load_yaml("mapping.yaml")
        for src in wanted:
            rows, dropped = sources.load(src, name_to_i, mapping, questions)
            used_sources.append(f"{src}:{len(rows)}")
            print(f"  {src}: {len(rows)} evidence rows, {dropped} dropped")
            for i, j, p_hat in rows:
                external.setdefault((i, j), []).append((p_hat, SOURCE_CONFIDENCE[src]))

    # --- stages 2 + 4: evidence -> P(yes | e, q) --------------------------
    matrix = []
    stats = {"y": 0, "n": 0, "l": 0, "u": 0, "prior_only": 0}
    for j, question in enumerate(questions):
        col = []
        for i, e in enumerate(entities):
            prior_p = float(question["def"][e["c"]])
            evidence = list(external.get((i, j), ()))
            hit = rule_evidence(question, tagsets[i])
            if hit:
                p_hat, conf, key = hit
                evidence.append((p_hat, conf))
                stats[key] += 1
            elif not evidence:
                stats["prior_only"] += 1
            col.append(aggregate(prior_p, evidence))
        matrix.append(col)

    priors = [float(e.get("p", 50)) for e in entities]
    qrank = rank_questions(matrix, priors, n)

    packed = [Entity(e["n"], e["c"], float(e.get("p", 50)), e["t"]) for e in entities]
    packed_q = [
        Question(x["text"], x["family"], dict(zip(CATEGORIES, x["def"])))
        for x in questions
    ]
    dataset_id = args.dataset_id or f"20q-v1-{n}x{q}"
    epoch = args.epoch
    if epoch is None:
        epoch = int(os.environ.get("SOURCE_DATE_EPOCH", time.time()))
    info = write_dataset(
        args.out,
        packed,
        packed_q,
        matrix,
        dataset_id=dataset_id,
        qrank=qrank,
        build_epoch=epoch,
    )

    cells = n * q
    covered = cells - stats["prior_only"]
    print(f"entities {n}  questions {q}  cells {cells}")
    print(
        f"  rules fired: y={stats['y']} n={stats['n']} l={stats['l']} u={stats['u']}"
        f"  prior-only={stats['prior_only']} ({100.0 * covered / cells:.1f}% covered)"
    )
    print(f"  sources: {', '.join(used_sources) if used_sources else 'ontology only'}")
    print(
        f"  wrote {info['path']}  {info['bytes'] / 1024.0:.1f} KB  crc {info['crc32']:#010x}"
    )
    print(f"  best opening questions: {[questions[j]['text'] for j in qrank[:3]]}")

    Path(args.report).parent.mkdir(parents=True, exist_ok=True)
    with open(args.report, "w") as f:
        json.dump(
            {
                "dataset_id": dataset_id,
                "entities": n,
                "questions": q,
                "coverage": covered / cells,
                "rule_stats": stats,
                "sources": used_sources,
                **info,
            },
            f,
            indent=2,
        )


if __name__ == "__main__":
    main()
