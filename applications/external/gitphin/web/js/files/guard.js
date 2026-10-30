/* Files tab: system folders cannot be deleted (Gitphin and the Flipper need them).
 * The GF block holds pure functions; pc/tests/test_page.sh runs them in node. */
const GF_MSG = "Needed by Gitphin";
/*GF-BEGIN*/
// Gitphin folders: the app writes the page and plugins on start; repos live in git/.
const GF_APP = {"/ext/apps_data/gitphin": 1, "/ext/apps_data/gitphin/git": 1, "/ext/apps_assets/gitphin": 1};
// Parents of those folders (other apps use them too): never delete them either.
const GF_SYS = {"/ext": 1, "/ext/apps_data": 1, "/ext/apps_assets": 1};
function gfNorm(p){
const out = [];
for(const s of String(p || "").split("/")){
if(!s || s === ".") continue;
if(s === "..") out.pop(); else out.push(s);
}
return "/" + out.join("/");
}
function gfApp(p){ return GF_APP[gfNorm(p)] === 1; }
function gfLocked(p){ const n = gfNorm(p); return GF_APP[n] === 1 || GF_SYS[n] === 1; }
// path= of a /api/delete request URL, or null
function gfDeletePath(url){
const m = /^\/api\/delete\?(.*)$/.exec(String(url || ""));
if(!m) return null;
for(const part of m[1].split("&")){
if(part.indexOf("path=") !== 0) continue;
try{ return decodeURIComponent(part.slice(5).replace(/\+/g, " ")); }catch(_){ return part.slice(5); }
}
return null;
}
/*GF-END*/
