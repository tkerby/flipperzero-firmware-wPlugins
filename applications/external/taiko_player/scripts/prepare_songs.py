#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 ZeroNinx

"""Prepare TJA + 8-bit mono PCM WAV files for Taiko Player's SD directory."""

import argparse
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
import wave


def decode_tja(path):
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "cp932", "gb18030"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    raise ValueError(f"Cannot decode TJA: {path}")


def wave_name(text):
    for line in text.splitlines():
        match = re.match(r"\s*WAVE\s*:(.*)", line)
        if match:
            name = match[1].split("//", 1)[0].strip().replace("\\", "/")
            path = PurePosixPath(name)
            if not name or path.is_absolute() or ".." in path.parts or ":" in name:
                raise ValueError(
                    f"WAVE must be a relative path inside the song folder: {name}"
                )
            return Path(path)
    raise ValueError("Missing WAVE in TJA")


def valid_wav(path, rate):
    try:
        with wave.open(str(path), "rb") as audio:
            return (
                audio.getnchannels(),
                audio.getsampwidth(),
                audio.getframerate(),
            ) == (1, 1, rate)
    except (OSError, EOFError, wave.Error):
        return False


def prepare(source, output, rate=22050, force=False):
    source, output = source.resolve(), output.resolve()
    if source == output or source in output.parents:
        raise ValueError("Output must be outside the source songs directory")
    if source.is_file():
        charts, root = [source], source.parent
    else:
        charts, root = (
            sorted(p for p in source.rglob("*") if p.suffix.lower() == ".tja"),
            source,
        )
    if not charts:
        raise ValueError(f"No TJA files found in {source}")
    converted = set()
    for chart in charts:
        text = decode_tja(chart)
        original_name = wave_name(text)
        audio_source = chart.parent / original_name
        if not audio_source.is_file():
            raise ValueError(f"Audio file not found: {audio_source}")
        destination = output / chart.relative_to(root)
        wav_name = original_name.with_suffix(".wav")
        wav_target = destination.parent / wav_name
        wav_target.parent.mkdir(parents=True, exist_ok=True)
        pair = (audio_source, wav_target)
        if pair not in converted:
            fresh = (
                wav_target.exists()
                and wav_target.stat().st_mtime >= audio_source.stat().st_mtime
                and valid_wav(wav_target, rate)
            )
            if force or not fresh:
                # Write atomically so interrupted conversions cannot leave a playable partial file.
                with tempfile.NamedTemporaryFile(
                    dir=wav_target.parent, suffix=".wav", delete=False
                ) as temp:
                    temporary = Path(temp.name)
                try:
                    subprocess.run(
                        [
                            "ffmpeg",
                            "-nostdin",
                            "-hide_banner",
                            "-loglevel",
                            "error",
                            "-y",
                            "-i",
                            str(audio_source),
                            "-vn",
                            "-ac",
                            "1",
                            "-ar",
                            str(rate),
                            "-c:a",
                            "pcm_u8",
                            "-map_metadata",
                            "-1",
                            "-fflags",
                            "+bitexact",
                            "-flags:a",
                            "+bitexact",
                            "-f",
                            "wav",
                            str(temporary),
                        ],
                        check=True,
                    )
                    if not valid_wav(temporary, rate):
                        raise ValueError(
                            f"Unexpected converted WAV format: {audio_source}"
                        )
                    temporary.replace(wav_target)
                finally:
                    temporary.unlink(missing_ok=True)
            converted.add(pair)
        lines = text.splitlines()
        lines = [
            f"WAVE:{wav_name.as_posix()}" if re.match(r"\s*WAVE\s*:", line) else line
            for line in lines
        ]
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text("\n".join(lines) + "\n", encoding="utf-8")
        with wave.open(str(wav_target), "rb") as audio:
            duration = audio.getnframes() / audio.getframerate()
        print(
            f"{destination.relative_to(output)}: {duration:.1f}s, {wav_target.stat().st_size:,} bytes"
        )
    return len(charts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, default=Path("songs"))
    parser.add_argument(
        "--output", type=Path, default=Path("sdcard/apps_data/taiko_player/songs")
    )
    parser.add_argument(
        "--sample-rate",
        type=int,
        choices=range(8000, 48001),
        metavar="8000..48000",
        default=22050,
    )
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if not shutil.which("ffmpeg"):
        parser.error("ffmpeg is required; install it using your system package manager")
    try:
        count = prepare(args.source, args.output, args.sample_rate, args.force)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    print(
        f"Prepared {count} chart(s). Copy the contents of sdcard/ to the SD card root."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
