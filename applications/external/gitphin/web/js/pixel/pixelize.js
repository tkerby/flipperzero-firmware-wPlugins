function pixelize(e){
if(!e.matches || !e.matches(PXQ)) return;
const s = pxScale(e);
for(const n of [...e.childNodes]){
if(n.nodeType !== 3) continue;
const raw = n.nodeValue.trim(); if(!raw) continue;
const t = raw.replace(/…/g, "...").replace(/·/g, "-").replace(/\s+/g, " ");
if(!/^[\x20-\x7e]{1,60}$/.test(t)) continue;
const w = document.createElement("span"), v = document.createElement("span");
w.className = "pxt"; w.append(pxSvg(t, s)); v.className = "vh"; v.textContent = raw;
n.replaceWith(w, v);
}
}
new MutationObserver(ms => {
for(const m of ms) for(const n of m.addedNodes){
if(n.nodeType === 1){ pixelize(n); if(n.querySelectorAll) n.querySelectorAll(PXQ).forEach(pixelize); }
else if(n.nodeType === 3 && n.parentNode && n.parentNode.nodeType === 1) pixelize(n.parentNode);
}
}).observe(document.body, {childList: true, subtree: true});
document.querySelectorAll(PXQ).forEach(pixelize);
