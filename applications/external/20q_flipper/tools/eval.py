#!/usr/bin/env python3
"""SPEC 11.2 -- offline evaluation and parameter sweep.

Rebuilds tests/tq_sim.c with each candidate setting and plays real games
through the C engine, so the tuning applies to the shipped code.

  python3 tools/eval.py                  # sweep TQ_EPS x TQ_CANDIDATES
  python3 tools/eval.py --quick          # one run at the current defaults
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def run(eps, candidates, games, noise, limit, guess_conf=None, data="files/20q.dat"):
    flags = [f"-DTQ_EPS={eps}f", f"-DTQ_CANDIDATES={candidates}"]
    if guess_conf is not None:
        flags.append(f"-DTQ_GUESS_CONFIDENCE={guess_conf}f")
    # Force a rebuild: the flags change but the target's timestamp would not.
    (REPO / "build" / "tq_sim").unlink(missing_ok=True)
    subprocess.run(
        ["make", "-C", "tests", "../build/tq_sim", f"SIMFLAGS={' '.join(flags)}"],
        cwd=REPO,
        check=True,
        capture_output=True,
    )
    out = subprocess.run(
        [
            "./build/tq_sim",
            data,
            "--games",
            str(games),
            "--noise",
            str(noise),
            "--limit",
            str(limit),
        ],
        cwd=REPO,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    m = re.search(r"^JSON (.*)$", out, re.M)
    if not m:
        raise SystemExit("simulator produced no JSON line:\n" + out)
    return json.loads(m.group(1))


def table(rows, title):
    print(f"\n{title}")
    print(
        f"  {'eps':>5} {'cand':>5} {'noise':>6} {'win%':>7} {'1st%':>7} {'top3%':>7} {'meanQ':>7}"
    )
    for r in rows:
        print(
            f"  {r['eps']:>5.2f} {r['candidates']:>5} {r['noise']:>6.2f} "
            f"{100 * r['win_rate']:>7.1f} {100 * r['first_guess']:>7.1f} "
            f"{100 * r['top3']:>7.1f} {r['mean_questions']:>7.1f}"
        )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--games", type=int, default=400)
    ap.add_argument("--limit", type=int, default=20)
    ap.add_argument("--quick", action="store_true")
    ap.add_argument(
        "--guess-sweep",
        action="store_true",
        help="sweep the guess-confidence threshold at fixed eps/candidates",
    )
    ap.add_argument("--data", default="files/20q.dat")
    ap.add_argument("--out", default="build/eval.json")
    args = ap.parse_args()

    results = []
    if args.guess_sweep:
        print("  conf   win%   1st%  meanQ   (eps 0.05, candidates 80)")
        for conf in (0.50, 0.60, 0.70, 0.85):
            for noise in (0.0, 0.10):
                r = run(
                    0.05,
                    80,
                    args.games,
                    noise,
                    args.limit,
                    guess_conf=conf,
                    data=args.data,
                )
                r["guess_conf"] = conf
                results.append(r)
                print(
                    f"  {conf:.2f} n={noise:.2f} {100 * r['win_rate']:>6.1f} "
                    f"{100 * r['first_guess']:>6.1f} {r['mean_questions']:>6.1f}"
                )
        (REPO / args.out).write_text(json.dumps(results, indent=2))
        return 0
    if args.quick:
        # the shipping defaults from helpers/twentyq_engine.h
        for noise in (0.0, 0.10):
            r = run(0.05, 80, args.games, noise, args.limit, data=args.data)
            results.append(r)
        table(results, "quick check (honest player, then 10% wrong answers)")
    else:
        for eps in (0.02, 0.05, 0.10):
            for cand in (20, 40, 80):
                for noise in (0.0, 0.10):
                    r = run(eps, cand, args.games, noise, args.limit, data=args.data)
                    results.append(r)
                    print(
                        f"  eps={eps} cand={cand} noise={noise} -> "
                        f"win {100 * r['win_rate']:.1f}%  meanQ {r['mean_questions']:.1f}"
                    )
        table([r for r in results if r["noise"] == 0.0], "honest player")
        table([r for r in results if r["noise"] > 0.0], "10% of answers wrong")

    honest = [r for r in results if r["noise"] == 0.0]
    best = max(honest, key=lambda r: (r["win_rate"], -r["mean_questions"]))
    print(
        f"\nbest honest-player setting: TQ_EPS={best['eps']} "
        f"TQ_CANDIDATES={best['candidates']} -> {100 * best['win_rate']:.1f}% "
        f"in {best['mean_questions']:.1f} questions"
    )
    target = "PASS" if best["win_rate"] >= 0.80 else "FAIL"
    print(f"SPEC 11.2 target (>= 80% honest win rate): {target}")

    Path(REPO / args.out).parent.mkdir(parents=True, exist_ok=True)
    (REPO / args.out).write_text(json.dumps(results, indent=2))
    return 0 if best["win_rate"] >= 0.80 else 1


if __name__ == "__main__":
    sys.exit(main())
