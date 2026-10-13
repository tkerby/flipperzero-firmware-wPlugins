"""Shared helpers for logic_check.py: git and logic_test runners, fail log, counters."""
import os, random, re, shutil, subprocess, sys, tempfile

BIN, REPOS, WORK = sys.argv[1:4]
ENV = dict(os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null",
           GIT_AUTHOR_NAME="x", GIT_AUTHOR_EMAIL="x@x", GIT_COMMITTER_NAME="x", GIT_COMMITTER_EMAIL="x@x")
fails = []

class S:
    checks = 0   # shared check counter
peaks = {}


def fail(msg):
    fails.append(msg)
    print("FAIL:", msg)


def git(repo, *args, inp=None, env=None, ok=True, raw=False):
    e = dict(ENV)
    if env:
        e.update(env)
    r = subprocess.run(["git", "--git-dir", repo] + list(args), input=inp, capture_output=True, env=e)
    if ok and r.returncode != 0:
        raise RuntimeError("git %s failed: %s" % (args, r.stderr.decode(errors="replace")))
    return r if raw else r.stdout.decode("utf-8", "surrogateescape")


def lt(kind, repo, *args, inp=None, env=None):
    e = dict(ENV)
    if env:
        e.update(env)
    if isinstance(inp, str):
        inp = inp.encode()
    r = subprocess.run([BIN, *args[:1], repo, *args[1:]], input=inp, capture_output=True, env=e)
    m = re.search(rb"heap_peak=(\d+)", r.stderr)
    if m:
        peaks[kind] = max(peaks.get(kind, 0), int(m.group(1)))
    if r.returncode != 0:
        fail("%s crashed/failed rc=%d: %s" % (args, r.returncode, r.stderr.decode(errors="replace")[:500]))
    return r.stdout.decode("utf-8", "surrogateescape")




ZERO = "0" * 40
