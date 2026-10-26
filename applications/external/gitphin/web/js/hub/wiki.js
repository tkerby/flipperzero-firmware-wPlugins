function wikiName(s){ return String(s).trim().replace(/\s+/g, "-").replace(/[^A-Za-z0-9._-]/g, "").replace(/^\.+/, "").slice(0, 80); }
function wikiHas(nm){ return !!(R && R.hub.wiki && R.hub.wiki.pages.some(p => p.name === nm)); }
async function loadWikiIndex(){
const d = await docGet("wiki/index.json");
let idx = {pages: []}; if(d){ try{ idx = JSON.parse(d.text); }catch(_){} }
idx.pages = idx.pages || []; R.hub.wiki = idx; return idx;
}
async function viewWiki(body, rest, tok){
const idx = await loadWikiIndex(); if(tok !== navTok) return;
const name = rest[0] ? wikiName(rest[0]) : "Home", editing = rest[1] === "edit";
const side = card(el("div", {class: "tools"}, el("h2", {style: "margin:0;flex:1"}, "Pages"),
el("button", {onclick: () => formDialog("New wiki page", [{key: "n", label: "Page name", hint: "Letters, digits, . _ -  (spaces become -)"}], "Create", async v => {
const nm = wikiName(v.n); if(!nm) throw new Error("Enter a page name"); go2(hs("wiki", nm, "edit")); })}, "New")));
const pages = idx.pages.slice().sort((a, b) => (b.name === "Home") - (a.name === "Home") || a.name.localeCompare(b.name));
if(!wikiHas("Home")) side.append(el("div", {class: "item"}, link(hs("wiki", "Home"), "Home ✎")));
pages.forEach(p => side.append(el("div", {class: "item"}, p.name === name ? el("b", {}, p.name.replace(/-/g, " ")) : link(hs("wiki", p.name), p.name.replace(/-/g, " ")))));
const main = el("div", {});
body.append(el("div", {class: "wiki"}, side, main));
const key = "wiki/" + name + ".md";
const d = await docGet(key); if(tok !== navTok) return;
if(editing){
const ta = el("textarea", {style: "width:100%;min-height:320px", spellcheck: "false"}); ta.value = d ? d.text : "";
let etag = d ? d.etag : null;
const st = el("div", {class: "status"});
const save = el("button", {class: "primary"}, "Save page");
const doSave = async () => {
save.disabled = true; st.className = "status"; st.textContent = "Saving…";
try{
if(new Blob([ta.value]).size > 60000) throw new Error("The page is over 60 KB.");
await docPut(key, ta.value, etag);
await updateJson("wiki/index.json", () => ({pages: []}), ix => { ix.pages = ix.pages || []; const e = {name, updated: new Date().toISOString()}; const k = ix.pages.findIndex(x => x.name === name); if(k < 0) ix.pages.push(e); else ix.pages[k] = e; });
setFlash("Saved " + name + "."); go2(hs("wiki", name));
}catch(e){
save.disabled = false; st.className = "status err";
if(e.conflict){
st.textContent = "Someone saved this page meanwhile. ";
st.append(el("button", {onclick: async () => { const f = await docGet(key); etag = f ? f.etag : null; doSave(); }}, "Overwrite with my text"));
}else st.textContent = e.message;
}
};
save.onclick = doSave;
main.append(card(el("h2", {}, (d ? "Edit " : "New page ") + name.replace(/-/g, " ")), ta, note("Link other pages with [[Page Name]]. Markdown is supported."),
el("div", {class: "bar", style: "margin:8px 0 0"}, save, el("button", {onclick: () => go2(hs("wiki", name))}, "Cancel"), st)));
return;
}
if(!d){
main.append(card(el("h2", {}, name.replace(/-/g, " ")), note("This page does not exist yet."), el("div", {class: "bar", style: "margin:8px 0 0"}, el("button", {class: "primary", onclick: () => go2(hs("wiki", name, "edit"))}, "Create it"))));
return;
}
const c = card(el("div", {class: "tools"}, el("h2", {style: "margin:0;flex:1"}, name.replace(/-/g, " ")),
el("button", {onclick: () => go2(hs("wiki", name, "edit"))}, "Edit"),
el("button", {class: "danger", onclick: async () => {
const cf = await confirmDialog("Delete page", "Delete the wiki page " + name + "?", "Delete", {danger: true}); if(!cf.ok) return;
try{
const f = await docGet(key); if(f) await docDelete(key, f.etag);
await updateJson("wiki/index.json", () => ({pages: []}), ix => { ix.pages = (ix.pages || []).filter(x => x.name !== name); });
setFlash("Deleted " + name + "."); go2(hs("wiki"));
}catch(e){ rstatus("Delete: " + e.message, true); }
}}, "Delete")), mdNode(d.text, {ref: defBranch(), wiki: true}));
main.append(c);
}
