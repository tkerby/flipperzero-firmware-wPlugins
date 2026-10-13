async function viewSettings(body, tok){
const desc = el("input", {type: "text", style: "flex:1;min-width:160px", autocomplete: "off"}); desc.value = R.meta.description || "";
const dsave = el("button", {class: "primary", onclick: async () => { try{ await gapi("POST", "describe", {name: R.name, text: desc.value.trim()}); setFlash("Description saved."); await reloadRepo(); }catch(e){ rstatus(e.message, true); } }}, "Save");
const bsel = el("select", {}); branches().forEach(b => bsel.append(el("option", {value: short(b.name)}, short(b.name)))); bsel.value = defBranch();
const bsave = el("button", {class: "primary", onclick: () => refAction(() => gapi("POST", "head", {name: R.name, branch: bsel.value}))}, "Save");
const gen = card(el("h2", {}, "Repository"),
el("div", {class: "k"}, "Description"), el("div", {class: "newref"}, desc, dsave),
el("div", {class: "k"}, "Default branch"), el("div", {class: "newref"}, bsel, bsave),
el("div", {class: "bar", style: "margin:4px 0 0"}, el("button", {onclick: () => renameDialog(R.name)}, "Rename repository")));
const line = el("div", {class: "note"}, el("span", {class: "spin"}), "Reading storage info (takes a few seconds)…"), pline = el("div", {class: "note"});
const packBtn = el("button", {class: "primary"}, "Pack loose objects"), cntBtn = el("button", {}, "Refresh");
const show = async force => {
line.innerHTML = ""; line.className = "note"; line.append(el("span", {class: "spin"}), "Reading storage info (takes a few seconds)…");
try{
const st = await storageInfo(force); if(tok !== navTok) return;
line.textContent = "Packs: " + st.packs + (st.pack_kib != null ? " (" + fmtSize(st.pack_kib * 1024) + ")" : "") + " · loose objects: " + st.loose +
(st.loose > 0 ? ". Updating in the background" + (st.loose >= 100 ? ", or pack them now." : ".") : ".");
}catch(e){ line.className = "note status err"; line.textContent = "Storage info: " + e.message; }
};
cntBtn.onclick = () => show(true);
show(true); refreshMeta();
packBtn.onclick = async () => {
packBtn.disabled = true; cntBtn.disabled = true;
let total = 0, merged = 0, idle = 0;
try{
for(let i = 0; i < 1000; i++){
pline.textContent = "Packing… " + total + " objects packed, " + merged + " packs merged. One step can take up to a minute.";
const j = await gapi("POST", "pack", {name: R.name});
total += j.packed || 0; merged += j.merged || 0;
if(!j.more) break;
idle = (j.packed > 0 || j.merged > 0) ? 0 : idle + 1;
if(idle >= 5) throw new Error("the Flipper made no progress");
if(tok !== navTok) return;
}
R.hub.storage = null;
setFlash("Packed " + total + " loose objects, merged " + merged + " packs."); await reloadRepo();
}catch(e){
pline.className = "note status err"; pline.textContent = "Pack failed after " + total + " objects: " + e.message;
packBtn.disabled = false; cntBtn.disabled = false;
}
};
const sto = card(el("h2", {}, "Storage"), line,
el("div", {class: "bar", style: "margin:6px 0"}, packBtn, cntBtn), pline,
el("div", {class: "note"}, "Clones of repos with many loose objects are slow. You can also pack them on the PC with ", el("code", {}, "git flipper gc " + R.name), "."));
const dz = card(el("h2", {}, "Danger zone"), note("Deleting removes the repository, its issues, releases and wiki from the Flipper.", ""),
el("div", {class: "bar", style: "margin:8px 0 0"}, el("button", {class: "danger", onclick: () => deleteDialog(R.name)}, "Delete this repository")));
body.append(gen, sto, dz);
}
