# Getting Started

## Build

Install uFBT, then run:

```powershell
ufbt
```

The built app will be written to:

```text
dist/debug_mode.fap
```

## Launch

Close qFlipper first if it is open, then run:

```powershell
ufbt launch
```

## Manual Install

Copy `dist/debug_mode.fap` to the Flipper SD card:

```text
/ext/apps/Tools/debug_mode.fap
```

## Basic Use

- `Start Debugging` creates a new case.
- `Cases` opens existing cases.
- `Link NFC Tag` links a PicoPass/iCLASS card to the current case.
- `Scan NFC Tag` opens a linked case.
- `Export Case` writes a text report.
- `Export Case Study` writes a Markdown report.
