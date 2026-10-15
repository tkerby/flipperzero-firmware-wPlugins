#!/usr/bin/env python3
"""Build and run portable C tests, including local songs when present."""

import os
from pathlib import Path
import shlex
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    build.mkdir(exist_ok=True)
    executable = build / "test_core"
    subprocess.run([
        *shlex.split(os.environ.get("CC", "cc")),
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-Isrc",
        "src/tja_parser.c", "src/wav_reader.c", "tests/test_core.c",
        "-lm", "-o", str(executable),
    ], cwd=root, check=True)
    songs = sorted((root / "songs").rglob("*.tja"))
    subprocess.run([str(executable), *map(str, songs)], cwd=root, check=True)
    gpio_test = build / "test_gpio"
    subprocess.run([
        *shlex.split(os.environ.get("CC", "cc")),
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-Itests/stubs", "-Isrc",
        "src/taiko_gpio.c", "tests/test_gpio.c", "-o", str(gpio_test),
    ], cwd=root, check=True)
    subprocess.run([str(gpio_test)], cwd=root, check=True)


if __name__ == "__main__":
    main()
