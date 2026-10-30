"""Backup on plug-in: mirror every repo of the Flipper to the PC.

When the link to the Flipper app comes up, a background thread copies each repo to
<dir>/<name>.git (default ~/GitphinBackup): `git clone --mirror` the first time,
`git remote update --prune` later. It reads through http://127.0.0.1:<port>/git/,
so it uses the bridge like any git client. It starts a repo only when no request
is in flight and the Flipper was idle for a moment, pauses between repos, runs git
at low priority, and shows one notification at the end. Settings:
~/.config/gitphin/backup.json. Status: GET /pc/backup. Start now: POST /pc/backup/now.
"""

import os
import re
import shutil
import subprocess
import sys
import threading
import time

from . import activity, config, link, upstream

NAME_RE = re.compile(r"^[A-Za-z0-9_-][A-Za-z0-9._-]{0,63}$")
IDLE_S = 1.5  # the Flipper must be quiet this long before a repo starts
PAUSE_S = 1.0  # pause between two repos
WAIT_STEP_S = 0.5


def log(msg):
    print(f"[gitphin] backup: {msg}", file=sys.stderr, flush=True)


def git_cmd(args):
    cmd = ["git", "-c", "http.proxy=", "-c", "gc.auto=0"] + args
    return ["nice", "-n", "19"] + cmd if shutil.which("nice") else cmd


def run_git(args):
    env = dict(
        os.environ, GIT_TERMINAL_PROMPT="0", NO_PROXY="127.0.0.1,localhost", LC_ALL="C"
    )
    p = subprocess.run(
        git_cmd(args), env=env, stdin=subprocess.DEVNULL, capture_output=True, text=True
    )
    if p.returncode != 0:
        lines = [l for l in (p.stderr or p.stdout).strip().splitlines() if l.strip()]
        raise RuntimeError(lines[-1] if lines else f"git exit {p.returncode}")


def plural(n):
    return f"{n} repo" + ("" if n == 1 else "s")


class Backup:
    def __init__(self, notifier=None):
        self.lock = threading.Lock()
        self.thread = None
        self.auto_done = False
        self.notifier = notifier
        self.state = {
            "running": False,
            "started": None,
            "finished": None,
            "summary": "",
            "repos": [],
        }

    # ---- entry points
    def on_connect(self):
        """The link to the app came up: one automatic run per bridge process."""
        if self.auto_done or not config.backup()["enabled"]:
            return
        self.auto_done = True
        self.start()

    def start(self):
        """Start a run unless one is active. Returns False then."""
        with self.lock:
            if self.thread and self.thread.is_alive():
                return False
            self.thread = threading.Thread(target=self._run, daemon=True, name="backup")
            self.state.update(
                running=True, started=int(time.time()), summary="", repos=[]
            )
            self.thread.start()
            return True

    def status(self):
        cfg = config.backup()
        with self.lock:
            return {
                "enabled": cfg["enabled"],
                "dir": cfg["dir"],
                **{k: v for k, v in self.state.items()},
                "repos": list(self.state["repos"]),
            }

    # ---- the run
    def _wait_idle(self):
        """Wait until nothing is in flight and the Flipper was quiet. False when the link is gone."""
        while True:
            if not link.LINK.connected:
                return False
            if activity.idle_for() >= IDLE_S and link.LINK.busy_since is None:
                return True
            time.sleep(WAIT_STEP_S)

    def _run(self):
        cfg = config.backup()
        results = []
        try:
            if not self._wait_idle():
                raise RuntimeError("the Flipper app is not connected")
            names = [
                r["name"]
                for r in upstream.api("/api/git/repos").get("repos", [])
                if NAME_RE.match(r.get("name", ""))
            ]
            os.makedirs(cfg["dir"], exist_ok=True)
            for name in names:
                if not self._wait_idle():
                    raise RuntimeError("the Flipper app went away")
                res = self._one(cfg["dir"], name)
                results.append(res)
                with self.lock:
                    self.state["repos"] = list(results)
                time.sleep(PAUSE_S)
            failed = sum(1 for r in results if not r["ok"])
            if not names:
                summary = "no repos on the Flipper"
            elif failed:
                summary = f"{len(names) - failed} of {plural(len(names))} up to date, {failed} failed"
            else:
                summary = f"{plural(len(names))} up to date"
        except (upstream.UpstreamError, RuntimeError, OSError) as e:
            summary = f"stopped: {e}"
        log(summary)
        with self.lock:
            self.state.update(running=False, finished=int(time.time()), summary=summary)
        if self.notifier:
            self.notifier("Backup: " + summary)

    def _one(self, folder, name):
        dest = os.path.join(folder, name + ".git")
        url = f"http://127.0.0.1:{upstream.PORT}/git/{name}.git"
        res = {
            "name": name,
            "ok": True,
            "action": "update",
            "error": "",
            "time": int(time.time()),
        }
        try:
            if os.path.isdir(dest):
                run_git(["-C", dest, "remote", "update", "--prune"])
            else:
                res["action"] = "clone"
                part = dest + ".partial"
                shutil.rmtree(part, ignore_errors=True)
                try:
                    run_git(["clone", "--mirror", "-q", url, part])
                    os.rename(part, dest)
                finally:
                    shutil.rmtree(part, ignore_errors=True)
        except (RuntimeError, OSError) as e:
            res.update(ok=False, error=str(e))
            log(f"{name}: {e}")
        return res


def notify_send(text):
    if shutil.which("notify-send"):
        subprocess.Popen(
            ["notify-send", "-a", "Gitphin", "Gitphin", text],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )


BACKUP = Backup(notify_send)
