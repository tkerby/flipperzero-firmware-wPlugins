const shell = {tab: "files", repos: [], repo: null, key: "code", counts: {}};
const SECS = [["code", "code", "Code"], ["commit", "commits", "Commits"], ["branch", "branches", "Branches"], ["tag", "tags", "Tags"],
["issue-open", "issues", "Issues"], ["pull-request", "pulls", "Pull requests"], ["release", "releases", "Releases"], ["book", "wiki", "Wiki"], ["gear", "settings", "Settings"]];
function sideLink(href, icon, label, on, px, n){
const a = el("a", {class: "si" + (on ? " on" : ""), href, title: label}, ic(icon, "l"), el("span", {class: "lb" + (px ? " px" : "")}, label));
if(n) a.append(el("span", {class: "n"}, String(n)));
return a;
}
function renderSide(){
const side = $("side"); if(!side) return;
side.innerHTML = "";
side.append(el("div", {class: "grp"}, "Device"), sideLink("#files", "folder", "Files", shell.tab === "files", true), sideLink("#collector", "info", "Collector", shell.tab === "collector", true));
side.append(el("div", {class: "grp"}, "Repositories"), sideLink("#repos", "repo", "All repositories", shell.tab === "repos" && !shell.repo, true));
shell.repos.forEach(n => side.append(sideLink("#repos/" + encodeURIComponent(n), "book", n, shell.repo === n && false)));
if(shell.tab === "repos" && shell.repo){
side.append(el("div", {class: "grp"}, "This repository"));
for(const [icon, key, label] of SECS){
const href = "#repos/" + encodeURIComponent(shell.repo) + (key === "code" ? "" : "/" + key);
side.append(sideLink(href, icon, label, shell.key === key, true, shell.counts[key]));
}
}
renderCrumb();
}
function renderCrumb(){
const c = $("crumb"); if(!c) return; c.innerHTML = "";
const sep = () => ic("chevron-right");
if(shell.tab !== "repos"){ c.append(el("b", {}, shell.tab === "files" ? "Files" : "Collector")); return; }
c.append(el("a", {href: "#repos"}, "Repositories"));
if(shell.repo){
const lab = (SECS.find(s => s[1] === shell.key) || [0, 0, ""])[2];
c.append(sep(), el("a", {href: "#repos/" + encodeURIComponent(shell.repo)}, shell.repo));
if(lab) c.append(sep(), el("b", {}, lab));
}
}
$("menubtn").onclick = () => document.body.classList.toggle("nav-open");
$("scrim").onclick = () => document.body.classList.remove("nav-open");
$("side").addEventListener("click", e => { if(e.target.closest("a")) document.body.classList.remove("nav-open"); });
let repoFilter = "";
$("gform").onsubmit = e => {
e.preventDefault(); const q = $("gq").value.trim();
if(shell.repo && R && R.name === shell.repo){ if(q) go2(hs("search", lastRef || defBranch()) + "?q=" + encodeURIComponent(q)); }
else{ repoFilter = q; go2("#repos"); if(location.hash === "#repos") route(); }
};
document.addEventListener("keydown", e => {
if(e.key === "/" && !e.ctrlKey && !e.metaKey && !/^(INPUT|TEXTAREA|SELECT)$/.test((document.activeElement || {}).tagName || "")){ e.preventDefault(); $("gq").focus(); }
else if(e.key === "Escape"){ document.body.classList.remove("nav-open"); if(document.activeElement === $("gq")) $("gq").blur(); }
});
