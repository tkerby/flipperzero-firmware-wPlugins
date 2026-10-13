async function loadRepoList(){
const box = $("repos-list"); box.innerHTML = ""; box.append(skel(4, 80));
try{
const j = await gapi("GET", "repos");
shell.repos = j.repos.map(r => r.name); renderSide();
const f = repoFilter.toLowerCase();
const list = j.repos.filter(r => !f || r.name.toLowerCase().includes(f) || (r.description || "").toLowerCase().includes(f));
box.innerHTML = "";
if(!list.length){
box.append(f ? emptyState("search", "No repository matches “" + repoFilter + "”.", el("button", {onclick: () => { repoFilter = ""; $("gq").value = ""; loadRepoList(); }}, "Clear search"))
: emptyState("repo", "No repositories yet. Git hosting in your pocket: create one, or push to flipper://<name>.", el("button", {class: "primary", onclick: newRepoDialog}, ic("plus"), "New repo")));
return;
}
for(const r of list){
const acts = el("div", {class: "acts"},
el("button", {class: "ghost sm", "aria-label": "Rename " + r.name, title: "Rename", onclick: () => renameDialog(r.name)}, ic("pencil")),
el("button", {class: "ghost sm", "aria-label": "Edit description of " + r.name, title: "Edit description", onclick: () => describeDialog(r.name, r.description)}, ic("book")),
el("button", {class: "ghost sm danger", "aria-label": "Delete " + r.name, title: "Delete", onclick: () => deleteDialog(r.name)}, ic("trash")));
box.append(el("div", {class: "rrow"}, avBox(r.name, 32),
el("div", {class: "main-c"}, el("a", {class: "rn", href: "#repos/" + encodeURIComponent(r.name)}, r.name), r.description ? el("div", {class: "cell-desc"}, r.description) : null),
el("div", {class: "meta"}, el("span", {style: "display:inline-flex;gap:4px;align-items:center"}, ic("branch"), r.head || "-"), el("span", {}, r.packs + (r.packs === 1 ? " pack" : " packs"))),
acts));
}
}catch(e){ box.innerHTML = ""; box.append(emptyState("alert", "Could not read the repository list: " + e.message, el("button", {onclick: loadRepoList}, "Try again"))); }
}
