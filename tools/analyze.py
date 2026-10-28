#!/usr/bin/env python3
"""Analyse RF Survey logs on a PC.

Two files come off the SD card and they answer different questions:

  survey_<ts>.csv   one row per sweep, one column per frequency bin
                    -> which channels are busy over time; aliased in time by design
  cadence_<ts>.csv  one row per sample at a SINGLE frequency, ~125 Hz
                    -> burst timing: period, jitter, duty, repeat groups

The firmware cannot tell you what a signal *is*; it can tell you how it behaves. This
script turns the logs into those behaviour numbers offline, where they are cheap to
compute and easy to compare between sessions.

Usage:
    ./analyze.py cadence_20261028_131500_042.csv
    ./analyze.py survey_20261028_131500_042.csv --trigger -85 --top 8
    ./analyze.py cadence_*.csv --plot cadence.png

Only numpy is required; --plot additionally needs matplotlib.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

INVALID = -128  # dBm sentinel for "band gap / not measured"


# --------------------------------------------------------------------------- parsing

@dataclass
class Log:
    kind: str                      # "survey" | "cadence"
    meta: dict = field(default_factory=dict)
    t_ms: np.ndarray = field(default_factory=lambda: np.array([]))
    # survey: (rows, bins) dBm, -128 = invalid; cadence: (rows, 1)
    data: np.ndarray = field(default_factory=lambda: np.zeros((0, 0)))


def _parse_meta(line: str, meta: dict) -> None:
    for token in line.lstrip("# ").split():
        if "=" in token:
            key, _, value = token.partition("=")
            try:
                meta[key] = int(value)
            except ValueError:
                meta[key] = value


def load(path: Path) -> Log:
    meta: dict = {}
    rows: list[list[int]] = []
    kind = ""
    with path.open() as fh:
        for line in fh:
            if line.startswith("#"):
                low = line.lower()
                if "binfreq" in low:
                    kind = "survey"
                elif "cadence" in low:
                    kind = "cadence"
                _parse_meta(line, meta)
                continue
            if not line.strip():
                continue
            rows.append([int(x) for x in line.split(",")])

    if not rows:
        raise SystemExit(f"{path}: no data rows")
    if not kind:  # header-less file: assume survey (1 timestamp + N bins)
        kind = "survey" if len(rows[0]) > 2 else "cadence"

    # int64 for the parse: the first column is the scheduler tick in ms and overflows int16;
    # the dBm payload is narrowed back to int16 afterwards, which halves the resident set.
    arr = np.array(rows, dtype=np.int64)
    log = Log(kind=kind, meta=meta, t_ms=arr[:, 0])
    log.data = arr[:, 1:].astype(np.int16)
    if kind == "survey" and "start" in meta and "step" in meta:
        nbins = log.data.shape[1]
        meta["binfreq"] = [
            int(meta["start"]) + i * int(meta["step"]) for i in range(nbins)
        ]
    return log


# --------------------------------------------------------------------------- stats

@dataclass
class Bursts:
    """Threshold crossings of one scalar series.

    `periods` is pre-filtered: an interval that spans an observation hole is not a
    measurement (the receiver was not listening, so a burst may have been missed inside it).
    The firmware applies the same rule on-device; this keeps the two views agreeing.
    """

    starts_ms: np.ndarray
    durations_ms: np.ndarray
    periods: np.ndarray = field(default_factory=lambda: np.array([]))
    dropped: int = 0

    @property
    def periods_ms(self) -> np.ndarray:
        return self.periods

    def summary(self) -> dict:
        per = self.periods_ms
        if not len(per):
            return {"bursts": len(self.starts_ms), "period_ms": 0.0, "cv": 0.0,
                    "sd_ms": 0.0, "duty": 0.0}
        mean = float(per.mean())
        sd = float(per.std())
        airtime = float(self.durations_ms.sum())
        span = float(self.starts_ms[-1] - self.starts_ms[0] + self.durations_ms[-1])
        return {
            "bursts": len(self.starts_ms),
            "period_ms": mean,
            "sd_ms": sd,
            "cv": sd / mean if mean else 0.0,
            "duty": airtime / span if span else 0.0,
        }


def find_holes(t_ms: np.ndarray, sample_ms: int, factor: int = 5) -> np.ndarray:
    """Timestamps where the sample clock skipped: the loop is 'sample, maybe write SD,
    sleep', so sample_ms is a floor. A hole means bursts inside it were never seen."""
    if len(t_ms) < 2:
        return np.array([], dtype=np.int64)
    dt = np.diff(t_ms)
    return t_ms[1:][dt > factor * sample_ms]


