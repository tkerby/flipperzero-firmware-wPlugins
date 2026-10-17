#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["bleak>=0.21"]
# ///
"""Kota host: keeps a BLE link to the Flipper and pushes Claude plan usage to it.

Usage:  uv run host/kota.py            (or ./host/kota.py)

Reads the OAuth token Claude Code stores in ~/.claude/.credentials.json and asks
Anthropic for the same numbers `/usage` shows. On macOS the terminal running this
needs Bluetooth permission (System Settings > Privacy & Security > Bluetooth).
"""

import asyncio
import datetime as dt
import json
import re
import sys
import time
import urllib.request
from pathlib import Path

from bleak import BleakClient, BleakScanner

# Flipper advertises 0x308X, where X is the body color (0x3083 on a transparent one).
FLIPPER_ADV_UUID = re.compile(r"^0000308[0-9a-f]-0000-1000-8000-00805f9b34fb$")
TX_CHAR = "19ed82ae-ed21-4c9d-4145-228e61fe0000"  # Flipper -> host (notify)
RX_CHAR = "19ed82ae-ed21-4c9d-4145-228e62fe0000"  # host -> Flipper (write)
CREDENTIALS = Path.home() / ".claude" / ".credentials.json"
USAGE_URL = "https://api.anthropic.com/api/oauth/usage"
REFRESH_S = 120
NOT_KOTA_BACKOFF_S = 30


def log(msg: str) -> None:
    print(f"{time.strftime('%H:%M:%S')} {msg}", flush=True)


def fetch_usage() -> dict:
    # Claude Code refreshes this token itself; if it has not run for hours this returns 401.
    token = json.loads(CREDENTIALS.read_text())["claudeAiOauth"]["accessToken"]
    req = urllib.request.Request(
        USAGE_URL,
        headers={"Authorization": f"Bearer {token}", "anthropic-beta": "oauth-2025-04-20"},
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.load(resp)


def seconds_until(iso: str | None, now: float) -> int:
    if not iso:
        return 0
    return max(0, int(dt.datetime.fromisoformat(iso).timestamp() - now))


def build_line(usage: dict, now: float) -> str:
    s = usage.get("five_hour") or {}
    w = usage.get("seven_day") or {}
    return "U {} {} {} {}\n".format(
        round(s.get("utilization") or 0),
        seconds_until(s.get("resets_at"), now),
        round(w.get("utilization") or 0),
        seconds_until(w.get("resets_at"), now),
    )


def is_flipper(device, adv) -> bool:
    return any(FLIPPER_ADV_UUID.match(u.lower()) for u in adv.service_uuids)


async def serve(client: BleakClient, gone: asyncio.Event, state: dict) -> None:
    hello = asyncio.Event()

    def on_notify(_handle, data: bytearray) -> None:
        if b"KOTA" in data:
            hello.set()

    await client.start_notify(TX_CHAR, on_notify)

    # Kota says hello within ~1s of connecting; the stock firmware profile never does.
    try:
        await asyncio.wait_for(hello.wait(), timeout=4)
    except asyncio.TimeoutError:
        log("Flipper found but Kota is not open on it")
        state["backoff"] = NOT_KOTA_BACKOFF_S
        return
    log("Kota connected")

    while client.is_connected:
        now = time.time()
        if now - state["fetched"] >= REFRESH_S:
            state["fetched"] = now
            try:
                state["usage"] = fetch_usage()
            except Exception as e:  # keep showing the last good numbers
                log(f"usage fetch failed: {e}")
        if state["usage"]:
            line = build_line(state["usage"], now)
            await client.write_gatt_char(RX_CHAR, line.encode(), response=True)
            log(f"sent {line.strip()}")
        try:
            await asyncio.wait_for(gone.wait(), timeout=REFRESH_S)
        except asyncio.TimeoutError:
            pass
    log("disconnected")


async def main() -> None:
    state = {"usage": None, "fetched": 0.0, "backoff": 0}
    log("looking for the Flipper (open Kota on it)")
    while True:
        if state["backoff"]:
            await asyncio.sleep(state["backoff"])
            state["backoff"] = 0
        device = await BleakScanner.find_device_by_filter(is_flipper, timeout=10)
        if device is None:
            continue
        try:
            gone = asyncio.Event()
            async with BleakClient(device, disconnected_callback=lambda _c: gone.set()) as client:
                await serve(client, gone, state)
        except Exception as e:
            log(f"ble error: {e}")
            await asyncio.sleep(3)


def _self_check() -> None:
    now = 1_000_000.0
    iso = dt.datetime.fromtimestamp(now + 3600, dt.timezone.utc).isoformat()
    usage = {"five_hour": {"utilization": 33.6, "resets_at": iso}, "seven_day": None}
    assert build_line(usage, now) == "U 34 3600 0 0\n", build_line(usage, now)


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-check"]:
        _self_check()
        print("ok")
    else:
        try:
            asyncio.run(main())
        except KeyboardInterrupt:
            pass
