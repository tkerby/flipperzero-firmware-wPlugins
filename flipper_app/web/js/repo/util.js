const ZERO = "0".repeat(40);
const TABS = ["files", "collector", "repos"];
const SUBS = ["code", "commits", "branches", "tags", "issues", "pulls", "releases", "wiki", "settings"];
let R = null;       // loaded repo: {name, meta, refs, head}
let curSub = "code";
function el(tag, attrs, ...kids){
const e = document.createElement(tag);
for(const k in (attrs || {})){
if(k === "class") e.className = attrs[k];
else if(k.startsWith("on")) e[k] = attrs[k];
else e.setAttribute(k, attrs[k]);
}
e.append(...kids.filter(x => x != null && x !== false));
return e;
}
function rstatus(msg, err){ if(!msg || /(\u2026|\.\.\.)$/.test(msg)) return; toast(msg, err ? "err" : "ok"); }
function validRepoName(n){ return /^[A-Za-z0-9._-]{1,64}$/.test(n) && n[0] !== "." && !/\.git$/.test(n); }
function validRefName(n){
if(!n || /\.\.|\/\/|@\{/.test(n) || /[\s\x00-\x1f\x7f~^:?*\[\\]/.test(n)) return false;
if(/[\/.]$/.test(n) || n[0] === "/") return false;
return n.split("/").every(c => c && c[0] !== "." && !/\.lock$/.test(c));
}
function enc(o){ return Object.keys(o).map(k => k + "=" + encodeURIComponent(o[k])).join("&"); }
function gapi(method, route, params){ return api(method, "/api/git/" + route + (params ? "?" + enc(params) : "")); }
function short(ref){ return ref.replace(/^refs\/(heads|tags)\//, ""); }
async function copyText(text, btn){
let ok = false;
try{ await navigator.clipboard.writeText(text); ok = true; }
catch(_){
const t = el("textarea", {}); t.value = text; document.body.appendChild(t); t.select();
try{ ok = document.execCommand("copy"); }catch(__){}
t.remove();
}
const old = btn.textContent; btn.textContent = ok ? "Copied" : "Copy failed";
setTimeout(() => btn.textContent = old, 1200);
}
function go2(hash){ if(location.hash === hash) route(); else location.hash = hash; }
