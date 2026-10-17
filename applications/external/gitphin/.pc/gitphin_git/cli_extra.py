"""`git flipper` commands about the PC side: webhook, backup, mirror-add, mirror-remove.

webhook and backup edit the settings files in ~/.config/gitphin/ (the running bridge
reads them again on each use) and ask the bridge for its status. mirror-add and
mirror-remove change the push URLs of one git remote of the current repository.
"""

import json
import os
import re
import shutil
import uuid

from gitphin_bridge import config, webhook_core

from .api import HubError, log, valid_name
from .gitutil import git, git_out

# ---------------------------------------------------------------- webhook

WEBHOOK_USAGE = (
    "usage: git flipper webhook list|add <url>|remove <url>|secret <text>|test"
)


def bridge_json(hub, method, route):
    """Answer of a /pc route as a dict, or None when the bridge is not reachable."""
    try:
        conn, resp = hub._open(method, route, timeout=10)
    except HubError:
        return None
    try:
        return json.loads(resp.read().decode("utf-8", "replace"))
    except ValueError:
        return None
    finally:
        conn.close()


def valid_url(url):
    return bool(re.match(r"^https?://[^\s/]+(/\S*)?$", url))


def cmd_webhook(hub, args):
    sub, rest = (args[0], args[1:]) if args else ("", [])
    cfg = config.webhooks()
    if sub == "list" and not rest:
        live = {
            u["url"]: u.get("last")
            for u in (bridge_json(hub, "GET", "/pc/webhooks") or {}).get("urls", [])
        }
        for url in cfg["urls"]:
            last = live.get(url)
            note = ""
            if last:
                note = f"  last: {last.get('event', '')} {last.get('code') or last.get('error')}"
            print(url + note)
        print(
            f"{len(cfg['urls'])} URL(s), secret {'set' if cfg['secret'] else 'not set'}"
        )
    elif sub == "add" and len(rest) == 1:
        if not valid_url(rest[0]):
            raise HubError("the URL must start with http:// or https://")
        if rest[0] not in cfg["urls"]:
            cfg["urls"].append(rest[0])
            config.save_webhooks(cfg)
        print(f"added {rest[0]}")
    elif sub == "remove" and len(rest) == 1:
        if rest[0] not in cfg["urls"]:
            raise HubError("no such URL (see: git flipper webhook list)")
        cfg["urls"].remove(rest[0])
        config.save_webhooks(cfg)
        print(f"removed {rest[0]}")
    elif sub == "secret" and len(rest) == 1:
        cfg["secret"] = rest[0]
        config.save_webhooks(cfg)
        print("secret set" if rest[0] else "secret cleared")
    elif sub == "test" and not rest:
        if not cfg["urls"]:
            raise HubError("no webhook URLs (git flipper webhook add <url>)")
        failed = 0
        body = webhook_core.body_of(webhook_core.ping_event())
        for url in cfg["urls"]:
            r = webhook_core.post(url, body, "ping", str(uuid.uuid4()), cfg["secret"])
            ok = webhook_core.ok(r)
            failed += not ok
            print(
                f"{url}: {'HTTP ' + str(r['code']) if ok else r['error'] or 'no answer'}"
            )
        if failed:
            raise HubError(f"{failed} of {len(cfg['urls'])} deliveries failed")
    else:
        raise HubError(WEBHOOK_USAGE)


# ---------------------------------------------------------------- backup

BACKUP_USAGE = "usage: git flipper backup [now|status|on|off|dir <path>]"


