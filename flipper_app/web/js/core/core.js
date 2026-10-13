const $ = id => document.getElementById(id);
let cwd = "/ext";
const inflight = new Map();
let inflightId = 0, hubStat = null, hubPolling = false;
const _fetch = window.fetch.bind(window);
const reqQueue = []; let reqRunning = false;
function exclusive(fn){ return new Promise((res, rej) => { reqQueue.push({fn, res, rej}); pumpQueue(); setProg(); }); }
function pumpQueue(){
if(reqRunning || !reqQueue.length) return;
reqRunning = true;
const j = reqQueue.shift(), id = ++inflightId; inflight.set(id, performance.now());
setTimeout(renderHubStat, 2100);
let done = false;
const release = () => { if(done) return; done = true; inflight.delete(id); reqRunning = false; renderHubStat(); pumpQueue(); setProg(); };
let p; try{ p = Promise.resolve(j.fn(release)); }catch(e){ release(); j.rej(e); return; }
p.then(j.res, e => { release(); j.rej(e); });
}
