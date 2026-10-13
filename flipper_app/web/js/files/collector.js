let collectorLoaded = false;
function showTab(name){
document.querySelectorAll(".panel").forEach(p => p.classList.toggle("active", p.id === "panel-" + name));
shell.tab = name; if(name !== "repos") shell.repo = null; renderSide();
if(name === "collector" && !collectorLoaded){ collectorLoaded = true; loadCollector(); }
}
function row(k, v){
const d = document.createElement("div"); d.className = "row";
const kd = document.createElement("div"); kd.className = "k"; kd.textContent = k;
const vd = document.createElement("div"); vd.className = "val"; vd.textContent = v;
d.append(kd, vd); return d;
}
function fmtUp(s){
if(s == null) return "—";
const d = Math.floor(s/86400), h = Math.floor(s%86400/3600), m = Math.floor(s%3600/60);
return (d ? d + "d " : "") + h + "h " + m + "m";
}
async function loadPC(){
const box = $("pc-rows"); box.innerHTML = ""; $("pc-note").textContent = "";
try{
const p = await (await fetch("/pc/info")).json();
box.append(
row("Hostname", p.hostname || "—"),
row("OS", p.os || "—"),
row("Kernel", (p.kernel || "") + (p.arch ? " · " + p.arch : "")),
row("CPU cores", p.cpu_count ?? "—"),
row("Memory", p.mem_total ? fmtSize(p.mem_available) + " free / " + fmtSize(p.mem_total) : "—"),
row("Uptime", fmtUp(p.uptime_s)),
row("User", p.user || "—"),
);
(p.ips || []).forEach(a => box.append(row("IP (" + a.iface + ")", a.cidr)));
(p.connections || []).forEach(c => box.append(row("Network", c.name + " (" + c.type.replace("802-11-wireless","wifi").replace("802-3-ethernet","ethernet") + ")")));
}catch(e){ $("pc-note").textContent = "Could not read PC info: " + e.message; }
}
function secretRow(label, value){
const d = document.createElement("div"); d.className = "row";
const kd = document.createElement("div"); kd.className = "k"; kd.textContent = label;
const wrap = document.createElement("div"); wrap.className = "secret";
const code = document.createElement("code"); code.textContent = "••••••••";
let shown = false;
const toggle = document.createElement("button"); toggle.textContent = "Show";
toggle.onclick = () => { shown = !shown; code.textContent = shown ? value : "••••••••"; toggle.textContent = shown ? "Hide" : "Show"; };
const copy = document.createElement("button"); copy.textContent = "Copy";
copy.onclick = async () => { try{ await navigator.clipboard.writeText(value); copy.textContent = "Copied"; setTimeout(() => copy.textContent = "Copy", 1200); }catch(e){} };
wrap.append(code, toggle, copy);
d.append(kd, wrap); return d;
}
async function loadWifi(){
const box = $("wifi-rows"); box.innerHTML = ""; $("wifi-note").textContent = "";
try{
const w = await (await fetch("/pc/wifi")).json();
if(!w.connected){ box.append(row("Status", "Not connected")); $("wifi-note").textContent = w.note || ""; return; }
box.append(row("Network name", w.ssid || "—"));
if(w.signal != null) box.append(row("Signal", w.signal + "%"));
if(w.rate) box.append(row("Rate", w.rate));
if(w.password){
box.append(secretRow("Password", w.password));
}else{
box.append(row("Password", "needs root"));
}
if(w.note) $("wifi-note").textContent = w.note;
}catch(e){ $("wifi-note").textContent = "Could not read Wi-Fi: " + e.message; }
}
function loadCollector(){ loadPC(); loadWifi(); }
$("collect-refresh").onclick = loadCollector;
