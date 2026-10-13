"""The git remote helper protocol (git-remote-flipper): list, fetch, push over flipper://<name>."""

import re
import shutil
import sys
import tempfile

from .api import SHA_RE, ZERO, Hub, HubError, check_name, log, valid_ref
from .gitutil import build_pack, fetch_packs, git, git_out, local_objects, pack_dir, upload_pack

class Helper:
    def __init__(self, name):
        self.name = name
        self.hub = Hub()
        self.refs = None   # {ref: sha} as read by `list`; None until read
        self.head = ""
        self.exists = False

    def load_refs(self):
        got = self.hub.refs(self.name)
        self.exists = got is not None
        self.refs, self.head = got if got else ({}, "")

    # -- commands
    def cmd_list(self, out):
        self.load_refs()
        for ref in sorted(self.refs):
            out(f"{self.refs[ref]} {ref}")
        if self.head in self.refs:
            out(f"@{self.head} HEAD")
        out("")

    def cmd_fetch(self, out):
        n = fetch_packs(self.hub, self.name, pack_dir())
        if n:
            log(f"fetched {n} pack(s)")
        out("")

    def cmd_push(self, specs, out):
        if self.refs is None:
            self.load_refs()
        results = self.push(specs)
        for dst, err in results:
            out(f"ok {dst}" if err is None else f"error {dst} {err}")
        out("")

    # -- push logic
    def plan_one(self, force, src, dst):
        """Returns (new_sha, None) or (None, error text) for one refspec."""
        if not valid_ref(dst):
            return None, "invalid ref name"
        old = self.refs.get(dst, ZERO)
        if not src:
            return (ZERO, None) if old != ZERO else (None, "remote ref does not exist")
        try:
            new = git_out(["rev-parse", "--verify", "-q", src])
        except HubError:
            new = ""
        if not SHA_RE.match(new):
            return None, "src refspec does not match any"
        if old == ZERO or old == new or force:
            return new, None
        if dst.startswith("refs/tags/"):
            return None, "already exists"
        if old not in local_objects([old]):
            return None, "fetch first"
        ok = git(["merge-base", "--is-ancestor", old, new], check=False).returncode == 0
        return (new, None) if ok else (None, "non-fast-forward")

    def push(self, specs):
        plan, results = [], {}
        for force, src, dst in specs:
            new, err = self.plan_one(force, src, dst)
            if err:
                results[dst] = err
            else:
                plan.append((dst, new))
        changes = [(d, n) for d, n in plan if n != self.refs.get(d, ZERO)]
        for dst, new in plan:
            results.setdefault(dst, None)
        if changes:
            try:
                uploaded = self.send_objects([n for _, n in changes if n != ZERO])
            except HubError as e:
                for dst, _ in changes:
                    results[dst] = str(e)
                return [(s[2], results[s[2]]) for s in specs]
            for dst, new in changes:
                results[dst] = self.update_ref(dst, new)
            self.finish(changes, results, uploaded)
        return [(s[2], results[s[2]]) for s in specs]

    def send_objects(self, tips):
        """Create the repo if needed, then pack and upload. Returns True when a pack went up."""
        if not self.exists:
            log(f"creating repository {self.name}")
            self.hub.api("POST", "/api/git/create", name=self.name)
            self.exists = True
        if not tips:
            return False
        outdir = tempfile.mkdtemp(prefix="flipper-pack-")
        try:
            excludes = sorted(local_objects(sorted(set(self.refs.values()))))
            h = build_pack(sorted(set(tips)), excludes, outdir)
            if not h:
                return False
            upload_pack(self.hub, self.name, outdir, h)
            return True
        finally:
            shutil.rmtree(outdir, ignore_errors=True)

    def update_ref(self, dst, new):
        old = self.refs.get(dst, ZERO)
        try:
            self.hub.api("POST", "/api/git/ref", name=self.name, ref=dst, old=old, new=new)
        except HubError as e:
            return "stale info" if e.status == 409 else str(e)
        if new == ZERO:
            self.refs.pop(dst, None)
        else:
            self.refs[dst] = new
        return None

    def finish(self, changes, results, uploaded):
        pushed = [d for d, n in changes if n != ZERO and results[d] is None and d.startswith("refs/heads/")]
        if pushed and self.head not in self.refs:
            branch = pushed[0][len("refs/heads/"):]
            try:
                self.hub.api("POST", "/api/git/head", name=self.name, branch=branch)
                self.head = "refs/heads/" + branch
            except HubError as e:
                log(f"cannot set HEAD: {e}")
        if uploaded or any(results[d] is None for d, _ in changes):
            try:
                self.hub.server_info(self.name)
            except HubError as e:
                log(f"server-info failed: {e}")


def read_batch(stdin, first):
    """The command line `first` plus the following lines up to the blank line."""
    batch = [first]
    for line in stdin:
        line = line.rstrip("\n")
        if not line:
            break
        batch.append(line)
    return batch


def parse_push(line):
    spec = line[len("push "):]
    force = spec.startswith("+")
    src, _, dst = spec.lstrip("+").partition(":")
    return force, src, dst


def helper_main(argv):
    if not argv:
        log("usage: git-remote-flipper <remote> [<url>]")
        return 2
    url = argv[-1]
    name = re.sub(r"^flipper:/*", "", url).strip("/")
    name = name[:-4] if name.endswith(".git") else name
    helper = Helper(check_name(name))

    def out(line):
        sys.stdout.write(line + "\n")
        if not line:
            sys.stdout.flush()

    stdin = iter(sys.stdin.readline, "")
    for line in stdin:
        line = line.rstrip("\n")
        if line == "capabilities":
            for cap in ("option", "fetch", "list", "push"):
                out(cap)
            out("")
        elif line.startswith("option "):
            out(handle_option(helper, line))
            sys.stdout.flush()
        elif line.startswith("list"):
            helper.cmd_list(out)
        elif line.startswith("fetch "):
            read_batch(stdin, line)
            helper.cmd_fetch(out)
        elif line.startswith("push "):
            specs = [parse_push(b) for b in read_batch(stdin, line)]
            helper.cmd_push(specs, out)
        elif not line:
            break
        else:
            log(f"unknown command: {line}")
            return 1
    sys.stdout.flush()
    return 0


def handle_option(helper, line):
    parts = line.split()
    if len(parts) == 3 and parts[1] == "progress":
        helper.hub.progress = parts[2] == "true"
        return "ok"
    if len(parts) == 3 and parts[1] == "verbosity":
        return "ok"
    return "unsupported"