def segment(values: np.ndarray, t_ms: np.ndarray, threshold: float,
            hole_ticks: np.ndarray | None = None) -> Bursts:
    above = values > threshold
    edges = np.diff(above.astype(np.int8), prepend=0)
    starts = t_ms[edges == 1]
    ends = t_ms[np.diff(above.astype(np.int8), append=0) == -1]
    n = min(len(starts), len(ends))
    starts, ends = starts[:n], ends[:n]

    periods = np.diff(starts) if n > 1 else np.array([], dtype=np.int64)
    dropped = 0
    if hole_ticks is not None and len(hole_ticks) and len(periods):
        # a hole strictly between two rising edges voids that period
        j = np.searchsorted(hole_ticks, starts[:-1], side="right")
        inside = (j < len(hole_ticks)) & (hole_ticks[np.minimum(j, len(hole_ticks) - 1)] < starts[1:])
        dropped = int(inside.sum())
        periods = periods[~inside]
        # and a burst whose duration contains a hole has no usable length
        k = np.searchsorted(hole_ticks, starts, side="right")
        bad = (k < len(hole_ticks)) & (hole_ticks[np.minimum(k, len(hole_ticks) - 1)] < ends)
        starts, ends = starts[~bad], ends[~bad]
    return Bursts(starts_ms=starts, durations_ms=(ends - starts), periods=periods, dropped=dropped)


def classify(s: dict, sample_ms: int) -> str:
    """Machine vs human. Absolute spread first: at a 5-10 ms sample clock a genuinely
    periodic 100 ms device still shows CV ~0.02, which would be read as 'not a machine'."""
    if s["bursts"] < 3:
        return "measuring (too few bursts)"
    if s["sd_ms"] <= 3 * sample_ms:
        return "MACHINE (crystal-locked)"
    if s["cv"] < 0.15:
        return "semi-periodic"
    return "HUMAN / event-driven"


def repeat_groups(b: Bursts, gap_ms: int = 250) -> np.ndarray:
    """How many frames each 'press' contains: a handheld remote sends a group of 3-10,
    a sensor sends one."""
    if not len(b.starts_ms):
        return np.array([], dtype=int)
    groups, count = [], 1
    for delta in np.diff(b.starts_ms):
        if delta < gap_ms:
            count += 1
        else:
            groups.append(count)
            count = 1
    groups.append(count)
    return np.array(groups, dtype=int)


# --------------------------------------------------------------------------- reports

def report_cadence(log: Log, threshold: float) -> None:
    sample_ms = int(log.meta.get("sample_ms", 8))
    rssi = log.data[:, 0].astype(float)
    freq = log.meta.get("freq", 0)

    print(f"freq       {freq / 1e6:.3f} MHz   preset {log.meta.get('preset', '?')}")
    print(f"samples    {len(rssi)}   {sample_ms} ms each  ({len(rssi) * sample_ms / 1000:.1f} s)")

    quiet = float(np.percentile(rssi, 25))
    loud = float(np.percentile(rssi, 99.5))
    print(f"floor      {quiet:.1f} dBm (p25)   peak {loud:.1f} dBm   contrast {loud - quiet:.1f} dB")

    thr = threshold if threshold is not None else quiet + 8
    holes = find_holes(log.t_ms, sample_ms)
    b = segment(rssi, log.t_ms, thr, holes)
    s = b.summary()

    print(f"threshold  {thr:.1f} dBm")
    if len(holes):
        share = len(holes) / max(len(log.t_ms), 1) * 100
        print(f"gaps       {len(holes)} sample holes ({share:.1f}% of samples) -- "
              f"{b.dropped} period(s) discarded for spanning one")
    print(f"bursts     {s['bursts']}")
    if s["period_ms"]:
        print(f"period     {s['period_ms'] / 1000:.3f} s   sd {s['sd_ms']:.1f} ms   "
              f"cv {s['cv']:.3f}")
        print(f"duty       {s['duty'] * 100:.2f} %")
        print(f"class      {classify(s, sample_ms)}")
        groups = repeat_groups(b)
        if len(groups):
            uniq, counts = np.unique(groups, return_counts=True)
            hist = "  ".join(f"x{u}:{c}" for u, c in zip(uniq, counts))
            print(f"repeats    {hist}")
    if len(b.durations_ms):
        print(f"burst len  min {b.durations_ms.min()}  median "
              f"{np.median(b.durations_ms):.0f}  max {b.durations_ms.max()} ms")


