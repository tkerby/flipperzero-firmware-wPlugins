(function buildSprite(){
let h = '<symbol id="i-logo" viewBox="0 0 34 25"><path d="' + GP_LOGO + '"/></symbol>';
for(const k in PXICONS){
const [ws, hex] = PXICONS[k].split(":"), w = +ws, n = (w + 3) >> 2, rows = hex.length / n;
let d = "";
for(let y = 0; y < rows; y++){
const v = parseInt(hex.substr(y * n, n), 16); let x = 0;
while(x < w){
if(v >> (n * 4 - 1 - x) & 1){ let e = x; while(e + 1 < w && (v >> (n * 4 - 2 - e) & 1)) e++; d += "M" + x + " " + y + "h" + (e - x + 1) + "v1h-" + (e - x + 1) + "z"; x = e + 1; }
else x++;
}
}
h += '<symbol id="i-' + k + '" viewBox="0 0 ' + w + " " + rows + '"><path d="' + d + '"/></symbol>';
}
document.getElementById("sprite").innerHTML = h;
})();
function ic(name, cls){ const s = document.createElementNS(NS, "svg"), u = document.createElementNS(NS, "use"); s.setAttribute("class", "ic" + (cls ? " " + cls : "")); u.setAttribute("href", "#i-" + name); s.append(u); return s; }
const PXSEL = [
["button.sm,td.act button,.pane>.ph button", 1],
["h1.pt,.brand b", 3],
[".badge,.state,.st,.k,#hubtxt,.side .grp,.px1", 1],
["button:not(.ib):not(.nopx),label.btn,.card>h2:first-child,.tools>h2,dialog.dlg h2,.side .lb.px,.px2", 2]
];
const PXQ = PXSEL.map(a => a[0]).join(",");
function pxScale(e){ for(const [q, s] of PXSEL) if(e.matches(q)) return s; return 2; }
