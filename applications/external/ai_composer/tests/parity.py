"""Compare composer_model.c with the Python reference on identical seeds.

usage (from repo root): python tests/parity.py
Builds tests/parity_main.c with the host compiler, then for several style mixes
and temperatures checks that both produce the same notes and that every logit
agrees within a small tolerance.
"""

import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "training"))
from reference import Reference, Xorshift  # noqa: E402
from teacher import DURATIONS  # noqa: E402

CASES = [
    (1, 0.8, [1, 0, 0, 0, 0, 0, 0, 0]),
    (2, 1.0, [0, 1, 0, 0, 0, 0, 0, 0]),
    (3, 0.6, [0, 0, 1, 0, 0, 0, 0, 0]),
    (4, 1.2, [0, 0, 0, 1, 0, 0, 0, 0]),
    (5, 1.0, [0, 0, 0, 0, 1, 0, 0, 0]),
    (9, 0.9, [0, 0, 0, 0, 0, 1, 0, 0]),
    (10, 0.85, [0, 0, 0, 0, 0, 0, 1, 0]),
    (11, 1.1, [0, 0, 0, 0, 0, 0, 0, 1]),
    (6, 0.9, [0.7, 0.3, 0, 0, 0, 0, 0, 0]),
    (7, 1.5, [1, 1, 1, 1, 1, 1, 1, 1]),
    (8, 0.3, [0, 0, 0, 0, 0, 0.5, 0.5, 0]),
]
MELODIES = 25


def main():
    exe = os.path.join(tempfile.mkdtemp(), "parity")
    subprocess.run(
        [
            "cc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-o",
            exe,
            os.path.join(ROOT, "tests", "parity_main.c"),
            os.path.join(ROOT, "composer_model.c"),
            os.path.join(ROOT, "composer_weights.c"),
            "-lm",
        ],
        check=True,
    )
    ref = Reference(os.path.join(ROOT, "training", "quantised.npz"))
    notes = same = 0
    max_diff = 0.0
    for seed, temp, weights in CASES:
        out = subprocess.run(
            [exe, str(seed), str(temp), *map(str, weights), str(MELODIES)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.split("END\n")[:-1]
        rand = Xorshift(seed)
        for block in out:
            ref.reset(weights)
            for line in block.strip().splitlines():
                values = line.split()
                c_p, c_d = int(values[0]), int(values[1])
                c_logits = list(map(float, values[2:]))
                step = ref.next(temp, rand)
                assert step is not None, "reference ended early"
                r_logits = list(ref.pitch_logits) + list(ref.dur_logits)
                max_diff = max(
                    max_diff, max(abs(a - b) for a, b in zip(c_logits, r_logits))
                )
                notes += 1
                same += step == (c_p, c_d)
                if step != (c_p, c_d):
                    # A borderline sample flipped; continue from the C choice to stay in sync.
                    ref.pos += DURATIONS[c_d] - DURATIONS[step[1]]
                    ref.prev_p, ref.prev_d = c_p, c_d
            assert ref.next(temp, rand) is None, "reference continued after C ended"
    print(
        f"notes {notes} | identical {same} ({same / notes:.2%}) | max logit diff {max_diff:.2e}"
    )
    assert max_diff < 1e-3, "logits diverge"
    assert same / notes > 0.995, "too many differing samples"
    print("PARITY OK")


if __name__ == "__main__":
    main()
