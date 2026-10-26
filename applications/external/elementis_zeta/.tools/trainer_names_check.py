#!/usr/bin/env python3
"""Every trainer's personal name is unique in every language (contract 54.1 #10, 58): the part of `gl.tr.*` after the class word must not repeat, must not be a
rival / hero preset (`gl.name.*`), and the same class has ONE name per language. Director Varo (3 fights, one character) is the only repeat allowed.
  python3 tools/trainer_names_check.py   exit 1 on a problem."""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZH_CLASSES = [
    "游泳者",
    "渔夫",
    "研究员",
    "技工",
    "工程师",
    "盗贼",
    "喷火人",
    "朋克",
    "空手家",
    "精英",
    "超能者",
    "特工",
    "登山者",
    "探险家",
    "灵媒",
    "弟子",
    "骑手",
    "赌徒",
    "水手",
    "捕虫少年",
    "学徒",
    "少女",
    "忍者",
    "女孩",
    "捕虫人",
    "虫迷",
    "朋克",
    "骑士",
    "队员",
    "探险",
    "少年",
    "团员",
]
ALLOW = {"Varo"}


def entries(lang):
    t = open(os.path.join(ROOT, "assets/text/%s.txt" % lang), encoding="utf-8").read()
    return {
        m.group(1): m.group(2)
        for m in re.finditer(r"^\[([^\]]+)\]\n(.*?)(?=\n\n|\n\[|\Z)", t, re.S | re.M)
    }


bad = 0
for lang in ("es", "en", "ja", "zh"):
    e = entries(lang)
    presets = {v.strip() for k, v in e.items() if k.startswith("gl.name.")}
    seen = {}
    for k, v in e.items():
        if not k.startswith("gl.tr.") or "\n" in v:
            continue
        if lang == "zh":
            name = v
            for c in sorted(ZH_CLASSES, key=len, reverse=True):
                if v.startswith(c):
                    name = v[len(c) :].strip()
                    break
        else:
            name = v.rsplit(" ", 1)[-1]
        if name in ALLOW or v.startswith("Director") or "Varo" in v or "{" in v:
            continue
        if name in presets:
            print(f"{lang}: {k} '{v}' uses the preset name '{name}'")
            bad += 1
        if name in seen and seen[name] != k:
            print(f"{lang}: '{name}' repeats: {seen[name]} / {k}")
            bad += 1
        seen[name] = k
print(
    "trainer_names_check: OK" if not bad else f"trainer_names_check: {bad} problem(s)"
)
sys.exit(1 if bad else 0)
