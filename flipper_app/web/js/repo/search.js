function markText(text, qs){
const f = document.createDocumentFragment(), lo = text.toLowerCase(), ql = qs.toLowerCase();
let i = 0;
while(ql){
const k = lo.indexOf(ql, i);
if(k < 0) break;
f.append(text.slice(i, k), el("mark", {}, text.slice(k, k + ql.length))); i = k + ql.length;
}
f.append(text.slice(i)); return f;
}
async function viewSearch(body, ref, qs, tok){
lastRef = ref; $("rv-q").value = qs;
const c = card(el("div", {class: "tools"}, el("h2", {style: "margin:0;flex:1"}, "Search results"),
refSelect(ref, v => go2(hs("search", v) + "?q=" + encodeURIComponent(qs)))));
body.append(c);
if(!qs){ c.append(note("Type something in the search box above.")); return; }
if(new Blob([qs]).size > 100){ c.append(errBox("The search text is limited to 100 bytes.")); return; }
const st = el("div", {class: "note"}, el("span", {class: "spin"}), "Searching for “" + qs + "” in " + ref + ". This can take several seconds on the Flipper…");
c.append(st);
const j = await gapi("GET", "search", {name: R.name, ref, q: qs, max: 100});
if(tok !== navTok) return;
st.innerHTML = "";
const hits = j.hits || [];
st.append((hits.length ? hits.length + (hits.length === 1 ? " hit" : " hits") : "No hits") + " for “" + qs + "”" +
(j.scanned != null ? " · scanned " + j.scanned + " files" : "") + (j.truncated ? " · more hits exist, narrow your search" : "") + (j.timed_out ? " · the time budget ran out, so not every file was scanned" : "") + " · files over 64 KiB are not scanned");
const groups = new Map();
hits.forEach(h => { if(!groups.has(h.path)) groups.set(h.path, []); groups.get(h.path).push(h); });
for(const [path, hs2] of groups){
const g = el("div", {class: "item"}, el("div", {class: "t"}, link(blobHash(ref, path), path)));
hs2.forEach(h => {
if(!h.line) { g.append(el("div", {class: "m"}, "file name matches")); return; }
const t = el("span", {}); t.append(markText(String(h.text || "").slice(0, 300), qs));
g.append(el("div", {class: "hit"}, el("a", {href: blobHash(ref, path) + "?L=" + h.line}, String(h.line)), t));
});
c.append(g);
}
}
