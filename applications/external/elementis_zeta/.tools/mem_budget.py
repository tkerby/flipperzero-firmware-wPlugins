#!/usr/bin/env python3
"""Automatic memory-budget guard for Nucleo (contract section 5.x "Memory budget (enforced)").

Two checks, both configured in tools/mem_budget.toml:

 1. STATIC sizes (text + data + bss of the linked ELFs: resident app, battle / menu / UI plugins).
    FAIL when a module is above its ratchet (it grew), WARN while it is above its target.
 2. HEAP model: the expected free heap on the device at the critical points (exploring, battle plugin load,
    battle scene peak, menu / shop / UI plugin open), computed from the module sizes, the host heap trackers
    (tools/host_battle/memtrack.*, anchored to the Flipper by device calibration constants). FAIL when a prediction is
    below its floor, except modes flagged `known_red`, which only fail when they get worse than their recorded baseline.

Usage (after `cd nucleo && ufbt build`; tools/mem_budget.sh does both):
  python3 tools/mem_budget.py                      check, print the table, write build/mem_budget/report.json, exit 1 on FAIL
  python3 tools/mem_budget.py --update             lower ratchets / raise baselines to the current values (never the other way)
  python3 tools/mem_budget.py --raise MODULE BYTES --reason "why"      raise a ratchet on purpose (logged in the toml)
  python3 tools/mem_budget.py --raise-baseline MODE BYTES --reason "why"   accept a worse known-red prediction on purpose
  python3 tools/mem_budget.py --calibrate NAME MEASURED_BYTES --ref "commit / note"   re-fit a device calibration offset
  python3 tools/mem_budget.py --rebase --reason "why"    set every ratchet to measured + tol and every baseline to the prediction (up OR down), logged in the toml
Two builds (contract 5.x): the RELEASE build (default `ufbt build`, no NUCLEO_DEBUG) is the gate (--elf-dir: ratchets, targets, floors, baselines); the DEV build
(`NUCLEO_DEBUG=1 ufbt build`, --dev-elf-dir) is only checked against the same heap floors (FAIL under a floor; no ratchet, no baseline, known_red ignored).
tools/mem_budget.sh builds both and passes the two directories.
Test hooks: --config, --elf-dir, --dev-elf-dir, --sizes-json, --dev-sizes-json, --dev-fal-json, --host-json, --maps-dir, --no-host (use build/mem_budget/host.json).
Exit status: 0 green (WARN allowed), 1 FAIL, 2 the tool could not measure something."""
import argparse, datetime, glob, json, pathlib, re, struct, subprocess, sys

try:
    import tomllib
except ImportError:  # pragma: no cover
    sys.exit(
        "mem_budget.py needs Python >= 3.11 (tomllib): run it inside the project venv"
    )

ROOT = pathlib.Path(__file__).resolve().parent.parent
import os

HOME = pathlib.Path(
    os.environ.get("UFBT_HOME") or (pathlib.Path.home() / ".ufbt")
)  # UFBT_HOME: a private ufbt state dir (parallel work trees)
DEFAULT_CFG = ROOT / "tools" / "mem_budget.toml"
OUT_DIR = ROOT / "build" / "mem_budget"
STATIC_ORDER = (
    "resident",
    "battle",
    "menu",
    "ui",
    "pc",
    "casino",
    "link",
    "ending",
)  # (a module / mode that the toml does not define is skipped: C4-C added the casino, the 5th plugin)
MODE_ORDER = (
    "explore_biggest",
    "explore_route1",
    "battle_load",
    "battle_peak_wild",
    "battle_peak_trainer",
    "menu_open",
    "shop_open",
    "ui_open",
    "pc_open",
    "casino_open",
    "link_open",
    "ending_open",
)


class ToolError(Exception):
    """Something could not be measured (exit status 2)."""


# ------------------------------------------------------------------------------------------------ measuring
def size_tool():
    found = sorted(
        glob.glob(str(HOME / "toolchain" / "*" / "bin" / "arm-none-eabi-size"))
    )
    if not found:
        raise ToolError(
            "arm-none-eabi-size not found under ~/.ufbt/toolchain: run `ufbt` once"
        )
    return found[0]


def measure_sizes(cfg, elf_dir):
    """{module: {text, data, bss, total}} from the linked ELFs (the .o files in ~/.ufbt/build/nucleo/ can be stale)."""
    out = {}
    tool = size_tool()
    for mod in STATIC_ORDER:
        if mod not in cfg["modules"]:
            continue
        elf = pathlib.Path(elf_dir) / cfg["modules"][mod]["elf"]
        if not elf.exists():
            raise ToolError(f"{elf} missing: run `cd nucleo && ufbt build` first")
        lines = (
            subprocess.check_output([tool, str(elf)], text=True).strip().splitlines()
        )
        text, data, bss = (int(x) for x in lines[-1].split()[:3])
        out[mod] = {"text": text, "data": data, "bss": bss, "total": text + data + bss}
    return out


def fal_need(fal):
    """What plugin_loader.c's pre-check reads from the .fal (same as tools/check_plugin_api.py load_need):
    (sum of the ALLOC sections, sum of the non-ALLOC PROGBITS = relocation data, biggest ALLOC section).
    """
    d = pathlib.Path(fal).read_bytes()
    shoff, n = struct.unpack_from("<I", d, 32)[0], struct.unpack_from("<H", d, 48)[0]
    ram = reloc = block = 0
    for i in range(n):
        ty, fl, _ad, _of, sz = struct.unpack_from("<IIIII", d, shoff + 40 * i + 4)
        if fl & 2:
            ram += (sz + 3) & ~3
            block = max(block, sz)
        elif ty == 1:
            reloc += sz
    return ram, reloc, block


