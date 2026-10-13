let codeUI = null;
function prefixes(path){ const out = [""]; const segs = pathSegs(path); segs.forEach((_, i) => out.push(segs.slice(0, i + 1).join("/"))); return out; }
function dirOf(p){ return p.split("/").slice(0, -1).join("/"); }
function skel(n, w){ const d = el("div", {class: "pad"}); for(let i = 0; i < n; i++) d.append(el("div", {class: "sk", style: "width:" + (w || (60 + (i * 37) % 40)) + "%"})); return d; }
function emptyState(icon, text, ...actions){
return el("div", {class: "empty"}, ic(icon), el("p", {}, text), actions.length ? el("div", {class: "bar", style: "margin:0;justify-content:center"}, ...actions) : null);
}
function copyBtn(text, label){
const b = el("button", {class: "ghost sm", "aria-label": label || "Copy"}, ic("copy"));
b.onclick = async () => { try{ await navigator.clipboard.writeText(text); }catch(_){ const t = el("textarea"); t.value = text; document.body.append(t); t.select(); try{ document.execCommand("copy"); }catch(__){} t.remove(); } toast("Copied " + (text.length > 40 ? "to the clipboard" : text), "ok"); };
return b;
}
function shaChip(sha){ return el("span", {class: "shachip"}, el("code", {}, sha.slice(0, 7)), copyBtn(sha, "Copy full SHA")); }
function buildCodeUI(ref, sha){
const ui = {name: R.name, ref, sha, dirs: new Map(), open: new Set([""]), cur: {mode: "tree", path: ""}, seq: 0};
ui.treeBody = el("div", {class: "pb"});
ui.viewBody = el("div", {class: "pb"});
ui.rh = el("div", {class: "ph"});
const lh = el("div", {class: "ph"}, ic("folder-open"), el("span", {class: "px2"}, "Files"), el("span", {class: "sp"}),
refSelect(ref, v => go2(treeHash(v, ui.cur.mode === "tree" ? ui.cur.path : dirOf(ui.cur.path)))));
ui.root = el("div", {class: "split"}, el("section", {class: "pane"}, lh, ui.treeBody), el("section", {class: "pane"}, ui.rh, ui.viewBody));
ui.loadDir = async path => {
const have = ui.dirs.get(path); if(have && have.entries) return have;
try{
const j = await gapi("GET", "tree", {name: R.name, ref: sha, path});
const entries = j.entries.slice().sort((a, b) => ((b.type === "tree") - (a.type === "tree")) || a.name.localeCompare(b.name));
const d = {entries}; ui.dirs.set(path, d); return d;
}catch(e){ const d = {entries: null, err: e.message}; ui.dirs.set(path, d); return d; }
};
ui.renderTree = () => {
const tb = ui.treeBody; tb.innerHTML = "";
const add = (path, depth) => {
const d = ui.dirs.get(path);
if(!d){ tb.append(skel(depth ? 2 : 6, 70)); return; }
if(d.err){ tb.append(el("div", {class: "pad status err", style: "padding:8px"}, d.err)); return; }
const lim = d.limit || 150;
d.entries.slice(0, lim).forEach(e => {
const full = path ? path + "/" + e.name : e.name, isDir = e.type === "tree", on = ui.open.has(full);
const row = el("a", {class: "tr" + (ui.cur.path === full ? " on" : ""), style: "--d:" + depth, title: e.name});
if(isDir){
const chev = ic(on ? "chevron-down" : "chevron-right", "c"); chev.addEventListener("click", ev => { ev.preventDefault(); ev.stopPropagation(); ui.toggle(full); });
row.href = treeHash(ref, full); row.append(chev, ic(on ? "folder-open" : "folder"));
row.onclick = ev => { ev.preventDefault(); ui.open.add(full); go2(treeHash(ref, full)); };
}else if(e.type === "blob"){ row.href = blobHash(ref, full); row.append(el("span", {style: "width:12px"}), ic("file")); }
else row.append(el("span", {style: "width:12px"}), ic(e.type === "commit" ? "repo" : "link"));
row.append(el("span", {class: "nm"}, e.name));
tb.append(row);
if(isDir && on) add(full, depth + 1);
});
if(d.entries.length > lim) tb.append(el("a", {class: "tr", style: "--d:" + depth, href: "#", onclick: ev => { ev.preventDefault(); d.limit = lim + 150; ui.renderTree(); }}, el("span", {class: "muted"}, "Show " + Math.min(150, d.entries.length - lim) + " more of " + (d.entries.length - lim) + "...")));
};
add("", 0);
};
ui.toggle = async full => {
if(ui.open.has(full)){ ui.open.delete(full); ui.renderTree(); return; }
ui.open.add(full); ui.renderTree(); await ui.loadDir(full); ui.renderTree();
};
return ui;
}
async function viewCode(body, mode, ref, path, tok){
if(!R.refs.length){ emptyRepoNote(body); return; }
ref = ref || defBranch(); lastRef = ref;
body.className = "fill";
const sha = refSha(ref);
if(!(codeUI && codeUI.name === R.name && codeUI.ref === ref && codeUI.sha === sha)) codeUI = buildCodeUI(ref, sha);
const ui = codeUI; body.append(ui.root);
looseBanner(body, tok);
const seq = ++ui.seq, live = () => seq === ui.seq && tok === navTok;
ui.cur = {mode, path};
ui.renderTree();
for(const p of prefixes(mode === "tree" ? path : dirOf(path))){
if(p) ui.open.add(p);
await ui.loadDir(p); if(!live()) return;
ui.renderTree();
}
renderCodeRight(ui, live);
}
function pathCrumbs(ref, path, lastIsFile){
const c = el("div", {class: "crumbs", style: "flex:1;color:var(--on-chrome2)"}, link(treeHash(ref, ""), R.name, ""));
const segs = pathSegs(path);
segs.forEach((sg, i) => {
c.append(" / ");
if(i === segs.length - 1 && lastIsFile) c.append(el("b", {style: "color:var(--on-chrome)"}, sg));
else c.append(link(treeHash(ref, segs.slice(0, i + 1).join("/")), sg, ""));
});
return c;
}
async function renderCodeRight(ui, live){
const {mode, path} = ui.cur, ref = ui.ref, isBr = isBranchRef(ref);
ui.rh.innerHTML = ""; ui.viewBody.innerHTML = ""; ui.viewBody.className = "pb";
ui.rh.append(pathCrumbs(ref, path, mode === "blob"));
if(mode === "tree"){
if(isBr){
const nf = el("button", {class: "sm"}, ic("plus"), "New file"); nf.onclick = () => go2(hs("new", ref, ...pathSegs(path)));
const up = el("button", {class: "sm"}, ic("upload"), "Upload"); up.onclick = () => go2(hs("upload", ref, ...pathSegs(path)));
ui.rh.append(nf, up);
}
const d = ui.dirs.get(path);
if(!d || d.err){ ui.viewBody.append(el("div", {class: "pad"}, errBox(d ? d.err : "Could not read this folder"))); return; }
const tb = el("tbody", {});
if(path) tb.append(el("tr", {}, el("td", {class: "name", colspan: "2", style: "padding-left:12px"}, link(treeHash(ref, dirOf(path)), ".. (up)", ""))));
d.entries.slice(0, 300).forEach(e => {
const full = path ? path + "/" + e.name : e.name;
const icn = ic(e.type === "tree" ? "folder" : e.type === "blob" ? "file" : e.type === "commit" ? "repo" : "link");
const nm = e.type === "tree" ? link(treeHash(ref, full), e.name) : e.type === "blob" ? link(blobHash(ref, full), e.name) : el("span", {class: "muted"}, e.name);
tb.append(el("tr", {}, el("td", {class: "name", style: "padding-left:12px;width:99%"}, el("span", {style: "display:inline-flex;gap:8px;align-items:center"}, icn, nm)), el("td", {class: "size small"}, e.type === "link" ? "link" : e.type === "commit" ? "submodule" : "")));
});
ui.viewBody.append(el("table", {}, tb));
if(d.entries.length > 300) ui.viewBody.append(el("div", {class: "note", style: "padding:0 12px 12px"}, "Showing the first 300 of " + d.entries.length + " entries."));
if(!d.entries.length) ui.viewBody.append(emptyState("folder-open", "This folder is empty."));
const rd = d.entries.filter(e => e.type === "blob" && /^readme(\.(md|markdown|txt|rst))?$/i.test(e.name))
.sort((a, b) => (/\.m/i.test(b.name) - /\.m/i.test(a.name)) || a.name.localeCompare(b.name))[0];
if(rd){
const box = el("div", {class: "pad", style: "padding:12px 16px;border-top:2px solid var(--bd)"}, el("div", {class: "k", style: "margin-bottom:8px"}, rd.name), skel(3));
ui.viewBody.append(box);
try{
const b = await fetchBlob(rd.sha); if(!live()) return;
box.lastChild.remove();
if(b.tooLarge) box.append(note("README is too large to show here."));
else if(b.binary) box.append(note("README is not text."));
else if(/\.(md|markdown)$/i.test(rd.name)) box.append(mdNode(b.text, {ref, dir: path}));
else box.append(el("pre", {class: "snip", style: "white-space:pre-wrap"}, b.text));
}catch(e){ box.lastChild.remove(); box.append(errBox("README: " + e.message)); }
}
return;
}
const rawUrl = "/api/git/blob?" + enc({name: R.name, ref, path}), dlUrl = rawUrl + "&dl=1";
const mk = (icon, label, fn) => { const b = el("button", {class: "sm"}, ic(icon), label); b.onclick = fn; return b; };
ui.rh.append(mk("code", "Raw", () => window.open(rawUrl, "_blank", "noopener")), mk("download", "Download", () => { location.href = dlUrl; }));
if(isBr){ ui.rh.append(mk("pencil", "Edit", () => go2(hs("edit", ref, ...pathSegs(path)))), mk("trash", "Delete", () => go2(hs("delete", ref, ...pathSegs(path))))); }
ui.viewBody.className = "pb nos"; ui.viewBody.append(skel(8, 80));
const r = await fetch("/api/git/blob?" + enc({name: R.name, ref: ui.sha, path}));
if(!live()) return;
const vb = ui.viewBody; vb.innerHTML = "";
if(r.status === 413){ vb.className = "pb"; vb.append(emptyState("file", "This file is too large to show here. Use Raw or Download.")); return; }
if(!r.ok){ vb.className = "pb"; const j = await r.json().catch(() => ({})); vb.append(el("div", {class: "pad"}, errBox(j.error || ("HTTP " + r.status)))); return; }
const ab = await r.arrayBuffer(); if(!live()) return;
const b = decodeBlob(ab);
if(b.binary){ vb.className = "pb"; vb.append(emptyState("file", "Binary file (" + fmtSize(b.size) + "). Use Download.")); return; }
if(b.tooLarge){ vb.className = "pb"; vb.append(emptyState("file", "This file is " + fmtSize(b.size) + ", too large to show here. Use Raw or Download.")); return; }
const lines = b.text.split("\n"); if(lines.length > 1 && lines[lines.length - 1] === "") lines.pop();
if(!b.size){ vb.className = "pb"; vb.append(emptyState("file", "This file is empty.")); return; }
const info = el("div", {class: "small muted", style: "padding:6px 12px;border-bottom:2px solid var(--line);display:flex;gap:12px;align-items:center;flex:none"}, el("span", {}, lines.length + " lines"), el("span", {}, fmtSize(b.size)), el("span", {style: "flex:1"}), copyBtn(path, "Copy path"));
const txt = el("pre", {class: "txt"}, lines.join("\n")); hlApply(txt, path, lines.join("\n"), lines);
const src = el("div", {class: "codebox"}, el("pre", {class: "gut"}, lines.map((_, i) => i + 1).join("\n")), txt);
vb.append(info);
if(/\.(md|markdown)$/i.test(path)){
const rendered = el("div", {class: "pb pad", style: "padding:12px 16px"}, mdNode(b.text, {ref, dir: dirOf(path)}));
src.hidden = true;
const bR = el("button", {class: "sm active"}, "Preview"), bS = el("button", {class: "sm"}, "Source");
bR.onclick = () => { rendered.hidden = false; src.hidden = true; bR.classList.add("active"); bS.classList.remove("active"); };
bS.onclick = () => { rendered.hidden = true; src.hidden = false; bS.classList.add("active"); bR.classList.remove("active"); };
info.insertBefore(el("div", {class: "seg"}, bR, bS), info.children[0]);
vb.append(rendered, src);
if(parseInt(routeQuery.get("L"), 10) > 0) bS.onclick();
}else vb.append(src);
const L = parseInt(routeQuery.get("L"), 10);
if(L > 0 && L <= lines.length && !src.hidden){
src.append(el("div", {class: "hl", style: "top:" + (6 + (L - 1) * 20) + "px"}));
src.scrollTop = Math.max(0, 6 + (L - 1) * 20 - 100);
}
}
