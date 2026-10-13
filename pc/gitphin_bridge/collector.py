"""PC-side data for the Collector tab.

Runs on the user's own machine, inside the bridge process. It reports
information about this PC and the Wi-Fi it is connected to. The Wi-Fi
password is read locally and sent only to the local browser; it is never
written to the log.

Linux / NetworkManager only for now. Every value degrades to null with a
short note instead of raising, so the page always renders.
"""

import json
import os
import platform
import re
import shutil
import socket
import subprocess
import time

TIMEOUT = 4


def _run(cmd):
    """Run a command, return stdout text, or '' on any failure."""
    try:
        out = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=TIMEOUT,
            check=False,
        )
        return out.stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def _first_line(path):
    try:
        with open(path, "r") as f:
            return f.readline().strip()
    except OSError:
        return ""


def _os_name():
    data = {}
    try:
        with open("/etc/os-release") as f:
            for line in f:
                k, _, v = line.partition("=")
                data[k] = v.strip().strip('"')
    except OSError:
        pass
    return data.get("PRETTY_NAME") or platform.platform()


def _uptime_seconds():
    raw = _first_line("/proc/uptime")
    try:
        return int(float(raw.split()[0]))
    except (ValueError, IndexError):
        return None


def _mem():
    total = avail = None
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemTotal:"):
                    total = int(line.split()[1]) * 1024
                elif line.startswith("MemAvailable:"):
                    avail = int(line.split()[1]) * 1024
                if total and avail:
                    break
    except OSError:
        pass
    return total, avail


def _ip_addresses():
    """Global IPv4 addresses with their interface, from `ip`."""
    out = _run(["ip", "-o", "-4", "addr", "show", "scope", "global"])
    addrs = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 4:
            addrs.append({"iface": parts[1], "cidr": parts[3]})
    return addrs


def _active_connections():
    """Active NetworkManager connections: name, type, device."""
    if not shutil.which("nmcli"):
        return []
    out = _run(["nmcli", "-t", "-f", "NAME,TYPE,DEVICE", "connection", "show", "--active"])
    conns = []
    for line in out.splitlines():
        # nmcli escapes ':' inside fields as '\:'; split on unescaped ':'.
        fields = re.split(r"(?<!\\):", line)
        fields = [f.replace("\\:", ":") for f in fields]
        if len(fields) >= 3 and fields[0]:
            conns.append({"name": fields[0], "type": fields[1], "device": fields[2]})
    return conns


def collect_pc():
    total, avail = _mem()
    return {
        "hostname": socket.gethostname(),
        "os": _os_name(),
        "kernel": platform.release(),
        "arch": platform.machine(),
        "cpu_count": os.cpu_count(),
        "mem_total": total,
        "mem_available": avail,
        "uptime_s": _uptime_seconds(),
        "user": os.environ.get("USER") or "",
        "ips": _ip_addresses(),
        "connections": _active_connections(),
    }


# ---------- Wi-Fi ----------


def _active_wifi_conn():
    """The active NetworkManager connection carrying Wi-Fi, or None."""
    for c in _active_connections():
        if c["type"].endswith("wireless"):
            return c
    return None


def _conn_field(name, field):
    """One field of a connection, e.g. '802-11-wireless.ssid'. '' on failure."""
    return _run(["nmcli", "-g", field, "connection", "show", name]).strip()


def _wifi_signal_rate():
    """Best-effort signal and rate of the connected AP, or (None, None).

    `nmcli device wifi` can trigger a scan and be slow, so this is optional;
    the SSID never depends on it.
    """
    out = _run(["nmcli", "-t", "-f", "ACTIVE,SSID,SIGNAL,RATE", "device", "wifi"])
    for line in out.splitlines():
        fields = [f.replace("\\:", ":") for f in re.split(r"(?<!\\):", line)]
        if fields and fields[0] == "yes":
            signal = fields[2] if len(fields) > 2 else None
            rate = fields[3] if len(fields) > 3 else None
            return signal, rate
    return None, None


def _read_psk(conn_name):
    """Try to read the saved PSK. Returns (password, source).

    Needs root for the secret, so this works when the bridge runs as root,
    or through the optional sudo helper (pc/wifi_psk.sh). Otherwise the
    password is None and the caller explains why.
    """
    if not conn_name or not shutil.which("nmcli"):
        return None, None

    # 1. Direct nmcli (works when the bridge itself is root).
    out = _run(
        ["nmcli", "-s", "-g", "802-11-wireless-security.psk", "connection", "show", conn_name]
    )
    psk = out.strip()
    if psk:
        return psk, "nmcli"

    # 2. Narrow sudo helper installed by root. It must be a root-owned path, so a
    #    NOPASSWD sudoers rule for it cannot be abused by editing the script.
    helper = "/usr/local/sbin/gitphin-wifi-psk"
    if os.path.exists(helper) and shutil.which("sudo"):
        out = _run(["sudo", "-n", helper, conn_name])
        psk = out.strip()
        if psk:
            return psk, "sudo helper"

    return None, None


def collect_wifi():
    conn = _active_wifi_conn()
    if not conn:
        return {
            "connected": False,
            "note": "No active Wi-Fi connection (or NetworkManager is not in use).",
        }

    name = conn["name"]
    # The SSID comes from the active connection, so it needs no Wi-Fi scan.
    ssid = _conn_field(name, "802-11-wireless.ssid") or name
    signal, rate = _wifi_signal_rate()
    psk, source = _read_psk(name)
    result = {
        "connected": True,
        "ssid": ssid,
        "signal": signal,
        "rate": rate,
        "password": psk,
        "password_source": source,
    }
    if psk is None:
        result["note"] = (
            "The saved password needs root to read. Run the bridge as root, "
            "or install the sudo helper (see README: Wi-Fi password)."
        )
    return result


def as_json(kind):
    if kind == "info":
        return json.dumps(collect_pc()).encode()
    if kind == "wifi":
        return json.dumps(collect_wifi()).encode()
    return None
