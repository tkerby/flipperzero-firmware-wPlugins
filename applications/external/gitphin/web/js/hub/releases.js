function relSummary(r){ return {id: r.id, tag: r.tag, title: r.title, draft: !!r.draft, prerelease: !!r.prerelease, created: r.created}; }
async function loadRelIndex(force){
if(!force && R.hub.rel) return R.hub.rel;
const d = await docGet("releases/index.json");
let idx = {next: 1, items: []};
if(d){ try{ idx = JSON.parse(d.text); }catch(_){} }
idx.items = idx.items || []; idx.next = idx.next || 1;
R.hub.rel = idx; return idx;
}
async function upsertRel(r){
const u = await updateJson("releases/index.json", () => ({next: 1, items: []}), idx => {
idx.items = idx.items || []; const s = relSummary(r), k = idx.items.findIndex(x => x.id === r.id);
if(k < 0) idx.items.push(s); else idx.items[k] = s;
idx.next = Math.max(idx.next || 1, r.id + 1);
});
R.hub.rel = u.value;
}
function assetPath(id, name){ return repoDir() + "/hub/releases/" + id + "/" + name; }
function cleanAsset(n){ return String(n).replace(/[^A-Za-z0-9._-]+/g, "-").replace(/^\.+/, "") || "file"; }
function relBadges(r){
const s = el("span", {});
if(r.draft) s.append(el("span", {class: "badge"}, "Draft"));
if(r.prerelease) s.append(el("span", {class: "badge"}, "Pre-release"));
return s;
}
async function docDelete(key, etag){
const r = await fetch("/api/git/doc-delete?" + enc({name: R.name, key}), {method: "POST", headers: {"If-Match": etag}});
const j = await r.json().catch(() => ({}));
if(r.status === 412){ const e = new Error("changed"); e.conflict = true; throw e; }
if(!r.ok || j.ok === false) throw new Error(j.error || ("HTTP " + r.status));
}
function viewReleases(body, rest, tok){
if(!rest.length) return viewRelList(body, tok);
if(rest[0] === "new") return viewRelForm(body, 0, tok);
const id = parseInt(rest[0], 10);
if(!(id > 0)) return Promise.reject(new Error("Bad release number"));
return rest[1] === "edit" ? viewRelForm(body, id, tok) : viewRelease(body, id, tok);
}
async function viewRelList(body, tok){
const idx = await loadRelIndex(true); if(tok !== navTok) return;
const c = card(el("div", {class: "tools"}, el("h2", {style: "margin:0;flex:1"}, "Releases"), el("button", {class: "primary", onclick: () => go2(hs("releases", "new"))}, "New release")));
idx.items.slice().sort((a, b) => b.id - a.id).forEach(r => {
c.append(el("div", {class: "item"},
el("div", {class: "t"}, link(hs("releases", r.id), r.title || r.tag), relBadges(r)),
el("div", {class: "m"}, el("code", {}, r.tag), " · ", timeEl(Date.parse(r.created)))));
});
if(!idx.items.length) c.append(note("No releases yet."));
body.append(c);
}
async function viewRelease(body, id, tok){
const d = await docGet("releases/" + id + ".json"); if(tok !== navTok) return;
if(!d){ body.append(card(note("Release " + id + " does not exist."))); return; }
let r; try{ r = JSON.parse(d.text); }catch(_){ body.append(errBox("releases/" + id + ".json is not valid JSON")); return; }
const c = card(el("div", {class: "title-row"}, el("h2", {}, r.title || r.tag), relBadges(r)),
el("div", {class: "note", style: "margin-top:4px"}, "Tag ", link(treeHash(r.tag, ""), r.tag), " · ", timeEl(Date.parse(r.created))),
el("div", {class: "bar", style: "margin:8px 0"}, el("button", {onclick: () => go2(hs("releases", id, "edit"))}, "Edit"),
el("button", {class: "danger", onclick: () => deleteRelease(r)}, "Delete"), el("button", {onclick: () => go2(hs("releases"))}, "All releases")));
if(r.body) c.append(mdNode(r.body, {ref: r.tag}));
const a = el("div", {style: "margin-top:12px"}, el("h2", {}, "Assets"));
(r.assets || []).forEach(x => a.append(el("div", {class: "item"}, el("a", {class: "lnk", href: "/api/file?dl=1&" + q(assetPath(r.id, x.name))}, "📦 " + x.name), el("span", {class: "muted small"}, " · " + fmtSize(x.size)))));
if(!(r.assets || []).length) a.append(note("No assets."));
c.append(a); body.append(c);
}
async function deleteRelease(r){
const c = await confirmDialog("Delete release", "Delete the release " + (r.title || r.tag) + " and its " + (r.assets || []).length + " asset file(s)? The tag stays.", "Delete", {danger: true});
if(!c.ok) return;
try{
for(const x of (r.assets || [])) await api("POST", "/api/delete?" + q(assetPath(r.id, x.name))).catch(() => {});
await api("POST", "/api/delete?" + q(repoDir() + "/hub/releases/" + r.id)).catch(() => {});
for(let i = 0; i < 5; i++){
const d = await docGet("releases/" + r.id + ".json"); if(!d) break;
try{ await docDelete("releases/" + r.id + ".json", d.etag); break; }catch(e){ if(!e.conflict) throw e; }
}
const u = await updateJson("releases/index.json", () => ({next: 1, items: []}), idx => { idx.items = (idx.items || []).filter(x => x.id !== r.id); });
R.hub.rel = u.value; setFlash("Deleted the release."); go2(hs("releases"));
}catch(e){ rstatus("Delete: " + e.message, true); }
}
async function viewRelForm(body, id, tok){
let rel = {id: 0, tag: "", title: "", body: "", draft: false, prerelease: false, assets: []};
if(id){
const d = await docGet("releases/" + id + ".json"); if(tok !== navTok) return;
if(!d){ body.append(card(note("Release " + id + " does not exist."))); return; }
try{ rel = JSON.parse(d.text); }catch(_){ body.append(errBox("releases/" + id + ".json is not valid JSON")); return; }
}
const tg = tags(), bl = branches();
const tagSel = el("select", {});
tg.forEach(t => tagSel.append(el("option", {value: short(t.name)}, short(t.name))));
if(rel.tag && !tg.some(t => short(t.name) === rel.tag)) tagSel.append(el("option", {value: rel.tag}, rel.tag + " (missing)"));
if(!id) tagSel.append(el("option", {value: "\u0000new"}, "➕ Create a new tag…"));
if(rel.tag) tagSel.value = rel.tag;
const newTag = el("input", {type: "text", placeholder: "new tag name, e.g. v1.0", autocomplete: "off", spellcheck: "false"});
const fromSel = el("select", {}); bl.forEach(b => fromSel.append(el("option", {value: b.sha}, short(b.name))));
const dBr = bl.find(b => short(b.name) === defBranch()); if(dBr) fromSel.value = dBr.sha;
const newRow = el("div", {class: "newref"}, newTag, el("span", {class: "pill"}, "from"), fromSel);
const sync = () => { newRow.hidden = tagSel.value !== "\u0000new"; };
tagSel.onchange = sync; if(!tg.length && !id) tagSel.value = "\u0000new"; sync();
const title = el("input", {type: "text", style: "width:100%", placeholder: "Release title", autocomplete: "off"}); title.value = rel.title;
const notes = el("textarea", {placeholder: "Release notes (Markdown)", style: "width:100%;min-height:160px"}); notes.value = rel.body;
const dr = el("input", {type: "checkbox"}), pre = el("input", {type: "checkbox"}); dr.checked = !!rel.draft; pre.checked = !!rel.prerelease;
const keep = (rel.assets || []).map(a => ({a, remove: false}));
const picked = [];
const assetBox = el("div", {});
const drawAssets = () => {
assetBox.innerHTML = "";
keep.forEach(k => { const cb = el("input", {type: "checkbox"}); cb.checked = k.remove; cb.onchange = () => { k.remove = cb.checked; };
assetBox.append(el("div", {class: "item"}, "📦 " + k.a.name + " · " + fmtSize(k.a.size) + " ", el("label", {class: "muted small"}, cb, " remove"))); });
picked.forEach((f, i) => assetBox.append(el("div", {class: "item"}, "➕ " + cleanAsset(f.name) + " · " + fmtSize(f.size) + " ", el("button", {onclick: () => { picked.splice(i, 1); drawAssets(); }, style: "padding:1px 8px;font-size:12px"}, "Remove"))));
if(!keep.length && !picked.length) assetBox.append(note("No assets. Optional files to attach."));
};
const fileIn = el("input", {type: "file"}); fileIn.multiple = true;
fileIn.onchange = () => { picked.push(...fileIn.files); fileIn.value = ""; drawAssets(); };
drawAssets();
const prog = el("div", {class: "progress", style: "display:none"}, el("div", {class: "track"}, el("div", {class: "fill"})), el("div", {class: "txt"}));
const st = el("div", {class: "status"});
const save = el("button", {class: "primary"}, id ? "Save release" : "Publish release");
const fail = m => { st.className = "status err"; st.textContent = m; save.disabled = false; prog.style.display = "none"; };
save.onclick = async () => {
st.className = "status"; st.textContent = "";
let tag = tagSel.value, mkTag = false;
if(tag === "\u0000new"){
tag = newTag.value.trim(); mkTag = true;
if(!validRefName(tag)) return fail("Enter a valid tag name.");
if(tg.some(t => short(t.name) === tag)) return fail("That tag already exists. Pick it from the list.");
if(!fromSel.value) return fail("There is no branch to tag.");
}
if(!tag) return fail("Pick or create a tag.");
const names = new Set(keep.filter(k => !k.remove).map(k => k.a.name));
for(const f of picked){ if(names.has(cleanAsset(f.name))) return fail("Two assets are named " + cleanAsset(f.name) + "."); names.add(cleanAsset(f.name)); }
save.disabled = true; st.textContent = "Saving…";
try{
if(mkTag) await gapi("POST", "ref", {name: R.name, ref: "refs/tags/" + tag, old: ZERO, new: fromSel.value});
const fields = r => { r.tag = tag; r.title = title.value.trim() || tag; r.body = notes.value; r.draft = dr.checked; r.prerelease = pre.checked; };
let rid = id;
if(!id){
const idx = await loadRelIndex(true);
const r0 = {id: 0, tag: "", title: "", body: "", draft: false, prerelease: false, created: new Date().toISOString(), assets: []}; fields(r0);
let cand = Math.max(idx.next || 1, ...idx.items.map(x => x.id + 1)), ok = false;
for(let i = 0; i < 12 && !ok; i++, cand++){ r0.id = cand; try{ await docPut("releases/" + cand + ".json", JSON.stringify(r0), null); rid = cand; ok = true; }catch(e){ if(!e.conflict) throw e; } }
if(!ok) throw new Error("Could not allocate a release number. Try again.");
await upsertRel(r0);
}
const gone = keep.filter(k => k.remove).map(k => k.a.name);
for(const n of gone) await api("POST", "/api/delete?" + q(assetPath(rid, n)));
const added = [];
if(picked.length){
await api("POST", "/api/mkdir?" + q(repoDir() + "/hub/releases/" + rid)).catch(() => {});
prog.style.display = "block"; const fill = prog.querySelector(".fill"), txt = prog.querySelector(".txt"); fill.style.width = "0";
let done = 0;
for(const f of picked){
done++; const nm = cleanAsset(f.name), t0 = performance.now();
await putFile("/api/file?" + q(assetPath(rid, nm)), f, (l, t) => {
const pct = t ? Math.round(l / t * 100) : 0; fill.style.width = pct + "%";
const secs = (performance.now() - t0) / 1000;
txt.textContent = "(" + done + "/" + picked.length + ") " + nm + " · " + pct + "% · " + (secs > 0.2 ? fmtSize(l / secs) + "/s" : "…");
});
added.push({name: nm, size: f.size});
}
}
const u = await updateJson("releases/" + rid + ".json", () => { throw new Error("release vanished"); }, r => {
fields(r); r.assets = (r.assets || []).filter(a => !gone.includes(a.name) && !added.some(x => x.name === a.name)).concat(added);
});
await upsertRel(u.value);
setFlash(id ? "Release saved." : "Release published."); go2(hs("releases", rid));
}catch(e){ fail(/ref changed|exists/i.test(e.message) ? "Could not create the tag: " + e.message : e.message); }
};
body.append(card(el("h2", {}, id ? "Edit release" : "New release"),
el("div", {class: "k"}, "Tag"), el("div", {class: "tools"}, tagSel), newRow,
el("div", {class: "k"}, "Title"), title, el("div", {style: "height:8px"}), el("div", {class: "k"}, "Notes"), notes,
el("div", {class: "kv"}, el("label", {}, dr, " Draft"), el("label", {}, pre, " Pre-release")),
el("div", {class: "k"}, "Assets"), assetBox, el("label", {class: "btn", style: "display:inline-block;margin-top:8px"}, "Add files", fileIn), prog,
el("div", {class: "bar", style: "margin-top:12px"}, save, el("button", {onclick: () => go2(id ? hs("releases", id) : hs("releases"))}, "Cancel")), st));
}
