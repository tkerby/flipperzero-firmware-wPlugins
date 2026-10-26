function repoDir(){ return "/ext/apps_data/gitphin/git/" + R.name + ".git"; }
function storageInfo(force){   // GET /api/git/storage: {packs, pack_kib, loose}. Slow on the device, so it is only cached for 15 s.
if(force || !R.hub.storage || Date.now() - R.hub.storageAt > 15000){
R.hub.storageAt = Date.now();
R.hub.storage = gapi("GET", "storage", {name: R.name}).then(st => { R.hub.storageVal = st; renderHeadMeta(); return st; })
.catch(e => { R.hub.storage = null; throw e; });
}
return R.hub.storage;
}
function looseBanner(body, tok){   // no request of its own: uses the storage numbers Settings read earlier
const st = R.hub.storageVal;
if(tok !== navTok || !st || !(st.loose >= 100)) return;
body.prepend(el("div", {class: "banner"}, "This repo has " + st.loose + " loose objects. Clones of loose repos are slow. ",
link(hs("settings"), "Pack them in Settings", "lnk"), " or run ", el("code", {}, "git flipper gc " + R.name), " on the PC."));
}
async function commitFile(branch, tip, path, message, action, body, newBranch){
const p = {name: R.name, branch, old: tip, path, message, author: getAuthor(), email: getEmail(), action};
if(newBranch) p.new_branch = newBranch;
const r = await fetch("/api/git/commit-file?" + enc(p), {method: "POST", body: action === "delete" ? undefined : body});
const j = await r.json().catch(() => ({}));
if(r.status === 409){ const e = new Error(j.error || "branch moved"); e.moved = true; throw e; }
if(!r.ok || j.ok === false) throw new Error(j.error || ("HTTP " + r.status));
return j.commit;
}
function validFilePath(p){
if(!p) return "Enter a file path";
const segs = p.split("/");
if(segs.some(x => !x || x === "." || x === ".." || x === ".git" || /[\x00-\x1f\x7f\\]/.test(x))) return "Invalid path";
return "";
}
const MAX_EDIT = 65536;
async function viewEdit(body, mode, branch, path, tok){
const emptyRepo = !R.refs.length && (mode === "new" || mode === "upload");
if(!branch || (!isBranchRef(branch) && !emptyRepo)){ body.append(card(note("Editing needs a branch. Open a branch in the Code view first."))); return; }
lastRef = branch;
await reloadRefs(); if(tok !== navTok) return;
const br = R.refs.find(r => r.name === "refs/heads/" + branch);
if(!br && R.refs.length){ body.append(card(note("Branch " + branch + " no longer exists."))); return; }
let tip = br ? br.sha : ZERO;   // ZERO = first commit on an empty repo
const back = mode === "edit" || mode === "delete" ? blobHash(branch, path) : treeHash(branch, path);
const titles = {edit: "Edit ", new: "New file in ", upload: "Upload file to ", delete: "Delete "};
const c = card(el("h2", {}, titles[mode] + (path || R.name) + " on " + branch));
body.append(c);
let ta = null, pathIn = null, fileIn = null, defMsg = "", getBody = async () => undefined, getPath = () => path;
if(mode === "edit"){
const r = await fetch("/api/git/blob?" + enc({name: R.name, ref: refSha(branch), path}));
if(tok !== navTok) return;
if(!r.ok){ c.append(errBox(r.status === 413 ? "This file is too large to edit here." : "Could not load the file (HTTP " + r.status + ")")); return; }
const ab = await r.arrayBuffer(), u = new Uint8Array(ab);
if(u.length > MAX_EDIT){ c.append(note("This file is " + fmtSize(u.length) + ". The page edits files up to 64 KiB.")); return; }
if(u.subarray(0, 8000).includes(0)){ c.append(note("Binary files cannot be edited here. Upload a new version instead.")); return; }
ta = el("textarea", {spellcheck: "false", style: "width:100%;min-height:340px;font:13px/1.45 ui-monospace,SFMono-Regular,Menlo,monospace"});
ta.value = new TextDecoder().decode(u);
c.append(ta); defMsg = "Update " + path; getBody = async () => ta.value;
}else if(mode === "new"){
pathIn = el("input", {type: "text", style: "width:100%", placeholder: "folder/new-file.txt (folders are created)", autocomplete: "off", spellcheck: "false"});
pathIn.value = path ? path + "/" : "";
ta = el("textarea", {spellcheck: "false", placeholder: "File content", style: "width:100%;min-height:260px;margin-top:8px;font:13px/1.45 ui-monospace,SFMono-Regular,Menlo,monospace"});
c.append(pathIn, ta); defMsg = "Create new file"; getPath = () => pathIn.value.trim().replace(/^\/+/, ""); getBody = async () => ta.value;
}else if(mode === "upload"){
fileIn = el("input", {type: "file"}); fileIn.style.display = "block";
pathIn = el("input", {type: "text", style: "width:100%;margin-top:8px", placeholder: "Name in the repo", autocomplete: "off"});
fileIn.onchange = () => { const f = fileIn.files[0]; if(f) pathIn.value = (path ? path + "/" : "") + f.name; };
c.append(note("Pick a file (up to 64 KiB).", ""), fileIn, pathIn); defMsg = "Add file"; getPath = () => pathIn.value.trim().replace(/^\/+/, "");
getBody = async () => { const f = fileIn.files[0]; if(!f) throw new Error("Pick a file first"); if(f.size > MAX_EDIT) throw new Error("The file is " + fmtSize(f.size) + ". The limit is 64 KiB."); return await f.arrayBuffer(); };
}else{
c.append(note("This removes " + path + " from the branch " + branch + ". The history keeps the old version.", ""));
defMsg = "Delete " + path;
}
const msg = el("input", {type: "text", style: "width:100%", placeholder: defMsg, autocomplete: "off"});
const rDirect = el("input", {type: "radio", name: "cm", checked: "checked"}), rNew = el("input", {type: "radio", name: "cm"});
const nbIn = el("input", {type: "text", placeholder: "new-branch-name", autocomplete: "off", spellcheck: "false", style: "flex:1;min-width:140px"});
const st = el("div", {class: "status"});
const commit = el("button", {class: "primary"}, mode === "delete" ? "Delete file" : "Commit changes");
nbIn.oninput = () => { rNew.checked = true; };
const suggest = () => { if(!nbIn.value) nbIn.placeholder = "patch-" + Math.random().toString(36).slice(2, 6); };
suggest();
const box = el("div", {style: "margin-top:12px;border-top:1px solid var(--line);padding-top:10px"},
el("div", {class: "k"}, "Commit message"), msg,
el("label", {class: "chk", style: "display:flex;gap:8px;align-items:center;margin-top:8px"}, rDirect, "Commit directly to ", el("b", {}, branch)),
el("label", {class: "chk", style: "display:flex;gap:8px;align-items:center;margin-top:6px;flex-wrap:wrap"}, rNew, "Create a new branch for this commit and start a pull request", nbIn),
el("div", {class: "note"}, "Committing as " + getAuthor() + " <" + getEmail() + ">."),
el("div", {class: "bar", style: "margin:8px 0 0"}, commit, link(back, "Cancel", ""), st));
c.append(box);
commit.onclick = async () => {
st.className = "status"; st.textContent = "";
const fp = getPath();
const bad = mode === "edit" || mode === "delete" ? "" : validFilePath(fp);
if(bad){ st.className = "status err"; st.textContent = bad; return; }
let nb = "";
if(rNew.checked){
nb = nbIn.value.trim() || nbIn.placeholder;
if(!validRefName(nb) || branches().some(b => short(b.name) === nb)){ st.className = "status err"; st.textContent = "Pick a valid branch name that does not exist yet."; return; }
}
let data;
try{
data = await getBody();
if(typeof data === "string" && new Blob([data]).size > MAX_EDIT) throw new Error("The content is over 64 KiB.");
}catch(e){ st.className = "status err"; st.textContent = e.message; return; }
commit.disabled = true; st.textContent = "Committing…";
try{
const sha = await commitFile(branch, tip, fp, msg.value.trim() || (mode === "new" || mode === "upload" ? defMsg + " " + fp : defMsg), mode === "delete" ? "delete" : "put", data, nb);
try{ await reloadRefs(); }catch(_){}
if(nb){ setFlash("Committed " + sha.slice(0, 7) + " to the new branch " + nb + "."); go2(hs("pulls", "new", branch, nb)); }
else{
setFlash("Committed " + sha.slice(0, 7) + " to " + branch + ".");
go2(mode === "delete" ? treeHash(branch, fp.split("/").slice(0, -1).join("/")) : blobHash(branch, fp));
}
}catch(e){
commit.disabled = false; st.className = "status err";
if(e.moved){
st.textContent = "The branch moved while you were editing (409). Your text is kept. ";
st.append(el("button", {onclick: async () => {
try{ await reloadRefs(); const b2 = R.refs.find(r => r.name === "refs/heads/" + branch); tip = b2 ? b2.sha : ZERO;
st.className = "status"; st.textContent = "Reloaded the branch tip (" + tip.slice(0, 7) + "). Check that nobody changed this file, then commit again."; }
catch(x){ st.textContent = "Reload failed: " + x.message; }
}}, "Reload branch tip"));
}else st.textContent = e.message;
}
};
}
