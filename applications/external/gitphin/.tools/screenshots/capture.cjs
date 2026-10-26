// Screenshots of the Gitphin web page for the README. Run by capture.sh.
// Usage: node capture.cjs <base url> <out dir>
const { chromium } = require("playwright-core");
const fs = require("fs");

const [base, out] = process.argv.slice(2);
const exe = process.env.CHROMIUM ||
    ["/opt/pw-browsers/chromium-1194/chrome-linux/chrome"].find(p => fs.existsSync(p));

const shots = [
    ["repos", "#repos"],
    ["code", "#repos/weather-station"],
    ["highlight", "#repos/weather-station/blob/main/src/bme280.c"],
    ["commits", "#repos/weather-station/commits"],
    ["diff", "#repos/weather-station/commit/HEAD", "Show diff"],
    ["pull-request", "#repos/weather-station/pulls/3"],
    ["issues", "#repos/weather-station/issues"],
];

(async () => {
    const browser = await chromium.launch({ executablePath: exe });
    for (const scheme of ["light", "dark"]) {
        const page = await browser.newPage({ viewport: { width: 1280, height: 800 }, deviceScaleFactor: 1, colorScheme: scheme });
        page.on("pageerror", e => console.error("page error:", e.message));
        await page.goto(base + "/");
        await page.evaluate(() => localStorage.setItem("hub.author", "Mika Tanaka"));
        for (const [name, hash, click] of shots) {
            if (scheme === "dark" && name !== "code" && name !== "highlight") continue;
            await page.goto(base + "/" + hash);
            await page.waitForLoadState("networkidle");
            await page.waitForTimeout(800);
            if (click) {
                await page.getByText(click, { exact: true }).first().click();
                await page.waitForLoadState("networkidle");
                await page.waitForTimeout(500);
            }
            const file = `${out}/${name}${scheme === "dark" ? "-dark" : ""}.png`;
            await page.screenshot({ path: file });
            console.log(file);
        }
        await page.close();
    }
    await browser.close();
})();
