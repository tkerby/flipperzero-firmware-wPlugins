function formDialog(title, fields, submitText, onSubmit, opts){
opts = opts || {};
const d = el("dialog", {class: "dlg"});
const form = el("form", {method: "dialog"});
const get = {};
form.append(el("h2", {}, title));
if(opts.message) form.append(el("div", {class: "msg"}, opts.message));
let first = null;
for(const f of fields){
const type = f.type || "text";
let node, ctl;
if(type === "textarea"){ ctl = el("textarea", {spellcheck: "false"}); ctl.value = f.value || ""; get[f.key] = () => ctl.value.trim(); }
else if(type === "select"){
ctl = el("select", {}); (f.options || []).forEach(o => { const op = el("option", {value: o.value}, o.label); ctl.append(op); });
if(f.value != null) ctl.value = f.value; get[f.key] = () => ctl.value;
}else if(type === "checks"){
ctl = el("div", {}); const boxes = [];
(f.options || []).forEach(o => { const c = el("input", {type: "checkbox"}); c.checked = (f.value || []).includes(o.value); boxes.push([c, o.value]);
ctl.append(el("label", {class: "chk"}, c, o.label)); });
if(!(f.options || []).length) ctl.append(el("div", {class: "hint"}, "Nothing to choose from yet."));
get[f.key] = () => boxes.filter(b => b[0].checked).map(b => b[1]);
}else if(type === "check"){
const c = el("input", {type: "checkbox"}); c.checked = !!f.value; get[f.key] = () => c.checked;
form.append(el("label", {class: "chk"}, c, f.text)); continue;
}else{
ctl = el("input", {type: "text", autocomplete: "off", spellcheck: "false"}); ctl.value = f.value || ""; get[f.key] = () => ctl.value.trim();
}
if(!first && type !== "checks") first = ctl;
form.append(el("label", {}, el("span", {}, f.label), ctl));
if(f.hint) form.append(el("div", {class: "hint"}, f.hint));
f._ctl = ctl;
}
const err = el("div", {class: "status err"});
const ok = el("button", {type: "submit", class: opts.danger ? "danger" : "primary"}, submitText);
const cancel = el("button", {type: "button", onclick: () => d.close()}, "Cancel");
form.append(err, el("div", {class: "btns"}, cancel, ok));
const lastCtl = fields.length ? fields[fields.length - 1]._ctl : null;
if(opts.confirm){
ok.disabled = true;
lastCtl.oninput = e => { ok.disabled = e.target.value !== opts.confirm; };
}
form.onsubmit = async e => {
e.preventDefault(); err.textContent = ""; ok.disabled = true;
const vals = {}; for(const k in get) vals[k] = get[k]();
try{ await onSubmit(vals); d.close(); }
catch(x){ err.textContent = x.message; ok.disabled = !!opts.confirm && lastCtl.value !== opts.confirm; }
};
d.onclose = () => { d.remove(); if(opts.onClose) opts.onClose(); };
d.append(form); document.body.appendChild(d); d.showModal();
(first || ok).focus();
}
function confirmDialog(title, msg, okText, opts){
opts = opts || {};
return new Promise(res => {
let done = false;
formDialog(title, opts.check ? [{key: "c", type: "check", text: opts.check, value: false}] : [], okText,
async v => { done = true; res({ok: true, checked: !!v.c}); },
{danger: opts.danger, message: msg, onClose: () => { if(!done) res({ok: false, checked: false}); }});
});
}
function newRepoDialog(){
formDialog("New repo", [
{key: "name", label: "Name", hint: "Letters, digits, . _ -  (max 64). Must not start with . or end with .git"},
{key: "description", label: "Description"},
{key: "branch", label: "Default branch", value: "main"},
], "Create repo", async v => {
if(!validRepoName(v.name)) throw new Error("Invalid repo name");
if(v.branch && !validRefName(v.branch)) throw new Error("Invalid branch name");
const p = {name: v.name, branch: v.branch || "main"};
if(v.description) p.description = v.description;
await gapi("POST", "create", p);
go2("#repos/" + encodeURIComponent(v.name));
});
}
function renameDialog(name){
formDialog("Rename " + name, [{key: "to", label: "New name", value: name}], "Rename", async v => {
if(!validRepoName(v.to)) throw new Error("Invalid repo name");
if(v.to === name) throw new Error("Same name");
await gapi("POST", "rename", {name, to: v.to});
if(R && R.name === name) R = null;
go2("#repos/" + (inRepoView() ? encodeURIComponent(v.to) : ""));
});
}
function describeDialog(name, text){
formDialog("Description of " + name, [{key: "text", label: "Description", value: text || ""}], "Save", async v => {
await gapi("POST", "describe", {name, text: v.text});
if(inRepoView()) await reloadRepo(); else loadRepoList();
});
}
function deleteDialog(name){
formDialog("Delete " + name, [{key: "typed", label: "Type the repo name to confirm", hint: "This removes the repo and all its data from the Flipper. It cannot be undone."}],
"Delete this repo", async () => {
await gapi("POST", "delete", {name});
R = null; go2("#repos");
}, {danger: true, confirm: name});
}