def report_survey(log: Log, threshold: float, top: int) -> None:
    dbm = log.data.astype(float)
    dbm[dbm == INVALID] = np.nan
    freqs = log.meta.get("binfreq", [0] * dbm.shape[1])
    thr = threshold if threshold is not None else float(np.nanpercentile(dbm, 75)) + 8

    span_s = (log.t_ms[-1] - log.t_ms[0]) / 1000 if len(log.t_ms) > 1 else 0
    sweep_s = span_s / max(len(log.t_ms) - 1, 1)
    print(f"sweeps     {len(log.t_ms)}   {sweep_s:.2f} s apart  ({span_s:.0f} s total)")
    print(f"bins       {dbm.shape[1]}   {log.meta.get('start', 0) / 1e6:.0f}-"
          f"{log.meta.get('end', 0) / 1e6:.0f} MHz")
    print(f"threshold  {thr:.1f} dBm")
    print()
    print("NOTE: each bin is sampled once per sweep, so anything shorter than a sweep is "
          "aliased.\n      Burst periods from this file are lower bounds. Use a cadence log "
          "for timing.")

    occupancy = np.nanmean(dbm > thr, axis=0)
    peak = np.nanmax(dbm, axis=0)
    order = np.argsort(occupancy)[::-1][:top]

    print()
    print(f"{'MHz':>12}  {'peak':>6}  {'busy%':>6}  behaviour")
    for idx in order:
        if not np.isfinite(occupancy[idx]) or occupancy[idx] == 0:
            continue
        col = dbm[:, idx]
        b = segment(np.nan_to_num(col, nan=INVALID).astype(float), log.t_ms, thr)
        s = b.summary()
        behaviour = classify(s, int(sweep_s * 1000)) if s["bursts"] >= 3 else "-"
        print(f"{freqs[idx] / 1e6:12.3f}  {peak[idx]:6.1f}  {occupancy[idx] * 100:6.1f}  "
              f"{behaviour}")


def plot(log: Log, path: Path, threshold: float) -> None:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        raise SystemExit("--plot needs matplotlib: pip install matplotlib")

    fig, ax = plt.subplots(figsize=(12, 5))
    if log.kind == "cadence":
        ax.plot(log.t_ms / 1000.0, log.data[:, 0])
        ax.axhline(threshold if threshold is not None else np.percentile(log.data[:, 0], 33) + 8,
                   color="r", lw=0.6, ls="--")
        ax.set_xlabel("t (s)")
        ax.set_ylabel("RSSI (dBm)")
        ax.set_title(f"cadence @ {log.meta.get('freq', 0) / 1e6:.3f} MHz")
    else:
        dbm = log.data.astype(float)
        dbm[dbm == INVALID] = np.nan
        img = ax.imshow(dbm.T, aspect="auto", origin="lower", cmap="inferno",
                        extent=[log.t_ms[0] / 1000, log.t_ms[-1] / 1000, 0, dbm.shape[1]])
        ax.set_xlabel("t (s)")
        ax.set_ylabel("bin")
        ax.set_title("survey")
        fig.colorbar(img, ax=ax, label="dBm")
    fig.tight_layout()
    fig.savefig(path, dpi=120)
    print(f"wrote {path}")


# --------------------------------------------------------------------------- cli

def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("-t", "--trigger", type=float, default=None,
                    help="dBm threshold (default: p25+8 for cadence, p75+8 for survey)")
    ap.add_argument("-n", "--top", type=int, default=6, help="survey: channels to list")
    ap.add_argument("-p", "--plot", type=Path, default=None)
    args = ap.parse_args(argv)

    for i, path in enumerate(args.files):
        if not path.exists():
            print(f"{path}: not found", file=sys.stderr)
            continue
        if i:
            print("\n" + "=" * 62 + "\n")
        log = load(path)
        print(f"== {path.name}  [{log.kind}]")
        if log.kind == "cadence":
            report_cadence(log, args.trigger)
        else:
            report_survey(log, args.trigger, args.top)
        if args.plot:
            out = args.plot if len(args.files) == 1 else args.plot.with_name(
                f"{args.plot.stem}-{i}{args.plot.suffix}")
            plot(log, out, args.trigger)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
