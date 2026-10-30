function mdEmph(t){
return t.replace(/\*\*([^*\n]+)\*\*/g, "<strong>$1</strong>").replace(/__([^_\n]+)__/g, "<strong>$1</strong>")
.replace(/(^|[^*\w])\*([^*\n]+)\*(?!\*)/g, "$1<em>$2</em>").replace(/(^|[^_\w])_([^_\n]+)_(?![_\w])/g, "$1<em>$2</em>")
.replace(/~~([^~\n]+)~~/g, "<del>$1</del>");
}
function mdHref(u, ctx){
if(/^(https?:\/\/|mailto:)/i.test(u)) return {href: u, ext: true};
if(u[0] === "#") return {href: u, ext: false};
if(/^[a-z][a-z0-9+.-]*:/i.test(u) || u.slice(0, 2) === "//") return null;
if(!ctx || !ctx.ref) return null;
const parts = (u[0] === "/" ? u.slice(1) : (ctx.dir ? ctx.dir + "/" + u : u)).split("#")[0].split("?")[0].split("/");
const out = [];
for(const p of parts){ if(p === "..") out.pop(); else if(p && p !== ".") out.push(p); }
if(!out.length) return null;
let dec; try{ dec = out.map(x => decodeURIComponent(x.replace(/&amp;/g, "&"))); }catch(_){ return null; }
return {href: esc(blobHash(ctx.ref, dec.join("/"))), ext: false};
}
function mdInline(src, ctx){
const tok = [];
const keep = h => "\0" + (tok.push(h) - 1) + "\0";
let s = src.replace(/\0/g, "");
s = s.replace(/`([^`\n]+)`/g, (m, c) => keep("<code>" + esc(c) + "</code>"));
s = esc(s);
if(ctx && ctx.wiki) s = s.replace(/\[\[([A-Za-z0-9 ._-]+)(?:\|([^\]\n]+))?\]\]/g, (m, pg, t) => {
const nm = wikiName(pg), miss = !wikiHas(nm);
return keep('<a href="' + esc(hs("wiki", nm, ...(miss ? ["edit"] : []))) + '"' + (miss ? ' class="missing" title="Page does not exist yet. Click to create it."' : "") + ">" + (t || esc(pg)) + (miss ? " ✎" : "") + "</a>");
});
s = s.replace(/\[([^\]\n]+)\]\(([^)\s]+)\)/g, (m, t, u) => {
const h = mdHref(u, ctx);
if(!h) return t;
return keep('<a href="' + h.href + '"' + (h.ext ? ' target="_blank" rel="noopener noreferrer"' : "") + ">" + mdEmph(t) + "</a>");
});
s = mdEmph(s);
return s.replace(/\0(\d+)\0/g, (m, i) => tok[+i]);
}
function mdBlocks(src, ctx){
const L = src.replace(/\r\n?/g, "\n").split("\n");
const out = []; let i = 0, para = [];
const flush = () => { if(para.length){ out.push("<p>" + mdInline(para.join("\n"), ctx) + "</p>"); para = []; } };
const isTableSep = l => /^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$/.test(l || "") && (l || "").includes("-");
const cells = l => l.trim().replace(/^\|/, "").replace(/\|$/, "").split("|").map(x => x.trim());
while(i < L.length){
const line = L[i];
let m;
if((m = /^\s*(```|~~~)/.exec(line))){
flush(); const fence = m[1]; const code = []; i++;
while(i < L.length && !L[i].trim().startsWith(fence)){ code.push(L[i]); i++; }
i++; out.push('<pre><code>' + esc(code.join("\n")) + "</code></pre>"); continue;
}
if(!line.trim()){ flush(); i++; continue; }
if((m = /^(#{1,6})\s+(.*?)\s*#*\s*$/.exec(line))){ flush(); out.push("<h" + m[1].length + ">" + mdInline(m[2], ctx) + "</h" + m[1].length + ">"); i++; continue; }
if(/^\s*([-*_])(\s*\1){2,}\s*$/.test(line)){ flush(); out.push("<hr>"); i++; continue; }
if(/^\s*>/.test(line)){
flush(); const q = [];
while(i < L.length && /^\s*>/.test(L[i])){ q.push(L[i].replace(/^\s*> ?/, "")); i++; }
out.push("<blockquote>" + mdBlocks(q.join("\n"), ctx) + "</blockquote>"); continue;
}
if(line.includes("|") && isTableSep(L[i + 1]) ){
flush(); const head = cells(line); i += 2; const rows = [];
while(i < L.length && L[i].includes("|") && L[i].trim()){ rows.push(cells(L[i])); i++; }
out.push("<table><thead><tr>" + head.map(c => "<th>" + mdInline(c, ctx) + "</th>").join("") + "</tr></thead><tbody>" +
rows.map(r => "<tr>" + head.map((_, k) => "<td>" + mdInline(r[k] || "", ctx) + "</td>").join("") + "</tr>").join("") + "</tbody></table>");
continue;
}
if(/^(\s*)([-*+]|\d+[.)])\s+/.test(line)){
flush(); const items = [];
while(i < L.length){
const mm = /^(\s*)([-*+]|\d+[.)])\s+(.*)$/.exec(L[i]);
if(mm){ items.push({indent: mm[1].replace(/\t/g, "    ").length, ordered: /\d/.test(mm[2]), text: mm[3]}); i++; }
else if(L[i].trim() && /^\s+/.test(L[i]) && items.length){ items[items.length - 1].text += "\n" + L[i].trim(); i++; }
else break;
}
let html = "", stack = [];
for(const it of items){
while(stack.length && it.indent < stack[stack.length - 1].indent) html += "</li></" + stack.pop().tag + ">";
const top = stack[stack.length - 1];
if(!top || it.indent > top.indent){ const tag = it.ordered ? "ol" : "ul"; html += "<" + tag + ">"; stack.push({indent: it.indent, tag}); }
else html += "</li>";
html += "<li>" + mdInline(it.text, ctx);
}
while(stack.length) html += "</li></" + stack.pop().tag + ">";
out.push(html); continue;
}
para.push(line); i++;
}
flush();
return out.join("\n");
}
function mdNode(src, ctx){ const d = el("div", {class: "md"}); d.innerHTML = mdBlocks(String(src), ctx); return d; }
