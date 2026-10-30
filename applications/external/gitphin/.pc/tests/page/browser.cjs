// Browser check of the page against page_server.py. Usage: node browser.cjs <base url> <sd dir> <log file>
const { chromium } = require("playwright-core");
const fs = require("fs"), assert = require("assert");
const [base, sd, reqlog] = process.argv.slice(2);
const exe = process.env.CHROMIUM || "/usr/bin/chromium";
let ok = 0, bad = 0;
const step = async (name, fn) => { try { await fn(); ok++; console.log("ok   " + name); } catch (e) { bad++; console.log("FAIL " + name + ": " + e.message.split("\n")[0]); } };
const settle = async p => { await p.waitForLoadState("networkidle"); await p.waitForTimeout(400); };
const png = async page => page.evaluate(async () => {   // a 300x300 test picture as PNG bytes
    const c = document.createElement("canvas"); c.width = c.height = 300; const x = c.getContext("2d");
    for (let i = 0; i < 300; i += 30) for (let j = 0; j < 300; j += 30) { x.fillStyle = (i + j) % 60 ? "#ff8200" : "#14110d"; x.fillRect(i, j, 30, 30); }
    const b = await new Promise(r => c.toBlob(r, "image/png")); return Array.from(new Uint8Array(await b.arrayBuffer()));
});