def cmd_backup(hub, args):
    sub, rest = (args[0], args[1:]) if args else ("status", [])
    cfg = config.backup()
    if sub == "status" and not rest:
        print(f"backup {'on' if cfg['enabled'] else 'off'}, folder {cfg['dir']}")
        st = bridge_json(hub, "GET", "/pc/backup")
        if st is None:
            print("the bridge does not run: no status")
            return
        if st.get("running"):
            print("running now")
        elif st.get("finished"):
            print(f"last run: {st.get('summary', '')}")
        else:
            print("no run since the bridge started")
        for r in st.get("repos", []):
            print(
                f"  {r['name']}: {r['action']} "
                + ("ok" if r["ok"] else "FAILED " + r["error"])
            )
    elif sub == "now" and not rest:
        got = bridge_json(hub, "POST", "/pc/backup/now")
        if got is None:
            raise HubError(f"cannot reach the bridge at {hub.base}")
        print(
            "backup started (see: git flipper backup status)"
            if got.get("started")
            else "a backup is running already"
        )
    elif sub in ("on", "off") and not rest:
        config.save_backup(sub == "on", cfg["dir_raw"])
        print(f"backup {sub}")
    elif sub == "dir" and len(rest) == 1:
        raw = rest[0] if rest[0].startswith("~") else os.path.abspath(rest[0])
        config.save_backup(cfg["enabled"], raw)
        print(f"backup folder: {os.path.expanduser(raw)}")
    else:
        raise HubError(BACKUP_USAGE)


# ---------------------------------------------------------------- mirror


def is_flipper_url(url, hub):
    return (
        url.startswith("flipper://")
        or url.startswith(hub.base + "/git/")
        or re.match(r"^https?://(127\.0\.0\.1|localhost)(:\d+)?/git/[^/]+\.git$", url)
        is not None
    )


def split_remote_args(args):
    """(remote, name or None) from `[remote] [--name N]`."""
    name, rest, it = None, [], iter(args)
    for a in it:
        if a == "--name":
            name = next(it, None)
            if not name:
                raise HubError("--name needs a value")
        else:
            rest.append(a)
    if len(rest) > 1:
        raise HubError("usage: git flipper mirror-add [remote] [--name <repo>]")
    return (rest[0] if rest else "origin"), name


def current_repo_name():
    try:
        top = git_out(["rev-parse", "--show-toplevel"])
    except HubError:
        try:
            top = git_out(["rev-parse", "--absolute-git-dir"])
        except HubError:
            raise HubError("this is not a git repository") from None
    name = os.path.basename(top.rstrip("/"))
    return name[:-4] if name.endswith(".git") else name


def push_urls(remote):
    out = git(
        ["config", "--get-all", f"remote.{remote}.pushurl"], check=False
    ).stdout.decode()
    return [l for l in out.splitlines() if l]


def remote_url(remote):
    got = git(["remote", "get-url", remote], check=False)
    if got.returncode != 0:
        raise HubError(f"no such remote: {remote}")
    return got.stdout.decode().strip()


def cmd_mirror_add(hub, args):
    remote, name = split_remote_args(args)
    name = name or current_repo_name()
    if not valid_name(name):
        raise HubError(f"{name!r} is not a valid Flipper repo name; use --name <repo>")
    fetch_url = remote_url(remote)
    flipper = (
        f"flipper://{name}"
        if shutil.which("git-remote-flipper")
        else f"{hub.base}/git/{name}.git"
    )
    urls = push_urls(remote)
    if flipper in urls:
        print(f"{remote} pushes to {flipper} already")
    else:
        if (
            not urls
        ):  # a push URL replaces the fetch URL for pushing: keep the old target first
            git(["remote", "set-url", "--add", "--push", remote, fetch_url])
        git(["remote", "set-url", "--add", "--push", remote, flipper])
        print(f"git push {remote} now pushes to {fetch_url} and {flipper}")
    try:
        if hub.refs(name) is None:
            hub.api("POST", "/api/git/create", name=name)
            print(f"created {name} on the Flipper")
    except HubError as e:
        log(
            f"cannot check the Flipper ({e}); create the repo later: git flipper create {name}"
        )


def cmd_mirror_remove(hub, args):
    remote, _ = split_remote_args(args)
    fetch_url = remote_url(remote)
    urls = push_urls(remote)
    keep = [u for u in urls if not is_flipper_url(u, hub)]
    if len(keep) == len(urls):
        print(f"{remote} has no Flipper push URL")
        return
    git(["config", "--unset-all", f"remote.{remote}.pushurl"], check=False)
    if keep and keep != [fetch_url]:
        for u in keep:
            git(["remote", "set-url", "--add", "--push", remote, u])
    print(f"removed the Flipper push URL from {remote}")
