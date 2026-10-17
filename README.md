# Kota

Your Claude plan usage on a Flipper Zero, over Bluetooth.

![Kota on a Flipper Zero](flipper/screenshots/1.png)

Kota shows the two limits `/usage` shows in Claude Code: the 5-hour session
window and the weekly window, each with a live countdown to its reset. A small
agent on your Mac fetches the numbers and pushes them to the Flipper every two
minutes. No cable.

*Kota* is Turkish for "quota".

## Install

**1. Flipper:** install Kota from the [Flipper Apps Catalog](https://lab.flipper.net/apps/kota)
(Apps > Bluetooth > Kota), or build it yourself with `cd flipper && ufbt launch`.

**2. Mac:** you need a logged-in [Claude Code](https://claude.com/claude-code).

```bash
brew install cobanov/tap/kota
brew services start kota
```

macOS asks once whether `kota` may use Bluetooth: allow it. From then on kota
runs in the background, starts at login, and finds the Flipper whenever you open
Kota on it.

Open **Apps > Bluetooth > Kota** on the Flipper. It links within a few seconds;
the dot in the top-right corner is filled while connected. Press **OK** to
refresh right away.

Check that the login works with `kota --once`, and see what it is doing with
`tail -f $(brew --prefix)/var/log/kota.log`.

**Linux / Windows:** run the Python companion with [uv](https://docs.astral.sh/uv/):

```bash
uv run https://raw.githubusercontent.com/cobanov/kota/main/host/kota.py
```

## How it works

```
Claude Code login ──> kota agent (Mac) ──BLE──> Kota app (Flipper)
```

- The agent reads the OAuth login Claude Code keeps (macOS keychain or
  `~/.claude/.credentials.json`) and asks Anthropic for your plan usage, the same
  numbers `/usage` shows.
- It finds the Flipper over Bluetooth LE, waits for the Kota app to say hello,
  and sends one line every two minutes: `U <session%> <session_reset_s> <week%> <week_reset_s>`.
- The Flipper app draws the meters and counts the resets down between updates.

Your login never leaves your Mac. Only the numbers go to the Flipper.

## Repo layout

- `flipper/`: the Flipper app (C, built with ufbt)
- `mac/`: the macOS agent (Swift, CoreBluetooth)
- `host/kota.py`: the cross-platform companion (Python, bleak)

## Notes

- Claude Code refreshes its login on its own. If it has not been opened for many
  hours the fetch fails with 401 and Kota keeps showing the last numbers it got.
- The usage endpoint is the one Claude Code itself uses; it is not a documented
  public API and may change.
- While Kota is open the Flipper's normal Bluetooth (mobile app connection) is
  paused. It comes back when you exit.
- The Python companion on macOS needs Bluetooth permission for the terminal
  running it; background terminal daemons (tmux servers and the like) may not get
  it. Prefer the brew agent on a Mac.

## License

MIT
