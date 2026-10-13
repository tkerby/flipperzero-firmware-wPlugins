// Frames of the promo video, drawn from real pictures: the web UI (docs/screenshots, made by
// tools/screenshots/capture.sh from the app's route code), the Flipper screen
// (flipper_app/screenshots, captures from the device) and the output of a real plain-git push
// (push.txt, see make_promo.sh). Run by make_promo.sh.
// Usage: node promo.cjs <out dir>
const { chromium } = require("playwright-core");
const fs = require("fs");
const path = require("path");

const outDir = process.argv[2];
const TOP = path.join(__dirname, "../..");
const exe = process.env.CHROMIUM ||
    ["/opt/pw-browsers/chromium-1194/chrome-linux/chrome", "/opt/pw-browsers/chromium"].find(p => fs.existsSync(p));
const FPS = 30, LENGTH = 44;
const uri = p => "data:image/png;base64," + fs.readFileSync(path.join(TOP, p)).toString("base64");
const logo = "data:image/svg+xml;base64," + fs.readFileSync(path.join(TOP, "docs/gitphin_logo_orange.svg")).toString("base64");

const IMG = {
    ready: uri("flipper_app/screenshots/ss0.png"), pushing: uri("flipper_app/screenshots/ss1.png"),
    droid: uri("tools/promo/droid.png"),
};
for (const n of ["repos", "highlight", "commits", "diff", "pull-request", "highlight-dark"]) IMG[n] = uri(`docs/screenshots/${n}.png`);
const push = fs.readFileSync(path.join(__dirname, "push.txt"), "utf8").split("\n").filter(l => l.trim());

