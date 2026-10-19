#!/usr/bin/env python3
"""Host test of tools/mem_budget.py with fake sizes (over / under the ratchet, target and floors). No ufbt, no Flipper, no host harness.
Run: `bash tools/mem_budget.sh --selftest` (or `python3 -I tools/mem_budget_test.py`)."""
import contextlib, importlib.util, io, json, pathlib, re, sys, tempfile, tomllib, unittest

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("mem_budget", HERE / "mem_budget.py")
mb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mb)

CFG = """
[meta]
tol = 256
[system]
free_heap = 146500
host_cache_bytes = 2439
loader_stays = 1500
real_maps_below = 90
route1_map = 3
shop_map = 6
[modules.resident]
elf = "elementis_zeta_d.elf"
target = 46000
ratchet = 47143
[modules.battle]
elf = "ez_battle_d.elf"
fal = "ez_battle.fal"
target = 30000
ratchet = 31988
[modules.menu]
elf = "ez_menu_d.elf"
fal = "ez_menu.fal"
target = 12000
ratchet = 11230
[modules.ui]
elf = "ez_ui_d.elf"
fal = "ez_ui.fal"
target = 6000
ratchet = 5850
[modules.pc]
elf = "ez_pc_d.elf"
fal = "ez_pc.fal"
target = 9000
ratchet = 8645
[calibration.explore]
measured = 53700
map = 3
offset = 9787
[calibration.battle_low]
measured = 23900
offset = 916
[calibration.menu_open]
offset = 0
[calibration.shop_open]
offset = 0
[modes.explore_biggest]
floor = 55000
known_red = true
baseline = 46224
[modes.explore_route1]
floor = 55000
known_red = true
baseline = 53700
[modes.battle_load]
floor = 30000
known_red = false
baseline = 33466
[modes.battle_peak_wild]
floor = 30000
known_red = true
baseline = 23900
[modes.battle_peak_trainer]
floor = 30000
known_red = true
baseline = 22325
[modes.menu_open]
floor = 30000
known_red = false
baseline = 30504
[modes.shop_open]
floor = 30000
known_red = false
baseline = 39716
[modes.ui_open]
floor = 30000
known_red = false
baseline = 36056
[modes.pc_open]
floor = 30000
known_red = false
baseline = 30000
[host]
battle_script = "x"
party_script = "y"
"""

SIZES = {"resident": 46887, "battle": 31732, "menu": 10970, "ui": 5596, "pc": 8389, "casino": 5077}
FALS = {"battle": {"sections": 31736, "reloc": 1413}, "menu": {"sections": 10972, "reloc": 2190}, "ui": {"sections": 5596, "reloc": 1945}, "pc": {"sections": 8392, "reloc": 2190}, "casino": {"sections": 5080, "reloc": 900}}
# the CLI tests run the tool's own source_defines(): the emulated battle block is whatever nucleo/plugin_link.c says today
EMU_BATTLE = int(re.search(r"#define EMU_BATTLE_BYTES (\d+)", (pathlib.Path(__file__).resolve().parent.parent / "nucleo" / "plugin_link.c").read_text()).group(1))
HOST = {"explore_live": 28190, "before_load": 12719, "peak_abs": EMU_BATTLE + 12719 + 12955, "party_peak": 11305, "bag_peak": 10209,
        "shop_peak": 14175, "box_peak": 11500, "stacks": 7936}
MAPS = {2: 14236, 3: 6760, 4: 12908, 6: 5024}   # lab, Ruta 1, Ciudad Nodo, tienda
SRC = {"stacks": 7936, "bookkeeping": 2560, "margins": {"battle": 10240, "menu": 3072, "ui": 3072, "pc": 3072}, "emu": {"battle": EMU_BATTLE, "menu": 7400, "menu_shop": 10900, "pc": 7200}}


def tot(d):
    return {k: {"text": v, "data": 0, "bss": 0, "total": v} for k, v in d.items()}


