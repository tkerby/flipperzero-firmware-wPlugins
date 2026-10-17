const pxCache = new Map();
function pxPath(t){
let c = pxCache.get(t); if(c) return c;
let x = 0, d = "";
for(const ch of t){
if(ch === " "){ x += 3; continue; }
let code = ch.charCodeAt(0) - 32; if(code < 0 || code > 94) code = 31;
const rows = []; for(let i = 0; i < 8; i++) rows.push(parseInt(PXFONT[code * 8 + i], 32));
let lo = 5, hi = -1;
for(const r of rows) for(let b = 0; b < 5; b++) if(r >> (4 - b) & 1){ lo = Math.min(lo, b); hi = Math.max(hi, b); }
if(hi < 0){ x += 3; continue; }
rows.forEach((r, y) => {
let b = lo;
while(b <= hi){
if(r >> (4 - b) & 1){ let e = b; while(e < hi && (r >> (3 - e) & 1)) e++; const n = e - b + 1; d += "M" + (x + b - lo) + " " + y + "h" + n + "v1h-" + n + "z"; b = e + 1; }
else b++;
}
});
x += hi - lo + 2;
}
c = {d, w: Math.max(1, x - 1)}; pxCache.set(t, c); return c;
}
function pxSvg(t, s){
const {d, w} = pxPath(t), svg = document.createElementNS(NS, "svg"), p = document.createElementNS(NS, "path");
svg.setAttribute("viewBox", "0 0 " + w + " 8"); svg.setAttribute("width", w * s); svg.setAttribute("height", 8 * s); svg.setAttribute("aria-hidden", "true");
p.setAttribute("d", d); svg.append(p); return svg;
}
