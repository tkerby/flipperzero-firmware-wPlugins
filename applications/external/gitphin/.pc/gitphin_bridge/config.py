"""User settings in ~/.config/gitphin/*.json (override the folder with GITPHIN_CONFIG_DIR).

The files are written with mode 0600 (the webhook secret is in one of them) and
replaced atomically. The bridge reads them again on each use, so the CLI
(`git flipper webhook ...`, `git flipper backup ...`) works while the bridge runs.
"""

import json
import os
import tempfile

DEFAULT_BACKUP_DIR = "~/GitphinBackup"


def config_dir():
    return os.environ.get("GITPHIN_CONFIG_DIR") or os.path.join(
        os.path.expanduser("~"), ".config", "gitphin"
    )


def load(name, default):
    """Read <config dir>/<name>.json; the default (merged over) when missing or broken."""
    try:
        with open(
            os.path.join(config_dir(), name + ".json"), "r", encoding="utf-8"
        ) as f:
            data = json.load(f)
        if isinstance(data, dict):
            return {**default, **data}
    except (OSError, ValueError):
        pass
    return dict(default)


def save(name, data):
    folder = config_dir()
    os.makedirs(folder, mode=0o700, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix=name + ".", suffix=".tmp", dir=folder)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
            f.write("\n")
        os.chmod(tmp, 0o600)
        os.replace(tmp, os.path.join(folder, name + ".json"))
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


# ---- webhooks.json: {"urls": [...], "secret": ""}


def webhooks():
    cfg = load("webhooks", {"urls": [], "secret": ""})
    urls = [
        u
        for u in cfg["urls"]
        if isinstance(u, str) and u.startswith(("http://", "https://"))
    ]
    return {
        "urls": urls,
        "secret": cfg["secret"] if isinstance(cfg["secret"], str) else "",
    }


def save_webhooks(cfg):
    save("webhooks", {"urls": cfg["urls"], "secret": cfg["secret"]})


# ---- backup.json: {"enabled": true, "dir": "~/GitphinBackup"}


def backup():
    cfg = load("backup", {"enabled": True, "dir": DEFAULT_BACKUP_DIR})
    folder = (
        cfg["dir"] if isinstance(cfg["dir"], str) and cfg["dir"] else DEFAULT_BACKUP_DIR
    )
    return {
        "enabled": bool(cfg["enabled"]),
        "dir": os.path.abspath(os.path.expanduser(folder)),
        "dir_raw": folder,
    }


def save_backup(enabled, folder):
    save("backup", {"enabled": bool(enabled), "dir": folder})
