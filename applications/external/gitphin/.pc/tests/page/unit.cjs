// Unit tests of the pure page functions: the highlighter (HL-BEGIN..HL-END blocks) and the
// Files guard (GF-BEGIN..GF-END). Usage: node unit.cjs <web dir>
const fs = require("fs"), path = require("path"), assert = require("assert");
const web = process.argv[2];
const rd = f => fs.readFileSync(path.join(web, f), "utf8");
const blocks = (src, tag) => [...src.matchAll(new RegExp("/\\*" + tag + "-BEGIN\\*/([\\s\\S]*?)/\\*" + tag + "-END\\*/", "g"))].map(m => m[1]);

const HLX = {};
for (const f of ["js/code/hl_langs.js", "js/code/hl_lines.js"]) {
    const b = blocks(rd(f), "HL"); assert.strictEqual(b.length, 1, f);
    new Function("HLX", b[0])(HLX);
}
const gb = blocks(rd("js/files/guard.js"), "GF"); assert.strictEqual(gb.length, 1);
const G = new Function(gb[0] + "\nreturn {gfNorm, gfApp, gfLocked, gfDeletePath};")();
const css = rd("css/highlight.css");

let n = 0;
const t = (name, fn) => { try { fn(); n++; } catch (e) { console.error("FAIL " + name + ": " + e.message); process.exitCode = 1; } };
t("gf", () => {
    for (const x of ["/ext/apps_data/gitphin", "/ext/apps_data/gitphin/", "/ext/apps_data/gitphin/git", "/ext/apps_assets/gitphin",
        "/ext//apps_data/gitphin/x/..", "/ext/foo/../apps_data/gitphin", "/ext/apps_data", "/ext/apps_assets", "/ext"]) assert(G.gfLocked(x), x);
    assert(!G.gfLocked("/ext/apps_data/gitphin/git/repo.git"));
    assert(!G.gfLocked("/ext/apps_data/other"));
    assert(!G.gfLocked("/ext/music"));
    assert(G.gfApp("/ext/apps_data/gitphin/git") && !G.gfApp("/ext/apps_data"));
    assert.strictEqual(G.gfDeletePath("/api/delete?path=%2Fext%2Fapps_data"), "/ext/apps_data");
    assert.strictEqual(G.gfDeletePath("/api/delete?x=1&path=%2Fext%2Fa+b"), "/ext/a b");
    assert.strictEqual(G.gfDeletePath("/api/list?path=%2Fext"), null);
});
const H = (lang, lines) => HLX.lines(lang, lines);
const plain = h => h.replace(/<[^>]*>/g, "").replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, '"').replace(/&amp;/g, "&");
t("detect", () => {
    const d = HLX.detect;
    assert.strictEqual(d("src/a.c"), "c"); assert.strictEqual(d("A.KT"), "java"); assert.strictEqual(d("x/Makefile"), "make");
    assert.strictEqual(d("run", "#!/usr/bin/env bash"), "sh"); assert.strictEqual(d("t", "#!/usr/bin/python3"), "py");
    assert.strictEqual(d("a.diff"), "diff"); assert.strictEqual(d("a.unknownext"), ""); assert.strictEqual(d("README.md"), "md");
});
t("c", () => {
    const r = H("c", ["#include <stdio.h>", "int x = 42; // hi", 'char* s = "a\\"b";']);
    assert(r[0].includes('hl-k">#include')); assert(r[0].includes('hl-s">&lt;stdio.h&gt;'));
    assert(r[1].includes('hl-t">int')); assert(r[1].includes('hl-n">42')); assert(r[1].includes('hl-c">// hi'));
    assert(r[2].includes('hl-s">&quot;a\\&quot;b&quot;'));
});
t("multiline", () => {
    const r = H("c", ["/* a", "int b", "c */ int d"]);
    assert(r[1].includes("hl-c") && !r[1].includes("hl-t"));
    assert(r[2].includes('hl-c">c */') && r[2].includes('hl-t">int'));
    const p = H("py", ['x = """a', "def b", 'c""" + 1']);
    assert(p[1].includes("hl-s") && !p[1].includes("hl-k")); assert(p[2].includes("hl-n"));
    const j = H("js", ["const a = `x", "${y}", "z`; let b"]);
    assert(j[1].includes("hl-s") && j[2].includes('hl-k">let'));
});
t("langs", () => {
    assert(H("json", ['{"a": [1, true, null]}'])[0].includes('hl-t">&quot;a&quot;'));
    assert(H("json", ['{"a": [1, true, null]}'])[0].includes('hl-k">true'));
    assert(H("sh", ["echo $HOME # c"])[0].includes('hl-t">$HOME') && H("sh", ["echo $HOME # c"])[0].includes('hl-c"># c'));
    assert(H("sh", ["echo ${x#y}"])[0].indexOf("hl-c") < 0);
    assert(H("py", ["def f(): return None"])[0].includes('hl-k">def'));
    assert(H("go", ["func main() {}"])[0].includes('hl-k">func'));
    assert(H("rust", ["let x: u32 = 'a';"])[0].includes('hl-t">u32'));
    assert(H("rust", ["fn f<'a>(x: &'a str) {}"])[0].includes('hl-k">fn'));
    assert(H("java", ["public class Foo {}"])[0].includes('hl-t">Foo'));
    assert(H("css", ["a { color: #fff; margin: 4px; }"])[0].includes('hl-n">#fff'));
    assert(H("css", ["  color: red;"])[0].includes('hl-t">color'));
    assert(H("css", ["@media (x) {"])[0].includes('hl-k">@media'));
    assert(H("yaml", ["name: x # c", "- key: 1"])[0].includes('hl-t">name'));
    assert(H("yaml", ["- key: 1"])[0].includes('hl-t">key'));
    assert(H("ini", ["[core]", "a = 1"])[0].includes('hl-k">[core]'));
    assert(H("ini", ["[core]", "a = 1"])[1].includes('hl-t">a'));
    assert(H("make", ["all: dep", "CC := gcc", "\t$(CC) $@"])[0].includes('hl-t">all'));
    assert(H("make", ["CC := gcc"])[0].includes('hl-t">CC'));
    assert(H("make", ["\t$(CC) $@"])[0].includes('hl-t">$(CC)'));
    assert(H("diff", ["+a"])[0].includes("hl-a") && H("diff", ["-b"])[0].includes("hl-d") && H("diff", ["@@ x"])[0].includes("hl-h"));
    assert(H("md", ["# Title", "see [x](http://y) and `code`"])[0].includes("hl-h"));
    assert(H("md", ["# Title", "see [x](http://y) and `code`"])[1].includes('hl-l">x'));
    const f = H("md", ["```c", "# not a heading", "```", "# real"]);
    assert(!f[1].includes("hl-h") && f[3].includes("hl-h"));
    const h = H("html", ["<!-- c", "x --><div class=\"a\" id=b>t</div>"]);
    assert(h[0].includes("hl-c") && h[1].includes('hl-k">div') && h[1].includes('hl-t">class'));
});
t("safe", () => {
    const samples = ["<script>alert(1)</script>", '<img src=x onerror="alert(1)">', "a & b < c > d \"q\" 'r'", "&lt;b&gt; &amp;", "x = '</pre><svg onload=1>'",
        "/* <b> */ // <i>", "`<u>`", "[<a>](javascript:alert(1))", "<!-- <script> -->", "</style><script>", "\\", "'", '"'];
    for (const lang of Object.keys(HLX.langs)) {
        H(lang, samples).forEach((o, i) => {
            assert.strictEqual(plain(o), samples[i], lang + " roundtrip: " + samples[i]);
            assert(!/<(script|img|svg|style|b|i|u|a)[\s>]/i.test(o.replace(/<span class="hl-[a-z]">/g, "").replace(/<\/span>/g, "")), lang + " injection: " + o);
        });
    }
    const long = "x".repeat(2500);
    assert.strictEqual(H("c", [long])[0], long);
    assert.strictEqual(H("nope", ["<b>"])[0], "&lt;b&gt;");
});
t("contrast", () => {   // WCAG AA of the token colors on --code (light #fffaf0, dark #100d09)
    const lum = h => { const v = [1, 3, 5].map(i => parseInt(h.substr(i, 2), 16) / 255).map(c => c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)); return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2]; };
    const cr = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
    const sets = css.match(/\{--hl-[^}]*\}/g); assert(sets.length >= 3);
    [[sets[0], "#fffaf0"], [sets[1], "#100d09"], [sets[2], "#100d09"]].forEach(([s, bg]) => {
        const cols = s.match(/--hl-[a-z]:#[0-9a-f]{6}/g); assert(cols.length >= 11);
        for (const c of cols) { const v = c.split(":")[1]; assert(cr(v, bg) >= 4.5, c + " on " + bg + " = " + cr(v, bg).toFixed(2)); }
    });
});
console.log("unit checks passed: " + n);
