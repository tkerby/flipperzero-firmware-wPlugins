"""Talks to the Flipper's CLI port: start the Gitphin app, reboot a stuck Flipper, notify the desktop."""

import shutil
import subprocess
import time

import serial

from .link import STUCK_S, SERIAL_ERRORS, log

APP_PATH = "/ext/apps/Tools/gitphin.fap"


def launch_app(cli_path):
    """Open the hub app on the Flipper through its CLI."""
    with serial.Serial(cli_path, timeout=0.5) as cli:
        cli.write(b"\r")
        time.sleep(0.3)
        cli.reset_input_buffer()
        cli.write(f"loader open {APP_PATH}\r".encode())
        time.sleep(1.0)
        try:
            answer = cli.read_all().decode(errors="replace")
        except SERIAL_ERRORS:
            # The app switches USB to two ports at once, so the CLI port drops.
            log("started app on the Flipper (USB switched)")
            return
    lines = [l.strip() for l in answer.splitlines() if l.strip() and ">:" not in l]
    log(f"started app on the Flipper: {' | '.join(lines[1:]) or 'ok'}")


def cli_command(cli_path, command):
    """Run one CLI command on the Flipper. The port may drop on purpose."""
    try:
        with serial.Serial(cli_path, timeout=0.5) as cli:
            cli.write(b"\r")
            time.sleep(0.3)
            cli.reset_input_buffer()
            cli.write(command.encode() + b"\r")
            time.sleep(1.0)
    except SERIAL_ERRORS:
        pass


def recover(cli_path):
    """The app runs (its port exists) but has not answered for STUCK_S.
    Reboot the Flipper. Do not try `loader close` first: closing an app whose
    worker is stuck makes the loader wait for that thread forever, which
    freezes the whole Flipper. Writes on the Flipper use temp + rename, so a
    reboot does not corrupt repositories."""
    log(f"app has not answered for {STUCK_S:.0f} s, rebooting the Flipper")
    cli_command(cli_path, "power reboot")


def notify(url, open_browser):
    if shutil.which("notify-send"):
        subprocess.Popen(["notify-send", "-a", "Gitphin", "Gitphin", f"Open {url}"])
    if open_browser and shutil.which("xdg-open"):
        subprocess.Popen(["xdg-open", url])
