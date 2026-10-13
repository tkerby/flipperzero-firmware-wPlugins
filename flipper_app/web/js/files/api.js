function fmtSize(n){
if(n < 1024) return n + " B";
const u = ["KiB","MiB","GiB"]; let i = -1;
do { n /= 1024; i++; } while(n >= 1024 && i < u.length - 1);
return n.toFixed(n < 10 ? 1 : 0) + " " + u[i];
}
function fmtUptime(s){
const d = Math.floor(s/86400), h = Math.floor(s%86400/3600), m = Math.floor(s%3600/60);
return (d ? d + "d " : "") + h + "h " + m + "m";
}
function status(msg, err){ const s = $("status"); s.textContent = msg || ""; s.className = "status" + (err ? " err" : ""); }
function q(path){ return "path=" + encodeURIComponent(path); }
function join(dir, name){ return dir.replace(/\/$/, "") + "/" + name; }
async function api(method, url, body){
const dp = gfDeletePath(url);   // last guard: no call may delete a system folder
if(dp !== null && gfLocked(dp)){ toast(GF_MSG, "err"); throw new Error(GF_MSG); }
const r = await fetch(url, {method, body});
const j = await r.json().catch(() => ({ok:false, error:"bad answer (" + r.status + ")"}));
if(!r.ok || j.ok === false) throw new Error(j.error || r.statusText);
return j;
}
