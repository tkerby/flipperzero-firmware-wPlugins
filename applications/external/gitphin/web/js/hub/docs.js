function defMeta(){ return {next: 1, labels: [], milestones: []}; }
function normMeta(m){ m = m || {}; m.next = m.next || 1; m.labels = m.labels || []; m.milestones = m.milestones || []; return m; }
function docUrl(key){ return "/api/git/doc?" + enc({name: R.name, key}); }
async function docGet(key){
const r = await fetch(docUrl(key));
if(r.status === 404) return null;
if(!r.ok){ const j = await r.json().catch(() => ({})); throw new Error(j.error || ("HTTP " + r.status)); }
return {text: await r.text(), etag: r.headers.get("ETag")};
}
async function docPut(key, text, etag){
const r = await fetch(docUrl(key), {method: "PUT", headers: etag ? {"If-Match": etag} : {"If-None-Match": "*"}, body: text});
const j = await r.json().catch(() => ({}));
if(r.status === 412){ const e = new Error("changed"); e.conflict = true; throw e; }
if(!r.ok || j.ok === false) throw new Error(j.error || ("HTTP " + r.status));
return j.etag;
}
async function updateJson(key, dflt, mutate){
for(let i = 0; i < 8; i++){
const d = await docGet(key);
let cur;
if(d){ try{ cur = JSON.parse(d.text); }catch(_){ throw new Error(key + " is not valid JSON"); } }
else cur = dflt();
const next = mutate(cur) || cur;
try{ const etag = await docPut(key, JSON.stringify(next), d && d.etag); return {value: next, etag}; }
catch(e){ if(!e.conflict) throw e; }
}
throw new Error("Could not save " + key + ": too many edit conflicts. Try again.");
}
async function loadIndex(force){
if(!force && R.hub.index && Date.now() - (R.hub.indexAt || 0) < 30000) return R.hub.index;
const d = await docGet("issues/index.json");
let idx = {items: []};
if(d){ try{ idx = JSON.parse(d.text); }catch(_){} }
idx.items = idx.items || [];
R.hub.index = idx; R.hub.indexAt = Date.now(); renderSubtabs();
return idx;
}
async function loadMeta(force){
if(!force && R.hub.metaLoaded && Date.now() - (R.hub.metaAt2 || 0) < 30000) return R.hub.meta;
const d = await docGet("meta.json");
let m = null; if(d){ try{ m = JSON.parse(d.text); }catch(_){} }
R.hub.meta = normMeta(m); R.hub.metaLoaded = true; R.hub.metaAt2 = Date.now();
return R.hub.meta;
}
function updateMeta(fn){
return updateJson("meta.json", defMeta, m => { normMeta(m); return fn(m); }).then(r => { R.hub.meta = normMeta(r.value); R.hub.metaLoaded = true; R.hub.metaAt2 = Date.now(); return r.value; });
}
function summary(it){ return {n: it.n, kind: it.kind, title: it.title, state: it.state, labels: it.labels || [], milestone: it.milestone == null ? null : it.milestone, updated: it.updated, comments: (it.comments || []).length}; }
async function upsertIndex(it){
const r = await updateJson("issues/index.json", () => ({items: []}), idx => {
idx.items = idx.items || [];
const s = summary(it), k = idx.items.findIndex(x => x.n === it.n);
if(k < 0) idx.items.push(s); else idx.items[k] = s;
return idx;
});
R.hub.index = r.value; R.hub.indexAt = Date.now(); renderSubtabs();
}
async function rebuildIndex(){
const l = await gapi("GET", "docs", {name: R.name, prefix: "issues"});
const keys = (l.keys || []).map(k => k.key).filter(k => /(^|\/)\d+\.json$/.test(k));
const items = [];
for(const k of keys){
const d = await docGet(k.startsWith("issues/") ? k : "issues/" + k.split("/").pop());
if(!d) continue;
try{ items.push(summary(JSON.parse(d.text))); }catch(_){}
}
items.sort((a, b) => a.n - b.n);
await updateJson("issues/index.json", () => ({items: []}), idx => { idx.items = items; return idx; });
await loadIndex(true);
}
async function createItem(kind, title, body, labels, milestone, pull){
const now = new Date().toISOString();
let n = 0;
await updateMeta(m => { n = m.next; m.next = n + 1; });   // number allocated by CAS on meta.json
const it = {n, kind, title, body, state: "open", labels, milestone, author: getAuthor(), created: now, updated: now, comments: [], pull: pull || null};
await docPut("issues/" + n + ".json", JSON.stringify(it), null);
await upsertIndex(it);
return it;
}
async function mutateItem(n, fn){
const r = await updateJson("issues/" + n + ".json", () => { throw new Error("#" + n + " not found"); }, it => { fn(it); it.comments = it.comments || []; it.updated = new Date().toISOString(); return it; });
await upsertIndex(r.value);
return r.value;
}
function milestoneTitle(id){ const m = (R.hub.meta.milestones || []).find(x => x.id === id); return m ? m.title : null; }
