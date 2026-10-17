const HEX40 = /^[0-9a-f]{40}$/;
const memCache = new Map(); let memBytes = 0;
const MEM_CAP = 4 * 1024 * 1024, SS_MAX = 64 * 1024;
function cacheKey(url, o){
if(o && o.method && o.method !== "GET") return null;
let u; try{ u = new URL(url, location.origin); }catch(_){ return null; }
const m = /^\/api\/git\/(blob|commit|diff|tree|log|compare)$/.exec(u.pathname); if(!m) return null;
const g = k => u.searchParams.get(k), x = v => v && HEX40.test(v);
const ok = m[1] === "blob" ? (x(g("sha")) || (x(g("ref")) && g("path") != null))
: m[1] === "commit" ? x(g("sha"))
: m[1] === "diff" ? (x(g("sha")) || (x(g("base")) && x(g("head"))))
: m[1] === "compare" ? (x(g("base")) && x(g("head")))
: x(g("ref"));
return ok && !g("dl") ? u.pathname + u.search : null;
}
function cacheGet(key){
let e = memCache.get(key);
if(e){ memCache.delete(key); memCache.set(key, e); return e; }   // refresh LRU order
try{
const t = sessionStorage.getItem("hubc:" + key);
if(t){ const j = JSON.parse(t); const bin = atob(j.b), u = new Uint8Array(bin.length); for(let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i);
e = {status: j.s, ct: j.c, sha: j.h, buf: u.buffer}; memPut(key, e, true); return e; }
}catch(_){}
return null;
}
function memPut(key, e, skipSession){
const size = e.buf.byteLength;
if(size > MEM_CAP / 2) return;
if(memCache.has(key)) memBytes -= memCache.get(key).buf.byteLength;
memCache.set(key, e); memBytes += size;
while(memBytes > MEM_CAP){ const k = memCache.keys().next().value; memBytes -= memCache.get(k).buf.byteLength; memCache.delete(k); }
if(!skipSession && size < SS_MAX){
try{ const u = new Uint8Array(e.buf); let bin = ""; for(let i = 0; i < u.length; i++) bin += String.fromCharCode(u[i]);
sessionStorage.setItem("hubc:" + key, JSON.stringify({s: e.status, c: e.ct, h: e.sha, b: btoa(bin)})); }catch(_){}
}
}
function cachedResponse(e){
const h = {}; if(e.ct) h["Content-Type"] = e.ct; if(e.sha) h["X-Git-Sha"] = e.sha;
return new Response(e.status === 204 ? null : e.buf.slice(0), {status: e.status, headers: h});
}
window.fetch = (u, o) => {
const url = String((u && u.url) || u);
if(url.indexOf("/pc/hub") === 0) return _fetch(u, o);
const key = cacheKey(url, o);
const hit = key && cacheGet(key);
if(hit){ netStats.hits++; return Promise.resolve(cachedResponse(hit)); }
netStats.net++;
return exclusive(release => _fetch(u, o).then(r => {
try{
r.clone().arrayBuffer().then(buf => {
if(key && (r.status === 200 || r.status === 413)) memPut(key, {status: r.status, ct: r.headers.get("Content-Type"), sha: r.headers.get("X-Git-Sha"), buf});
}).catch(() => {}).then(release);
}catch(_){ release(); }
return r;
}));
};