const html = `<!doctype html><meta charset="utf-8"><style>
*{box-sizing:border-box}
html,body{margin:0;width:1280px;height:720px;overflow:hidden;background:#16130f;
  font-family:"Inter","DejaVu Sans",system-ui,sans-serif;color:#fff3e0}
.scene{position:absolute;inset:0;opacity:0}
.bg{position:absolute;inset:0;background:radial-gradient(circle at 70% 25%,#3a2a12 0,#16130f 62%)}
.cap{position:absolute;left:56px;top:40px;font-size:34px;font-weight:700}
.cap b{color:#ff8200}
.sub{position:absolute;left:58px;top:88px;font-size:19px;color:#d8c2a0}
.term{position:absolute;left:56px;top:150px;width:640px;height:470px;border-radius:12px;overflow:hidden;
  background:#0d0c0b;border:2px solid #3b342c;box-shadow:0 24px 70px rgba(0,0,0,.6)}
.term .bar,.win .bar{height:30px;display:flex;align-items:center;gap:8px;padding:0 12px;background:#24201b}
.dot{width:11px;height:11px;border-radius:50%;background:#5a5148}
.bar span{margin-left:10px;color:#a8957a;font:13px monospace}
.term pre{margin:0;padding:16px 18px;font:15.5px/1.55 "DejaVu Sans Mono",monospace;white-space:pre-wrap;color:#e8dcc8}
.term .p{color:#ff8200}.term .ok{color:#7bd88f}
.cursor{display:inline-block;width:9px;height:18px;background:#ff8200;vertical-align:-3px}
.flipper{position:absolute;left:745px;top:200px;width:480px;height:250px;border-radius:44px 44px 70px 70px;
  background:linear-gradient(160deg,#f4f1ea,#d9d4ca);box-shadow:0 30px 80px rgba(0,0,0,.7),inset 0 -6px 0 rgba(0,0,0,.08)}
.flipper .scr{position:absolute;left:34px;top:40px;width:270px;height:135px;border-radius:6px;overflow:hidden;
  background:#ff8200;border:6px solid #222}
.flipper .scr img{position:absolute;inset:0;width:100%;height:100%;image-rendering:pixelated}
.dpad{position:absolute;left:338px;top:58px;width:110px;height:110px;border-radius:50%;background:#2a2622}
.dpad i{position:absolute;left:35px;top:35px;width:40px;height:40px;border-radius:50%;background:#ff8200}
.usb{position:absolute;left:1180px;top:308px;width:110px;height:14px;background:#2a2622;border-radius:4px}
.win{position:absolute;left:150px;top:132px;width:980px;border-radius:12px;overflow:hidden;
  border:2px solid #ff8200;box-shadow:0 24px 80px rgba(0,0,0,.65);background:#16130f}
.win .view{position:relative;height:612px;overflow:hidden}
.win .view img{position:absolute;left:0;top:0;width:100%;transform-origin:30% 30%}
.chip{position:absolute;padding:8px 16px;border-radius:999px;background:#ff8200;color:#16130f;
  font-weight:700;font-size:19px;box-shadow:0 8px 24px rgba(0,0,0,.5);white-space:nowrap}
.feat{position:absolute;left:700px;top:150px;font-size:23px;line-height:1.95}
.feat small{color:#d8c2a0;font-size:16px;margin-left:8px}
.center{position:absolute;left:0;right:0;text-align:center}
.note{position:absolute;left:0;right:0;bottom:14px;text-align:center;font-size:13px;color:#8c7a62}
</style>
<div class="bg"></div>

<div class="scene" id="s1">
  <img id="logo" src="${logo}" style="position:absolute;left:560px;top:150px;width:160px">
  <div class="center" style="top:330px;font-size:64px;font-weight:800;color:#ff8200">Gitphin 1.2</div>
  <div class="center" style="top:420px;font-size:30px;font-weight:600">Git hosting on your Flipper Zero</div>
  <div class="center" style="top:475px;font-size:20px;color:#d8c2a0">A Git server with a web UI that runs on the Flipper itself · plain git over USB · no cloud</div>
</div>

<div class="scene" id="s2">
  <div class="cap">① <b>git push</b> to your Flipper</div>
  <div class="sub">plain git over the USB cable · no helper, no plugin</div>
  <div class="term"><div class="bar"><i class="dot"></i><i class="dot"></i><i class="dot"></i><span>~/weather-station</span></div><pre id="tt"></pre></div>
  <div class="usb"></div>
  <div class="flipper"><div class="scr"><img id="f0" src="${IMG.ready}"><img id="f1" src="${IMG.pushing}"></div><div class="dpad"><i></i></div></div>
  <div class="chip" id="c2" style="left:56px;top:640px">weather-station is on the Flipper ✓</div>
  <div class="note">Flipper screens: captures from the device. Push output: a real push to the same Gitphin core.</div>
</div>

<div class="scene" id="s3">
  <div class="cap">② Browse it at <b>localhost:8080</b></div>
  <div class="win"><div class="bar"><i class="dot"></i><i class="dot"></i><i class="dot"></i><span id="url"></span></div><div class="view" id="wv"></div></div>
  <div class="chip" id="c3" style="left:58px;top:86px"></div>
</div>

<div class="scene" id="s4">
  <div class="cap">③ New in <b>1.2</b></div>
  <div class="win" style="left:56px;top:150px;width:600px"><div class="bar"><i class="dot"></i><i class="dot"></i><i class="dot"></i></div>
    <div class="view" style="height:375px"><img src="${IMG["highlight-dark"]}"></div></div>
  <div class="feat" id="ft">
    <div>🎨 Syntax highlighting <small>14 languages, no libraries</small></div>
    <div>🖼️ Repo avatars <small>in the list and the header</small></div>
    <div>📦 Download ZIP <small>any branch, tag or commit</small></div>
    <div>🛡️ Protected system folders</div>
    <div>💾 Backup on plug-in <small>~/GitphinBackup</small></div>
    <div>🔁 Push to GitHub and Flipper at once</div>
    <div>🔔 Webhooks <small>HMAC-SHA256 signed</small></div>
  </div>
</div>

<div class="scene" id="s5">
  <div class="cap">④ Now also on <b>Android</b></div>
  <div class="sub">Gitphin Droid: the same core and web UI on your phone · USB, tethering or Wi-Fi</div>
  <img id="dr" src="${IMG.droid}" style="position:absolute;left:190px;top:150px;width:900px;border-radius:16px">
</div>

<div class="scene" id="s6">
  <img src="${logo}" style="position:absolute;left:575px;top:110px;width:130px">
  <div class="center" style="top:270px;font-size:56px;font-weight:800;color:#ff8200">Gitphin 1.2</div>
  <div class="center" style="top:350px;font-size:26px">Free and open source · MIT · official Flipper firmware</div>
  <div class="center" style="top:400px;font-size:22px;color:#d8c2a0">Plain git · offline · your repos stay normal bare repos on the SD card</div>
  <div class="center" style="top:470px;font-size:30px;font-weight:700;color:#ffd9a8">github.com/Adhamura/gitphin</div>
  <div class="center" style="top:520px;font-size:20px;color:#d8c2a0">Android: github.com/Adhamura/gitphinDroid</div>
  <div class="note">Demo data. The web UI is rendered by the app's route code on a PC.</div>
</div>`;

