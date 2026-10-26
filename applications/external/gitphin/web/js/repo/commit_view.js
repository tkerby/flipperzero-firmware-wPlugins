async function viewCommit(body, sha, tok){
body.className = "fill";
const hold = el("div", {class: "card", style: "margin-bottom:12px;flex:none"}, skel(3, 50)); body.append(hold);
const j = await gapi("GET", "commit", {name: R.name, sha});
if(tok !== navTok) return;
const cm = (j.commit && typeof j.commit === "object") ? j.commit : j;
const lines = (cm.message || "").replace(/\n+$/, "").split("\n");
const c = el("section", {class: "card", style: "margin-bottom:12px;flex:none;max-height:40%;overflow:auto"},
el("div", {class: "title-row"}, ic("commit", "l"), el("h2", {}, lines[0] || "(no message)")),
lines.length > 1 ? el("pre", {class: "snip", style: "white-space:pre-wrap;max-height:120px;overflow:auto"}, lines.slice(1).join("\n").replace(/^\n+/, "")) : null,
el("div", {class: "kv small"},
el("span", {class: "shachip"}, ic("commit"), el("code", {}, cm.sha), copyBtn(cm.sha, "Copy full SHA")),
el("span", {}, ic("user"), " ", cm.author.name, " ", el("span", {class: "muted"}, "committed ", timeEl((cm.committer && cm.committer.time ? cm.committer.time : cm.author.time) * 1000))),
cm.committer && (cm.committer.name !== cm.author.name) ? el("span", {class: "muted"}, "committer " + cm.committer.name) : null,
el("span", {}, el("span", {class: "muted"}, cm.parents.length === 1 ? "parent " : "parents "), ...(cm.parents.length ? cm.parents.flatMap((p, i) => [i ? ", " : "", link(commitHash(p), p.slice(0, 7))]) : ["none (root)"])),
link(treeHash(cm.sha, ""), "Browse files")));
hold.replaceWith(c);
await diffSection(body, {sha: cm.sha}, tok);
}
async function diffSection(body, params, tok, fixed){
const split = el("div", {class: "split" + (fixed ? " h" : "")});
const lb = el("div", {class: "pb fl"}, skel(6, 80)), rb = el("div", {class: "pb"});
const lh = el("div", {class: "ph"}, ic("file"), el("span", {class: "px2"}, "Changed files"));
const rh = el("div", {class: "ph"}, ic("code"), el("span", {class: "px2"}, "Changes"), el("span", {class: "sp"}));
split.append(el("section", {class: "pane"}, lh, lb), el("section", {class: "pane"}, rh, rb));
body.append(split);
const j = await gapi("GET", "diff", Object.assign({name: R.name}, params));
if(tok !== navTok) return;
lb.innerHTML = "";
const files = j.files || [];
const add = files.filter(f => f.status === "A").length, del = files.filter(f => f.status === "D").length;
lh.append(el("span", {class: "sp"}), el("span", {class: "small"}, files.length + (j.truncated ? "+" : "") + " | +" + add + " -" + del));
if(!files.length){ rb.append(emptyState("check", "No file changes in this commit.")); return; }
const loaders = [], secs = [];
let shown = 0;
const moreL = el("a", {class: "tr", href: "#"}, el("span", {class: "muted"}, ""));
const addFile = (f, idx) => {
const area = el("div", {}); let state = "idle";
const btn = el("button", {class: "sm"}, "Show diff");
const head = el("div", {class: "fhead"}, el("span", {class: "st " + f.status}, f.status), el("span", {class: "p"}, f.path), btn);
const sec = el("div", {class: "fsec"}, head, area); secs[idx] = sec; rb.insertBefore(sec, rb.querySelector("#dhint"));
const open = async () => {
if(state === "open"){ area.hidden = true; state = "closed"; btn.textContent = "Show diff"; return; }
if(state === "closed"){ area.hidden = false; state = "open"; btn.textContent = "Hide diff"; return; }
if(state === "loading") return;
state = "loading"; btn.disabled = true; area.innerHTML = ""; area.append(el("div", {class: "note", style: "padding:0 12px 8px"}, el("span", {class: "spin"}), "Loading diff..."));
try{ await fileDiff(area, f, tok); }catch(e){ area.innerHTML = ""; area.append(errBox(e.message)); }
state = "open"; btn.disabled = false; btn.textContent = "Hide diff";
};
btn.onclick = open; loaders[idx] = () => state === "idle" ? open() : null;
const name = f.path.split("/").pop(), dir = dirOf(f.path);
const row = el("a", {class: "tr", href: "#", title: f.path}, el("span", {class: "st " + f.status}, f.status), el("span", {class: "fp"}, name, dir ? el("small", {}, " " + dir) : null));
row.onclick = ev => { ev.preventDefault(); lb.querySelectorAll(".tr.on").forEach(x => x.classList.remove("on")); row.classList.add("on"); sec.scrollIntoView({block: "start"}); open(); };
lb.insertBefore(row, moreL.parentNode ? moreL : null);
};
rb.append(el("div", {class: "note", style: "padding:8px 12px", id: "dhint"}, "Pick a file on the left, or use Show all diffs."));
const PAGE = 100;
const renderMore = () => {
const end = Math.min(files.length, shown + PAGE);
for(; shown < end; shown++) addFile(files[shown], shown);
moreL.remove();
if(shown < files.length){
moreL.firstChild.textContent = "Show " + Math.min(PAGE, files.length - shown) + " more of " + (files.length - shown) + "...";
lb.append(moreL);
}
};
moreL.onclick = ev => { ev.preventDefault(); renderMore(); };
renderMore();
if(files.length > 1){
const all = el("button", {class: "sm"}, "Show all diffs");
all.onclick = async () => { all.disabled = true; for(let i = 0; i < loaders.length; i++){ if(tok !== navTok) return; if(loaders[i]) await loaders[i](); } all.remove(); };
rh.append(all);
}
}
