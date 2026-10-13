"""Local git work: run git, read and write packs."""

import os
import subprocess
import sys

from .api import PACK_RE, HubError, log, repo_path

def git(args, git_dir=None, input_text=None, check=True):
    """Run git with stdout captured (stdout belongs to the protocol). Returns the process."""
    env = dict(os.environ)
    if git_dir:
        env["GIT_DIR"] = git_dir
    proc = subprocess.run(
        ["git"] + args, env=env, stdout=subprocess.PIPE, stderr=None if check else subprocess.DEVNULL,
        input=input_text.encode() if input_text is not None else None,
        stdin=None if input_text is not None else subprocess.DEVNULL)
    if check and proc.returncode != 0:
        raise HubError(f"git {' '.join(args[:2])} failed ({proc.returncode})")
    return proc


def git_out(args, **kw):
    return git(args, **kw).stdout.decode().strip()


def local_objects(shas):
    """Subset of shas whose objects exist in the local repository."""
    if not shas:
        return set()
    out = git_out(["cat-file", "--batch-check"], input_text="\n".join(shas) + "\n")
    return {line.split()[0] for line in out.splitlines() if not line.endswith("missing")}


def pack_dir(git_dir=None):
    p = git_out(["rev-parse", "--git-path", "objects/pack"], git_dir=git_dir)
    return os.path.abspath(p)


def fetch_packs(hub, name, dest_dir, git_dir=None):
    """Download remote packs missing in dest_dir and index them locally. Returns the new count."""
    os.makedirs(dest_dir, exist_ok=True)
    fetched = 0
    for fname in hub.packs(name):
        if not PACK_RE.match(fname):
            continue
        stem = fname[:-5]
        if os.path.exists(os.path.join(dest_dir, stem + ".idx")):
            continue
        tmp = os.path.join(dest_dir, f"tmp_flipper_{os.getpid()}")
        try:
            hub.download(f"/git/{name}.git/objects/pack/{fname}", tmp + ".pack", f"fetch {stem[:15]}")
            git(["index-pack", "-o", tmp + ".idx", tmp + ".pack"], git_dir=git_dir)
            os.replace(tmp + ".pack", os.path.join(dest_dir, fname))
            os.replace(tmp + ".idx", os.path.join(dest_dir, stem + ".idx"))
        finally:
            for ext in (".pack", ".idx"):
                if os.path.exists(tmp + ext):
                    os.unlink(tmp + ext)
        fetched += 1
    return fetched


def build_pack(tips, excludes, outdir):
    """One `pack-objects --revs`. Returns the pack hash, or None when the pack is empty."""
    lines = list(tips) + ["^" + s for s in excludes]
    args = ["pack-objects", "--revs", "--delta-base-offset"]
    if not sys.stderr.isatty():
        args.append("-q")
    out = git_out(args + [os.path.join(outdir, "pack")], input_text="\n".join(lines) + "\n")
    if not out:
        return None
    h = out.splitlines()[-1]
    with open(os.path.join(outdir, f"pack-{h}.pack"), "rb") as f:
        header = f.read(12)  # "PACK", version, object count (big endian)
    return h if int.from_bytes(header[8:12], "big") else None


def upload_pack(hub, name, outdir, h):
    stem = f"pack-{h}"
    for ext in (".pack", ".idx"):  # pack first: an idx without its pack is the bad state
        hub.upload(repo_path(name, f"objects/pack/{stem}{ext}"), os.path.join(outdir, stem + ext),
                   f"push {stem[:15]}{ext}")
