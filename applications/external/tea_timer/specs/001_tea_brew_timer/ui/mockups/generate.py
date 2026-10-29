#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Генератор референсных макетов UI приложения Tea Timer.

Рисует экраны в реальном разрешении Flipper Zero (128x64) в SVG
и (если доступен rsvg-convert) растровые превью в PNG.

Запуск:
    python3 ui/mockups/generate.py

Правки макетов делаются здесь: словари SCREENS ниже.
Все размеры — в пикселях экрана 128x64.
"""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

BASE = Path(__file__).resolve().parent
PREVIEW = BASE / "preview"

W, H = 128, 64
BG = "#F4F1E3"  # светлый фон «LCD»
INK = "#1A1A1A"  # «пиксели»
FONT = "Menlo, Consolas, DejaVu Sans Mono, monospace"

# Базовые линии строк (стандартная сетка экрана)
Y_TITLE = 9
Y_ROW1, Y_ROW2, Y_ROW3, Y_ROW4 = 21, 32, 43, 54
Y_HINT = 63


def esc(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text_el(text, y, size=10, align="l", fill=INK, bold=False, x=None):
    if align == "c":
        anchor, tx = "middle", W // 2 if x is None else x
    elif align == "r":
        anchor, tx = "end", W - 3 if x is None else x
    else:
        anchor, tx = "start", 3 if x is None else x
    weight = ' font-weight="bold"' if bold else ""
    return (
        f'<text x="{tx}" y="{y}" font-family="{FONT}" font-size="{size}" '
        f'fill="{fill}" text-anchor="{anchor}"{weight}>{esc(text)}</text>'
    )


def band_el(y_baseline, height=12, top=None, fill=INK, stroke=None, x=0, w=W):
    top = y_baseline - 9 if top is None else top
    s = f' stroke="{stroke}" stroke-width="1"' if stroke else ""
    return f'<rect x="{x}" y="{top}" width="{w}" height="{height}" fill="{fill}"{s}/>'


def status_icons():
    """Иконки статус-бара в правом верхнем углу (батарея + карта)."""
    return "".join(
        [
            f'<rect x="113" y="2" width="10" height="5" fill="none" stroke="{INK}" stroke-width="1"/>',
            f'<rect x="115" y="3" width="4" height="3" fill="{INK}"/>',
            f'<rect x="124" y="3" width="2" height="3" fill="{INK}"/>',
            f'<rect x="103" y="2" width="7" height="5" fill="none" stroke="{INK}" stroke-width="1"/>',
            f'<rect x="104" y="4" width="2" height="1" fill="{INK}"/>',
        ]
    )


# --- Описание экранов -------------------------------------------------------
# line = dict(text=..., y=..., size=10, align='l'|'c'|'r', inv=False, bold=False)
# button = dict(label=..., top=..., h=..., inv=True)
# band = dict(text=..., y=..., inv=True)  -> инвертированная полоса во всю ширину
# dialog = dict(title=..., lines=[...], hint=...)

SCREENS = {
    "01-presets-list": {
        "caption": "Список пресетов (стартовый экран)",
        "icons": True,
        "lines": [
            {"text": "Tea Timer", "y": Y_TITLE, "align": "c"},
            {"text": "Green Tea", "y": Y_ROW1, "inv": True},
            {"text": "Milk Oolong", "y": Y_ROW2},
            {"text": "Ripe Puer", "y": Y_ROW3},
            {"text": "Tie Guan Yin", "y": Y_ROW4},
        ],
        "hint": "OK: open",
    },
    "02-brew-idle": {
        "caption": "Экран заваривания, таймер не запущен",
        "icons": True,
        "lines": [
            {"text": "Green Tea", "y": Y_TITLE, "align": "c"},
            {"text": "Pours 0 / 6", "y": Y_ROW1, "align": "c"},
            {"text": "Leaf 5.0 g/100ml", "y": Y_ROW2, "align": "c"},
            {"text": "Pour 1  00:20", "y": Y_ROW3, "align": "c"},
        ],
        "button": {"label": "START", "top": 45, "h": 11, "inv": True},
        "hint": "BACK: presets",
    },
    "03-brew-running": {
        "caption": "Экран заваривания, идёт обратный отсчёт",
        "icons": True,
        "lines": [
            {"text": "Green Tea", "y": Y_TITLE, "align": "c"},
            {"text": "00:15", "y": 31, "size": 20, "align": "c", "bold": True},
            {"text": "Pours 0 / 6", "y": 42, "align": "c"},
            {"text": "Leaf 5.0 g/100ml", "y": 53, "align": "c"},
        ],
        "hint": "OK: stop",
    },
    "04-time-up": {
        "caption": "Пролив завершён: сигнал и время следующего пролива",
        "icons": True,
        "lines": [
            {"text": "Green Tea", "y": Y_TITLE, "align": "c"},
            {"text": "Time is up!", "y": Y_ROW1, "align": "c", "inv": True},
            {"text": "Pours 1 / 6", "y": 34, "align": "c"},
            {"text": "Pour 2  00:25", "y": 45, "align": "c"},
        ],
        "hint": "OK: start next pour",
    },
    "05-over-limit": {
        "caption": "Продолжение сверх рекомендованного количества проливов",
        "icons": True,
        "lines": [
            {"text": "Green Tea", "y": Y_TITLE, "align": "c"},
            {"text": "Pours 9 / 8", "y": Y_ROW1, "align": "c"},
            {"text": "Leaf 5.0 g/100ml", "y": Y_ROW2, "align": "c"},
            {"text": "Pour 10  01:05", "y": Y_ROW3, "align": "c"},
        ],
        "badge": {"label": "OVER", "x": 99, "y": 12, "w": 27, "h": 11},
        "button": {"label": "START", "top": 45, "h": 11, "inv": True},
        "hint": "OK: start",
    },
    "06-no-presets": {
        "caption": "Пустой каталог / нет корректных пресетов",
        "icons": True,
        "lines": [
            {"text": "Tea Timer", "y": Y_TITLE, "align": "c"},
            {
                "text": "No presets found",
                "y": 26,
                "align": "c",
                "size": 12,
                "bold": True,
            },
            {"text": "Add .tea files to", "y": 38, "align": "c"},
            {"text": "apps_data/tea_timer", "y": 48, "align": "c"},
        ],
        "hint": "OK: reload",
    },
    "07-skipped-files": {
        "caption": "Сообщение о пропущенном файле пресета",
        "icons": False,
        "dialog": {
            "title": "Skipped 1 file",
            "lines": [
                {"text": "invalid values in", "y": 32, "size": 9},
                {"text": "bad.tea", "y": 44, "size": 10, "bold": True},
            ],
        },
        "hint": "OK: continue",
    },
    "08-sd-missing": {
        "caption": "SD-карта отсутствует или недоступна",
        "icons": True,
        "lines": [
            {"text": "Tea Timer", "y": Y_TITLE, "align": "c"},
            {
                "text": "SD card not found",
                "y": 26,
                "align": "c",
                "size": 12,
                "bold": True,
            },
            {"text": "Insert the card", "y": 38, "align": "c"},
            {"text": "and press OK", "y": 49, "align": "c"},
        ],
        "hint": "BACK: exit",
    },
}


def render(screen: dict) -> str:
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W * 4}" height="{H * 4}" '
        f'viewBox="0 0 {W} {H}" shape-rendering="crispEdges">',
        f'<rect x="0" y="0" width="{W}" height="{H}" fill="{BG}"/>',
    ]

    if screen.get("icons"):
        parts.append(status_icons())

    for line in screen.get("lines", []):
        y = line["y"]
        if line.get("inv"):
            parts.append(band_el(y, height=line.get("h", 12)))
            parts.append(
                text_el(
                    line["text"],
                    y,
                    size=line.get("size", 10),
                    align=line.get("align", "l"),
                    fill=BG,
                    bold=line.get("bold", False),
                )
            )
        else:
            parts.append(
                text_el(
                    line["text"],
                    y,
                    size=line.get("size", 10),
                    align=line.get("align", "l"),
                    fill=INK,
                    bold=line.get("bold", False),
                )
            )

    badge = screen.get("badge")
    if badge:
        parts.append(
            f'<rect x="{badge["x"]}" y="{badge["y"]}" width="{badge["w"]}" '
            f'height="{badge["h"]}" fill="{INK}"/>'
        )
        parts.append(
            text_el(
                badge["label"],
                badge["y"] + 8,
                size=8,
                align="c",
                fill=BG,
                x=badge["x"] + badge["w"] // 2,
            )
        )

    button = screen.get("button")
    if button:
        fill = INK if button.get("inv") else "none"
        parts.append(
            f'<rect x="0" y="{button["top"]}" width="{W}" height="{button["h"]}" '
            f'fill="{fill}" stroke="{INK}" stroke-width="1"/>'
        )
        parts.append(
            text_el(
                button["label"],
                button["top"] + button["h"] - 3,
                size=10,
                align="c",
                fill=BG if button.get("inv") else INK,
                bold=True,
            )
        )

    dialog = screen.get("dialog")
    if dialog:
        parts.append(
            f'<rect x="6" y="6" width="116" height="50" fill="{BG}" stroke="{INK}" stroke-width="2"/>'
        )
        parts.append(f'<rect x="8" y="8" width="112" height="12" fill="{INK}"/>')
        parts.append(text_el(dialog["title"], 17, size=9, align="c", fill=BG))
        for i, line in enumerate(dialog["lines"]):
            if isinstance(line, str):
                parts.append(text_el(line, 32 + i * 11, size=10, align="c"))
            else:
                parts.append(
                    text_el(
                        line["text"],
                        line.get("y", 32 + i * 11),
                        size=line.get("size", 10),
                        align="c",
                        bold=line.get("bold", False),
                    )
                )

    hint = screen.get("hint")
    if hint:
        parts.append(text_el(hint, Y_HINT, size=9, align="c"))

    parts.append("</svg>")
    return "\n".join(parts)


def main() -> None:
    PREVIEW.mkdir(parents=True, exist_ok=True)
    rsvg = shutil.which("rsvg-convert")
    for name, screen in SCREENS.items():
        svg_path = BASE / f"{name}.svg"
        svg_path.write_text(render(screen), encoding="utf-8")
        if rsvg:
            subprocess.run(
                [
                    rsvg,
                    "-w",
                    str(W * 4),
                    "-o",
                    str(PREVIEW / f"{name}.png"),
                    str(svg_path),
                ],
                check=True,
            )
        print(f"ok: {name}")


if __name__ == "__main__":
    main()
