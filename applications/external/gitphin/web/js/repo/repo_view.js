async function openRepo(name, rest){
const tok = ++navTok;
$("repos-list-view").hidden = true; $("repo-view").hidden = false;
if(!R || R.name !== name){
$("rv-body").innerHTML = "";
if(!(await reloadRepo(name, true)) || tok !== navTok) return;
}
else{
if(Date.now() - (R.refsAt || 0) > 30000){ try{ await reloadRefs(); }catch(_){} if(tok !== navTok) return; }
if(Date.now() - (R.metaAt || 0) > 30000){ await refreshMeta(); if(tok !== navTok) return; }
}
renderRepo(rest, tok);
}
async function reloadRepo(name, quiet){
name = name || (R && R.name);
rstatus("Loading…");
try{
const l = await gapi("GET", "repos");
const meta = l.repos.find(x => x.name === name);
if(!meta) throw new Error("repo not found");
const r = await gapi("GET", "refs", {name});
R = {name, meta, refs: r.refs || [], head: r.head || ("refs/heads/" + meta.head), hub: {meta: null, index: null}};
R.hub.meta = defMeta(); R.metaAt = Date.now(); R.refsAt = Date.now();
rstatus("");
try{ await loadIndex(true); }catch(_){ R.hub.index = {items: []}; }
if(!quiet) route();
return true;
}catch(e){
R = null; $("repo-view").hidden = true; $("repos-list-view").hidden = false;
loadRepoList().then(() => rstatus(name + ": " + e.message, true));
return false;
}
}
async function reloadRefs(){
const r = await gapi("GET", "refs", {name: R.name});
R.refs = r.refs || []; R.head = r.head || R.head; R.refsAt = Date.now();
}
function cloneRow(label, url){
const code = el("code", {}, url);
const b = el("button", {}, "Copy"); b.onclick = () => copyText(url, b);
return el("div", {class: "clone"}, el("div", {class: "k"}, label), code, b);
}
function viewKey(v){
return {tree: "code", blob: "code", code: "code", edit: "code", new: "code", upload: "code", delete: "code", search: "code", commit: "commits", labels: "issues"}[v] || v;
}
function openCount(kind){ return R.hub.index.items.filter(x => x.kind === kind && x.state === "open").length; }
let curKey = "code";
function renderHeadMeta(){
if(!R || $("repo-view").hidden) return;
const meta = $("rv-meta"); const st = R.hub.storageVal;
meta.textContent = "Default branch: " + defBranch() + " · " + R.meta.packs + (R.meta.packs === 1 ? " pack" : " packs") +
(st ? " · " + st.loose + " loose" + (st.loose > 0 ? " (updating in the background)" : "") : "") + " · Posting as ";
meta.append(el("b", {}, getAuthor()), " ", el("a", {href: "#", class: "lnk", onclick: e => { e.preventDefault(); authorDialog(); }}, "change"));
}
function renderSubtabs(){
if(!R || $("repo-view").hidden) return;
shell.repo = R.name; shell.key = curKey;
shell.counts = {branches: branches().length, tags: tags().length, issues: openCount("issue"), pulls: openCount("pull")};
renderSide();
}
async function refreshMeta(){
const name = R && R.name;
try{
const l = await gapi("GET", "repos"); const m = l.repos.find(x => x.name === name);
if(R && R.name === name && m){ R.meta = m; R.metaAt = Date.now(); renderHeadMeta(); }
}catch(_){}
}
function renderRepo(rest, tok){
const n = R.name;
$("rv-name").textContent = n;
$("rv-desc").textContent = R.meta.description || "No description";
renderHeadMeta();
const cl = $("rv-clone"); cl.innerHTML = "";
cl.append(cloneRow("Flipper", "flipper://" + n),
cloneRow("HTTP (plain git)", location.origin + "/git/" + n + ".git"));
$("rv-empty").hidden = R.refs.length !== 0;
$("rv-snippet").textContent = "git remote add flipper flipper://" + n + "\ngit push -u flipper " + defBranch();
const view = (rest && rest[0]) || "code"; curKey = viewKey(view);
const av = $("rv-avatar"); avShow(av, avCache.get(n) || ""); if(!avCache.has(n)) avFill(av, n);
const zr = zipRef(view, rest || []); $("rv-zip").hidden = !zr; $("rv-zip").onclick = () => zipDownload(n, zr);
renderSubtabs();
const body = $("rv-body"); body.innerHTML = ""; body.className = ""; body.scrollTop = 0;
if(flash){ rstatus(flash.msg, flash.err); flash = null; }
const fail = e => { if(tok === navTok) body.append(errBox(e.message)); };
try{
let p;
switch(view){
case "branches": body.append(refSection(true)); break;
case "tags": body.append(refSection(false)); break;
case "commits": p = viewCommits(body, rest[1], tok); break;
case "commit": p = viewCommit(body, rest[1], tok); break;
case "blob": p = viewCode(body, "blob", rest[1], rest.slice(2).join("/"), tok); break;
case "issues": case "pulls": case "labels": p = viewHub(body, view, rest.slice(1), tok); break;
case "edit": case "new": case "upload": case "delete": p = viewEdit(body, view, rest[1], rest.slice(2).join("/"), tok); break;
case "search": p = viewSearch(body, rest[1] || defBranch(), routeQuery.get("q") || "", tok); break;
case "releases": p = viewReleases(body, rest.slice(1), tok); break;
case "wiki": p = viewWiki(body, rest.slice(1), tok); break;
case "settings": p = viewSettings(body, tok); break;
default: p = viewCode(body, "tree", view === "tree" ? rest[1] : null, view === "tree" ? rest.slice(2).join("/") : "", tok);
}
if(p) p.catch(fail);
}catch(e){ fail(e); }
}
function authorDialog(){
formDialog("Your name", [{key: "a", label: "Author name for issues, comments, pull requests and commits", value: getAuthor()},
{key: "e", label: "Email for commits", value: getEmail(), hint: "Both are stored only in this browser."}], "Save", async v => {
if(!v.a) throw new Error("Enter a name");
if(!/^[^\s@<>]+@[^\s@<>]+$/.test(v.e)) throw new Error("Enter an email like me@example.com");
setAuthor(v.a); setEmail(v.e); route();
});
}
async function refAction(fn){
let msg = "", isErr = false;
try{ await fn(); }
catch(e){
isErr = true;
msg = /ref changed/i.test(e.message) ? "The ref changed on the Flipper while you worked (409 ref changed). The list was reloaded. Please try again." : e.message;
}
try{ await reloadRefs(); }catch(e){ msg = "Reload: " + e.message; isErr = true; }
setFlash(msg, isErr); route();
}
function refSection(isBranch){
const list = isBranch ? branches() : tags();
const kind = isBranch ? "branch" : "tag";
const c = card(el("h2", {}, isBranch ? "Branches" : "Tags"));
const def = defBranch();
const name = el("input", {type: "text", placeholder: "New " + kind + " name", autocomplete: "off", spellcheck: "false"});
const sel = el("select", {});
const srcB = branches(), srcT = isBranch ? tags() : [];
const og = (label, arr) => { if(arr.length){ const g = el("optgroup", {label}); arr.forEach(r => { const o = el("option", {value: r.peeled || r.sha}, short(r.name)); if(isBranch === (label === "Branches") && short(r.name) === def) o.selected = true; g.append(o); }); sel.append(g); } };
og("Branches", srcB); og("Tags", srcT);
const create = el("button", {class: "primary"}, "Create " + kind);
create.disabled = !sel.options.length;
create.onclick = () => {
const nm = name.value.trim();
if(!validRefName(nm)){ rstatus("Invalid " + kind + " name", true); return; }
if(list.some(r => short(r.name) === nm)){ rstatus("A " + kind + " named " + nm + " already exists", true); return; }
refAction(() => gapi("POST", "ref", {name: R.name, ref: (isBranch ? "refs/heads/" : "refs/tags/") + nm, old: ZERO, new: sel.value}));
};
name.onkeydown = e => { if(e.key === "Enter") create.onclick(); };
c.append(el("div", {class: "newref"}, name, el("span", {class: "pill"}, "from"), sel, create));
if(!sel.options.length) c.append(note("Nothing to create from yet. Push a branch first."));
const tb = el("tbody", {});
list.forEach(r => {
const nm = short(r.name);
const tr = el("tr", {});
const c1 = el("td", {class: "name"}, link(treeHash(nm, ""), nm));
if(isBranch && nm === def) c1.append(el("span", {class: "badge"}, "default"));
const cs = r.peeled || r.sha;   // annotated tags: show the commit the tag points at
const c2 = el("td", {class: "sha"}, el("code", {}, link(commitHash(cs), cs.slice(0, 7))));
if(r.peeled) c2.append(el("div", {class: "muted", style: "font-size:11px"}, "tag object " + r.sha.slice(0, 7)));
const act = el("td", {class: "act"});
if(isBranch && nm !== def)
act.append(el("button", {onclick: () => refAction(() => gapi("POST", "head", {name: R.name, branch: nm}))}, "Set default"), " ");
if(!(isBranch && nm === def))
act.append(el("button", {class: "danger", onclick: async () => {
const c = await confirmDialog("Delete " + kind, "Delete the " + kind + " " + nm + "?", "Delete", {danger: true});
if(!c.ok) return;
refAction(() => gapi("POST", "ref", {name: R.name, ref: r.name, old: r.sha, new: ZERO}));
}}, "Delete"));
tr.append(c1, c2, act); tb.append(tr);
});
c.append(el("table", {}, tb));
if(!list.length) c.append(note("No " + kind + "s yet."));
return c;
}
function refSelect(cur, onPick){
const sel = el("select", {class: "refsel", title: "Branch or tag"});
const add = (label, arr) => { if(arr.length){ const g = el("optgroup", {label}); arr.forEach(r => g.append(el("option", {value: short(r.name)}, short(r.name)))); sel.append(g); } };
add("Branches", branches()); add("Tags", tags());
if(![...sel.options].some(o => o.value === cur)) sel.insertBefore(el("option", {value: cur}, cur.slice(0, 10)), sel.firstChild);
sel.value = cur;
sel.onchange = () => onPick(sel.value);
return sel;
}
function emptyRepoNote(body){ body.append(card(note("This repository has no commits yet. Push a branch, or add the first file here:", ""), el("div", {class: "bar", style: "margin:8px 0 0"}, el("button", {onclick: () => go2(hs("new", defBranch()))}, "New file"), el("button", {onclick: () => go2(hs("upload", defBranch()))}, "Upload file")))); }
