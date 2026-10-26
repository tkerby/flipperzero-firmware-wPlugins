function myersOps(a, b){
const N = a.length, M = b.length;
if(!N) return b.map(t => [2, t]);
if(!M) return a.map(t => [1, t]);
const LIM = Math.min(N + M, 2000), off = LIM + 1;
let v = new Int32Array(2 * LIM + 3);
const trace = [];
let found = -1;
for(let d = 0; d <= LIM && found < 0; d++){
trace.push(v.slice(off - d - 1, off + d + 2));
for(let k = -d; k <= d; k += 2){
let x = (k === -d || (k !== d && v[off + k - 1] < v[off + k + 1])) ? v[off + k + 1] : v[off + k - 1] + 1;
let y = x - k;
while(x < N && y < M && a[x] === b[y]){ x++; y++; }
v[off + k] = x;
if(x >= N && y >= M){ found = d; break; }
}
}
if(found < 0) return a.map(t => [1, t]).concat(b.map(t => [2, t]));
const out = [];
let x = N, y = M;
for(let d = found; d > 0; d--){
const vv = trace[d], k = x - y, g = i => vv[i + d + 1];
const pk = (k === -d || (k !== d && g(k - 1) < g(k + 1))) ? k + 1 : k - 1;
const px = g(pk), py = px - pk;
while(x > px && y > py){ x--; y--; out.push([0, a[x]]); }
if(x === px) { y--; out.push([2, b[y]]); } else { x--; out.push([1, a[x]]); }
}
while(x > 0 && y > 0){ x--; y--; out.push([0, a[x]]); }
return out.reverse();
}
function splitLines(t){ if(!t) return []; const l = t.split("\n"); if(l[l.length - 1] === "") l.pop(); return l; }
function lineDiff(oldText, newText){
const a = splitLines(oldText), b = splitLines(newText);
let p = 0; while(p < a.length && p < b.length && a[p] === b[p]) p++;
let s = 0; while(s < a.length - p && s < b.length - p && a[a.length - 1 - s] === b[b.length - 1 - s]) s++;
const mid = myersOps(a.slice(p, a.length - s), b.slice(p, b.length - s));
const ops = [];
for(let i = 0; i < p; i++) ops.push([0, a[i]]);
for(const o of mid) ops.push(o);
for(let i = a.length - s; i < a.length; i++) ops.push([0, a[i]]);
return ops;
}
function unifiedHunks(ops, ctx){
ctx = ctx == null ? 3 : ctx;
let ao = 1, bo = 1, added = 0, removed = 0;
const rows = [], ob = [], nb = [];
for(const o of ops){
ob.push(ao - 1); nb.push(bo - 1);
rows.push([o[0], o[0] === 2 ? 0 : ao, o[0] === 1 ? 0 : bo, o[1]]);
if(o[0] !== 2) ao++; if(o[0] !== 1) bo++;
if(o[0] === 1) removed++; if(o[0] === 2) added++;
}
const ch = []; rows.forEach((r, i) => { if(r[0]) ch.push(i); });
const hunks = [];
let gi = 0;
while(gi < ch.length){
const first = ch[gi]; let last = first; gi++;
while(gi < ch.length && ch[gi] - last - 1 <= 2 * ctx){ last = ch[gi]; gi++; }
const start = Math.max(0, first - ctx), end = Math.min(rows.length, last + ctx + 1);
const h = rows.slice(start, end);
const ol = h.filter(r => r[0] !== 2).length, nl = h.filter(r => r[0] !== 1).length;
hunks.push({os: ol ? ob[start] + 1 : ob[start], ol, ns: nl ? nb[start] + 1 : nb[start], nl, rows: h});
}
return {hunks, added, removed};
}
function renderHunks(hk){
const tb = el("tbody", {});
const frag = document.createDocumentFragment();
for(const h of hk.hunks){
tb.append(el("tr", {class: "hunk"}, el("td", {colspan: "4"}, "@@ -" + h.os + "," + h.ol + " +" + h.ns + "," + h.nl + " @@")));
for(const r of h.rows){
tb.append(el("tr", {class: r[0] === 1 ? "del" : r[0] === 2 ? "add" : ""},
el("td", {class: "ln"}, r[1] || ""), el("td", {class: "ln"}, r[2] || ""),
el("td", {class: "ln"}, r[0] === 1 ? "-" : r[0] === 2 ? "+" : " "), el("td", {class: "tx"}, r[3])));
}
}
return el("div", {class: "dwrap"}, el("table", {class: "diff"}, tb));
}
async function fileDiff(area, f, tok){
const oldB = f.status === "A" ? {text: ""} : await fetchBlob(f.old_sha);
if(tok !== navTok) return;
const newB = f.status === "D" ? {text: ""} : await fetchBlob(f.new_sha);
if(tok !== navTok) return;
area.innerHTML = "";
if(oldB.tooLarge || newB.tooLarge){ area.append(note("File changed (too large to diff here).")); return; }
if(oldB.binary || newB.binary){ area.append(note("File changed (binary).")); return; }
if(f.status === "T" || (f.old_mode && f.new_mode && f.old_mode !== f.new_mode && oldB.text === newB.text)){
area.append(note("Mode changed " + (f.old_mode || "") + " → " + (f.new_mode || "") + (oldB.text === newB.text ? "" : ", content changed") + "."));
if(oldB.text === newB.text) return;
}
const hk = unifiedHunks(lineDiff(oldB.text, newB.text), 3);
if(!hk.hunks.length){ area.append(note("No line changes.")); return; }
const info = note("+" + hk.added + " −" + hk.removed, "");
area.append(info);
const changed = hk.added + hk.removed;
if(changed > 500){
const btn = el("button", {}, "Large diff (" + changed + " changed lines) - show");
btn.onclick = () => { btn.remove(); area.append(renderHunks(hk)); };
area.append(btn);
}else area.append(renderHunks(hk));
}
