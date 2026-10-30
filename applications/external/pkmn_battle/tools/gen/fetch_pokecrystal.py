#!/usr/bin/env python3
"""Fetch pret/pokecrystal at the revisions pinned in pokecrystal.lock.

Creates (gitignored):
  tools/gen/.cache/pokecrystal/          source tree at lock 'master'
  tools/gen/.cache/symbols/pokecrystal.sym   symbol file from lock 'symbols'

Idempotent: does nothing if the checkout is already at the pinned SHA.
"""
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
CACHE = HERE / ".cache"


def read_lock():
    lock = {}
    for line in (HERE / "pokecrystal.lock").read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            k, v = line.split("=", 1)
            lock[k.strip()] = v.strip()
    return lock


def git(*args, cwd):
    subprocess.run(["git", *args], cwd=cwd, check=True)


def git_out(*args, cwd):
    return subprocess.run(
        ["git", *args], cwd=cwd, check=True, capture_output=True, text=True
    ).stdout.strip()


def checkout(dest: pathlib.Path, repo: str, sha: str):
    if (dest / ".git").exists():
        try:
            if git_out("rev-parse", "HEAD", cwd=dest) == sha:
                return
        except subprocess.CalledProcessError:
            pass
    else:
        dest.mkdir(parents=True, exist_ok=True)
        git("init", "-q", cwd=dest)
        git("remote", "add", "origin", repo, cwd=dest)
    git("fetch", "-q", "--depth", "1", "origin", sha, cwd=dest)
    git("-c", "advice.detachedHead=false", "checkout", "-q", "--force", sha, cwd=dest)


def pokecrystal_dir() -> pathlib.Path:
    return CACHE / "pokecrystal"


def symbols_dir() -> pathlib.Path:
    return CACHE / "symbols"


def ensure():
    lock = read_lock()
    checkout(pokecrystal_dir(), lock["repo"], lock["master"])
    checkout(symbols_dir(), lock["repo"], lock["symbols"])
    return pokecrystal_dir()


if __name__ == "__main__":
    d = ensure()
    print(f"pokecrystal at {git_out('rev-parse', 'HEAD', cwd=d)} in {d}")
    s = symbols_dir()
    print(
        f"symbols at {git_out('rev-parse', 'HEAD', cwd=s)}: "
        f"{sorted(p.name for p in s.glob('*.sym'))}"
    )
    sys.exit(0)
