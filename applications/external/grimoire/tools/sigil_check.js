// Runs the site's temple/sigil.js with a fake page and prints compose() the way tools/sigil_test.c does.
// usage: node tools/sigil_check.js path/to/sigil.js "the wish"
const fs = require("fs");
const src = fs.readFileSync(process.argv[2], "utf8");
const wish = process.argv[3] || "this is my wish";
const noop = () => {};
const ctx = new Proxy({}, { get: () => noop, set: () => true });
const el = { getContext: () => ctx, parentNode: { getBoundingClientRect: () => ({ width: 100, height: 100 }), scrollIntoView: noop }, addEventListener: noop, setAttribute: noop, textContent: "", value: "", blur: noop };
global.window = global;
global.document = { getElementById: () => el };
global.matchMedia = () => ({ matches: false });
global.addEventListener = noop;
global.requestAnimationFrame = noop;
global.devicePixelRatio = 1;
eval(src);
const letters = Sigil.reduce(wish);
const strokes = Sigil.compose(letters, wish);
console.log(letters.join(""));
console.log(strokes.length + " strokes " + strokes.reduce((n, s) => n + s.length, 0) + " points");
for (const s of strokes) console.log(s.length + ":" + s.map(p => " " + p[0].toFixed(4) + "," + p[1].toFixed(4)).join(""));
