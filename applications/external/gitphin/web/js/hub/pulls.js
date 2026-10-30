async function viewNewPull(body, base0, head0, tok){
const bl = branches();
if(bl.length < 2){ body.append(card(note("A pull request needs at least two branches. Create a branch first."))); return; }
await loadMeta(); if(tok !== navTok) return;
const names = bl.map(b => short(b.name));
const base = base0 && names.includes(base0) ? base0 : defBranch();
const head = head0 && names.includes(head0) ? head0 : (names.find(n => n !== base) || base);
const mk = v => { const s = el("select", {}); names.forEach(n => s.append(el("option", {value: n}, n))); s.value = v; return s; };
const bSel = mk(base), hSel = mk(head);
const title = el("input", {type: "text", placeholder: "Title", autocomplete: "off", style: "width:100%"});
const txt = el("textarea", {placeholder: "Describe the change (Markdown)", style: "width:100%;min-height:120px"});
const prev = el("div", {});
const st = el("div", {});
const go = el("button", {class: "primary"}, "Create pull request"); go.disabled = true;
let cmp = null, titleTouched = false, pv = 0;
title.oninput = () => { titleTouched = true; };
async function preview(){
const tk = ++pv;
prev.innerHTML = ""; go.disabled = true; cmp = null; st.textContent = "";
if(bSel.value === hSel.value){ prev.append(note("Pick two different branches.")); return; }
prev.append(el("div", {class: "note"}, el("span", {class: "spin"}), "Comparing on the Flipper, this can take a minute…"));
const live = () => tk === pv && tok === navTok;
try{
const c = await gapi("GET", "compare", {name: R.name, base: refSha(bSel.value), head: refSha(hSel.value)});   // one request at a time
if(!live()) return;
cmp = c; prev.innerHTML = "";
prev.append(note(c.ahead_count + (c.ahead_count === 1 ? " commit" : " commits") + " ahead, " + c.behind_count + " behind" + (c.capped ? " (counts are lower bounds)" : "") + ". " +
(c.ff ? "Can be merged by fast-forward." : "Cannot be fast-forwarded."), ""));
(c.ahead || []).forEach(cm => prev.append(el("div", {class: "item"}, el("div", {class: "t"}, link(commitHash(cm.sha), (cm.message || "").split("\n")[0] || "(no message)")),
el("div", {class: "m"}, cm.author.name + " · ", timeEl(commitTime(cm)), " · ", el("code", {}, cm.sha.slice(0, 7))))));
if(!titleTouched && c.ahead && c.ahead.length){ title.value = c.ahead.length === 1 ? (c.ahead[0].message || "").split("\n")[0] : hSel.value; }
go.disabled = c.ahead_count === 0;
if(c.ahead_count === 0){ prev.append(note("Nothing to merge: " + hSel.value + " has no commits that " + bSel.value + " lacks.")); return; }
const fl = el("div", {});
const fb = el("button", {style: "margin-top:8px"}, "List changed files");
fb.onclick = async () => {
fb.disabled = true; fb.textContent = "Listing…";
try{
const d = await gapi("GET", "diff", {name: R.name, base: refSha(bSel.value), head: refSha(hSel.value)});
if(!live()) return;
fl.innerHTML = "";
const files = d.files || [];
fl.append(el("div", {class: "note"}, files.length + (files.length === 1 ? " file" : " files") + " differ" + (d.truncated ? " (truncated)" : "") + ":"));
files.forEach(f => fl.append(el("div", {class: "fhead"}, el("span", {class: "st " + f.status}, f.status), el("span", {class: "p"}, f.path))));
}catch(e){ fb.disabled = false; fb.textContent = "List changed files"; fl.append(errBox(e.message)); }
};
fl.append(fb); prev.append(fl);
}catch(e){ if(live()) prev.append(errBox(e.message)); }
}
bSel.onchange = hSel.onchange = preview;
go.onclick = async () => {
if(!title.value.trim()){ st.className = "status err"; st.textContent = "Enter a title."; return; }
go.disabled = true; st.className = "status"; st.textContent = "Saving…";
try{
const it = await createItem("pull", title.value.trim(), txt.value, [], null, {base: bSel.value, head: hSel.value, merged_sha: null, merged_at: null});
setFlash("Created pull request #" + it.n); go2(hs("pulls", it.n));
}catch(e){ st.className = "status err"; st.textContent = e.message; go.disabled = false; }
};
body.append(card(el("h2", {}, "New pull request"),
el("div", {class: "tools"}, el("span", {class: "muted"}, "merge"), hSel, el("span", {class: "muted"}, "into"), bSel),
prev, el("div", {style: "height:10px"}), title, el("div", {style: "height:8px"}), txt,
el("div", {class: "bar", style: "margin-top:10px"}, go, el("button", {onclick: () => go2(hs("pulls"))}, "Cancel")), st));
preview();
}
async function viewPull(body, n, tab, tok){
const d = await docGet("issues/" + n + ".json"); await loadMeta();
if(tok !== navTok) return;
if(!d){ body.append(card(note("Pull request #" + n + " does not exist."))); return; }
let it; try{ it = JSON.parse(d.text); }catch(_){ body.append(errBox("issues/" + n + ".json is not valid JSON")); return; }
if(it.kind !== "pull"){ go2(hs("issues", n)); return; }
const host = el("div", {}); body.append(host);
const state = {pending: false, cmp: null, err: null, promise: null};
if(it.state === "open" && tab !== "files"){
state.pending = true;
state.promise = gapi("GET", "compare", {name: R.name, base: refSha(it.pull.base), head: refSha(it.pull.head)})
.then(c => { state.cmp = c; }).catch(e => { state.err = e.message; }).then(() => { state.pending = false; });
}
const draw = cur => {
if(tok !== navTok) return;
it = cur || it; host.innerHTML = "";
const p = it.pull, closed = it.state === "closed", merged = it.state === "merged";
const head = card(
el("div", {class: "title-row"}, el("h2", {}, it.title), el("span", {class: "muted"}, "#" + it.n), stateBadge(it.state)),
el("div", {class: "note", style: "margin-top:4px"}, it.author + " wants to merge ", el("code", {}, p.head), " into ", el("code", {}, p.base), " · opened ", timeEl(Date.parse(it.created)),
merged && p.merged_at ? [" · merged ", timeEl(Date.parse(p.merged_at))] : ""),
metaRows(it, draw),
el("div", {class: "bar", style: "margin:8px 0 0"}, el("button", {onclick: () => editDialog(it, draw)}, "Edit"), el("button", {onclick: () => go2(hs("pulls"))}, "Back to pull requests")));
const nav = el("nav", {class: "subtabs", style: "margin:12px 0"});
[["conv", "Conversation"], ["commits", "Commits"], ["files", "Files changed"]].forEach(([k, t]) => {
nav.append(el("button", {class: k === tab ? "active" : "", onclick: () => go2(hs("pulls", it.n, ...(k === "conv" ? [] : [k])))}, t));
});
host.append(head, nav);
const sub = el("div", {}); host.append(sub);
if(tab === "commits") prCommits(sub, it, state, tok);
else if(tab === "files") prFiles(sub, it, tok);
else prConversation(sub, it, state, draw, tok);
};
draw(it);
}
async function prCommits(sub, it, state, tok){
const p = it.pull;
try{
if(state.pending){
const w = el("div", {class: "note"}, el("span", {class: "spin"}), "Comparing on the Flipper, this can take a minute…"); sub.append(w);
await state.promise; w.remove();
}
if(tok !== navTok) return;
let c = state.cmp;
if(!c){
if(it.state === "open") throw new Error(state.err || "compare failed");
const base = p.base_sha || p.base, head = p.head_sha || p.merged_sha || p.head;
c = await gapi("GET", "compare", {name: R.name, base, head});
}
if(tok !== navTok) return;
const k = card(el("h2", {}, c.ahead_count + (c.ahead_count === 1 ? " commit" : " commits") + (c.capped ? " (at least)" : "")));
(c.ahead || []).forEach(cm => k.append(el("div", {class: "item"}, el("div", {class: "t"}, link(commitHash(cm.sha), (cm.message || "").split("\n")[0] || "(no message)")),
el("div", {class: "m"}, cm.author.name + " · ", timeEl(commitTime(cm)), " · ", el("code", {}, cm.sha.slice(0, 7))))));
if(c.ahead_count > (c.ahead || []).length) k.append(note("Showing the newest " + c.ahead.length + "."));
sub.append(k);
}catch(e){ sub.append(errBox(e.message)); }
}
async function prFiles(sub, it, tok){
const p = it.pull;
const params = it.state === "merged" && p.base_sha ? {base: p.base_sha, head: p.head_sha || p.merged_sha} : {base: p.base, head: p.head};
if(params.base && !HEX40.test(params.base)) params.base = refSha(params.base);
if(params.head && !HEX40.test(params.head)) params.head = refSha(params.head);
try{ await diffSection(sub, params, tok, true); }catch(e){ sub.append(errBox(e.message)); }
}
function prConversation(sub, it, state, draw, tok){
const p = it.pull, merged = it.state === "merged", closed = it.state === "closed";
const mergeBox = card(); mergeBox.style.marginTop = "12px";
const fill = () => {
mergeBox.innerHTML = "";
const cmp = state.cmp, cmpErr = state.err;
if(merged){
mergeBox.append(el("div", {class: "title-row"}, stateBadge("merged"), el("span", {}, "This pull request was merged" + (p.merged_sha ? " as " : "."), p.merged_sha ? el("code", {}, p.merged_sha.slice(0, 7)) : "")));
}else if(closed){
mergeBox.append(note("This pull request is closed.", ""));
}else if(state.pending){
mergeBox.append(el("div", {class: "note", style: "margin:0"}, el("span", {class: "spin"}), "Comparing on the Flipper, this can take a minute…"));
}else if(cmpErr){
mergeBox.append(errBox("Could not compare the branches: " + cmpErr), note("A branch may have been deleted. You can close this pull request."));
}else if(cmp){
if(cmp.ahead_count === 0) mergeBox.append(note("Nothing to merge: " + p.base + " already contains " + p.head + ".", ""));
else if(cmp.ff){
mergeBox.append(el("div", {}, el("b", {}, "Ready to merge. "), el("span", {class: "muted"}, p.base + " can be fast-forwarded to " + p.head + " (" + cmp.ahead_count + (cmp.ahead_count === 1 ? " commit" : " commits") + ").")),
el("div", {class: "bar", style: "margin:8px 0 0"}, el("button", {class: "primary", onclick: () => doMerge(it, cmp)}, "Merge (fast-forward)")));
}else{
mergeBox.append(el("div", {}, el("b", {}, "This branch cannot be fast-forwarded. "), el("span", {class: "muted"}, p.base + " has " + cmp.behind_count + " commit" + (cmp.behind_count === 1 ? "" : "s") + " that " + p.head + " lacks.")));
if(FEATURES.mergeCommit) mergeBox.append(el("div", {class: "bar", style: "margin:8px 0 0"}, el("button", {class: "primary", onclick: () => doMergeCommit(it, cmp)}, "Create merge commit")));
if(mergeConflict && mergeConflict.n === it.n)
mergeBox.append(errBox("The merge has conflicts in " + mergeConflict.paths.length + (mergeConflict.paths.length === 1 ? " path" : " paths") + ":"), el("pre", {class: "snip"}, mergeConflict.paths.join("\n")));
mergeBox.append(note("Merge locally and push:", ""),
el("pre", {class: "snip"}, "git fetch flipper\ngit checkout " + p.base + "\ngit merge flipper/" + p.head + "\ngit push flipper " + p.base));
}
}
};
fill();
if(state.pending) state.promise.then(() => { if(tok === navTok) fill(); });
sub.append(conversation(it, p.head, draw, stateButtons(it, draw), mergeBox));
}
function stateButtons(it, draw){
if(it.state === "merged") return [];
const closed = it.state === "closed";
const b = el("button", {class: closed ? "" : "danger"}, closed ? "Reopen" : "Close pull request");
b.run = async text => { const r = await mutateItem(it.n, x => { addComment(x, text); if(x.state !== "merged") x.state = closed ? "open" : "closed"; }); return r; };
return [b];
}
async function doMerge(it, cmp){
const p = it.pull;
const delOk = p.head !== defBranch();
const c = await confirmDialog("Merge pull request #" + it.n, "Fast-forward " + p.base + " to " + p.head + " (" + cmp.head.slice(0, 7) + ").", "Merge", delOk ? {check: "Delete branch " + p.head + " after merging"} : {});
if(!c.ok) return;
let msg = "Merged #" + it.n + ".", err = false;
try{
await gapi("POST", "ref", {name: R.name, ref: "refs/heads/" + p.base, old: cmp.base, new: cmp.head});
}catch(e){
err = true; msg = /ref changed/i.test(e.message) ? "The branch " + p.base + " changed while you worked (409 ref changed). Nothing was merged. Check again." : "Merge failed: " + e.message;
try{ await reloadRefs(); }catch(_){}
setFlash(msg, true); route(); return;
}
try{
await mutateItem(it.n, x => { x.state = "merged"; x.pull.merged_sha = cmp.head; x.pull.merged_at = new Date().toISOString(); x.pull.base_sha = cmp.base; x.pull.head_sha = cmp.head; });
}catch(e){ msg = "The branch was updated, but marking the pull request as merged failed: " + e.message; err = true; }
if(c.checked && delOk){
try{ await gapi("POST", "ref", {name: R.name, ref: "refs/heads/" + p.head, old: cmp.head, new: ZERO}); msg += " Deleted branch " + p.head + "."; }
catch(e){ msg += " Could not delete " + p.head + ": " + e.message; err = true; }
}
try{ await reloadRefs(); }catch(_){}
setFlash(msg, err); route();
}
async function doMergeCommit(it, cmp){
const p = it.pull;
const c = await confirmDialog("Create merge commit", "Merge " + p.head + " into " + p.base + " with a merge commit.", "Merge", {});
if(!c.ok) return;
let msg, err = false;
try{
const r = await fetch("/api/git/merge?" + enc({name: R.name, base: p.base, head: p.head, message: "Merge pull request #" + it.n + " from " + p.head, author: getAuthor(), email: getEmail()}), {method: "POST"});
const j = await r.json().catch(() => ({}));
if(r.status === 409 && j.error === "conflict"){
mergeConflict = {n: it.n, paths: j.paths || []}; err = true;
msg = "The merge has conflicts. Nothing was changed. Merge locally instead.";
}else if(!r.ok || j.ok === false) throw new Error(j.error || ("HTTP " + r.status));
else{
mergeConflict = null;
await mutateItem(it.n, x => { x.state = "merged"; x.pull.merged_sha = j.commit; x.pull.merged_at = new Date().toISOString(); x.pull.base_sha = cmp.base; x.pull.head_sha = cmp.head; });
msg = "Merged #" + it.n + " with merge commit " + j.commit.slice(0, 7) + ".";
}
}catch(e){ err = true; msg = "Merge failed: " + e.message; }
try{ await reloadRefs(); }catch(_){}
setFlash(msg, err); route();
}
