"""The `git flipper` commands for repositories on the Flipper."""

import os
import shutil
import tempfile

from . import cli_extra
from .api import PACK_RE, Hub, HubError, check_name, log
from .gitutil import fetch_packs, git, upload_pack


def cmd_list(hub, args):
    repos = hub.api("GET", "/api/git/repos").get("repos", [])
    width = max([len(r["name"]) for r in repos] + [4])
    for r in repos:
        print(
            f"{r['name']:<{width}}  {r.get('head', ''):<10} {r.get('packs', 0):>3} pack(s)  "
            f"{r.get('description', '')}"
        )


def cmd_create(hub, args):
    opts = {"-d": None, "-b": None}
    names, it = [], iter(args)
    for a in it:
        if a in opts:
            opts[a] = next(it, None)
            if opts[a] is None:
                raise HubError(f"{a} needs a value")
        else:
            names.append(a)
    if len(names) != 1:
        raise HubError("usage: git flipper create <name> [-d text] [-b branch]")
    name = check_name(names[0])
    hub.api(
        "POST", "/api/git/create", name=name, branch=opts["-b"], description=opts["-d"]
    )
    print(f"created {name}")
    print(f"flipper://{name}")


def cmd_delete(hub, args):
    if len(args) != 2 or args[1] != "--yes":
        raise HubError("usage: git flipper delete <name> --yes")
    hub.api("POST", "/api/git/delete", name=check_name(args[0]))
    print(f"deleted {args[0]}")


def cmd_rename(hub, args):
    if len(args) != 2:
        raise HubError("usage: git flipper rename <old> <new>")
    hub.api("POST", "/api/git/rename", name=check_name(args[0]), to=check_name(args[1]))
    print(f"renamed {args[0]} to {args[1]}")


def cmd_describe(hub, args):
    if len(args) < 2:
        raise HubError("usage: git flipper describe <name> <text>")
    hub.api(
        "POST", "/api/git/describe", name=check_name(args[0]), text=" ".join(args[1:])
    )


def cmd_url(hub, args):
    if len(args) != 1:
        raise HubError("usage: git flipper url <name>")
    name = check_name(args[0])
    print(f"flipper://{name}")
    print(f"{hub.base}/git/{name}.git")


def cmd_gc(hub, args):
    if len(args) != 1:
        raise HubError("usage: git flipper gc <name>")
    name = check_name(args[0])
    got = hub.refs(name)
    if got is None:
        raise HubError(f"no such repository: {name}")
    refs, _ = got
    old = hub.packs(name)
    old_packs = [f for f in old if PACK_RE.match(f)]
    if not old_packs:
        print("nothing to do")
        return
    tmp = tempfile.mkdtemp(prefix="flipper-gc-")
    try:
        repo = os.path.join(tmp, "r.git")
        git(["init", "--bare", "-q", repo])
        fetch_packs(hub, name, os.path.join(repo, "objects", "pack"), git_dir=repo)
        for ref, sha in refs.items():
            git(["update-ref", ref, sha], git_dir=repo)
        git(["repack", "-a", "-d", "-q"], git_dir=repo)
        new = [
            f
            for f in os.listdir(os.path.join(repo, "objects", "pack"))
            if PACK_RE.match(f)
        ]
        if len(new) != 1:
            raise HubError(f"repack made {len(new)} packs, expected 1")
        keep = new[0][:-5]
        if new[0] not in old_packs:
            upload_pack(
                hub, name, os.path.join(repo, "objects", "pack"), keep[len("pack-") :]
            )
        hub.server_info(name)
        for fname in old:
            if fname.startswith("pack-") and not fname.startswith(keep + "."):
                hub.delete_file(name, f"objects/pack/{fname}")
        hub.server_info(name)
        print(f"gc {name}: {len(old_packs)} pack(s) -> 1")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


COMMANDS = {
    "list": cmd_list,
    "create": cmd_create,
    "delete": cmd_delete,
    "rename": cmd_rename,
    "describe": cmd_describe,
    "url": cmd_url,
    "gc": cmd_gc,
    "webhook": cli_extra.cmd_webhook,
    "backup": cli_extra.cmd_backup,
    "mirror-add": cli_extra.cmd_mirror_add,
    "mirror-remove": cli_extra.cmd_mirror_remove,
}


def cli_main(argv):
    if not argv or argv[0] not in COMMANDS:
        log("usage: git flipper {" + "|".join(COMMANDS) + "} ...")
        return 2
    COMMANDS[argv[0]](Hub(), argv[1:])
    return 0
