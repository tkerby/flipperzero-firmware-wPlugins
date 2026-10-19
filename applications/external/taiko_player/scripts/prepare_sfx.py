#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 ZeroNinx

"""Convert drum effects to PCM WAV and optionally prepare a silent test chart."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import wave

from prepare_songs import valid_wav


def prepare(source, output):
    output.mkdir(parents=True, exist_ok=True)
    for name in ("dong", "ka"):
        original = source / f"{name}.ogg"
        if not original.is_file():
            print(f"Skipping missing effect: {original}")
            continue
        target = output / f"{name}.wav"
        if not (
            target.exists()
            and target.stat().st_mtime >= original.stat().st_mtime
            and valid_wav(target, 22050)
        ):
            with tempfile.NamedTemporaryFile(
                dir=output, suffix=".wav", delete=False
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
                        str(original),
                        "-vn",
                        "-ac",
                        "1",
                        "-ar",
                        "22050",
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
                if not valid_wav(temporary, 22050):
                    raise ValueError(f"Unexpected WAV format: {original}")
                with wave.open(str(temporary), "rb") as audio:
                    if audio.getnframes() > 32768:
                        raise ValueError(
                            f"Effect exceeds the device's 32768-sample limit: {original}"
                        )
                temporary.replace(target)
            finally:
                temporary.unlink(missing_ok=True)
        with wave.open(str(target), "rb") as audio:
            if audio.getnframes() > 32768:
                raise ValueError(
                    f"Effect exceeds the device's 32768-sample limit: {target}"
                )
            print(
                f"{target}: {audio.getnframes() / audio.getframerate():.3f}s, "
                f"{audio.getnframes():,} PCM bytes"
            )
    # Keep upstream attribution with the converted sounds and USB uploads.
    for name in ("SFX-LICENSE.txt", "SFX-NOTICE.md"):
        notice = source / name
        if notice.is_file() and notice.resolve() != (output / name).resolve():
            shutil.copyfile(notice, output / name)


def test_song(output):
    output.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output / "silence.wav"), "wb") as audio:
        audio.setparams((1, 1, 22050, 0, "NONE", "not compressed"))
        audio.writeframes(b"\x80" * (22050 * 10))
    (output / "sound_test.tja").write_text(
        "TITLE:Drum Sound Test\nBPM:60\nWAVE:silence.wav\nOFFSET:-1\n"
        "COURSE:Easy\nLEVEL:1\n#START\n#MEASURE 1/4\n"
        "1,\n2,\n1,\n2,\n3,\n4,\n#BPMCHANGE 120\n1,\n2,\n1,\n2,\n#END\n",
        encoding="utf-8",
    )
    print(f"Prepared {output}: 10-second silent backing with alternating drum notes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, default=Path("assets"))
    parser.add_argument(
        "--output", type=Path, default=Path("sdcard/apps_data/taiko_player/sfx")
    )
    parser.add_argument("--test-song", action="store_true")
    args = parser.parse_args()
    prepare(args.source, args.output)
    if args.test_song:
        test_song(args.output.parent / "songs" / "Drum Sound Test")


if __name__ == "__main__":
    main()