const script = ({ IMG, push }) => {
    const $ = id => document.getElementById(id);
    const cl = (x, a, b) => Math.max(a, Math.min(b, x));
    const ease = x => x < 0 ? 0 : x > 1 ? 1 : x * x * (3 - 2 * x);
    const win = (t, a, b, f = 0.5) => ease((t - a) / f) * (1 - ease((t - b + f) / f));
    const scenes = [["s1", 0, 3.6], ["s2", 3.4, 14.2], ["s3", 14, 24.2], ["s4", 24, 32.2], ["s5", 32, 38.2], ["s6", 38, 44.5]];
    const wv = $("wv"), pages = [["repos", "Repos with avatars (new)", "#repos"],
        ["highlight", "Syntax highlighting (new)", "#repos/weather-station/blob/main/src/bme280.c"],
        ["commits", "Commits", "#repos/weather-station/commits"], ["diff", "Diffs", "#repos/weather-station/commit/HEAD"],
        ["pull-request", "Pull requests with merge on the device", "#repos/weather-station/pulls/3"]];
    pages.forEach(p => { const im = document.createElement("img"); im.src = IMG[p[0]]; wv.append(im); });
    const cmds = [["git remote add flipper http://127.0.0.1:8080/git/weather-station.git", 4.0, 6.0], ["git push flipper main", 6.3, 7.1]];
    const outStart = 7.3, outEnd = 10.4;

    window.render = t => {
        for (const [id, a, b] of scenes) $(id).style.opacity = win(t, a, b, 0.4);
        const k = ease((t - 0.1) / 0.9);
        $("logo").style.transform = `scale(${0.6 + 0.4 * k}) rotate(${(1 - k) * -12}deg)`;
        // 2. terminal + Flipper
        let txt = "";
        for (const [c, a, b] of cmds) {
            if (t < a) break;
            const n = Math.round(cl((t - a) / (b - a), 0, 1) * c.length);
            txt += `<span class="p">$</span> ${c.slice(0, n)}${t < b + 0.15 ? '<i class="cursor"></i>' : ""}\n`;
        }
        if (t >= outStart) {
            const n = Math.floor(cl((t - outStart) / (outEnd - outStart), 0, 1) * push.length);
            txt += push.slice(0, n).map(l => l.includes("->") ? `<span class="ok">${l}</span>` : l).join("\n") + "\n";
        }
        if (t >= outEnd + 0.4) txt += '<span class="p">$</span> <i class="cursor"></i>';
        $("tt").innerHTML = txt;
        const busy = t >= outStart + 0.6 && t < outEnd;
        $("f0").style.opacity = busy ? 0 : 1;
        $("f1").style.opacity = busy ? 1 : 0;
        $("c2").style.opacity = ease((t - 11) / 0.4);
        $("c2").style.transform = `translateY(${(1 - ease((t - 11) / 0.4)) * 20}px)`;
        // 3. browser pages
        const pi = cl(Math.floor((t - 14.2) / 2), 0, pages.length - 1);
        [...wv.children].forEach((im, i) => {
            const a = 14.2 + i * 2;
            im.style.opacity = i === 0 ? (t < a + 2 ? 1 : 1 - ease((t - a - 2) / 0.35)) : ease((t - a) / 0.35) * (i === pages.length - 1 ? 1 : 1 - ease((t - a - 2) / 0.35));
            im.style.transform = `scale(${1 + 0.035 * cl((t - a) / 2.4, 0, 1)})`;
        });
        $("c3").textContent = pages[pi][1];
        $("url").textContent = "http://localhost:8080/" + pages[pi][2];
        // 4. features one by one
        [...$("ft").children].forEach((d, i) => {
            const v = ease((t - 24.5 - i * 0.7) / 0.4);
            d.style.opacity = v;
            d.style.transform = `translateX(${(1 - v) * 30}px)`;
        });
        // 5. Droid
        const d = ease((t - 32.3) / 0.8);
        $("dr").style.transform = `translateY(${(1 - d) * 40}px) scale(${0.97 + 0.03 * d})`;
    };
};

(async () => {
    fs.mkdirSync(outDir, { recursive: true });
    const browser = await chromium.launch({ executablePath: exe });
    const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
    await page.setContent(html);
    await page.evaluate(script, { IMG, push });
    await page.evaluate(() => Promise.all([...document.images].map(i => i.decode().catch(() => {}))));
    const n = LENGTH * FPS;
    for (let f = 0; f < n; f++) {
        await page.evaluate(t => window.render(t), f / FPS);
        await page.screenshot({ path: path.join(outDir, String(f).padStart(5, "0") + ".png") });
        if (f % 300 === 0) console.log(`frame ${f}/${n}`);
    }
    await browser.close();
})();
