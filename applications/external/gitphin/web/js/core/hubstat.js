const netStats = {net: 0, hits: 0};
function renderHubStat(){
const box = document.getElementById("hubstat"); if(!box) return;
const now = performance.now();
let age = 0; inflight.forEach(t => { age = Math.max(age, (now - t) / 1000); });
let cls = "unknown", txt = "", title = "Flipper status unknown";
const h = hubStat;
if(h && h.error === "net"){ cls = "stuck"; txt = "not answering"; title = "The bridge does not answer"; }
else if(h && h.connected === false){ cls = "dead"; txt = "not answering"; title = "App not running / reconnecting"; }
else if(h && h.connected){
let busy = h.busy_s == null ? null : Number(h.busy_s);
if(age > 2) busy = Math.max(busy == null ? 0 : busy, age);
if(busy == null || busy <= 0){ cls = "idle"; txt = "idle"; title = "Flipper idle"; }
else if(busy < 10){ cls = "work"; txt = "working…"; title = "Flipper is working"; }
else if(busy < 60){ cls = "slow"; txt = "working " + Math.floor(busy) + " s"; title = "Flipper is busy for " + Math.floor(busy) + " s"; }
else{ cls = "stuck"; txt = "not answering"; title = "No answer for " + Math.floor(busy) + " s. The app may be stuck."; }
}else if(age > 2){ cls = "work"; txt = "working…"; title = "A request is taking long"; }
if(box.className !== "hs " + cls) box.className = "hs " + cls;
document.getElementById("hubtxt").textContent = txt;
box.title = title;
}
async function pollHub(){
if(document.visibilityState !== "visible" || hubPolling) return;
hubPolling = true;
try{
const ac = new AbortController(), t = setTimeout(() => ac.abort(), 4000);
const r = await _fetch("/pc/hub", {signal: ac.signal, cache: "no-store"}); clearTimeout(t);
hubStat = r.ok ? await r.json() : null;   // no /pc/hub route (older bridge): stay neutral
}catch(_){ hubStat = {error: "net"}; }
hubPolling = false; renderHubStat();
}
setInterval(pollHub, 2000);
document.addEventListener("visibilitychange", () => { if(document.visibilityState === "visible") pollHub(); });
pollHub(); renderHubStat();
