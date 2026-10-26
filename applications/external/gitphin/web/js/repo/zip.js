/* "Download ZIP" on the repo page: the PC bridge streams a .zip of the ref (GET /pc/zip?name=&ref=). */
const ZIP_REF_VIEWS = ["tree", "blob", "commits", "search", "edit", "new", "upload", "delete"];
// the ref of the open view (branch or tag name), else the default branch; null for an empty repo
function zipRef(view, rest){
if(!R.refs.length) return null;
return (ZIP_REF_VIEWS.includes(view) && rest[1]) || defBranch();
}
function zipUrl(name, ref){ return "/pc/zip?" + enc({name, ref}); }
async function zipDownload(name, ref){
const b = $("rv-zip"); b.disabled = true;
try{
const r = await fetch(zipUrl(name, ref));
if(!r.ok){ const j = await r.json().catch(() => ({})); throw new Error(j.error || ("HTTP " + r.status)); }
const blob = await r.blob();
const a = el("a", {href: URL.createObjectURL(blob), download: name + "-" + ref.replace(/[^A-Za-z0-9._-]+/g, "-") + ".zip"});
document.body.append(a); a.click(); a.remove();
setTimeout(() => URL.revokeObjectURL(a.href), 60000);
}catch(e){ toast("ZIP: " + e.message, "err"); }
b.disabled = false;
}