(async () => {
    const browser = await chromium.launch({ executablePath: exe, args: ["--no-sandbox"] });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 }, acceptDownloads: true });
    const page = await ctx.newPage();
    const errors = [];
    page.on("pageerror", e => errors.push("pageerror: " + e.message));
    page.on("console", m => { if (m.type() === "error" && !/status of 404|Failed to load resource/.test(m.text())) errors.push("console: " + m.text()); });
    const noErrors = () => assert.deepStrictEqual(errors.splice(0), []);

    await step("repos list: loads, no errors, repo rows", async () => {
        await page.goto(base + "/#repos"); await settle(page);
        assert((await page.locator("#repos-list .rrow").count()) >= 3);
        noErrors();
    });
    await step("repo page: Clone, Download ZIP and Avatar buttons", async () => {
        await page.goto(base + "/#repos/weather-station"); await settle(page);
        for (const id of ["rv-clone-btn", "rv-zip", "rv-avatar-btn"]) assert(await page.locator("#" + id).isVisible(), id);
        noErrors();
    });
    await step("code file: highlight spans, text unchanged", async () => {
        await page.goto(base + "/#repos/weather-station/blob/main/src/bme280.c"); await settle(page);
        const n = await page.locator(".codebox pre.txt span[class^='hl-']").count();
        assert(n > 5, "spans: " + n);
        const shown = await page.locator(".codebox pre.txt").evaluate(e => e.textContent);
        const raw = await page.evaluate(async () => { const r = await fetch("/api/git/blob?name=weather-station&ref=main&path=src%2Fbme280.c"); return (await r.text()).replace(/\n$/, ""); });
        assert.strictEqual(shown, raw);
        assert(await page.locator(".codebox pre.txt .hl-k").first().evaluate(e => getComputedStyle(e).color !== getComputedStyle(e.parentElement).color), "keyword color");
        noErrors();
    });
    await step("markdown file source view: still opens", async () => {
        await page.goto(base + "/#repos/weather-station/blob/main/docs/wiring.md"); await settle(page);
        assert(await page.locator(".md").count() > 0); noErrors();
    });
    await step("files tab: system folders have no Delete button", async () => {
        const del = async dir => {
            await page.goto(base + "/#files"); await page.evaluate(d => go(d), dir); await settle(page);
            return page.locator("#list tr").evaluateAll(rs => rs.map(r => ({ n: r.querySelector("td.name")?.textContent.trim(), d: !!r.querySelector("td.act button:last-child") && /Delete/.test(r.querySelector("td.act").textContent), note: r.querySelector(".sys")?.textContent })));
        };
        let rows = await del("/ext");
        assert(rows.some(r => /apps_data/.test(r.n) && !r.d && r.note), "apps_data row");
        rows = await del("/ext/apps_data");
        assert(rows.find(r => /gitphin/.test(r.n)).note === "Needed by Gitphin" && !rows.find(r => /gitphin/.test(r.n)).d);
        rows = await del("/ext/apps_data/gitphin");
        assert(rows.find(r => /git$/.test(r.n)).note === "Needed by Gitphin");
        rows = await del("/ext/apps_assets");
        assert(rows.find(r => /gitphin/.test(r.n)).note === "Needed by Gitphin");
        rows = await del("/ext/apps_data/gitphin/git");   // the repo folders themselves stay deletable here
        assert(rows.length > 0 && rows.every(r => r.d || !r.n.includes(".git")), JSON.stringify(rows));
        noErrors();
    });
    await step("delete guard: api() refuses a system folder, no request sent", async () => {
        await page.goto(base + "/#files"); await settle(page);
        const before = fs.readFileSync(reqlog, "utf8").split("\n").filter(l => l.includes("/api/delete")).length;
        const msg = await page.evaluate(async () => { try { await api("POST", "/api/delete?path=" + encodeURIComponent("/ext/apps_data/gitphin/")); return "no error"; } catch (e) { return e.message; } });
        assert.strictEqual(msg, "Needed by Gitphin");
        assert.strictEqual(fs.readFileSync(reqlog, "utf8").split("\n").filter(l => l.includes("/api/delete")).length, before);
        assert(await page.locator(".toast.err").count() > 0, "toast");
    });
    const file = sd + "/apps_data/gitphin/git/weather-station.git/hub/avatar.png";
    await step("avatar: upload shrinks to <=128 px and <16 KB, header and list show it", async () => {
        await page.goto(base + "/#repos/weather-station"); await settle(page);
        await page.click("#rv-avatar-btn");
        await page.setInputFiles("dialog.dlg input[type=file]", { name: "pic.png", mimeType: "image/png", buffer: Buffer.from(await png(page)) });
        await page.waitForSelector(".toast.ok"); await settle(page);
        const b = fs.readFileSync(file);
        assert(b.length > 0 && b.length <= 16 * 1024, "size " + b.length);
        const w = b.readUInt32BE(16), h = b.readUInt32BE(20);
        assert(w <= 128 && h <= 128 && w >= 64, w + "x" + h);
        assert.strictEqual(await page.locator("#rv-avatar img").count(), 1);
        await page.keyboard.press("Escape");
        await page.goto(base + "/#repos"); await settle(page); await page.waitForTimeout(800);
        const row = page.locator("#repos-list .rrow", { hasText: "weather-station" });
        assert.strictEqual(await row.locator(".av img").count(), 1, "list image");
        assert.strictEqual(await page.locator("#repos-list .rrow .av img").count(), 1, "other repos keep the icon");
        noErrors();
    });
    await step("avatar: survives a reload (read back with GET /api/file)", async () => {
        await page.reload(); await page.waitForTimeout(500); await settle(page); await page.waitForTimeout(800);
        assert.strictEqual(await page.locator("#repos-list .rrow .av img").count(), 1);
        const nat = await page.locator("#repos-list .rrow .av img").evaluate(i => i.naturalWidth);
        assert(nat > 0 && nat <= 128, "natural width " + nat);
    });
    await step("avatar: bad file gives an error toast and writes nothing", async () => {
        await page.goto(base + "/#repos/weather-station"); await settle(page);
        fs.unlinkSync(file);
        await page.click("#rv-avatar-btn");
        await page.setInputFiles("dialog.dlg input[type=file]", { name: "x.png", mimeType: "image/png", buffer: Buffer.from("not a picture") });
        await page.waitForSelector(".toast.err");
        assert(!fs.existsSync(file));
        await page.keyboard.press("Escape");
    });
    await step("avatar: remove deletes the file and restores the icon", async () => {
        await page.goto(base + "/#repos/weather-station"); await settle(page);
        await page.click("#rv-avatar-btn");
        await page.setInputFiles("dialog.dlg input[type=file]", { name: "pic.png", mimeType: "image/png", buffer: Buffer.from(await png(page)) });
        await page.waitForSelector(".toast.ok"); await page.waitForTimeout(300);
        assert(fs.existsSync(file));
        await page.locator("dialog.dlg button.danger", { hasText: "Remove" }).click();
        await page.locator("dialog.dlg button.danger", { hasText: /^Remove$/ }).last().click();   // confirm dialog
        await page.waitForTimeout(1200);
        assert(!fs.existsSync(file), "file still there");
        assert.strictEqual(await page.locator("#rv-avatar img").count(), 0);
        await page.keyboard.press("Escape");
        noErrors();
    });
    await step("Download ZIP: asks /pc/zip with name and ref, saves a file", async () => {
        await page.goto(base + "/#repos/weather-station"); await settle(page);
        const [dl] = await Promise.all([page.waitForEvent("download"), page.click("#rv-zip")]);
        assert.strictEqual(dl.suggestedFilename(), "weather-station-main.zip");
        assert(fs.readFileSync(reqlog, "utf8").includes("GET /pc/zip?name=weather-station&ref=main"));
    });
    await step("Download ZIP: a failing answer shows a toast", async () => {
        await page.route("**/pc/zip*", r => r.fulfill({ status: 500, contentType: "application/json", body: '{"ok":false,"error":"zip failed"}' }));
        await page.click("#rv-zip"); await page.waitForSelector(".toast.err:has-text('zip failed')");
        const tx = await page.locator(".toast.err").allTextContents(); assert(tx.some(x => /ZIP: zip failed/.test(x)), JSON.stringify(tx));
        await page.unroute("**/pc/zip*");
    });
    await step("dark theme: highlight colors switch", async () => {
        await page.goto(base + "/?theme=dark#repos/weather-station/blob/main/src/bme280.c"); await settle(page);
        const c = await page.locator(".codebox pre.txt .hl-k").first().evaluate(e => getComputedStyle(e).color);
        assert.strictEqual(c, "rgb(255, 154, 46)"); noErrors();
    });
    await browser.close();
    console.log(`browser checks: ${ok} ok, ${bad} failed`);
    process.exit(bad ? 1 : 0);
})();