def measure_fals(cfg, elf_dir):
    out = {}
    for mod in ("battle", "menu", "ui", "pc", "casino", "link", "ending"):
        if mod not in cfg["modules"]:
            continue
        fal = pathlib.Path(elf_dir) / cfg["modules"][mod]["fal"]
        if not fal.exists():
            raise ToolError(f"{fal} missing: run `cd nucleo && ufbt build` first")
        ram, reloc, _ = fal_need(fal)
        out[mod] = {"sections": ram, "reloc": reloc}
    return out


def source_defines(root):
    """Constants the model reads from the sources so they cannot drift: GAME_STACK + MAIN_STACK (config.h),
    LOADER_BOOKKEEPING and each plugin's scene margin (plugin_loader.c), the host harness' emulated plugin blocks (plugin_link.c).
    """
    cfgh = (root / "nucleo" / "config.h").read_text()
    stacks = 0
    for name in ("GAME_STACK", "MAIN_STACK"):
        m = re.search(r"#define\s+" + name + r"\s+\(?([0-9*\s]+)\)?", cfgh)
        if not m:
            raise ToolError(f"{name} not found in nucleo/config.h")
        stacks += eval(m.group(1), {"__builtins__": {}})
    ldr = (root / "nucleo" / "plugin_loader.c").read_text()
    m = re.search(r"#define LOADER_BOOKKEEPING (\d+)u", ldr)
    if not m:
        raise ToolError("LOADER_BOOKKEEPING not found in nucleo/plugin_loader.c")
    margins = {}
    for mod, ver in (
        ("battle", "BATTLE"),
        ("menu", "MENU"),
        ("ui", "UI"),
        ("pc", "PC"),
        ("casino", "CASINO"),
        ("link", "LINK"),
        ("ending", "ENDING"),
    ):
        mm = re.search(ver + r"_PLUGIN_API_VERSION, ([\d* ]+)\}", ldr)
        margins[mod] = eval(mm.group(1), {"__builtins__": {}}) if mm else 0
    link = (root / "nucleo" / "plugin_link.c").read_text()
    emu = {}
    for key, name in (("battle", "EMU_BATTLE_BYTES"), ("menu", "EMU_MENU_BYTES")):
        mm = re.search(r"#define " + name + r" (\d+)", link)
        if not mm:
            raise ToolError(f"{name} not found in nucleo/plugin_link.c")
        emu[key] = int(mm.group(1))
    mk = (root / "tools" / "host_shop" / "Makefile").read_text()
    mm = re.search(r"-DEMU_MENU_BYTES=(\d+)", mk)
    emu["menu_shop"] = int(mm.group(1)) if mm else emu["menu"]
    mm = re.search(
        r"EMU_PC_BOX \?= (\d+)", mk
    )  # M5-C: the PC box test's emulated plugin size (the PC plugin since the M5 memory recovery, contract 25.6)
    emu["pc"] = int(mm.group(1)) if mm else 7200
    mm = re.search(
        r"EMU_CASINO_REG \?= (\d+)", mk
    )  # C2-S2 / contract 76: the Registro test's emulated plugin size (the CASINO plugin with scene_reg.c in it since contract 76; it was the PC plugin's EMU_PC_REG)
    mc = re.search(r"#define EMU_CASINO_BYTES (\d+)", link)
    emu["casino_reg"] = int(mm.group(1)) if mm else int(mc.group(1)) if mc else 5308
    return {
        "stacks": stacks,
        "bookkeeping": int(m.group(1)),
        "margins": margins,
        "emu": emu,
    }


STREAM_CACHE_TABLE = 200  # sizeof(ScCache) on the device (nucleo/map.c): 16 blocks x 12 B + owner + byte count


