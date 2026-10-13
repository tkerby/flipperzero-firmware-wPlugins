function personLine(p){ return p.name + (p.time ? " · " : ""); }
async function viewCommits(body, ref, tok){
if(!R.refs.length){ emptyRepoNote(body); return; }
ref = ref || defBranch();
const c = card();
c.append(el("div", {class: "tools"}, refSelect(ref, v => go2(hs("commits", v))), el("div", {class: "crumbs muted"}, "First-parent history")));
const list = el("div", {}), more = el("button", {}, "Older"), status2 = el("div", {});
c.append(list, el("div", {class: "bar", style: "margin:10px 0 0"}, more), status2); body.append(c);
let skip = 0;
const N = 30;
async function load(){
more.disabled = true; status2.textContent = "";
try{
const j = await gapi("GET", "log", {name: R.name, ref: refSha(ref), skip, n: N});
if(tok !== navTok) return;
for(const cm of j.commits){
const subj = (cm.message || "").split("\n")[0] || "(no message)";
list.append(el("div", {class: "item"},
el("div", {class: "t"}, link(commitHash(cm.sha), subj)),
el("div", {class: "m"}, cm.author.name + " committed ", timeEl(commitTime(cm)), " · ", el("code", {}, cm.sha.slice(0, 7)))));
}
skip = j.next_skip != null ? j.next_skip : skip + j.commits.length;   // the device may return fewer than n
more.hidden = !j.more;
if(!skip) list.append(note("No commits."));
}catch(e){ status2.className = "status err"; status2.textContent = e.message; }
more.disabled = false;
}
more.onclick = load;
await load();
}
