#!/usr/bin/env python3
"""Host orchestrator: manifest.toml -> Blender Workbench renders at final pixel size ->
4-level quantisation -> assets/art/objects/<name>.lv.png + <name>.json {w,h,ax,ay}.

  python3 tools/render_objects.py <manifest.toml> [--only name]
  python3 tools/render_objects.py --selftest

Anchor convention (see tools/blender_render_obj.py): the world point (fw,-fh,0) lands on a
pixel CORNER of the render.  ax,ay are the integer coordinates (inside the cropped sprite's
coordinate frame) of that corner, i.e. the pixel whose top-left corner is the anchor point.
For an object whose lowest geometry is that vertex, ay == h (one past the last row): blit at
top-left = anchor_point - (ax, ay) and the geometry is exact (continuous model: tile diamond
centred on the corner (cx,cy), anchor point = corner (cx, cy+7)).
"""
import argparse, json, os, subprocess, sys, tempfile
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLENDER = "/Applications/Blender.app/Contents/MacOS/Blender"
BLENDER_SCRIPT = os.path.join(ROOT, "tools", "blender_render_obj.py")
GRAY = [255, 170, 85, 0]


def rp(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


# ---------------------------------------------------------------- quantise
def quantize(im, t0=0.78, t1=0.50, t2=0.26):
    """RGBA image -> (levels[y][x] 0..3, mask[y][x] bool)."""
    im = im.convert("RGBA")
    W, H = im.size
    px = im.load()
    lv = [[0] * W for _ in range(H)]
    mask = [[False] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            if a < 128:
                continue
            mask[y][x] = True
            L = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0
            lv[y][x] = 0 if L >= t0 else 1 if L >= t1 else 2 if L >= t2 else 3
    for y in range(H):
        for x in range(W):
            if not mask[y][x]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < W and 0 <= ny < H) or not mask[ny][nx]:
                    lv[y][x] = 3
                    break
    return lv, mask


def crop_and_write(res, job, out_dir):
    """Quantise raw render, crop to the opaque bbox, write .lv.png + .json. Returns dict."""
    im = Image.open(res["raw"]).convert("RGBA")
    lv, mask = quantize(im, job.get("t0", 0.78), job.get("t1", 0.50), job.get("t2", 0.26))
    H, W = len(lv), len(lv[0])
    ys = [y for y in range(H) if any(mask[y])]
    xs = [x for x in range(W) if any(mask[y][x] for y in range(H))]
    if not ys:
        raise RuntimeError("empty render for " + job["name"])
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    w, h = x1 - x0 + 1, y1 - y0 + 1
    out = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    op = out.load()
    for y in range(h):
        for x in range(w):
            if mask[y0 + y][x0 + x]:
                g = GRAY[lv[y0 + y][x0 + x]]
                op[x, y] = (g, g, g, 255)
    meta = {"w": w, "h": h, "ax": res["ACX"] - x0, "ay": res["ACY"] - y0}
    info = dict(meta, levels=lv, mask=mask, crop=(x0, y0), canvas=(W, H), image=out)
    name = job["name"]
    os.makedirs(out_dir, exist_ok=True)
    if os.path.exists(os.path.join(out_dir, name + ".locked")):
        print("  [%s] .locked present -> not written" % name)
    else:
        out.save(os.path.join(out_dir, name + ".lv.png"))
        with open(os.path.join(out_dir, name + ".json"), "w") as f:
            json.dump(meta, f)
    return info


# ---------------------------------------------------------------- blender
def run_blender(jobs):
    for j in jobs:
        j["raw_path"] = os.path.join(ROOT, "build", "art", "raw", j["name"] + ".png")
        os.makedirs(os.path.dirname(j["raw_path"]), exist_ok=True)
    fd, spec = tempfile.mkstemp(suffix=".json", prefix="render_jobs_")
    os.close(fd)
    rfd, resf = tempfile.mkstemp(suffix=".json", prefix="render_res_")
    os.close(rfd)
    with open(spec, "w") as f:
        json.dump({"jobs": jobs, "results": resf}, f)
    cmd = [BLENDER, "-b", "--disable-autoexec", "-P", BLENDER_SCRIPT, "--", spec]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        sys.stderr.write(p.stdout[-3000:] + p.stderr[-3000:])
        raise SystemExit("Blender failed (rc=%d)" % p.returncode)
    try:
        results = json.load(open(resf))
    finally:
        os.remove(spec)
        os.remove(resf)
    for r in results:
        if "error" in r:
            sys.stderr.write(p.stdout[-3000:] + p.stderr[-3000:])
            raise SystemExit("job %s failed: %s" % (r["name"], r["error"]))
    return {r["name"]: r for r in results}


def load_manifest(path):
    import tomllib
    with open(path, "rb") as f:
        data = tomllib.load(f)
    jobs = []
    for j in data.get("job", []):
        j = dict(j)
        j.setdefault("rot_z", 0)
        j.setdefault("fit", "footprint")
        j.setdefault("fw", 1)
        j.setdefault("fh", 1)
        j.setdefault("cavity", False)
        j.setdefault("out_dir", "assets/art/objects")
        m = j["model"]
        if not m.startswith("procedural:"):
            j["model"] = rp(m)
        jobs.append(j)
    return jobs


def process(jobs):
    res = run_blender(jobs)
    infos = {}
    for j in jobs:
        info = crop_and_write(res[j["name"]], j, rp(j["out_dir"]))
        infos[j["name"]] = info
        print("  %-14s %3dx%-3d ax=%d ay=%d  (canvas %dx%d, anchor corner %d,%d)" % (
            j["name"], info["w"], info["h"], info["ax"], info["ay"],
            res[j["name"]]["W"], res[j["name"]]["H"], res[j["name"]]["ACX"], res[j["name"]]["ACY"]))
    return infos


# ---------------------------------------------------------------- selftest
LCD_PAPER = (226, 233, 196)
LCD_INK = (28, 36, 22)


def lcd(level):
    t = [0.0, 0.33, 0.66, 1.0][level]
    return tuple(int(LCD_PAPER[k] + (LCD_INK[k] - LCD_PAPER[k]) * t) for k in range(3))


def in_footprint(x, y, Ox, Oy, fw, fh):
    """Pixel (x,y) centre inside the footprint parallelogram whose top lattice point is (Ox,Oy)."""
    u = (x + 0.5 - Ox) / 14.0
    v = (y + 0.5 - Oy) / 7.0
    i, j = (u + v) / 2.0, (v - u) / 2.0
    return 0 < i < fw and 0 < j < fh


def selftest():
    boxes = [("t_box_1x1x0.2", 1, 1, 0.2), ("t_box_2x1x1", 2, 1, 1.0), ("t_box_2x2x1.5", 2, 2, 1.5)]
    jobs = [{"name": n, "model": "procedural:box:%g,%g,%g" % (fw, fh, h), "rot_z": 0, "fit": "none",
             "fw": fw, "fh": fh, "cavity": False} for n, fw, fh, h in boxes]
    infos = process_selftest(jobs)
    PW, PH = 150, 128
    panel_cols = len(boxes)
    rgb = Image.new("RGB", (PW * panel_cols, PH), lcd(0))
    pp = rgb.load()
    fails = 0

    def check(name, cond, msg=""):
        nonlocal fails
        print("%s  %s%s" % ("PASS" if cond else "FAIL", name, (" -- " + msg) if msg else ""))
        if not cond:
            fails += 1

    marks = []
    for k, (n, fw, fh, bh) in enumerate(boxes):
        info = infos[n]
        px0 = PW * k
        Ox, Oy = px0 + 75, 34                    # lattice point (i=0,j=0) of this panel
        Ax, Ay = Ox + 14 * (fw - fh), Oy + 7 * (fw + fh)   # anchor corner = lattice (fw,fh)
        # ground: 28x14 diamonds, tile (i,j) centre = lattice (i+.5, j+.5)
        for i in range(-2, fw + 2):
            for j in range(-2, fh + 2):
                cx, cy = Ox + 14 * (i - j), Oy + 7 * (i + j) + 7
                fp = 0 <= i < fw and 0 <= j < fh
                dia = {}
                for y in range(cy - 7, cy + 7):
                    for x in range(cx - 14, cx + 14):
                        if abs(x + 0.5 - cx) / 14.0 + abs(y + 0.5 - cy) / 7.0 <= 1.0:
                            dia[(x, y)] = True
                for (x, y) in dia:
                    edge = any((x + dx, y + dy) not in dia for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                    if 0 <= x - px0 < PW and 0 <= y < PH:
                        pp[x, y] = lcd(2) if edge else (lcd(1) if fp else lcd(0))
        # sprite
        spr, ax, ay = info["image"], info["ax"], info["ay"]
        sx, sy = Ax - ax, Ay - ay
        sp = spr.load()
        opaque = {}
        for y in range(info["h"]):
            for x in range(info["w"]):
                if sp[x, y][3]:
                    lvl = info["levels"][info["crop"][1] + y][info["crop"][0] + x]
                    pp[sx + x, sy + y] = lcd(lvl)
                    opaque[(sx + x, sy + y)] = True
        # ---- checks (screen coords of the panel)
        exp = {}
        for y in range(PH):
            for x in range(px0, px0 + PW):
                if in_footprint(x, y, Ox, Oy, fw, fh):
                    exp.setdefault(x, []).append(y)
        exp_bot = {x: max(v) for x, v in exp.items()}
        act_bot = {}
        for (x, y) in opaque:
            act_bot[x] = max(act_bot.get(x, -1), y)
        mism = [x for x in exp_bot if act_bot.get(x) != exp_bot[x]]
        check("%s lower silhouette == footprint base on all %d columns" % (n, len(exp_bot)),
              not mism, "mismatched cols %s" % mism[:6])
        xl, xr = Ox - 14 * fh, Ox + 14 * fw - 1     # vertical box edges sit on the left/right base vertices
        got_l, got_r = min(x for x, _ in opaque), max(x for x, _ in opaque)
        check("%s left/right extent x=%d..%d" % (n, xl - px0, xr - px0),
              got_l == xl and got_r == xr, "got %d..%d" % (got_l - px0, got_r - px0))
        check("%s bottom vertex pixels (%d,%d),(%d,%d) opaque, row below empty" % (
              n, Ax - 1 - px0, Ay - 1, Ax - px0, Ay - 1),
              (Ax - 1, Ay - 1) in opaque and (Ax, Ay - 1) in opaque and
              (Ax - 1, Ay) not in opaque and (Ax, Ay) not in opaque)
        left_run = [act_bot[x] for x in range(min(exp_bot), Ax)]
        steps = [left_run[i + 1] - left_run[i] for i in range(len(left_run) - 1)]
        right_run = [act_bot[x] for x in range(Ax, max(exp_bot) + 1)]
        rsteps = [right_run[i + 1] - right_run[i] for i in range(len(right_run) - 1)]
        ok_edges = (all(s in (0, 1) for s in steps) and all(s in (0, -1) for s in rsteps) and
                    left_run[-1] - left_run[0] == (len(left_run) - 1) // 2 and
                    sum(1 for s in steps if s == 1) == len(steps) // 2 and
                    sum(1 for s in rsteps if s == -1) == len(rsteps) // 2)
        check("%s edges step 2:1 (1 row per 2 columns)" % n, ok_edges)
        # top: highest geometry point = back-top vertex of the box
        import math
        top_exp = Oy - bh * 28 / math.sqrt(2) * math.sin(math.radians(60))
        top_act = min(y for _, y in opaque)
        check("%s top row ~ %.1f (+-1)" % (n, top_exp), abs(top_act - top_exp) <= 1.0 + 1e-6,
              "got %d" % top_act)
        # tones: more than one level present (face shading visible)
        lv_used = {info["levels"][info["crop"][1] + y][info["crop"][0] + x]
                   for y in range(info["h"]) for x in range(info["w"]) if sp[x, y][3]}
        check("%s uses >=2 tone levels %s" % (n, sorted(lv_used)), len(lv_used) >= 2)
        marks += [(Ax, Ay), (Ox, Oy), (Ox + 14 * fw, Oy + 7 * fw), (Ox - 14 * fh, Oy + 7 * fh)]
    big = rgb.resize((rgb.width * 8, rgb.height * 8), Image.NEAREST)
    bp = big.load()
    for (mx, my) in marks:      # red marks at expected lattice corners (pixel corners)
        for d in range(-6, 7):
            for t in (0, 1):
                for (X, Y) in ((mx * 8 + d, my * 8 + t - 1), (mx * 8 + t - 1, my * 8 + d)):
                    if 0 <= X < big.width and 0 <= Y < big.height:
                        bp[X, Y] = (220, 30, 30)
    out = os.path.join(ROOT, "build", "art", "selftest.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    big.save(out)
    print("wrote", out)
    print("SELFTEST", "PASS" if fails == 0 else "FAIL (%d)" % fails)
    return 0 if fails == 0 else 1


def process_selftest(jobs):
    for j in jobs:
        j["out_dir"] = "build/art/selftest"
    return process(jobs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest", nargs="?")
    ap.add_argument("--only")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(selftest())
    if not a.manifest:
        ap.error("manifest required")
    jobs = load_manifest(rp(a.manifest))
    if a.only:
        jobs = [j for j in jobs if j["name"] == a.only]
    if not jobs:
        raise SystemExit("no jobs")
    process(jobs)


if __name__ == "__main__":
    main()
