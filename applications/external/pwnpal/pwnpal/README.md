# Pwnpal

A Flipper Zero app that makes your Flipper a **social pwngrid peer**, so a nearby
Pwnagotchi actually detects a friend, greets it ("Hello flippy! Nice to meet you."),
and — because the friend keeps a stable identity that grows over time — eventually
counts it as a *good friend* and shows the ♥‿‿♥ face.

Your Pwnagotchi can already be *detected* by Marauder (`sniffpwn`), but nothing ever
says hi back. Pwnpal fixes the loneliness: it drives the Flipper's ESP32 Wi-Fi board
to broadcast a Pwnagotchi-compatible advertisement beacon while also listening for real
units to show on screen.

## How it works

```
 Flipper (this app)                 ESP32 board (Marauder fork)          Air
 ------------------                 ---------------------------          ---
 persona: name, identity,           pwnpal command:
 face, uptime, friends met   --UART--> builds pwngrid beacon   --beacon--> your
 (grows, saved to SD)                  + sniffs pwnagotchis                 Pwnagotchi
                             <--UART-- PWNPAL_PEER reports  <--beacon--  says hi!
```

- The **persona** (who your friend is) lives on the Flipper and is saved to
  `/ext/apps_data/pwnpal/persona.bin`. Its 64-hex `identity` is minted once and kept,
  so the friendship persists across sessions and the encounter count on your Pwnagotchi
  keeps climbing.
- The Flipper pushes the persona to the ESP32 over UART; the ESP32 broadcasts it as a
  pwngrid beacon (source MAC `de:ad:be:ef:de:ad`, JSON in vendor IE 222) across the 2.4
  GHz channels, and reports any Pwnagotchis it hears back to the Flipper.
- The friend **grows**: uptime accumulates, it meets other units, and (in full
  pwnagotchi mode, below) it captures real handshakes that count as `pwnd_tot`, so its
  level ticks up for real. Its mood/face reacts — lonely when no one's around, excited on
  a new meeting, ♥ when a good friend lingers nearby.

## Full pwnagotchi mode

The friend isn't just a costume anymore. Beyond saying hi it can behave like a real
pwnagotchi:

- **scans** the APs around it (each one drives the on-screen APS count),
- passively **captures** WPA handshakes / PMKIDs from networks in range,
- saves a **crackable `.pcap` per network** on the Flipper SD
  (`/ext/apps_data/pwnpal/handshakes/<bssid>.pcap`, linktype 105). Each file bundles the
  network's ESSID beacon with its EAPOL/PMKID frames, so it opens straight in
  aircrack-ng / hcxtools (hashcat mode 22000) / Wireshark — no manual ESSID needed,
- **geotags** every sighting when a GPS is attached (Feberis Pro) and logs a
  **WiGLE-importable** `wardrive.csv` (`/ext/apps_data/pwnpal/wardrive.csv`),
- earns **real** pwnd from each capture, so `pwnd_run`/`pwnd_tot` are earned handshakes
  now, not just units met,
- and reacts with pwnagotchi **moods / faces** as it works.

There's an optional active **deauth** to nudge a client into re-handshaking. It's off by
default, resets to off every launch, and needs its own confirmation before it'll transmit.

## Controls

- **OK** — start / pause saying hi (advertising).
- **Back** — exit (persona is saved on the way out).

The screen shows the friend's face, name, level, mood, lifetime/session friends met,
the broadcast channel, and the units currently in range with signal bars.

## Requirements

- An ESP32 Wi-Fi board wired to the Flipper's default UART (GPIO 13/14, 115200) — the
  official **Flipper Wi-Fi Dev Board** and the **Feberis Pro** both work as-is.
- That board running a Marauder build with the `pwnpal` command — see
  [`../pwnpal-marauder/PATCH.md`](../pwnpal-marauder/PATCH.md).

## Build

Drop `pwnpal/` into your firmware's `applications_user/` and:

```
./fbt launch_app APPSRC=applications_user/pwnpal
```

## Protocol

The pwngrid air format and the Flipper↔ESP32 serial contract are documented in
[`../doc/PwnpalProtocol.md`](../doc/PwnpalProtocol.md).

## Safety & legality

By default the friend only says hi and listens — it advertises a presence beacon and
shows units in range. That's lawful anywhere and needs no setup.

It can *optionally* be switched into capture mode, which passively records WPA
handshakes / PMKIDs, and — if you separately enable it — deauth, which actively kicks
clients to force a handshake. **Both are off by default and gated behind an explicit
opt-in** (deauth resets to off every launch and needs its own confirmation).

> **⚠️ Capture and deauth are only legal on networks you own or are explicitly authorized
> to test.** Unauthorized use may be illegal where you live. You alone are responsible for
> how you use this. Educational use only.

When deauth is on, the friend advertises `policy.deauth: true` to the mesh — it does not
hide what it's doing.
