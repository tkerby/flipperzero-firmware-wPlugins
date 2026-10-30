const blobCache = new Map();
async function fetchBlob(sha, maxView){
if(!sha || /^0+$/.test(sha)) return {text: "", size: 0};
const key = R.name + "/" + sha;
if(blobCache.has(key)) return blobCache.get(key);
const r = await fetch("/api/git/blob?" + enc({name: R.name, sha}));
let out;
if(r.status === 413) out = {tooLarge: true};
else if(!r.ok){ const j = await r.json().catch(() => ({})); throw new Error(j.error || ("HTTP " + r.status)); }
else out = decodeBlob(await r.arrayBuffer());
if(blobCache.size > 40) blobCache.delete(blobCache.keys().next().value);
blobCache.set(key, out);
return out;
}
const MAX_VIEW = 512 * 1024;
function decodeBlob(ab){
const u = new Uint8Array(ab), size = u.length;
if(size > MAX_VIEW) return {tooLarge: true, size};
const head = u.subarray(0, 8000);
for(let i = 0; i < head.length; i++) if(head[i] === 0) return {binary: true, size};
return {text: new TextDecoder("utf-8").decode(u), size};
}
