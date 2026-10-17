function toast(msg, kind){
const t = el("div", {class: "toast " + (kind || ""), role: "status"}, ic(kind === "err" ? "alert" : kind === "ok" ? "check" : "info", "l"), el("div", {}, msg));
t.onclick = () => t.remove(); $("toasts").append(t);
while($("toasts").children.length > 4) $("toasts").firstChild.remove();
setTimeout(() => t.remove(), kind === "err" ? 9000 : 4500);
}
function setProg(){ $("prog").classList.toggle("on", reqRunning || reqQueue.length > 0); }
function curTheme(){ return document.documentElement.dataset.theme || (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light"); }
function applyTheme(t, save){
document.documentElement.dataset.theme = t;
if(save) try{ localStorage.setItem("gp.theme", t); }catch(_){}
const b = $("themebtn"); b.innerHTML = ""; b.append(ic(t === "dark" ? "sun" : "moon", "l"));
}
(function initTheme(){
let t = new URLSearchParams(location.search).get("theme");
if(t !== "dark" && t !== "light"){ try{ t = localStorage.getItem("gp.theme"); }catch(_){ t = null; } }
applyTheme(t === "dark" || t === "light" ? t : curTheme(), false);
$("themebtn").onclick = () => applyTheme(curTheme() === "dark" ? "light" : "dark", true);
})();