def run_model(sizes=None, host=None, maps=None, fals=None, cfg=None):
    cfg = cfg or tomllib.loads(CFG)
    s = dict(SIZES, **(sizes or {}))
    h = dict(HOST, **(host or {}))
    pred, detail = mb.model(cfg, tot(s), fals or FALS, h, maps or MAPS, SRC)
    return pred, detail


class ModelTests(unittest.TestCase):
    def test_anchored_to_the_device(self):
        pred, d = run_model()
        self.assertEqual(pred["explore_route1"], 53700)       # calibration point 1
        self.assertEqual(pred["battle_peak_wild"], 23900)     # calibration point 2
        self.assertEqual(d["biggest_map"], 2)
        self.assertEqual(pred["explore_biggest"], 53700 - (14236 - 6760))

    def test_growth_moves_the_right_modes(self):
        base, _ = run_model()
        res, _ = run_model(sizes={"resident": SIZES["resident"] + 1000})        # +1 KB resident: everything loses 1 KB
        for k in base:
            self.assertEqual(res[k], base[k] - 1000, k)
        bat, _ = run_model(sizes={"battle": SIZES["battle"] + 1000})            # +1 KB battle plugin: only the battle modes
        for k in ("battle_load", "battle_peak_wild", "battle_peak_trainer"):
            self.assertEqual(bat[k], base[k] - 1000, k)
        for k in ("explore_biggest", "explore_route1", "menu_open", "shop_open", "ui_open", "pc_open"):
            self.assertEqual(bat[k], base[k], k)
        pc, _ = run_model(sizes={"pc": SIZES["pc"] + 1000})                     # +1 KB PC plugin: only pc_open (the party / bag / shop screens do not carry it)
        self.assertEqual(pc["pc_open"], base["pc_open"] - 1000)
        for k in base:
            if k != "pc_open":
                self.assertEqual(pc[k], base[k], k)
        menu, _ = run_model(sizes={"menu": SIZES["menu"] + 1000})              # +1 KB menu plugin: menu_open and shop_open, not pc_open
        self.assertEqual(menu["pc_open"], base["pc_open"])
        self.assertEqual(menu["menu_open"], base["menu_open"] - 1000)
        box, _ = run_model(host={"box_peak": HOST["box_peak"] + 5000})         # the PC visit's heap peak (big enough to beat the load transient): only pc_open (it left menu_open in the M5 memory recovery)
        self.assertLess(box["pc_open"], base["pc_open"])
        self.assertEqual(box["menu_open"], base["menu_open"])
        reg, dd = run_model(host={"reg_peak": 5308 + 2439 + 6000})              # C2-S2 / contract 76: the Registro screen's heap peak (6 KB over the emulated plugin + the host cache; EMU_CASINO_REG falls back to the casino block here): the CASINO plugin's scene (the PC plugin no longer carries it)
        self.assertLess(reg["casino_open"], base["casino_open"])
        self.assertEqual(reg["pc_open"], base["pc_open"])
        self.assertEqual(dd["pc_reg_scene"], 6000)
        self.assertEqual(reg["menu_open"], base["menu_open"])
        small, _ = run_model(host={"reg_peak": 5308 + 2439 + 100})              # a smaller Registro peak than the loader's margin changes nothing
        self.assertEqual(small["casino_open"], base["casino_open"])
        self.assertEqual(small["pc_open"], base["pc_open"])
        scene, _ = run_model(host={"peak_abs": HOST["peak_abs"] + 700})         # the scene's heap peak grows: only the peaks
        self.assertEqual(scene["battle_peak_wild"], base["battle_peak_wild"] - 700)
        self.assertEqual(scene["battle_load"], base["battle_load"])
        big, _ = run_model(maps={**MAPS, 2: 20000})                       # a bigger map file: the biggest map loses it, Ruta 1 not
        self.assertEqual(big["explore_biggest"], base["explore_biggest"] - (20000 - 14236))
        self.assertEqual(big["explore_route1"], base["explore_route1"])
        self.assertEqual(big["menu_open"], base["menu_open"] - (20000 - 14236))
        rel, _ = run_model(fals=dict(FALS, battle={"sections": 31736, "reloc": 3413}))   # relocation data: only the load transient
        self.assertEqual(rel["battle_load"], base["battle_load"] - 2000)
        self.assertEqual(rel["battle_peak_wild"], base["battle_peak_wild"])
        heap, _ = run_model(host={"explore_live": HOST["explore_live"] + 400})   # a new resident heap block: exploring and the battle's base
        self.assertEqual(heap["explore_route1"], base["explore_route1"] - 400)

    def test_resident_data_tables(self):
        """Contract 27: the species / move / trainer records are read on demand through small caches, and the battle data tables are no longer released across the
        plugin load. A table that stays in the heap is in the exploring heap, in `before_load` (live right before the plugin block) and in the peak; one that is released
        across the load (the pre-M5 scheme) is in the exploring heap and the peak only. Either way the battle peak loses exactly the bytes, the exploring heap too;
        only the load differs: it gains the released bytes back."""
        base, _ = run_model()
        kept, _ = run_model(host={"explore_live": HOST["explore_live"] + 900, "before_load": HOST["before_load"] + 900, "peak_abs": HOST["peak_abs"] + 900})
        released, _ = run_model(host={"explore_live": HOST["explore_live"] + 900, "peak_abs": HOST["peak_abs"] + 900})
        for k in ("explore_route1", "explore_biggest", "battle_peak_wild", "battle_peak_trainer"):
            self.assertEqual(kept[k], base[k] - 900, k)
            self.assertEqual(released[k], base[k] - 900, k)
        self.assertEqual(kept["battle_load"], base["battle_load"] - 900)
        self.assertEqual(released["battle_load"], base["battle_load"])
        self.assertEqual(kept["ui_open"], base["ui_open"] - 900)

    def test_resident_bytes_and_stream_cache(self):
        """Streamed actors (contract 3.1): the model charges the resident bytes + the cache estimate of a map, not its file size."""
        base, _ = run_model()
        maps = {**MAPS, 2: {"file": 14236, "res": 6460, "cache": 1872}, 3: {"file": 6760, "res": 4712, "cache": 456}}
        pred, d = run_model(maps=maps)
        # Ruta 1 is the host probe's map (its bank is in explore_live, its NPC cache is not): it pays only its cache
        self.assertEqual(pred["explore_route1"], base["explore_route1"] - 456)
        # Ciudad Nodo (12908 B, no cache) is now the biggest
        self.assertEqual(d["biggest_map"], 4)
        self.assertEqual(pred["explore_biggest"], base["explore_route1"] - (12908 - 4712))
        self.assertEqual(d["explore_by_map"][2], base["explore_route1"] - (6460 - 4712) - 1872)

    def test_map_cost_reads_the_header(self):
        import struct as st
        def fake(bank_res, streamed, npc_spr=0):
            bank = bytearray(8 + 16)
            bank[1] = 1
            bank[8:14] = bytes([2, 19, 35, 4, 5, 1 | (8 if streamed else 0)])   # actor 19 x 35, 4 dirs x 5 frames, masked
            hdr = bytearray(64)
            hdr[:4] = b"NMAP"
            hdr[4] = 2
            hdr[14] = 1
            st.pack_into("<H", hdr, 18, bank_res // 4)
            st.pack_into("<I", hdr, 32, 64)      # npcs at 64: one NpcDef (12 B), bank at 76
            st.pack_into("<I", hdr, 48, 76)
            return bytes(hdr) + bytes([npc_spr]) + bytes(11) + bytes(bank) + bytes(5000)
        c = mb.map_cost(fake(2000, True))
        self.assertEqual(mb.follower_unit(), 160)                       # contract 69: one facing of the follower's 20 x 20 sprite
        self.assertEqual((c["res"], c["cache"]), (76 + 2000, 5 * 280 + mb.follower_unit() + mb.STREAM_CACHE_TABLE))   # one 1,400 B facing + the follower's block + the table
        self.assertEqual(mb.map_cost(fake(2000, False))["cache"], mb.follower_unit() + mb.STREAM_CACHE_TABLE)   # nothing streams but the follower
        old = bytes(6760)
        self.assertEqual(mb.map_cost(old), {"file": 6760, "res": 6760, "cache": 0})                  # not an NMAP v2 file: the file size

    def test_trainer_has_its_own_probe(self):
        pred, _ = run_model()                                       # no trainer numbers: same as the wild peak
        self.assertEqual(pred["battle_peak_wild"], pred["battle_peak_trainer"])
        t = {"trainer_before_load": HOST["before_load"], "trainer_peak_abs": HOST["peak_abs"] + 300}   # the trainer run peaks 300 B higher
        pred2, _ = run_model(host=t)
        self.assertEqual(pred2["battle_peak_wild"], pred["battle_peak_wild"])
        self.assertEqual(pred2["battle_peak_trainer"], pred["battle_peak_wild"] - 300)
        t = {"trainer_before_load": HOST["before_load"] + 50, "trainer_peak_abs": HOST["peak_abs"] + 50}  # same scene, the world was 50 B bigger
        self.assertEqual(run_model(host=t)[0]["battle_peak_trainer"], pred["battle_peak_wild"] - 50)


class VerdictTests(unittest.TestCase):
    def stat(self, **sizes):
        cfg = tomllib.loads(CFG)
        return {r["module"]: r for r in mb.check_static(cfg, tot(dict(SIZES, **sizes)))}

    def test_static_levels(self):
        self.assertEqual(self.stat()["resident"]["status"], "WARN")                       # above target, within the ratchet
        self.assertEqual(self.stat(resident=47143)["resident"]["status"], "WARN")         # exactly at the ratchet
        self.assertEqual(self.stat(resident=47144)["resident"]["status"], "FAIL")         # one byte over: it grew
        self.assertEqual(self.stat(resident=45000)["resident"]["status"], "OK")           # under the target
        self.assertEqual(self.stat(battle=31989)["battle"]["status"], "FAIL")
        self.assertEqual(self.stat(menu=11231)["menu"]["status"], "FAIL")
        self.assertEqual(self.stat()["menu"]["status"], "OK")
        self.assertEqual(self.stat(ui=5851)["ui"]["status"], "FAIL")

    def modes(self, pred_over=None, cfg_over=None):
        cfg = tomllib.loads(CFG)
        for (m, k), v in (cfg_over or {}).items():
            cfg["modes"][m][k] = v
        pred, _ = run_model()
        pred.update(pred_over or {})
        return {r["mode"]: r for r in mb.check_modes(cfg, pred)}

    def test_known_red_is_a_warning_until_it_gets_worse(self):
        self.assertEqual(self.modes()["battle_peak_wild"]["status"], "WARN")                       # at the baseline
        self.assertEqual(self.modes({"battle_peak_wild": 23900 - 256})["battle_peak_wild"]["status"], "WARN")   # inside the tolerance
        self.assertEqual(self.modes({"battle_peak_wild": 23900 - 257})["battle_peak_wild"]["status"], "FAIL")   # regression
        self.assertEqual(self.modes({"battle_peak_wild": 26000})["battle_peak_wild"]["status"], "WARN")         # better, still red
        self.assertEqual(self.modes({"battle_peak_wild": 30000})["battle_peak_wild"]["status"], "OK")           # floor met
        self.assertIn("known_red", self.modes({"battle_peak_wild": 31000})["battle_peak_wild"]["note"])

    def test_floor_without_known_red_fails(self):
        self.assertEqual(self.modes()["menu_open"]["status"], "OK")
        self.assertEqual(self.modes({"menu_open": 29999})["menu_open"]["status"], "FAIL")
        self.assertEqual(self.modes({"battle_load": 29000})["battle_load"]["status"], "FAIL")

    def test_eroded_margin_warns(self):
        self.assertEqual(self.modes({"battle_load": 33466 - 256})["battle_load"]["status"], "OK")
        self.assertEqual(self.modes({"battle_load": 33466 - 257})["battle_load"]["status"], "WARN")

    def test_overall(self):
        cfg = tomllib.loads(CFG)
        pred, _ = run_model()
        stat, modes = mb.check_static(cfg, tot(SIZES)), mb.check_modes(cfg, pred)
        self.assertEqual(mb.overall(stat, modes), "WARN")
        self.assertEqual(mb.overall(mb.check_static(cfg, tot(dict(SIZES, battle=33000))), modes), "FAIL")


class TomlTests(unittest.TestCase):
    def test_set_keeps_comments_and_neighbours(self):
        t = "[a]\nx = 1   # keep\ny = 2\n[b]\nx = 3\n"
        r = mb.toml_set(t, "a", "x", "9")
        self.assertEqual(r, "[a]\nx = 9   # keep\ny = 2\n[b]\nx = 3\n")
        self.assertEqual(tomllib.loads(mb.toml_set(t, "b", "x", "7"))["b"]["x"], 7)
        with self.assertRaises(mb.ToolError):
            mb.toml_set(t, "a", "z", "1")

    def test_dotted_tables(self):
        r = mb.toml_set(CFG, "modules.battle", "ratchet", "100")
        c = tomllib.loads(r)
        self.assertEqual(c["modules"]["battle"]["ratchet"], 100)
        self.assertEqual(c["modules"]["menu"]["ratchet"], 11230)


class CliTests(unittest.TestCase):
    """The whole tool on fake inputs: temporary toml + sizes + host numbers + map files."""
    def setUp(self):
        self.td = tempfile.TemporaryDirectory()
        d = pathlib.Path(self.td.name)
        self.cfg = d / "b.toml"
        self.cfg.write_text(CFG)
        self.maps = d / "maps"
        self.maps.mkdir()
        for mid, n in MAPS.items():
            (self.maps / f"map_{mid:02d}.bin").write_bytes(b"\0" * n)
        (self.maps / "map_90.bin").write_bytes(b"\0" * 99999)     # a test map: must not count as "the biggest"
        (d / "host.json").write_text(json.dumps(HOST))
        (d / "fal.json").write_text(json.dumps(FALS))
        self.d = d

    def tearDown(self):
        self.td.cleanup()

    def run_cli(self, sizes=None, *extra):
        (self.d / "sizes.json").write_text(json.dumps(dict(SIZES, **(sizes or {}))))
        argv = ["--config", str(self.cfg), "--sizes-json", str(self.d / "sizes.json"), "--fal-json", str(self.d / "fal.json"),
                "--host-json", str(self.d / "host.json"), "--maps-dir", str(self.maps), "--json", str(self.d / "r.json"), *extra]
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
            rc = mb.main(argv)
        return rc, buf.getvalue()

    def test_green_with_warnings(self):
        rc, out = self.run_cli()
        self.assertEqual(rc, 0, out)
        rep = json.loads((self.d / "r.json").read_text())
        self.assertEqual(rep["status"], "WARN")
        self.assertEqual({m["mode"]: m["predicted"] for m in rep["modes"]}["explore_route1"], 53700)

    def test_inflated_modules_fail(self):
        for mod, size in (("resident", 47144), ("battle", 31989), ("menu", 11231), ("ui", 5851), ("pc", 8646)):
            rc, out = self.run_cli({mod: size})
            self.assertEqual(rc, 1, f"{mod}: {out}")
            self.assertIn("FAIL", out)
        rc, out = self.run_cli({"resident": 46887 + 400})      # inside the ratchet, but the heap model sees the regression
        self.assertEqual(rc, 1, out)
        self.assertIn("REGRESSION", out)

    def test_update_only_lowers_the_ratchet(self):
        rc, _ = self.run_cli({"resident": 46300, "battle": 31000}, "--update")
        self.assertEqual(rc, 0)
        c = tomllib.loads(self.cfg.read_text())
        self.assertEqual(c["modules"]["resident"]["ratchet"], 46300 + 256)
        self.assertEqual(c["modules"]["battle"]["ratchet"], 31000 + 256)
        self.assertEqual(c["modules"]["menu"]["ratchet"], 11226 if False else min(11230, 10970 + 256))
        self.assertGreater(c["modes"]["explore_route1"]["baseline"], 53700)          # the heap improved: the baseline follows
        before = self.cfg.read_text()
        rc, out = self.run_cli({"resident": 46900}, "--update")                        # bigger than the new ratchet: FAIL, nothing is written
        self.assertEqual(rc, 1)
        self.assertEqual(self.cfg.read_text(), before)
        self.assertIn("refused", out)

    def test_update_clears_known_red_when_the_floor_is_met(self):
        self.run_cli({"resident": 40000}, "--update")
        c = tomllib.loads(self.cfg.read_text())
        self.assertFalse(c["modes"]["explore_route1"]["known_red"])
        self.assertTrue(c["modes"]["explore_biggest"]["known_red"] is False or c["modes"]["explore_biggest"]["baseline"] > 46224)

    def test_raise_needs_a_reason_and_is_logged(self):
        rc, out = self.run_cli(None, "--raise", "battle", "33000")
        self.assertEqual(rc, 2)
        self.assertEqual(tomllib.loads(self.cfg.read_text())["modules"]["battle"]["ratchet"], 31988)
        rc, out = self.run_cli(None, "--raise", "battle", "33000", "--reason", "new status effects")
        self.assertEqual(rc, 0, out)
        c = tomllib.loads(self.cfg.read_text())
        self.assertEqual(c["modules"]["battle"]["ratchet"], 33000)
        self.assertEqual(c["raise_log"][-1]["reason"], "new status effects")
        self.assertEqual((c["raise_log"][-1]["old"], c["raise_log"][-1]["new"]), (31988, 33000))
        rc, out = self.run_cli({"battle": 32500})        # fits the raised ratchet, but the battle's heap is below its known-red baseline
        self.assertEqual(rc, 1)
        self.assertIn("REGRESSION", out)
        self.assertNotIn("grew:", out)
        rc, _ = self.run_cli(None, "--raise", "nonsense", "1", "--reason", "x")
        self.assertEqual(rc, 2)

    def test_raise_baseline(self):
        rc, _ = self.run_cli({"battle": 31700, "resident": 46887 + 0}, "--raise-baseline", "battle_peak_wild", "20000", "--reason", "trainer art")
        self.assertEqual(rc, 0)
        c = tomllib.loads(self.cfg.read_text())
        self.assertEqual(c["modes"]["battle_peak_wild"]["baseline"], 20000)
        self.assertEqual(c["raise_log"][-1]["kind"], "baseline")

    def test_calibrate_refits_the_offset(self):
        rc, _ = self.run_cli(None, "--calibrate", "explore", "50000", "--ref", "unit test")
        self.assertEqual(rc, 0)
        c = tomllib.loads(self.cfg.read_text())
        self.assertEqual(c["calibration"]["explore"]["measured"], 50000)
        rc, _ = self.run_cli()
        rep = json.loads((self.d / "r.json").read_text())
        self.assertEqual({m["mode"]: m["predicted"] for m in rep["modes"]}["explore_route1"], 50000)

    def green_cfg(self):
        """The fixture has known-red modes (the old pre-recovery numbers); the real toml has none. Dev has no known_red: make these modes meet a low floor."""
        t = self.cfg.read_text()
        for m in ("explore_biggest", "explore_route1", "battle_peak_wild", "battle_peak_trainer"):
            t = mb.toml_set(t, f"modes.{m}", "floor", "20000")
            t = mb.toml_set(t, f"modes.{m}", "known_red", "false")
        self.cfg.write_text(t)

    def run_dev(self, dev_sizes, *extra, sizes=None):
        """run_cli() with a DEV build (--dev-sizes-json / --dev-fal-json) next to the release one."""
        (self.d / "dev_sizes.json").write_text(json.dumps(dev_sizes))
        (self.d / "dev_fal.json").write_text(json.dumps(FALS))
        return self.run_cli(sizes, "--dev-sizes-json", str(self.d / "dev_sizes.json"), "--dev-fal-json", str(self.d / "dev_fal.json"), *extra)

    def test_dev_build_is_only_checked_against_the_floors(self):
        self.green_cfg()
        # a dev build 1.6 KB bigger than the release: ratchets do not apply to dev, the floors do
        rel = dict(SIZES, resident=SIZES["resident"] - 1500)
        rc, out = self.run_dev(dict(SIZES, resident=SIZES["resident"] + 100), sizes=rel)
        self.assertEqual(rc, 0, out)                     
        self.assertIn("DEV build", out)
        rep = json.loads((self.d / "r.json").read_text())
        self.assertEqual(rep["status"], "WARN")
        self.assertTrue(all(r["status"] == "OK" for r in rep["dev"]["modes"] if r["mode"] in ("menu_open", "shop_open", "ui_open", "pc_open")))
        # far over the resident ratchet: still no static FAIL for dev
        rc, out = self.run_dev(dict(SIZES, resident=SIZES["resident"] + 1000), sizes=rel)
        self.assertNotIn("grew:", out)

    def test_dev_build_under_a_floor_fails(self):
        self.green_cfg()
        rel = dict(SIZES, ui=3000)
        rc, out = self.run_dev(dict(SIZES, ui=SIZES["ui"] + 20000), sizes=rel)      # the dev UI plugin is huge: ui_open falls under 30 KB
        self.assertEqual(rc, 1, out)
        self.assertIn("DEV build", out)
        rep = json.loads((self.d / "r.json").read_text())
        self.assertEqual({r["mode"]: r["status"] for r in rep["dev"]["modes"]}["ui_open"], "FAIL")
        self.assertEqual({r["mode"]: r["status"] for r in rep["modes"]}["ui_open"], "OK")   # the release gate itself is fine
        # known_red does not apply to dev: a known-red release mode (tolerated at its baseline) is a FAIL in the dev build
        self.cfg.write_text(CFG)
        rc, out = self.run_dev(dict(SIZES))
        self.assertEqual(rc, 1, out)

    def test_no_dev_prints_a_warning(self):
        rc, out = self.run_cli()
        self.assertIn("DEV build not checked", out)

    def test_rebase_sets_ratchets_and_baselines_and_logs(self):
        rc, out = self.run_cli(None, "--rebase")
        self.assertEqual(rc, 2, out)                        # needs a reason
        self.green_cfg()
        rel = dict(SIZES, resident=SIZES["resident"] - 1500, battle=SIZES["battle"] - 1000)
        rc, out = self.run_cli(rel, "--rebase", "--reason", "release build: NUCLEO_DEBUG off")
        self.assertEqual(rc, 0, out)
        c = tomllib.loads(self.cfg.read_text())
        self.assertEqual(c["modules"]["resident"]["ratchet"], rel["resident"] + 256)
        self.assertEqual(c["modules"]["battle"]["ratchet"], rel["battle"] + 256)
        self.assertEqual(c["modules"]["menu"]["ratchet"], SIZES["menu"] + 256)      # raised too: rebase goes both ways
        log = c["raise_log"]
        self.assertTrue(all(e["reason"] == "release build: NUCLEO_DEBUG off" for e in log))
        self.assertIn(("ratchet", "resident"), {(e["kind"], e["name"]) for e in log})
        self.assertIn(("baseline", "battle_load"), {(e["kind"], e["name"]) for e in log})
        rc, out = self.run_cli(rel)                         # the rebased numbers are the new baseline: no FAIL, no eroded-margin WARN rows
        self.assertEqual(rc, 0, out)
        rep = json.loads((self.d / "r.json").read_text())
        self.assertTrue(all(r["status"] == "OK" for r in rep["modes"]), out)

    def test_missing_inputs_exit_2(self):
        rc, out = mb.main(["--config", str(self.cfg), "--elf-dir", str(self.d / "nothing"), "--host-json", str(self.d / "host.json"),
                           "--maps-dir", str(self.maps)]), ""
        self.assertEqual(rc, 2)


if __name__ == "__main__":
    unittest.main(verbosity=1)
