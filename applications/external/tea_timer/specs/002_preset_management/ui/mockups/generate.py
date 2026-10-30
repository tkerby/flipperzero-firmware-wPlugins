#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Генератор референсных макетов UI приложения Tea Timer (фича 002).

Рисует экраны в реальном разрешении Flipper Zero (128x64) в SVG
и (если доступен rsvg-convert) растровые превью в PNG.

Запуск:
    python3 specs/002-preset-management/ui/mockups/generate.py

Правки макетов делаются здесь: словарь SCREENS ниже.
Все размеры — в пикселях экрана 128x64.
"""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

BASE = Path(__file__).resolve().parent
PREVIEW = BASE / "preview"

W, H = 128, 64
BG = "#F4F1E3"
INK = "#1A1A1A"
FONT = "Menlo, Consolas, DejaVu Sans Mono, monospace"

Y_TITLE = 9
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


def rect(x, y, w, h, fill=INK, stroke=None, sw=1):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}"{s}/>'


def status_icons():
    """Иконки статус-бара в правом верхнем углу (карта памяти + батарея)."""
    return "".join(
        [
            rect(113, 2, 10, 5, fill="none", stroke=INK),
            rect(115, 3, 4, 3),
            rect(124, 3, 2, 3),
            rect(103, 2, 7, 5, fill="none", stroke=INK),
            rect(104, 4, 2, 1),
        ]
    )


# --- Описание экранов -------------------------------------------------------
# line   = dict(text, y, size=10, align='l'|'c'|'r', inv=False, bold=False)
# button = dict(label, top, h, inv=True)
# badge  = dict(label, x, y, w, h)
# dialog = dict(title, lines=[str|dict(text, y, size, bold)])
# field  = dict(text, y, size=10)             — строка ввода с курсором и подчёркиванием
# keys   = dict(rows=[...], cursor=(row, col), x0, dx, y0, dy, size, cell_w, cell_h)
# filelist = dict(rows=[(icon, text), ...], y0, dy)

SCREENS = {
    "01-main-menu": {
        "caption": "Главное меню: выбор пресета или создание нового",
        "icons": True,
        "lines": [
            {"text": "Tea Timer", "y": Y_TITLE, "align": "c"},
            {"text": "Presets", "y": 25, "inv": True},
            {"text": "New preset", "y": 37},
        ],
        "hint": "OK: open",
    },
    "02-file-browser": {
        "caption": "Выбор пресета: стандартный экран выбора файла",
        "icons": True,
        "lines": [
            {"text": "tea_timer", "y": Y_TITLE, "align": "l", "bold": True},
        ],
        "filelist": {
            "y0": 21,
            "dy": 11,
            "rows": [
                ("dir", ".."),
                ("file", "green_tea.tea"),
                ("file", "milk_oolong.tea"),
                ("file", "ripe_puer.tea"),
            ],
            "cursor": 1,
        },
        "hint": "OK: open",
    },
    "03-preset-card": {
        "caption": "Карточка выбранного пресета (значения из файла)",
        "icons": True,
        "lines": [
            {"text": "Green Tea", "y": Y_TITLE, "align": "c"},
            {"text": "Leaf 5.0 g/100ml", "y": 19, "align": "c"},
            {"text": "First 00:20  Step 00:05", "y": 29, "align": "c", "size": 9},
            {"text": "Max 6 pours", "y": 39, "align": "c"},
        ],
        "button": {"label": "START", "top": 42, "h": 11, "inv": True},
        "hint": ">: actions",
    },
    "04-name-keyboard": {
        "caption": "Создание пресета: ввод названия стандартной клавиатурой",
        "icons": False,
        "lines": [
            {"text": "New preset", "y": 8, "align": "c", "size": 9},
        ],
        "field": {"text": "Green Tea", "y": 19},
        "keys": {
            "rows": ["1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm.,"],
            "cursor": (3, 2),
            "x0": 6,
            "dx": 12,
            "y0": 31,
            "dy": 8,
            "size": 7.5,
            "cell_w": 11,
            "cell_h": 7,
        },
        "hint": "OK: enter",
    },
    "05-number-keypad": {
        "caption": "Создание пресета: ввод числового значения",
        "icons": False,
        "lines": [
            {"text": "Leaf g/100ml", "y": 8, "align": "c", "size": 9},
        ],
        "field": {"text": "5.0", "y": 19},
        "keypad": {
            "rows": [
                ["1", "2", "3"],
                ["4", "5", "6"],
                ["7", "8", "9"],
                [".", "0", "<"],
            ],
            "cursor": (3, 1),
            "x0": 34,
            "dx": 30,
            "y0": 30,
            "dy": 8,
            "size": 8,
            "cell_w": 18,
            "cell_h": 7,
        },
        "hint": "OK: enter",
    },
    "06-new-summary": {
        "caption": "Сводка перед сохранением нового пресета",
        "icons": True,
        "lines": [
            {"text": "New preset", "y": Y_TITLE, "align": "c"},
            {"text": "Green Tea", "y": 20, "align": "c", "bold": True},
            {"text": "Leaf 5.0 g/100ml", "y": 30, "align": "c"},
            {"text": "First 00:20  Step 00:05", "y": 40, "align": "c", "size": 9},
            {"text": "Max 6 pours", "y": 50, "align": "c"},
        ],
        "hint": "OK: save",
    },
    "07-error-value": {
        "caption": "Сообщение о недопустимом значении",
        "icons": False,
        "dialog": {
            "title": "Out of range",
            "lines": [
                {"text": "first pour must be", "y": 31, "size": 9},
                {"text": "1..3600 sec", "y": 43, "size": 10, "bold": True},
            ],
        },
        "hint": "OK: retry",
    },
    "08-saved": {
        "caption": "Пресет сохранён на устройстве",
        "icons": False,
        "dialog": {
            "title": "Saved",
            "lines": [
                {"text": "green_tea.tea", "y": 34, "size": 10, "bold": True},
                {"text": "in apps_data/tea_timer", "y": 45, "size": 8},
            ],
        },
        "hint": "OK: file list",
    },
    "09-preset-actions": {
        "caption": "Меню действий на карточке пресета: правка или удаление",
        "icons": True,
        "lines": [
            {"text": "Green Tea", "y": Y_TITLE, "align": "c"},
            {"text": "Edit", "y": 27, "inv": True},
            {"text": "Delete", "y": 39},
        ],
        "hint": "OK: select",
    },
    "10-delete-confirm": {
        "caption": "Подтверждение удаления пресета (по умолчанию — No)",
        "icons": False,
        "dialog": {
            "title": "Delete preset",
            "lines": [
                {"text": "Green Tea", "y": 33, "size": 9, "bold": True},
                {"text": "green_tea.tea", "y": 45, "size": 8},
            ],
        },
        "hint": "OK: No   >: Yes",
    },
}


def render_keys(keys: dict, keypad: bool = False) -> str:
    parts = []
    cur_row, cur_col = keys["cursor"]
    for r, row in enumerate(keys["rows"]):
        for c, label in enumerate(row):
            x = keys["x0"] + c * keys["dx"]
            y = keys["y0"] + r * keys["dy"]
            cell_x = x - keys["cell_w"] // 2
            cell_y = y - keys["size"]
            selected = (r, c) == (cur_row, cur_col)
            if selected:
                parts.append(
                    rect(cell_x, cell_y, keys["cell_w"], keys["cell_h"], fill=INK)
                )
            parts.append(
                text_el(
                    label,
                    y,
                    size=keys["size"],
                    align="c",
                    fill=BG if selected else INK,
                    x=x,
                )
            )
    return "".join(parts)


def render(screen: dict) -> str:
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W * 4}" height="{H * 4}" '
        f'viewBox="0 0 {W} {H}" shape-rendering="crispEdges">',
        rect(0, 0, W, H, fill=BG),
    ]

    if screen.get("icons"):
        parts.append(status_icons())

    for line in screen.get("lines", []):
        y = line["y"]
        size = line.get("size", 10)
        if line.get("inv"):
            parts.append(rect(0, y - 9, W, 12))
            parts.append(text_el(line["text"], y, size, line.get("align", "l"), BG))
        else:
            parts.append(
                text_el(
                    line["text"],
                    y,
                    size,
                    line.get("align", "l"),
                    INK,
                    line.get("bold", False),
                )
            )

    filelist = screen.get("filelist")
    if filelist:
        for i, (kind, name) in enumerate(filelist["rows"]):
            y = filelist["y0"] + i * filelist["dy"]
            selected = i == filelist.get("cursor", -1)
            if selected:
                parts.append(rect(0, y - 8, W, 10))
            color = BG if selected else INK
            if kind == "dir":
                parts.append(
                    f'<path d="M3 {y - 6} h5 l2 2 h5 v5 h-12 z" fill="none" '
                    f'stroke="{color}" stroke-width="1"/>'
                )
            else:
                parts.append(rect(4, y - 6, 7, 8, fill="none", stroke=color))
            parts.append(text_el(name, y, 9, "l", color, x=17))

    field = screen.get("field")
    if field:
        parts.append(text_el(field["text"] + "_", field["y"], 10, "l", INK, x=3))
        parts.append(rect(2, field["y"] + 2, W - 4, 1))

    keys = screen.get("keys")
    if keys:
        parts.append(render_keys(keys))

    keypad = screen.get("keypad")
    if keypad:
        parts.append(render_keys(keypad, keypad=True))

    button = screen.get("button")
    if button:
        fill = INK if button.get("inv") else "none"
        parts.append(rect(0, button["top"], W, button["h"], fill=fill, stroke=INK))
        parts.append(
            text_el(
                button["label"],
                button["top"] + button["h"] - 3,
                10,
                "c",
                BG if button.get("inv") else INK,
                bold=True,
            )
        )

    badge = screen.get("badge")
    if badge:
        parts.append(rect(badge["x"], badge["y"], badge["w"], badge["h"]))
        parts.append(
            text_el(
                badge["label"],
                badge["y"] + 8,
                8,
                "c",
                BG,
                x=badge["x"] + badge["w"] // 2,
            )
        )

    dialog = screen.get("dialog")
    if dialog:
        parts.append(rect(6, 6, 116, 50, fill=BG, stroke=INK, sw=2))
        parts.append(rect(8, 8, 112, 12))
        parts.append(text_el(dialog["title"], 17, 9, "c", BG))
        for i, line in enumerate(dialog["lines"]):
            if isinstance(line, str):
                parts.append(text_el(line, 32 + i * 11, 10, "c"))
            else:
                parts.append(
                    text_el(
                        line["text"],
                        line.get("y", 32 + i * 11),
                        line.get("size", 10),
                        "c",
                        INK,
                        line.get("bold", False),
                    )
                )

    hint = screen.get("hint")
    if hint:
        parts.append(text_el(hint, Y_HINT, 9, "c"))

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
