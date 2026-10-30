/* Syntax highlighter 2/3: the tokenizer (pure functions, no DOM).
 * Output is HTML with <span class="hl-X"> tokens; all text is escaped, so code can never inject markup. */
(function () {
  "use strict";
  /*HL-BEGIN*/
  var LANGS = HLX.langs;
  var MAX_LINE = 2000;
  var NUM = /0[xX][0-9a-fA-F_]+[uUlL]*|0[bB][01_]+|(?:\d[\d_]*\.?[\d_]*|\.\d[\d_]*)(?:[eE][+-]?\d+)?[uUlLfFdD]?/y;
  var IDENT = /[A-Za-z_$][\w$]*/y;
  var PUNCT = /[{}()\[\];,.<>+\-*\/%=!&|^~?:@]+/y;
  var CHR = /'(?:\\(?:x[0-9a-fA-F]+|u[0-9a-fA-F]+|[0-7]{1,3}|.)|[^'\\])'/y;

  function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  }
  function sp(cls, t) { return t === "" ? "" : '<span class="hl-' + cls + '">' + esc(t) + "</span>"; }
  function at(re, line, i) { re.lastIndex = i; return re.exec(line); }
  function scanStr(line, from, delim) {
    for (var k = from; k < line.length; k++) {
      if (line.charAt(k) === "\\" && delim !== "`") { k++; continue; }
      if (line.substr(k, delim.length) === delim) return k + delim.length;
    }
    return -1;
  }

  /* per-language extras: return [length, class] for a token starting at i, or null */
  var VARSH = /\$(?:\{[^}]*\}|[A-Za-z_]\w*|[@#?$!*0-9-])/y;
  var MKVAR2 = /\$\([^)]*\)|\$\{[^}]*\}|\$[@<^?*%+]/y;
  var CSSAT = /@[\w-]+/y, CSSHEX = /#[0-9a-fA-F]+\b/y, CSSNUM = /-?(?:\d+\.?\d*|\.\d+)(?:%|[a-zA-Z]+)?/y;
  var CSSSEL = /[.#][A-Za-z_-][\w-]*/y, CSSPROP = /-{0,2}[a-zA-Z_][\w-]*(?=\s*:(?!:))/y;
  var YKEY = /(?:"[^"]*"|'[^']*'|[^\s:#'"\[\]{},&*!|>-][^:#]*?)(?=\s*:(?:\s|$))/y;
  var INISEC = /\[\[?[^\]]*\]\]?/y, INIKEY = /[\w.\-"\/@ ]+?(?=\s*[=:])/y;
  var MKASSIGN = /[A-Za-z_][\w.]*(?=\s*[:+?]?=)/y, MKTARGET = /[^\s:=#][^:=#]*?(?=\s*:(?!=))/y;
  var EXTRA = {
    shvar: function (line, i) {
      if (line.charAt(i) !== "$") return null;
      var m = at(VARSH, line, i); return m ? [m[0].length, "t"] : null;
    },
    css: function (line, i, x) {
      var c = line.charAt(i), m, prev = i ? line.charAt(i - 1) : " ";
      if (c === "@") { m = at(CSSAT, line, i); if (m) return [m[0].length, "k"]; }
      if (c === "#") {
        m = at(CSSHEX, line, i);
        if (m && /^#(?:[0-9a-fA-F]{3,4}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(m[0])) return [m[0].length, "n"];
      }
      if ((c === "." || c === "#") && !/[\w-]/.test(prev)) { m = at(CSSSEL, line, i); if (m) return [m[0].length, "t"]; }
      if (/[0-9.\-]/.test(c) && !/[\w-]/.test(prev)) { m = at(CSSNUM, line, i); if (m) return [m[0].length, "n"]; }
      if (/[a-zA-Z_-]/.test(c) && !/[\w-]/.test(prev) && (x.lead > 0 || line.substr(i).search(/^[\w-]+:\s/) === 0)) {
        m = at(CSSPROP, line, i); if (m) return [m[0].length, "t"];
      }
      return null;
    },
    yaml: function (line, i, x) {
      var c = line.charAt(i), m;
      if (i === x.lead) {
        if (c === "-" && /\s/.test(line.charAt(i + 1) || " ")) {
          var w = /^-\s*/.exec(line.slice(i))[0].length; x.lead = i + w; return [1, "p"];
        }
        m = at(YKEY, line, i); if (m) return [m[0].length, "t"];
      }
      if ((c === "&" || c === "*") && (i === 0 || /\s/.test(line.charAt(i - 1)))) {
        m = /^[&*][\w-]+/.exec(line.slice(i)); if (m) return [m[0].length, "n"];
      }
      return null;
    },
    ini: function (line, i, x) {
      if (i !== x.lead) return null;
      var m = at(INISEC, line, i); if (m && line.charAt(i) === "[") return [m[0].length, "k"];
      m = at(INIKEY, line, i); if (m) return [m[0].length, "t"];
      return null;
    },
    make: function (line, i, x) {
      var c = line.charAt(i), m;
      if (c === "$") { m = at(MKVAR2, line, i); if (m) return [m[0].length, "t"]; }
      if (i === 0 && x.lead === 0) {
        m = at(MKASSIGN, line, i); if (m) return [m[0].length, "t"];
        m = at(MKTARGET, line, i); if (m) return [m[0].length, "t"];
      }
      return null;
    }
  };

  /* one line of a generic language; st.m is the carried state (null, "bc" or the open string delimiter) */
  function genLine(cfg, line, st) {
    var out = "", buf = "", i = 0, n = line.length, m, j, k;
    var x = { lead: /^\s*/.exec(line)[0].length };
    function put(cls, t) { if (buf) { out += esc(buf); buf = ""; } out += sp(cls, t); }
    if (st.m === "bc") {
      j = line.indexOf(cfg.bc[1]);
      if (j < 0) return sp("c", line);
      i = j + cfg.bc[1].length; put("c", line.slice(0, i)); st.m = null;
    } else if (st.m) {
      j = scanStr(line, 0, st.m);
      if (j < 0) return sp("s", line);
      put("s", line.slice(0, j)); i = j; st.m = null;
    }
    outer:
    while (i < n) {
      var c = line.charAt(i);
      if (cfg.extra) {
        var r = EXTRA[cfg.extra](line, i, x);
        if (r) { put(r[1], line.substr(i, r[0])); i += r[0]; continue; }
      }
      if (cfg.lc) {
        for (k = 0; k < cfg.lc.length; k++) {
          var lc = cfg.lc[k];
          if (line.substr(i, lc.length) === lc && (lc.length > 1 || lc === "//" || i === 0 || /\s/.test(line.charAt(i - 1)))) {
            put("c", line.slice(i)); i = n; break outer;
          }
        }
      }
      if (cfg.pre && c === "#" && i === x.lead) {
        m = /^#\s*\w+/.exec(line.slice(i));
        if (m) {
          put("k", m[0]); i += m[0].length;
          var inc = /^(\s*)(<[^>]*>)/.exec(line.slice(i));
          if (inc && /include|import/.test(m[0])) { buf += inc[1]; put("s", inc[2]); i += inc[0].length; }
          continue;
        }
      }
      if (cfg.bc && line.substr(i, cfg.bc[0].length) === cfg.bc[0]) {
        j = line.indexOf(cfg.bc[1], i + cfg.bc[0].length);
        if (j < 0) { put("c", line.slice(i)); st.m = "bc"; i = n; break; }
        j += cfg.bc[1].length; put("c", line.slice(i, j)); i = j; continue;
      }
      if (cfg.ml) {
        for (k = 0; k < cfg.ml.length; k++) {
          var d = cfg.ml[k];
          if (line.substr(i, d.length) === d) {
            j = scanStr(line, i + d.length, d);
            if (j < 0) { put("s", line.slice(i)); st.m = d; i = n; break outer; }
            put("s", line.slice(i, j)); i = j; continue outer;
          }
        }
      }
      if (cfg.q && cfg.q.indexOf(c) >= 0) {
        j = scanStr(line, i + 1, c); if (j < 0) j = n;
        var key = cfg.keys && /^\s*:/.test(line.slice(j));
        put(key ? "t" : "s", line.slice(i, j)); i = j; continue;
      }
      if (cfg.chr && c === "'") {
        m = at(CHR, line, i);
        if (m) { put("s", m[0]); i += m[0].length; continue; }
      }
      if ((c >= "0" && c <= "9") || (c === "." && /\d/.test(line.charAt(i + 1))))  {
        if (i === 0 || !/[\w$]/.test(line.charAt(i - 1))) {
          m = at(NUM, line, i);
          if (m && m[0]) { put("n", m[0]); i += m[0].length; continue; }
        }
      }
      if (/[A-Za-z_$]/.test(c)) {
        m = at(IDENT, line, i);
        var w = m[0], cls = null;
        if (cfg.kw && cfg.kw[w] === 1) cls = "k";
        else if (cfg.ty && cfg.ty[w] === 1) cls = "t";
        else if (cfg.cap && w.length > 1 && c >= "A" && c <= "Z") cls = "t";
        if (cls) put(cls, w); else buf += w;
        i += w.length; continue;
      }
      m = at(PUNCT, line, i);
      if (m) { put("p", m[0]); i += m[0].length; continue; }
      buf += c; i++;
    }
    if (buf) out += esc(buf);
    return out;
  }

  function diffLine(line) {
    if (!line) return "";
    if (/^(\+\+\+|---) /.test(line) || /^(diff |index |new file|deleted file|similarity|rename )/.test(line)) return sp("m", line);
    var c = line.charAt(0);
    if (c === "+") return sp("a", line);
    if (c === "-") return sp("d", line);
    if (line.slice(0, 2) === "@@") return sp("h", line);
    return esc(line);
  }

  var MDINL = /`[^`]+`|\[[^\]]*\]\([^)\s]*(?:\s+"[^"]*")?\)|<https?:\/\/[^>\s]+>|^\s*(?:[-*+]|\d+[.)])(?=\s)|\*\*[^*]+\*\*/g;
  function mdLine(line, st) {
    if (/^\s*(```|~~~)/.test(line)) { st.f = !st.f; return sp("s", line); }
    if (st.f) return sp("s", line);
    if (/^#{1,6}\s/.test(line)) return sp("h", line);
    if (/^\s*>/.test(line)) return sp("c", line);
    if (/^\s*([-*_])\s*(\1\s*){2,}$/.test(line)) return sp("p", line);
    var out = "", last = 0, m;
    MDINL.lastIndex = 0;
    while ((m = MDINL.exec(line))) {
      out += esc(line.slice(last, m.index));
      var t = m[0], c = t.charAt(0);
      if (c === "`") out += sp("s", t);
      else if (c === "[") {
        var b = t.indexOf("](");
        out += sp("p", "[") + sp("l", t.slice(1, b)) + sp("p", "](") + sp("c", t.slice(b + 2, -1)) + sp("p", ")");
      } else if (c === "<") out += sp("l", t);
      else if (c === "*" && t.charAt(1) === "*") out += sp("k", t);
      else out += sp("p", t);
      last = m.index + t.length;
      if (!t.length) MDINL.lastIndex++;
    }
    return out + esc(line.slice(last));
  }

  var HTAG = /<[\/!?]?[A-Za-z][\w:.\-]*/y, HATTR = /[^\s=>\/"']+/y;
  function htmlLine(line, st) {
    var out = "", i = 0, n = line.length, m, j;
    while (i < n) {
      if (st.m === "c") {
        j = line.indexOf("-->", i);
        if (j < 0) return out + sp("c", line.slice(i));
        out += sp("c", line.slice(i, j + 3)); i = j + 3; st.m = null; continue;
      }
      var c = line.charAt(i);
      if (st.m === "t") {
        if (c === ">" || (c === "/" && line.charAt(i + 1) === ">") || (c === "?" && line.charAt(i + 1) === ">")) {
          var len = c === ">" ? 1 : 2; out += sp("p", line.substr(i, len)); i += len; st.m = null;
        } else if (c === '"' || c === "'") {
          j = line.indexOf(c, i + 1); j = j < 0 ? n : j + 1; out += sp("s", line.slice(i, j)); i = j;
        } else if (c === "=") { out += sp("p", c); i++; }
        else if (/\s/.test(c)) { out += esc(c); i++; }
        else { m = at(HATTR, line, i); if (m) { out += sp("t", m[0]); i += m[0].length; } else { out += esc(c); i++; } }
        continue;
      }
      if (line.substr(i, 4) === "<!--") { st.m = "c"; continue_c(); continue; }
      if (c === "<") {
        m = at(HTAG, line, i);
        if (m) {
          var pre = /^<[\/!?]?/.exec(m[0])[0];
          out += sp("p", pre) + sp("k", m[0].slice(pre.length)); i += m[0].length; st.m = "t"; continue;
        }
      }
      j = line.indexOf("<", i + 1); if (j < 0) j = n;
      out += esc(line.slice(i, j)); i = j;
    }
    return out;
    function continue_c() { out += sp("c", "<!--"); i += 4; }
  }

  /* highlight an array of lines; returns an array of HTML strings (same length) */
  function hlLines(lang, lines) {
    var cfg = LANGS[lang], res = [], st = {}, i, line;
    for (i = 0; i < lines.length; i++) {
      line = lines[i];
      if (!cfg || line.length > MAX_LINE) { res.push(esc(line)); continue; }
      if (cfg.special === "diff") res.push(diffLine(line));
      else if (cfg.special === "md") res.push(mdLine(line, st));
      else if (cfg.special === "html") res.push(htmlLine(line, st));
      else res.push(genLine(cfg, line, st));
    }
    return res;
  }
  HLX.lines = hlLines;
  HLX.esc = esc;
  /*HL-END*/
})();
