async function viewLabels(body, tok){
await loadMeta(); if(tok !== navTok) return;
const host = el("div", {}); body.append(host);
const draw = () => {
if(tok !== navTok) return;
const m = R.hub.meta; host.innerHTML = "";
const lc = card(el("div", {class: "tools"}, el("h2", {style: "margin:0;flex:1"}, "Labels"),
el("button", {class: "primary", onclick: () => labelDialog(null)}, "New label"), el("button", {onclick: () => go2(hs("issues"))}, "Back to issues")));
m.labels.forEach(l => lc.append(el("div", {class: "item bar", style: "margin-bottom:0"}, chip(l.name), el("span", {class: "muted small", style: "flex:1"}, l.color),
el("button", {onclick: () => labelDialog(l)}, "Edit"),
el("button", {class: "danger", onclick: async () => { const c = await confirmDialog("Delete label", "Delete the label " + l.name + "? Issues keep the name but it loses its colour.", "Delete", {danger: true}); if(c.ok) save(x => { x.labels = x.labels.filter(k => k.name !== l.name); }); }}, "Delete"))));
if(!m.labels.length) lc.append(note("No labels yet."));
const mc = card(el("div", {class: "tools"}, el("h2", {style: "margin:0;flex:1"}, "Milestones"), el("button", {class: "primary", onclick: () => milestoneDialog(null)}, "New milestone")));
m.milestones.forEach(x => {
const cnt = R.hub.index.items.filter(i => i.milestone === x.id);
mc.append(el("div", {class: "item bar", style: "margin-bottom:0"},
el("span", {style: "flex:1;min-width:0;overflow-wrap:anywhere"}, el("b", {}, x.title), el("span", {class: "muted small"}, (x.due ? " · due " + x.due : "") + " · " + cnt.filter(i => i.state === "open").length + " open, " + cnt.filter(i => i.state !== "open").length + " closed")),
stateBadge(x.state === "closed" ? "closed" : "open"),
el("button", {onclick: () => milestoneDialog(x)}, "Edit"),
el("button", {onclick: () => save(k => { const t = k.milestones.find(y => y.id === x.id); if(t) t.state = t.state === "closed" ? "open" : "closed"; })}, x.state === "closed" ? "Reopen" : "Close"),
el("button", {class: "danger", onclick: async () => { const c = await confirmDialog("Delete milestone", "Delete the milestone " + x.title + "?", "Delete", {danger: true}); if(c.ok) save(k => { k.milestones = k.milestones.filter(y => y.id !== x.id); }); }}, "Delete")));
});
if(!m.milestones.length) mc.append(note("No milestones yet."));
host.append(lc, mc);
};
const save = async fn => { try{ await updateMeta(fn); }catch(e){ rstatus(e.message, true); return; } rstatus(""); draw(); };
const labelDialog = old => formDialog(old ? "Edit label" : "New label", [
{key: "name", label: "Name", value: old ? old.name : ""}, {key: "color", label: "Colour (#rrggbb)", value: old ? old.color : "#ff8200"}], "Save", async v => {
if(!v.name || /[\n,]/.test(v.name)) throw new Error("Enter a name without commas");
if(!/^#[0-9a-fA-F]{6}$/.test(v.color)) throw new Error("Colour must look like #ff8200");
await updateMeta(m => {
const dup = m.labels.find(l => l.name === v.name && (!old || l.name !== old.name));
if(dup) throw new Error("A label named " + v.name + " exists");
if(old){ const t = m.labels.find(l => l.name === old.name); if(t){ t.name = v.name; t.color = v.color; } else m.labels.push({name: v.name, color: v.color}); }
else m.labels.push({name: v.name, color: v.color});
});
if(old && old.name !== v.name) setFlash("Renamed the label. Existing issues keep the old name.");
draw();
});
const milestoneDialog = old => formDialog(old ? "Edit milestone" : "New milestone", [
{key: "title", label: "Title", value: old ? old.title : ""}, {key: "due", label: "Due date (YYYY-MM-DD, optional)", value: old ? old.due || "" : ""}], "Save", async v => {
if(!v.title) throw new Error("Enter a title");
if(v.due && !/^\d{4}-\d{2}-\d{2}$/.test(v.due)) throw new Error("Due date must look like 2026-12-31");
await updateMeta(m => {
if(old){ const t = m.milestones.find(y => y.id === old.id); if(t){ t.title = v.title; t.due = v.due || null; } }
else m.milestones.push({id: Math.max(0, ...m.milestones.map(y => y.id)) + 1, title: v.title, due: v.due || null, state: "open"});
});
draw();
});
draw();
}
