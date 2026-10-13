/* Syntax highlighter 3/3: the code view hook. Colors are in css/highlight.css. */
const HL_MAX_BYTES = 200 * 1024;   // bigger files stay plain
// Fills the <pre class="txt"> of the code view with colored spans. The text never changes.
function hlApply(pre, path, text, lines){
try{
if(text.length > HL_MAX_BYTES) return;
const lang = HLX.detect(path, lines[0]);
if(!lang) return;
pre.innerHTML = HLX.lines(lang, lines).join("\n");
if(pre.textContent !== text) pre.textContent = text;   // safety net
}catch(_){ pre.textContent = text; }
}
