function route(){
let parts;
const raw = location.hash.replace(/^#/, ""), qi = raw.indexOf("?");
routeQuery = new URLSearchParams(qi < 0 ? "" : raw.slice(qi + 1));
try{ parts = (qi < 0 ? raw : raw.slice(0, qi)).split("/").map(decodeURIComponent); }catch(_){ parts = []; }
const tab = TABS.includes(parts[0]) ? parts[0] : "files";
showTab(tab);
if(tab !== "repos") return;
if(parts[1]) openRepo(parts[1], parts.slice(2));
else { ++navTok; R = null; shell.repo = null; renderSide(); $("repo-view").hidden = true; $("repos-list-view").hidden = false; loadRepoList(); }
}
window.addEventListener("hashchange", route);
$("repo-new").onclick = newRepoDialog;
$("repos-refresh").onclick = loadRepoList;
$("rv-refresh").onclick = () => reloadRepo();
$("rv-avatar-btn").onclick = () => avDialog(R.name);
$("rv-clone-btn").onclick = () => { const b = $("rv-clonebox"); b.hidden = !b.hidden; $("rv-clone-btn").setAttribute("aria-expanded", String(!b.hidden)); };
$("rv-snippet-copy").onclick = e => copyText($("rv-snippet").textContent, e.target);
