async function loadInfo(){
try{
const i = await (await fetch("/api/info")).json();
$("dev").textContent = i.name;
const cards = [
["Device", i.name],
["Firmware", i.firmware],
["Battery", i.battery + "%" + (i.charging ? " ⚡" : "")],
["SD free", i.sd_ok ? fmtSize(i.sd_free_kib*1024) + " / " + fmtSize(i.sd_total_kib*1024) : "no card"],
["Free RAM", fmtSize(i.heap_free)],
["Uptime", fmtUptime(i.uptime_s)],
];
$("info").innerHTML = "";
for(const [k, v] of cards){
const c = document.createElement("div"); c.className = "card";
c.innerHTML = '<div class="k"></div><div class="v"></div>';
c.children[0].textContent = k; c.children[1].textContent = v;
$("info").appendChild(c);
}
}catch(e){ status("Info: " + e.message, true); }
}
function renderCrumbs(){
const c = $("crumbs"); c.innerHTML = "";
const parts = cwd.split("/").filter(Boolean);
parts.forEach((p, idx) => {
const a = document.createElement("a"); a.href = "#"; a.textContent = p;
const target = "/" + parts.slice(0, idx + 1).join("/");
a.onclick = e => { e.preventDefault(); go(target); };
c.append("/", a);
});
}
async function go(path){
cwd = path; renderCrumbs(); status("Loading…");
try{
const j = await api("GET", "/api/list?" + q(path));
const rows = j.entries
.filter(f => !f.name.endsWith(".part"))  // hide uploads in progress
.sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name));
const tb = $("list"); tb.innerHTML = "";
if(path.split("/").filter(Boolean).length > 1){
const tr = tb.insertRow(); const td = tr.insertCell(); td.className = "name"; td.colSpan = 3;
const a = document.createElement("a"); a.href = "#"; a.textContent = "↑ ..";
a.onclick = e => { e.preventDefault(); go(path.replace(/\/[^/]+$/, "")); };
td.appendChild(a);
}
for(const f of rows){
const full = join(path, f.name);
const tr = tb.insertRow();
const name = tr.insertCell(); name.className = "name";
const a = document.createElement("a");
a.textContent = (f.dir ? "📁 " : "📄 ") + f.name;
if(f.dir){ a.href = "#"; a.onclick = e => { e.preventDefault(); go(full); }; }
else { a.href = "/api/file?" + q(full); a.target = "_blank"; }
name.appendChild(a);
const size = tr.insertCell(); size.className = "size"; size.textContent = f.dir ? "" : fmtSize(f.size);
const act = tr.insertCell(); act.className = "act";
if(!f.dir){
const dl = document.createElement("button"); dl.textContent = "Download";
dl.onclick = () => { location.href = "/api/file?dl=1&" + q(full); };
act.appendChild(dl); act.append(" ");
}
if(gfLocked(full)){
const lk = document.createElement("span"); lk.className = "sys"; lk.textContent = GF_MSG;   // no Delete button
if(!gfApp(full)) lk.textContent = "System folder";
act.appendChild(lk); continue;
}
const del = document.createElement("button"); del.textContent = "Delete";
del.onclick = async () => {
const cf = await confirmDialog("Delete file", "Delete " + full + " from the Flipper?", "Delete", {danger: true});
if(!cf.ok) return;
try{ await api("POST", "/api/delete?" + q(full)); go(cwd); }
catch(e){ status("Delete: " + e.message, true); }
};
act.appendChild(del);
}
status(rows.length ? rows.length + " items" : "Empty folder");
}catch(e){ status("List: " + e.message, true); }
}
$("refresh").onclick = () => { loadInfo(); go(cwd); };
$("mkdir").onclick = async () => {
const name = prompt("Folder name");
if(!name) return;
try{ await api("POST", "/api/mkdir?" + q(join(cwd, name))); go(cwd); }
catch(e){ status("New folder: " + e.message, true); }
};
function putFile(url, file, onProgress){
return exclusive(release => {
const pr = new Promise((resolve, reject) => {
const xhr = new XMLHttpRequest();
xhr.open("PUT", url);
xhr.upload.onprogress = e => { if(e.lengthComputable) onProgress(e.loaded, e.total); };
xhr.onload = () => {
let j = {}; try{ j = JSON.parse(xhr.responseText); }catch(_){}
if(xhr.status >= 200 && xhr.status < 300 && j.ok !== false) resolve(j);
else reject(new Error((j && j.error) || ("HTTP " + xhr.status)));
};
xhr.onerror = () => reject(new Error("network error"));
xhr.send(file);
});
pr.then(release, release);
return pr;
});
}
$("upload").onchange = async e => {
const files = [...e.target.files];
e.target.value = "";
if(!files.length) return;
const prog = $("progress"), fill = $("progress-fill"), txt = $("progress-text");
prog.style.display = "block"; fill.style.width = "0";
status("");
let done = 0;
for(const f of files){
done++;
const start = performance.now();
try{
await putFile("/api/file?" + q(join(cwd, f.name)), f, (loaded, total) => {
const pct = total ? Math.round(loaded / total * 100) : 0;
fill.style.width = pct + "%";
const secs = (performance.now() - start) / 1000;
const speed = secs > 0.2 ? fmtSize(loaded / secs) + "/s" : "…";
txt.textContent = `(${done}/${files.length}) ${f.name} · ${pct}% · ${speed}`;
});
}catch(err){
prog.style.display = "none";
status("Upload " + f.name + ": " + err.message, true);
go(cwd);
return;
}
}
fill.style.width = "100%";
txt.textContent = files.length > 1 ? files.length + " files uploaded" : "Uploaded";
setTimeout(() => { prog.style.display = "none"; }, 1500);
go(cwd);
};
