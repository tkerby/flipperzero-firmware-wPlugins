const F = {issue: {state: "open", label: ""}, pull: {state: "open", label: ""}};
function viewHub(body, view, rest, tok){
if(view === "labels") return viewLabels(body, tok);
const kind = view === "pulls" ? "pull" : "issue";
if(!rest.length) return viewList(body, kind, tok);
if(rest[0] === "new") return kind === "issue" ? viewNewIssue(body, tok) : viewNewPull(body, rest[1], rest[2], tok);
const n = parseInt(rest[0], 10);
if(!(n > 0)) return Promise.reject(new Error("Bad number"));
return kind === "issue" ? viewIssue(body, n, tok) : viewPull(body, n, rest[1] || "conv", tok);
}
async function viewList(body, kind, tok){
const isPull = kind === "pull";
await loadIndex(); const meta = await loadMeta();
if(tok !== navTok) return;
const f = F[kind];
const c = card();
const head = el("div", {class: "tools"});
const seg = el("div", {class: "seg"});
const labelSel = el("select", {title: "Filter by label"}, el("option", {value: ""}, "All labels"));
(meta.labels || []).forEach(l => labelSel.append(el("option", {value: l.name}, l.name)));
labelSel.value = f.label;
const nw = el("button", {class: "primary", onclick: () => go2(hs(isPull ? "pulls" : "issues", "new"))}, isPull ? "New pull request" : "New issue");
const lab = el("button", {onclick: () => go2(hs("labels"))}, "Labels & milestones");
const list = el("div", {});
const draw = () => {
const items = R.hub.index.items.filter(x => x.kind === kind);
const isOpen = x => x.state === "open";
const nOpen = items.filter(isOpen).length, nClosed = items.length - nOpen;
seg.innerHTML = "";
const b1 = el("button", {class: f.state === "open" ? "active" : "", onclick: () => { f.state = "open"; draw(); }}, "Open (" + nOpen + ")");
const b2 = el("button", {class: f.state === "closed" ? "active" : "", onclick: () => { f.state = "closed"; draw(); }}, "Closed (" + nClosed + ")");
seg.append(b1, b2);
list.innerHTML = "";
const shown = items.filter(x => (f.state === "open") === isOpen(x) && (!f.label || (x.labels || []).includes(f.label)))
.sort((a, b) => b.n - a.n);
for(const x of shown){
const t = el("div", {class: "t"}, link(hs(isPull ? "pulls" : "issues", x.n), x.title), " ");
(x.labels || []).forEach(l => t.append(chip(l)));
const m = el("div", {class: "m"}, "#" + x.n + " ");
if(isPull) m.append(stateBadge(x.state), " ");
m.append("updated ", timeEl(Date.parse(x.updated)));
if(x.comments) m.append(" · " + x.comments + (x.comments === 1 ? " comment" : " comments"));
const ms = x.milestone != null ? milestoneTitle(x.milestone) : null;
if(ms) m.append(" · 🏁 " + ms);
list.append(el("div", {class: "item"}, t, m));
}
if(!shown.length) list.append(note(items.length ? "Nothing matches this filter." : "No " + (isPull ? "pull requests" : "issues") + " yet."));
};
labelSel.onchange = () => { f.label = labelSel.value; draw(); };
head.append(seg, labelSel, el("span", {style: "flex:1"}), lab, nw);
const foot = el("div", {class: "note"}, "The list reads issues/index.json. ",
el("a", {href: "#", class: "lnk", onclick: async e => { e.preventDefault(); try{ await rebuildIndex(); draw(); rstatus("Index rebuilt."); }catch(x){ rstatus("Rebuild: " + x.message, true); } }}, "Rebuild the index"),
" if it ever looks wrong.");
c.append(head, list, foot); body.append(c);
draw();
}
function labelPickerField(cur){
return {key: "labels", label: "Labels", type: "checks", value: cur || [], options: (R.hub.meta.labels || []).map(l => ({value: l.name, label: l.name}))};
}
function milestoneField(cur){
return {key: "milestone", label: "Milestone", type: "select", value: cur == null ? "" : String(cur),
options: [{value: "", label: "No milestone"}].concat((R.hub.meta.milestones || []).filter(m => m.state !== "closed" || m.id === cur).map(m => ({value: String(m.id), label: m.title})))};
}
function msVal(v){ return v === "" ? null : parseInt(v, 10); }
async function viewNewIssue(body, tok){
await loadMeta(); if(tok !== navTok) return;
const title = el("input", {type: "text", placeholder: "Title", autocomplete: "off", style: "width:100%"});
const txt = el("textarea", {placeholder: "Describe the issue (Markdown)", style: "width:100%;min-height:160px"});
const labs = labelPickerField([]), ms = milestoneField(null);
const chk = el("div", {class: "kv"});
const boxes = (R.hub.meta.labels || []).map(l => { const cb = el("input", {type: "checkbox"}); chk.append(el("label", {}, cb, " ", chip(l.name))); return [cb, l.name]; });
const msSel = el("select", {}); ms.options.forEach(o => msSel.append(el("option", {value: o.value}, o.label)));
const st = el("div", {});
const go = el("button", {class: "primary"}, "Create issue");
go.onclick = async () => {
if(!title.value.trim()){ st.className = "status err"; st.textContent = "Enter a title."; return; }
go.disabled = true; st.className = "status"; st.textContent = "Saving…";
try{
const it = await createItem("issue", title.value.trim(), txt.value, boxes.filter(b => b[0].checked).map(b => b[1]), msVal(msSel.value), null);
setFlash("Created #" + it.n); go2(hs("issues", it.n));
}catch(e){ st.className = "status err"; st.textContent = e.message; go.disabled = false; }
};
const c = card(el("h2", {}, "New issue"), title, el("div", {style: "height:8px"}), txt,
el("div", {class: "kv"}, el("span", {class: "muted"}, "Labels")), chk, el("div", {class: "kv"}, el("span", {class: "muted"}, "Milestone"), msSel),
el("div", {class: "bar", style: "margin-top:10px"}, go, el("button", {onclick: () => go2(hs("issues"))}, "Cancel")), st);
body.append(c); title.focus();
}
function conversation(it, ctxRef, redraw, extraButtons, beforeCompose){
const wrap = el("div", {});
const mk = (author, when, edited, text, onEdit) => {
const h = el("div", {class: "h"}, el("b", {}, author), " commented ", timeEl(Date.parse(when)));
if(edited) h.append(el("span", {}, "· edited"));
h.append(el("span", {class: "sp"}));
if(onEdit) h.append(el("button", {onclick: onEdit, style: "padding:1px 8px;font-size:12px"}, "Edit"));
return el("section", {class: "card cmt"}, h, text.trim() ? mdNode(text, {ref: ctxRef}) : el("div", {class: "muted"}, "No description provided."));
};
wrap.append(mk(it.author, it.created, false, it.body || "", () => editDialog(it, redraw)));
(it.comments || []).forEach(cm => wrap.append(mk(cm.author, cm.created, cm.edited, cm.body, () => {
formDialog("Edit comment", [{key: "b", label: "Comment", type: "textarea", value: cm.body}], "Save", async v => {
if(!v.b) throw new Error("A comment cannot be empty");
const r = await mutateItem(it.n, x => { const c2 = x.comments.find(k => k.id === cm.id); if(!c2) throw new Error("comment not found"); c2.body = v.b; c2.edited = new Date().toISOString(); });
redraw(r);
});
})));
const ta = el("textarea", {placeholder: "Leave a comment (Markdown)", style: "width:100%"});
const st = el("div", {});
const add = el("button", {class: "primary"}, "Comment");
const post = async after => {
const text = ta.value.trim();
if(!text && !after){ st.className = "status err"; st.textContent = "Write a comment first."; return; }
add.disabled = true; st.className = "status"; st.textContent = "Saving…";
try{
const r = await (after ? after(text) : mutateItem(it.n, x => { addComment(x, text); }));
redraw(r);
}catch(e){ st.className = "status err"; st.textContent = e.message; add.disabled = false; }
};
add.onclick = () => post(null);
const bar = el("div", {class: "bar", style: "margin:8px 0 0"}, add);
(extraButtons || []).forEach(b => { b.onclick = () => post(b.run); bar.append(b); });
if(beforeCompose) wrap.append(beforeCompose);
wrap.append(el("section", {class: "card cmt cbox"}, ta, bar, st));
return wrap;
}
function addComment(x, text){
if(!text) return;
const id = Math.max(0, ...x.comments.map(c => c.id || 0)) + 1;
x.comments.push({id, author: getAuthor(), body: text, created: new Date().toISOString(), edited: null});
}
function editDialog(it, redraw){
formDialog("Edit #" + it.n, [{key: "t", label: "Title", value: it.title}, {key: "b", label: "Description", type: "textarea", value: it.body}], "Save", async v => {
if(!v.t) throw new Error("Enter a title");
redraw(await mutateItem(it.n, x => { x.title = v.t; x.body = v.b; }));
});
}
function metaRows(it, redraw){
const row = el("div", {class: "kv small"});
const lab = el("span", {}, el("span", {class: "muted"}, "Labels "));
if(!(it.labels || []).length) lab.append(el("span", {class: "muted"}, "none "));
(it.labels || []).forEach(l => lab.append(chip(l)));
lab.append(el("a", {href: "#", class: "lnk", onclick: e => { e.preventDefault();
formDialog("Labels of #" + it.n, [labelPickerField(it.labels)], "Save", async v => { redraw(await mutateItem(it.n, x => { x.labels = v.labels; })); }); }}, "edit"));
const ms = el("span", {}, el("span", {class: "muted"}, "Milestone "), it.milestone != null ? (milestoneTitle(it.milestone) || "#" + it.milestone) : el("span", {class: "muted"}, "none"), " ",
el("a", {href: "#", class: "lnk", onclick: e => { e.preventDefault();
formDialog("Milestone of #" + it.n, [milestoneField(it.milestone)], "Save", async v => { redraw(await mutateItem(it.n, x => { x.milestone = msVal(v.milestone); })); }); }}, "edit"));
row.append(lab, ms);
return row;
}
async function viewIssue(body, n, tok){
const d = await docGet("issues/" + n + ".json"); await loadMeta();
if(tok !== navTok) return;
if(!d){ body.append(card(note("Issue #" + n + " does not exist."))); return; }
let it; try{ it = JSON.parse(d.text); }catch(_){ body.append(errBox("issues/" + n + ".json is not valid JSON")); return; }
if(it.kind === "pull"){ go2(hs("pulls", n)); return; }
const host = el("div", {}); body.append(host);
const draw = cur => {
if(tok !== navTok) return;
it = cur || it; host.innerHTML = "";
const closed = it.state !== "open";
const head = card(
el("div", {class: "title-row"}, el("h2", {}, it.title), el("span", {class: "muted"}, "#" + it.n), stateBadge(it.state)),
el("div", {class: "note", style: "margin-top:4px"}, it.author + " opened this ", timeEl(Date.parse(it.created))),
metaRows(it, draw),
el("div", {class: "bar", style: "margin:8px 0 0"},
el("button", {onclick: () => editDialog(it, draw)}, "Edit"),
el("button", {onclick: () => go2(hs("issues"))}, "Back to issues")));
const toggle = el("button", {class: closed ? "" : "danger"}, closed ? "Reopen" : "Close issue");
toggle.run = async text => mutateItem(it.n, x => { addComment(x, text); x.state = closed ? "open" : "closed"; });
host.append(head, conversation(it, defBranch(), draw, [toggle]));
};
draw(it);
}
