/* Repo pictures. No firmware route: the picture is an ordinary file on the SD card,
 * /ext/apps_data/gitphin/git/<name>.git/hub/avatar.png, moved with the file API
 * (PUT /api/file, GET /api/file, POST /api/mkdir, POST /api/delete).
 * The browser shrinks it to at most 128x128 and under 16 KB before the upload. */
const AV_MAX_PX = 128, AV_MAX_BYTES = 16 * 1024, AV_IN_MAX = 20 * 1024 * 1024;
const avCache = new Map();   // repo name -> object URL of its picture, or "" when it has none
const avPending = new Map(); // repo name -> promise of a running read
let avChain = Promise.resolve();
function avDir(name){ return "/ext/apps_data/gitphin/git/" + name + ".git/hub"; }
function avPath(name){ return avDir(name) + "/avatar.png"; }
// the picture of a repo as an object URL, or "" (cached; the request is queued by the page fetch)
function avRead(name){
if(avCache.has(name)) return Promise.resolve(avCache.get(name));
if(avPending.has(name)) return avPending.get(name);
const p = (async () => {
let url = "";
try{
const r = await fetch("/api/file?" + q(avPath(name)));
if(r.ok){ const b = await r.blob(); if(b.size && b.size <= 4 * AV_MAX_BYTES) url = URL.createObjectURL(new Blob([b], {type: "image/png"})); }
}catch(_){}
avCache.set(name, url); avPending.delete(name);
return url;
})();
avPending.set(name, p);
return p;
}
// show the picture in box (a span.av with the generic repo icon), or the icon when there is none
function avShow(box, url){
const old = box.querySelector("img"); if(old) old.remove();
const svg = box.querySelector("svg");
if(url){ box.append(el("img", {src: url, alt: ""})); svg.style.display = "none"; }
else svg.style.display = "";
}
const avIdle = () => new Promise(res => { const t = () => (reqRunning || reqQueue.length) ? setTimeout(t, 250) : res(); t(); });
// fill box with the picture of name; the reads go one by one and wait until the page has no other request
function avFill(box, name){
if(avCache.has(name)){ avShow(box, avCache.get(name)); return; }
avChain = avChain.then(async () => {
if(!box.isConnected) return;
await avIdle();
if(box.isConnected) avShow(box, await avRead(name));
}).catch(() => {});
}
// repo icon for the list (size 32) or the header (size 48)
function avBox(name, size){
const box = el("span", {class: "av s" + size}, ic("repo", "l"));
avFill(box, name);
return box;
}
// the browser picture -> PNG, at most 128x128 and 16 KB; smaller sizes are tried when it is too big
function avShrink(file){
return new Promise((resolve, reject) => {
if(file.size > AV_IN_MAX) return reject(new Error("The picture is too big (max 20 MB)."));
const url = URL.createObjectURL(file), im = new Image();
im.onerror = () => { URL.revokeObjectURL(url); reject(new Error("The browser cannot read this picture.")); };
im.onload = async () => {
URL.revokeObjectURL(url);
const w = im.naturalWidth, h = im.naturalHeight;
if(!w || !h) return reject(new Error("The browser cannot read this picture."));
for(const max of [AV_MAX_PX, 96, 64, 48, 32]){
const k = Math.min(1, max / Math.max(w, h)), c = document.createElement("canvas");
c.width = Math.max(1, Math.round(w * k)); c.height = Math.max(1, Math.round(h * k));
c.getContext("2d").drawImage(im, 0, 0, c.width, c.height);
const b = await new Promise(r => c.toBlob(r, "image/png"));
if(b && b.size <= AV_MAX_BYTES) return resolve(b);
}
reject(new Error("The picture is too detailed for 16 KB. Use a simpler one."));
};
im.src = url;
});
}
function avSet(name, url){
const old = avCache.get(name); if(old) URL.revokeObjectURL(old);
avCache.set(name, url);
if(R && R.name === name) avShow($("rv-avatar"), url);
}
async function avUpload(name, file){
const png = await avShrink(file);
try{ await api("POST", "/api/mkdir?" + q(avDir(name))); }catch(_){}   // hub/ may not exist yet
await putFile("/api/file?" + q(avPath(name)), png, () => {});
avSet(name, URL.createObjectURL(png));
}
async function avRemove(name){
await api("POST", "/api/delete?" + q(avPath(name)));
avSet(name, "");
}
function avDialog(name){
const d = el("dialog", {class: "dlg"});
const prev = el("span", {class: "av"}, ic("repo", "l"));
const msg = el("div", {class: "hint"}, "A picture for this repository. It is shrunk to 128 px and 16 KB in the browser.");
const file = el("input", {type: "file", accept: "image/png,image/jpeg,image/gif,image/webp", hidden: true});
const up = el("button", {type: "button", class: "primary"}, ic("upload"), "Upload picture");
const rm = el("button", {type: "button", class: "danger"}, ic("trash"), "Remove");
const close = el("button", {type: "button"}, "Close");
const err = e => toast(e.message, "err");
const sync = () => { const u = avCache.get(name) || ""; avShow(prev, u); rm.hidden = !u; };
up.onclick = () => file.click();
file.onchange = async () => {
const f = file.files[0]; file.value = ""; if(!f) return;
up.disabled = true;
try{ await avUpload(name, f); toast("Picture saved", "ok"); sync(); }catch(e){ err(e); }
up.disabled = false;
};
rm.onclick = async () => {
const cf = await confirmDialog("Remove picture", "Remove the picture of " + name + "?", "Remove", {danger: true});
if(!cf.ok) return;
try{ await avRemove(name); toast("Picture removed", "ok"); sync(); }catch(e){ err(e); }
};
close.onclick = () => d.close();
d.append(el("h2", {}, "Avatar"), el("div", {class: "av-prev"}, prev, msg), file,
el("div", {class: "btns"}, rm, close, up));
d.onclose = () => d.remove();
document.body.appendChild(d); d.showModal();
rm.hidden = true;
avRead(name).then(sync);
}
