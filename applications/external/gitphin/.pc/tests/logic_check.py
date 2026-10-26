#!/usr/bin/env python3
"""Compare hub_gitdiff / hub_gitcommit (via logic_test) with git.
Usage: logic_check.py <logic_test binary> <repos dir from make_test_repo.sh> <work dir>
The checks live in pc/tests/logic/ (run with python3 -I: the dir is put on sys.path here).
"""
import os, random, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "logic"))
from common import *
from diffcheck import *
from handrepo import *
from comparecheck import *
from cfilecheck import *
from mergecheck import *


def main():
    rnd = random.Random(42)
    os.makedirs(WORK, exist_ok=True)
    for name in ("onepack", "loose", "multi", "refdelta"):
        test_diff_repo(name, rnd)
    h = test_diff_hand(WORK)
    for name in ("onepack", "multi", "loose"):
        test_compare_repo(name, rnd)
    test_compare_hand(h)
    for name in ("loose", "onepack"):
        test_commit_file(name, rnd)
    for name in ("loose", "onepack"):
        test_merge_repo(name, rnd)
    test_merge_hand(h)
    test_wide(h)
    print("\nchecks: %d" % S.checks)
    print(
        "heap peaks (bytes, library code incl. gitobj/inflate, default limit 32 KB): "
        + ", ".join("%s=%d" % kv for kv in sorted(peaks.items()))
    )
    if fails:
        print("%d FAILURES" % len(fails))
        sys.exit(1)
    print("python checks passed")


main()
