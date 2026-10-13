function inRepoView(){ return !$("repo-view").hidden; }
const FEATURES = {mergeCommit: true};  // POST /api/git/merge (tree merge on the Flipper)
let mergeConflict = null, lastRef = null, routeQuery = new URLSearchParams();
function refSha(ref){   // branch/tag name -> tip sha (from the loaded refs), so answers can be cached
if(HEX40.test(ref)) return ref;
const h = R.refs.find(r => r.name === "refs/heads/" + ref); if(h) return h.sha;
const t = R.refs.find(r => r.name === "refs/tags/" + ref); return t ? (t.peeled || t.sha) : ref;
}
function isBranchRef(ref){ return branches().some(b => short(b.name) === ref); }
let navTok = 0;           // bumps on every navigation; async views stop when it changes
let flash = null;         // message to show after the next route()
function setFlash(msg, err){ flash = {msg, err: !!err}; }
function esc(s){ return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;"); }
function relTime(ms){
if(!isFinite(ms)) return "";
let s = Math.round((Date.now() - ms) / 1000);
if(s < 45) return s < -45 ? "in the future" : "just now";
const u = [["minute", 60], ["hour", 3600], ["day", 86400], ["month", 2592000], ["year", 31536000]];
let best = ["minute", 60];
for(const x of u) if(s >= x[1]) best = x;
const n = Math.floor(s / best[1]);
return n + " " + best[0] + (n === 1 ? "" : "s") + " ago";
}
function timeEl(ms){ const t = el("span", {title: new Date(ms).toLocaleString()}, relTime(ms)); return t; }
function getAuthor(){ try{ return localStorage.getItem("hub.author") || "me"; }catch(_){ return "me"; } }
function getEmail(){ try{ return localStorage.getItem("hub.email") || "hub@flipper.local"; }catch(_){ return "hub@flipper.local"; } }
function setEmail(v){ try{ localStorage.setItem("hub.email", v); }catch(_){} }
function setAuthor(v){ try{ localStorage.setItem("hub.author", v); }catch(_){} }
function hs(...segs){ return "#repos/" + encodeURIComponent(R.name) + segs.map(x => "/" + encodeURIComponent(x)).join(""); }
function pathSegs(path){ return path ? path.split("/") : []; }
function treeHash(ref, path){ return hs("tree", ref, ...pathSegs(path)); }
function blobHash(ref, path){ return hs("blob", ref, ...pathSegs(path)); }
function commitHash(sha){ return hs("commit", sha); }
function link(href, text, cls){ return el("a", {href, class: cls || "lnk"}, text); }
function note(text, cls){ return el("div", {class: "note " + (cls || "")}, text); }
function errBox(msg){ return el("div", {class: "status err"}, msg); }
function card(...kids){ return el("section", {class: "card", style: "margin-bottom:12px"}, ...kids); }
function branches(){ return R.refs.filter(r => r.name.startsWith("refs/heads/")); }
function tags(){ return R.refs.filter(r => r.name.startsWith("refs/tags/")); }
function defBranch(){ return R.head.replace(/^refs\/heads\//, ""); }
function textColor(hex){ const n = parseInt(hex.slice(1), 16), r = n >> 16, g = (n >> 8) & 255, b = n & 255; return (r * 299 + g * 587 + b * 114) / 1000 > 150 ? "#1d1d1b" : "#ffffff"; }
function chip(name){
const l = (R.hub.meta.labels || []).find(x => x.name === name);
const c = l && /^#[0-9a-fA-F]{6}$/.test(l.color) ? l.color : null;
const e = el("span", {class: "chip"}, name);
if(c){ e.style.background = c; e.style.color = textColor(c); } else { e.style.borderColor = "var(--line)"; e.style.color = "var(--muted)"; }
return e;
}
function stateBadge(st){ return el("span", {class: "state " + st}, st[0].toUpperCase() + st.slice(1)); }
function commitTime(c){ return (c.committer && c.committer.time ? c.committer.time : c.author.time) * 1000; }