def follower_unit():
    """Contract 69: bytes of the ONE block the follower keeps in the cache (one facing of its 20 x 20 sprite, pixels + mask), from FOL_W / FOL_H of nucleo/map.h."""
    src = (ROOT / "nucleo" / "map.h").read_text()
    w, h = (int(re.search(r"#define FOL_%s (\d+)" % k, src).group(1)) for k in "WH")
    return ((w + 3) // 4) * h + ((w + 7) // 8) * h


STREAM_CACHE_CAP = 2560  # SC_CAP (nucleo/map.c): bytes the cache keeps beyond what the current frame draws


def map_cost(d):
    """Heap cost of one map file while it is loaded: {file, res, cache}.
    res = the data block + the RESIDENT part of the sprite bank (file v2: header u16 at 18 = bank bytes / 4; older files and anything that is not an
    NMAP file: the whole file). cache = the streamed-actor cache of nucleo/map.c (contract 3.1), estimated as ONE full facing of every distinct streamed
    sprite an NPC uses (+ the follower's one block, contract 69) (the idle frame of a single-frame sprite, all the frames of a walking one), capped at STREAM_CACHE_CAP, plus the table; 0 when
    nothing is streamed. It is what the cache holds when every NPC type is on screen once; the true value moves with what is visible.
    """
    n = len(d)
    if n < 64 or d[:4] != b"NMAP" or d[4] < 2:
        return {"file": n, "res": n, "cache": 0}
    off_bank = struct.unpack_from("<I", d, 48)[0]
    res = off_bank + struct.unpack_from("<H", d, 18)[0] * 4
    n_npcs, off_npcs = d[14], struct.unpack_from("<I", d, 32)[0]
    units = {}
    for k in range(n_npcs):
        spr = d[off_npcs + 12 * k]
        e = d[off_bank + 8 + 16 * spr : off_bank + 24 + 16 * spr]
        kind, w, h, ndirs, nframes, flags = e[:6]
        if kind != 2 or not flags & 8:  # SPRF_STREAM
            continue
        fsz = ((w + 3) // 4) * h + (((w + 7) // 8) * h if flags & 1 else 0)
        units[spr] = (
            fsz * nframes
        )  # one facing: nframes frames (1 for the single-frame NPCs)
    # contract 69: the follower is drawn on EVERY map (the first able creature walks behind the hero): one more streamed block, and the table even where no NPC streams
    cache = (
        min(STREAM_CACHE_CAP, sum(units.values()) + follower_unit())
        + STREAM_CACHE_TABLE
    )
    return {"file": n, "res": res, "cache": cache}


def map_files(maps_dir, below, extra=()):
    """{map id: map_cost()} for the real maps (ids < `below`, plus the ids of `extra`: D1's two maps 229 / 230 live above the test fixtures' base, contract 68): the model charges the resident bytes + the stream cache estimate, not the file size."""
    out = {}
    for p in sorted(pathlib.Path(maps_dir).glob("map_*.bin")):
        mid = int(p.stem.split("_")[1])
        if (
            mid == 99
        ):  # an id-99 file is never a game map (the M1 host fixture of tools/make_test_map.py lives in build/m1_test since contract 49; ids 200+ are the test maps, C4b contract 41): the hole of MAP_COUNT
            continue
        if mid < below or mid in extra:
            out[mid] = map_cost(p.read_bytes())
    if not out:
        raise ToolError(f"no map_NN.bin under {maps_dir}")
    return out


def _run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def host_probes(cfg, root):
    """Build (incrementally) and run the host harnesses; returns the raw numbers the model needs."""
    out = OUT_DIR
    out.mkdir(parents=True, exist_ok=True)
    builds = [
        (root / "tools/host_battle", ["make", "-s", "uibuild"]),
        (
            root / "tools/host_world",
            ["make", "-s", "all", "../../build/host_world/bag_check"],
        ),
        (
            root / "tools/host_shop",
            [
                "make",
                "-s",
                "../../build/host_shop/shop_check",
                "../../build/host_shop/box_check",
                "../../build/host_shop/reg_check",
            ],
        ),
    ]
    for cwd, cmd in builds:
        rc, txt = _run(cmd, cwd)
        if rc != 0:
            raise ToolError(
                f"host build failed ({' '.join(cmd)} in {cwd.relative_to(root)}; another agent mid-edit?):\n{txt[-1500:]}"
            )
    env_map = str(cfg["system"]["route1_map"])
    # battle: the heap of exploring Ruta 1 and the battle's absolute peak (tools/host_battle/ui_main.c `$` token)
    import os

    env = dict(os.environ, UI_MAP=env_map)
    r = subprocess.run(
        [
            str(root / "build/host_battle/ui_main"),
            str(out / "ui"),
            "play",
            cfg["host"]["battle_script"],
            "1",
            "1",
            "1",
            "mb",
        ],
        cwd=root / "tools/host_battle",
        env=env,
        capture_output=True,
        text=True,
    )
    pts = {}
    for line in r.stdout.splitlines():
        m = re.match(r"MEMTRK (\S+) (.*)", line)
        if m:
            pts[m.group(1)] = {
                k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", m.group(2))
            }
    if "a" not in pts or "e" not in pts or not pts["e"].get("before_load"):
        raise ToolError(
            "the battle probe printed no MEMTRK lines (does the script open a battle?):\n"
            + (r.stdout + r.stderr)[-1500:]
        )
    # a trainer battle won to the end (intro, fight, the beaten trainer's slide-in): its own peak (the hero's facing cache is freed when the intro is done, contract 22.5)
    tr = None
    if cfg["host"].get("trainer_script"):
        r2 = subprocess.run(
            [
                str(root / "build/host_battle/ui_main"),
                str(out / "uit"),
                "play",
                cfg["host"]["trainer_script"],
                "1",
                "1",
                "1",
                "mt",
            ],
            cwd=root / "tools/host_battle",
            env=env,
            capture_output=True,
            text=True,
        )
        for line in r2.stdout.splitlines():
            m = re.match(r"MEMTRK e (.*)", line)
            if m:
                tr = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", m.group(1))}
        if not tr or not tr.get("before_load"):
            raise ToolError(
                "the trainer battle probe printed no MEMTRK e line:\n"
                + (r2.stdout + r2.stderr)[-1500:]
            )
    # the streamed NPC frames that are in the probe's heap (host pointer sizes) are not part of explore_live: the model adds each map's estimated cache itself
    host = {
        "explore_live": pts["a"]["live"] - pts["a"].get("stream", 0),
        "before_load": pts["e"]["before_load"],
        "peak_abs": pts["e"]["peak"],
    }
    if tr:
        host.update(trainer_before_load=tr["before_load"], trainer_peak_abs=tr["peak"])

    def peaks(cmd, cwd):
        r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
        txt = r.stdout + r.stderr
        # the streamed NPC frames (map.c) are part of the exploring heap of the map, which the model counts there: leave them out of the screen's own peak
        vals = [
            int(a) - int(b or 0)
            for a, b in re.findall(
                r"heap peak since J: (\d+) B(?: \([^)]*?of which (\d+) B streamed)?",
                txt,
            )
        ]
        if not vals:
            raise ToolError(
                f"{' '.join(cmd[:2])} printed no `heap peak since J` line:\n{txt[-1000:]}"
            )
        return max(vals)

    host["party_peak"] = peaks(
        [
            str(root / "build/host_world/hw_main"),
            str(out / "p"),
            "lab",
            cfg["host"]["party_script"],
            "2",
            "4",
            "3",
            "mp",
        ],
        root / "tools/host_world",
    )
    host["bag_peak"] = peaks(
        [str(root / "build/host_world/bag_check"), str(out / "bag")],
        root / "tools/host_world",
    )
    host["shop_peak"] = peaks(
        [str(root / "build/host_shop/shop_check"), str(out / "sh")],
        root / "tools/host_shop",
    )
    host["box_peak"] = peaks(
        [str(root / "build/host_shop/box_check"), str(out / "bx")],
        root / "tools/host_shop",
    )  # M5-C: the PC visit / store scene (contract 25.5)
    host["reg_peak"] = peaks(
        [str(root / "build/host_shop/reg_check"), str(out / "dx")],
        root / "tools/host_shop",
    )  # C2-S2: the Registro (contract 30.6): list + a caught entry's picture, name, types and description
    return host


# ------------------------------------------------------------------------------------------------ the model
def model(cfg, sizes, fals, host, maps, src):
    """Expected free heap (bytes) at each critical point. See contract 5.x for the formulas.
    Every term is either measured (module sizes, host trackers, map files) or a named constant from the toml.
    """
    sysm = cfg["system"]
    cal = cfg["calibration"]
    img = sizes["resident"]["total"]
    base = (
        sysm["free_heap"] - img - src["stacks"]
    )  # heap left once the image and the two thread stacks are in
    bk = src["bookkeeping"]
    stays = sysm["loader_stays"]
    r1 = sysm["route1_map"]

    def cost(
        m,
    ):  # resident bytes (data block + resident bank) of a map; an int = a whole file
        return maps[m]["res"] if isinstance(maps[m], dict) else maps[m]

    def scache(m):  # estimated streamed-actor cache of a map (map_cost)
        return maps[m]["cache"] if isinstance(maps[m], dict) else 0

    def explore_raw(
        m,
    ):  # the host's exploring heap on Ruta 1 (its bank is in it, its NPC cache is not), the other maps by resident bytes
        return base - host["explore_live"] - (cost(m) - cost(r1)) - scache(m)

    k_expl = cal["explore"]["offset"]

    def explore(m):
        return explore_raw(m) - k_expl

    real = sorted(maps)
    biggest = max(real, key=lambda m: cost(m) + scache(m))
    out, detail = {}, {}
    out["explore_route1"] = explore(r1)
    out["explore_biggest"] = explore(biggest)
    detail["biggest_map"] = biggest
    detail["explore_by_map"] = {m: explore(m) for m in real}
    detail["map_cost"] = {
        m: {
            "res": cost(m),
            "cache": scache(m),
            "file": (
                maps[m].get("file", cost(m)) if isinstance(maps[m], dict) else maps[m]
            ),
        }
        for m in real
    }

    pb = sizes["battle"]["total"]
    scene_total = (
        host["peak_abs"] - src["emu"]["battle"] - host["before_load"]
    )  # restored font / idle set + the scene's own peak (the battle data tables stay in the heap since contract 27: they are in before_load AND in the peak, so the peak does not depend on them)
    detail["scene_total"] = scene_total
    load_b = base - host["before_load"] - (pb + fals["battle"]["reloc"] + bk) - k_expl
    peak_b = (
        base
        - host["before_load"]
        - scene_total
        - pb
        - stays
        - k_expl
        + cal["battle_low"]["offset"]
    )
    out["battle_load"] = load_b
    out["battle_peak_wild"] = peak_b
    if (
        "trainer_peak_abs" in host
    ):  # a trainer battle measured on its own (the hero's facing is freed when the intro is done)
        t_total = (
            host["trainer_peak_abs"]
            - src["emu"]["battle"]
            - host["trainer_before_load"]
        )
        out["battle_peak_trainer"] = (
            base
            - host["trainer_before_load"]
            - t_total
            - pb
            - stays
            - k_expl
            + cal["battle_low"]["offset"]
        )
        detail["scene_total_trainer"] = t_total
    else:
        out["battle_peak_trainer"] = peak_b

    def plugin_mode(mod, e, scene, resid):
        p = sizes[mod]["total"]
        load = e - (p + fals[mod]["reloc"] + bk)
        steady = e - p - stays - scene
        detail[mod + "_scene"] = scene
        return min(load, steady) + resid

    cache = sysm["host_cache_bytes"]
    s_party = host["party_peak"] - src["emu"]["menu"] - cache
    s_bag = host["bag_peak"] - src["emu"]["menu"] - cache
    s_shop = host["shop_peak"] - src["emu"]["menu_shop"] - cache
    s_box = (
        host["box_peak"] - src["emu"].get("pc", 7200) - cache
        if "box_peak" in host
        else 0
    )
    s_reg = (
        host["reg_peak"] - src["emu"].get("casino_reg", 5308) - cache
        if "reg_peak" in host
        else 0
    )  # C2-S2: the Registro screen (contract 30.6); the casino plugin's scene since contract 76
    detail.update(
        menu_party_scene=s_party,
        menu_bag_scene=s_bag,
        shop_scene=s_shop,
        pc_box_scene=s_box,
        pc_reg_scene=s_reg,
    )
    out["menu_open"] = plugin_mode(
        "menu", explore(biggest), max(s_party, s_bag, 0), cal["menu_open"]["offset"]
    )
    out["shop_open"] = plugin_mode(
        "menu", explore(sysm["shop_map"]), max(s_shop, 0), cal["shop_open"]["offset"]
    )
    out["ui_open"] = plugin_mode(
        "ui", explore(biggest), src["margins"]["ui"], 0
    )  # no host probe for the UI screens: the loader's margin
    # the PC plugin (box screen, trainer card, post-battle store: the 4th plugin, contract 25.6) on the biggest map; its scene is the PC visit's host peak
    out["pc_open"] = plugin_mode(
        "pc", explore(biggest), max(s_box, 0), 0
    )  # (the Registro screen left for the casino plugin, contract 76)
    # the casino plugin (slot machine, prize counter, coin purchase: the 5th plugin, contract 37) on the biggest map; its scene state is ~0.1 KB + the dialogs: the loader's margin
    if "casino" in sizes:
        out["casino_open"] = plugin_mode(
            "casino",
            explore(biggest),
            max(src["margins"].get("casino", 2048), s_reg),
            0,
        )  # contract 76: the loader's margin or the Registro screen's host peak, whichever is larger
    # the link plugin (IR link: trade + exhibition battle, the 6th plugin, contract 45) on the biggest map; its scene (session + game ~1.8 KB, IR driver ~0.4 KB, the result dialog) is the loader's margin
    if "link" in sizes:
        out["link_open"] = plugin_mode(
            "link", explore(biggest), src["margins"].get("link", 4096), 0
        )
    # the ending plugin (Hall of Legacy scenes + browser, credits, FIN: the 7th plugin, contract 50) on the biggest map; its scene state (~2 KB) + the pictures (<= 2 KB) + hof.bin's 1.5 KB image is the loader's margin
    if "ending" in sizes:
        out["ending_open"] = plugin_mode(
            "ending", explore(biggest), src["margins"].get("ending", 5120), 0
        )
    detail["base_free"] = base
    return out, detail


# ------------------------------------------------------------------------------------------------ verdicts
def check_static(cfg, sizes):
    rows = []
    for mod in STATIC_ORDER:
        if mod not in cfg["modules"]:
            continue
        c = cfg["modules"][mod]
        size = sizes[mod]["total"]
        if size > c["ratchet"]:
            st, note = (
                "FAIL",
                f"grew: {size - c['ratchet']:,} B over the ratchet (raise on purpose: --raise {mod} {size} --reason \"...\")",
            )
        elif size > c["target"]:
            st, note = "WARN", f"{size - c['target']:,} B above the target"
        else:
            st, note = "OK", ""
        rows.append(
            {
                "module": mod,
                "size": size,
                "text": sizes[mod]["text"],
                "data": sizes[mod]["data"],
                "bss": sizes[mod]["bss"],
                "ratchet": c["ratchet"],
                "target": c["target"],
                "status": st,
                "note": note,
            }
        )
    return rows


def check_modes(cfg, pred):
    tol = cfg["meta"]["tol"]
    rows = []
    for name in MODE_ORDER:
        if name not in cfg["modes"]:
            continue
        c = cfg["modes"][name]
        p, floor, base, kr = pred[name], c["floor"], c["baseline"], c["known_red"]
        if p >= floor:
            st, note = "OK", ("floor met: `--update` clears known_red" if kr else "")
            if not kr and base and p < base - tol:
                st, note = (
                    "WARN",
                    f"margin eroded: {base - p:,} B below the recorded baseline {base:,} (still above the floor)",
                )
        elif kr:
            if p >= base - tol:
                st, note = "WARN", f"known-red: {floor - p:,} B under the floor" + (
                    f", {p - base:+,} B vs baseline" if base and p != base else ""
                )
            else:
                st, note = (
                    "FAIL",
                    f"REGRESSION: {base - p:,} B below the recorded baseline {base:,} (tolerance {tol})",
                )
        else:
            st, note = "FAIL", f"{floor - p:,} B under the floor"
        rows.append(
            {
                "mode": name,
                "predicted": p,
                "floor": floor,
                "baseline": base,
                "known_red": kr,
                "status": st,
                "note": note,
            }
        )
    return rows


def check_dev(cfg, pred):
    """The DEV build (NUCLEO_DEBUG=1) against the same heap floors as the release gate: FAIL under a floor, otherwise OK. No ratchet, no baseline, no known_red:
    the maintainer's device tests run this build, so it must never leave the room the floors guarantee.
    """
    rows = []
    for name in MODE_ORDER:
        if name not in cfg["modes"]:
            continue
        p, floor = pred[name], cfg["modes"][name].get(
            "dev_floor", cfg["modes"][name]["floor"]
        )  # `dev_floor` (C4-B): a lower floor for the DEV build only (logged in the toml); the release gate never reads it
        st, note = (
            ("OK", "")
            if p >= floor
            else ("FAIL", f"DEV build {floor - p:,} B under the floor")
        )
        rows.append(
            {"mode": name, "predicted": p, "floor": floor, "status": st, "note": note}
        )
    return rows


# ------------------------------------------------------------------------------------------------ toml editing
def toml_get_section(text, table):
    m = re.search(r"^\[" + re.escape(table) + r"\][^\n]*\n", text, re.M)
    if not m:
        raise ToolError(f"[{table}] not found in the toml")
    nxt = re.search(r"^\[", text[m.end() :], re.M)
    end = m.end() + (nxt.start() if nxt else len(text) - m.end())
    return m.end(), end


def toml_set(text, table, key, literal):
    """Replace `key = ...` inside [table], keeping the trailing comment. literal is the toml text of the new value."""
    s, e = toml_get_section(text, table)
    sec = text[s:e]
    pat = re.compile(r"^(" + re.escape(key) + r"\s*=\s*)([^#\n]*?)(\s*(#.*)?)$", re.M)
    if not pat.search(sec):
        raise ToolError(f"{key} not found in [{table}]")
    sec = pat.sub(lambda m: m.group(1) + literal + m.group(3), sec, count=1)
    return text[:s] + sec + text[e:]


def append_raise_log(text, kind, name, old, new, reason):
    entry = (
        f'\n[[raise_log]]\ndate = "{datetime.date.today().isoformat()}"\nkind = "{kind}"\nname = "{name}"\n'
        f"old = {old}\nnew = {new}\nreason = {json.dumps(reason)}\n"
    )
    return text.rstrip("\n") + "\n" + entry


# ------------------------------------------------------------------------------------------------ output
def kb(n):
    return f"{n / 1000:.1f}"


def print_report(stat, modes, detail, cfg, out=None):
    w = (out or sys.stdout).write
    w("== Static sizes (text + data + bss of the linked ELFs, bytes)\n")
    w(f"{'module':<10}{'size':>9}{'ratchet':>9}{'target':>9}  status\n")
    for r in stat:
        w(
            f"{r['module']:<10}{r['size']:>9,}{r['ratchet']:>9,}{r['target']:>9,}  {r['status']}{(' - ' + r['note']) if r['note'] else ''}\n"
        )
    w(
        f"\n== Predicted free heap on the device (KB = 1000 B; system free {cfg['system']['free_heap']:,} B with no app)\n"
    )
    w(f"{'mode':<22}{'predicted':>10}{'KB':>7}{'floor':>9}{'baseline':>10}  status\n")
    for r in modes:
        w(
            f"{r['mode']:<22}{r['predicted']:>10,}{kb(r['predicted']):>7}{r['floor']:>9,}{r['baseline']:>10,}  {r['status']}{(' - ' + r['note']) if r['note'] else ''}\n"
        )
    if detail.get("explore_by_map"):
        w(
            "\n== Exploring free heap per real map (KB; resident = data block + resident bank, cache = estimated streamed-actor cache, file = map_NN.bin)\n"
        )
        w(f"{'map':>4}{'file':>8}{'resident':>9}{'cache':>7}{'free':>9}{'KB':>6}\n")
        for m, v in detail["explore_by_map"].items():
            c = detail["map_cost"][m]
            w(f"{m:>4}{c['file']:>8,}{c['res']:>9,}{c['cache']:>7,}{v:>9,}{kb(v):>6}\n")
    w(
        f"(biggest map: {detail['biggest_map']}; scene peak total {detail['scene_total']:,} B; party screen {detail['menu_party_scene']:,}, bag {detail['menu_bag_scene']:,}, shop {detail['shop_scene']:,}, PC visit {detail.get('pc_box_scene', 0):,}, Registro {detail.get('pc_reg_scene', 0):,} B)\n"
    )


def print_dev(dev_stat, dev_modes, out=None):
    w = (out or sys.stdout).write
    w("\n== DEV build (NUCLEO_DEBUG=1): floors only, no ratchets / baselines\n")
    w(f"{'module':<10}{'size':>9}{'text':>9}{'data':>7}{'bss':>7}\n")
    for mod in STATIC_ORDER:
        if mod not in dev_stat:
            continue
        r = dev_stat[mod]
        w(f"{mod:<10}{r['total']:>9,}{r['text']:>9,}{r['data']:>7,}{r['bss']:>7,}\n")
    w(f"{'mode':<22}{'predicted':>10}{'KB':>7}{'floor':>9}  status\n")
    for r in dev_modes:
        w(
            f"{r['mode']:<22}{r['predicted']:>10,}{kb(r['predicted']):>7}{r['floor']:>9,}  {r['status']}{(' - ' + r['note']) if r['note'] else ''}\n"
        )


def overall(stat, modes, dev_modes=()):
    sts = (
        [r["status"] for r in stat]
        + [r["status"] for r in modes]
        + [r["status"] for r in dev_modes]
    )
    return "FAIL" if "FAIL" in sts else ("WARN" if "WARN" in sts else "OK")


# ------------------------------------------------------------------------------------------------ main
def evaluate(cfg, sizes, fals, host, maps, src):
    pred, detail = model(cfg, sizes, fals, host, maps, src)
    stat = check_static(cfg, sizes)
    modes = check_modes(cfg, pred)
    return stat, modes, detail, pred


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--config", default=str(DEFAULT_CFG))
    ap.add_argument("--elf-dir", default=str(HOME / "build"))
    ap.add_argument("--maps-dir", default=str(ROOT / "nucleo" / "files"))
    ap.add_argument(
        "--dev-elf-dir",
        help="the DEV build's ELF / .fal directory (NUCLEO_DEBUG=1): checked against the heap floors only",
    )
    ap.add_argument(
        "--dev-sizes-json", help="test hook: like --sizes-json for the dev build"
    )
    ap.add_argument(
        "--dev-fal-json", help="test hook: like --fal-json for the dev build"
    )
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="set every ratchet to measured + tol and every baseline to the prediction of the RELEASE build (logged; needs --reason)",
    )
    ap.add_argument(
        "--sizes-json",
        help="test hook: {module: total_bytes or {text,data,bss}} instead of arm-none-eabi-size",
    )
    ap.add_argument(
        "--fal-json",
        help="test hook: {module: {sections, reloc}} instead of reading the .fal files",
    )
    ap.add_argument(
        "--host-json",
        help="test hook: the host numbers instead of running the harnesses",
    )
    ap.add_argument(
        "--no-host",
        action="store_true",
        help="reuse build/mem_budget/host.json from the last run (host build broken by someone else)",
    )
    ap.add_argument("--json", default=str(OUT_DIR / "report.json"))
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--raise", dest="raise_", nargs=2, metavar=("MODULE", "BYTES"))
    ap.add_argument("--raise-baseline", nargs=2, metavar=("MODE", "BYTES"))
    ap.add_argument("--calibrate", nargs=2, metavar=("NAME", "MEASURED"))
    ap.add_argument("--reason")
    ap.add_argument("--ref")
    a = ap.parse_args(argv)

    cfg_path = pathlib.Path(a.config)
    cfg_text = cfg_path.read_text()
    cfg = tomllib.loads(cfg_text)
    # -------- explicit edits of the toml (need no measurements)
    if a.raise_:
        mod, nbytes = a.raise_[0], int(a.raise_[1])
        if not a.reason:
            print(
                'mem_budget: --raise needs --reason "why this growth is accepted"',
                file=sys.stderr,
            )
            return 2
        if mod not in cfg["modules"]:
            print(
                f"mem_budget: unknown module {mod} (one of {', '.join(STATIC_ORDER)})",
                file=sys.stderr,
            )
            return 2
        old = cfg["modules"][mod]["ratchet"]
        t = toml_set(cfg_text, f"modules.{mod}", "ratchet", str(nbytes))
        cfg_path.write_text(append_raise_log(t, "ratchet", mod, old, nbytes, a.reason))
        print(f"ratchet of {mod}: {old:,} -> {nbytes:,} B (logged: {a.reason})")
        return 0
    if a.raise_baseline:
        mode, nbytes = a.raise_baseline[0], int(a.raise_baseline[1])
        if not a.reason:
            print("mem_budget: --raise-baseline needs --reason", file=sys.stderr)
            return 2
        if mode not in cfg["modes"]:
            print(
                f"mem_budget: unknown mode {mode} (one of {', '.join(MODE_ORDER)})",
                file=sys.stderr,
            )
            return 2
        old = cfg["modes"][mode]["baseline"]
        t = toml_set(cfg_text, f"modes.{mode}", "baseline", str(nbytes))
        cfg_path.write_text(
            append_raise_log(t, "baseline", mode, old, nbytes, a.reason)
        )
        print(f"baseline of {mode}: {old:,} -> {nbytes:,} B (logged: {a.reason})")
        return 0
    try:
        # -------- inputs
        if a.sizes_json:
            raw = json.loads(pathlib.Path(a.sizes_json).read_text())
            sizes = {
                m: (
                    {"text": v, "data": 0, "bss": 0, "total": v}
                    if isinstance(v, int)
                    else dict(v, total=v["text"] + v["data"] + v["bss"])
                )
                for m, v in raw.items()
            }
        else:
            sizes = measure_sizes(cfg, a.elf_dir)
        fals = (
            json.loads(pathlib.Path(a.fal_json).read_text())
            if a.fal_json
            else measure_fals(cfg, a.elf_dir)
        )
        dev_sizes = dev_fals = None
        if a.dev_sizes_json:
            raw = json.loads(pathlib.Path(a.dev_sizes_json).read_text())
            dev_sizes = {
                m: (
                    {"text": v, "data": 0, "bss": 0, "total": v}
                    if isinstance(v, int)
                    else dict(v, total=v["text"] + v["data"] + v["bss"])
                )
                for m, v in raw.items()
            }
        elif a.dev_elf_dir:
            dev_sizes = measure_sizes(cfg, a.dev_elf_dir)
        if a.dev_fal_json:
            dev_fals = json.loads(pathlib.Path(a.dev_fal_json).read_text())
        elif a.dev_elf_dir:
            dev_fals = measure_fals(cfg, a.dev_elf_dir)
        src = source_defines(ROOT)
        maps = map_files(
            a.maps_dir,
            cfg["system"]["real_maps_below"],
            cfg["system"].get("extra_real_maps", []),
        )
        cache = OUT_DIR / "host.json"
        if a.host_json:
            host = json.loads(pathlib.Path(a.host_json).read_text())
        elif a.no_host:
            if not cache.exists():
                raise ToolError(
                    "--no-host but there is no build/mem_budget/host.json yet"
                )
            host = json.loads(cache.read_text())
            print("WARN: host numbers are from the last good run (--no-host)")
        else:
            host = host_probes(cfg, ROOT)
            OUT_DIR.mkdir(parents=True, exist_ok=True)
            cache.write_text(json.dumps(host, indent=1))
        if "stacks" in host:  # test hook: override the source-derived constants
            src = dict(src, stacks=host["stacks"])
        for k in (
            "explore_live",
            "before_load",
            "peak_abs",
            "party_peak",
            "bag_peak",
            "shop_peak",
        ):
            if k not in host:
                raise ToolError(f"host numbers lack {k}")
    except ToolError as e:
        print(f"mem_budget: cannot measure: {e}", file=sys.stderr)
        return 2

    if a.calibrate:
        name, measured = a.calibrate[0], int(a.calibrate[1])
        if name not in cfg["calibration"]:
            print(
                f"mem_budget: unknown calibration {name} (one of {', '.join(cfg['calibration'])})",
                file=sys.stderr,
            )
            return 2
        cfg_text = calibrate(
            cfg, cfg_text, name, measured, sizes, fals, host, maps, src, a.ref or ""
        )
        cfg_path.write_text(cfg_text)
        print(f"calibration {name}: measured {measured:,} B recorded; re-run the check")
        return 0

    # -------- evaluation
    stat, modes, detail, pred = evaluate(cfg, sizes, fals, host, maps, src)
    print_report(stat, modes, detail, cfg)
    dev_rows = []
    if dev_sizes is not None and dev_fals is not None:
        pred_dev, _ = model(cfg, dev_sizes, dev_fals, host, maps, src)
        dev_rows = check_dev(cfg, pred_dev)
        print_dev(dev_sizes, dev_rows)
        # what the debug code costs (release vs dev), for the log
        print(
            "   (dev - release: "
            + ", ".join(
                f"{m} {dev_sizes[m]['total'] - sizes[m]['total']:+,}"
                for m in STATIC_ORDER
                if m in dev_sizes
            )
            + " B)"
        )
    else:
        print(
            "\nWARN: DEV build not checked (no --dev-elf-dir): the lead's debug build may be under a floor"
        )
    res = overall(stat, modes, dev_rows)

    if a.rebase:
        if not a.reason:
            print("mem_budget: --rebase needs --reason", file=sys.stderr)
            return 2
        t = cfg_text
        for r in stat:
            new = r["size"] + cfg["meta"]["tol"]
            if new != r["ratchet"]:
                t = toml_set(t, f"modules.{r['module']}", "ratchet", str(new))
                t = append_raise_log(
                    t, "ratchet", r["module"], r["ratchet"], new, a.reason
                )
                print(f"rebase ratchet {r['module']}: {r['ratchet']:,} -> {new:,}")
        for r in modes:
            m = r["mode"]
            if r["predicted"] != r["baseline"] or (
                r["known_red"] and r["predicted"] >= r["floor"]
            ):
                t = toml_set(t, f"modes.{m}", "baseline", str(r["predicted"]))
                if r["known_red"] and r["predicted"] >= r["floor"]:
                    t = toml_set(t, f"modes.{m}", "known_red", "false")
                if r["predicted"] != r["baseline"]:
                    t = append_raise_log(
                        t, "baseline", m, r["baseline"], r["predicted"], a.reason
                    )
                print(f"rebase baseline {m}: {r['baseline']:,} -> {r['predicted']:,}")
        cfg_path.write_text(t)
        print(
            "rebased: re-run the check (the statuses above are those BEFORE the rebase)"
        )

    if a.update:
        if res == "FAIL":
            print(
                "\n--update refused: fix the FAIL rows (or --raise on purpose) first."
            )
        else:
            t = cfg_text
            for r in stat:
                new = r["size"] + cfg["meta"]["tol"]
                if new < r["ratchet"]:
                    t = toml_set(t, f"modules.{r['module']}", "ratchet", str(new))
                    print(f"ratchet {r['module']}: {r['ratchet']:,} -> {new:,}")
            for r in modes:
                m = r["mode"]
                if r["predicted"] >= r["floor"]:
                    if r["known_red"]:
                        t = toml_set(t, f"modes.{m}", "known_red", "false")
                        print(f"mode {m}: floor met, known_red cleared")
                    if r["predicted"] > r["baseline"]:
                        t = toml_set(t, f"modes.{m}", "baseline", str(r["predicted"]))
                elif r["known_red"] and r["predicted"] > r["baseline"]:
                    t = toml_set(t, f"modes.{m}", "baseline", str(r["predicted"]))
                    print(f"baseline {m}: {r['baseline']:,} -> {r['predicted']:,}")
            if t != cfg_text:
                cfg_path.write_text(t)
            else:
                print("--update: nothing to tighten")

    report = {
        "status": res,
        "date": datetime.date.today().isoformat(),
        "static": stat,
        "modes": modes,
        "dev": {"static": dev_sizes, "modes": dev_rows} if dev_rows else None,
        "inputs": {
            "host": host,
            "stacks": src["stacks"],
            "map_files": {str(k): v for k, v in maps.items()},
            "fal": fals,
            "model": detail,
            "system_free_heap": cfg["system"]["free_heap"],
            "calibration": {
                k: {
                    kk: vv
                    for kk, vv in v.items()
                    if kk in ("measured", "offset", "date")
                }
                for k, v in cfg["calibration"].items()
            },
        },
    }
    pathlib.Path(a.json).parent.mkdir(parents=True, exist_ok=True)
    pathlib.Path(a.json).write_text(json.dumps(report, indent=1))
    print(
        f"\nmem_budget: {res}   (json: {pathlib.Path(a.json).relative_to(ROOT) if pathlib.Path(a.json).is_relative_to(ROOT) else a.json})"
    )
    if res == "FAIL":
        if any(r["status"] == "FAIL" for r in dev_rows):
            print(
                "A DEV row FAILs: the debug build (NUCLEO_DEBUG=1) is under a heap floor; the device tests would run out of room."
            )
        print(
            "A FAIL row means this change made the build bigger / the heap lower than the recorded budget. Fix the growth; "
            "if it is accepted on purpose use --raise / --raise-baseline with a --reason (contract 5.x)."
        )
        return 1
    return 0


def calibrate(cfg, text, name, measured, sizes, fals, host, maps, src, ref):
    """Re-fit the device offset of one calibration from the current tree (the model's input snapshot is stored beside it)."""
    flat = json.loads(json.dumps(cfg))
    today = datetime.date.today().isoformat()
    if name == "explore":
        m = flat["calibration"]["explore"].get("map", flat["system"]["route1_map"])
        flat["calibration"]["explore"]["offset"] = 0
        raw = model(flat, sizes, fals, host, maps, src)[0]["explore_route1"]
        fields = {
            "offset": raw - measured,
            "image": sizes["resident"]["total"],
            "host_live": host["explore_live"],
            "map": m,
        }
    else:
        flat["calibration"][name]["offset"] = 0
        k = {
            "battle_low": "battle_peak_wild",
            "menu_open": "menu_open",
            "shop_open": "shop_open",
        }.get(name)
        if k is None:
            raise ToolError(f"cannot calibrate {name}")
        raw = model(flat, sizes, fals, host, maps, src)[0][k]
        fields = {"offset": measured - raw, "image": sizes["resident"]["total"]}
        if name == "battle_low":
            fields.update(
                plugin=sizes["battle"]["total"],
                before_load=host["before_load"],
                scene_total=host["peak_abs"]
                - src["emu"]["battle"]
                - host["before_load"],
            )
    sec = f"calibration.{name}"
    try:
        toml_get_section(text, sec)
    except ToolError:
        text = text.rstrip("\n") + f"\n\n[{sec}]\n"
    s, e = toml_get_section(text, sec)
    body = (
        f'date = "{today}"\nref = {json.dumps(ref)}\nmeasured = {measured}\n'
        + "".join(
            f"{k} = {json.dumps(v) if isinstance(v, str) else v}\n"
            for k, v in fields.items()
        )
    )
    return text[:s] + body + "\n" + text[e:]


if __name__ == "__main__":
    sys.exit(main())
