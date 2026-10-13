/* Syntax highlighter 1/3 (same as gitphinDroid): language table and file name detection. No library.
 * The blocks between HL-BEGIN and HL-END are pure functions; pc/tests/test_page.sh runs them in node. */
const HLX = {};
(function () {
  "use strict";
  /*HL-BEGIN*/
  function S(w) { var o = {}, a = w.split(" "); for (var i = 0; i < a.length; i++) if (a[i]) o[a[i]] = 1; return o; }
  var CKW = "if else for while do switch case default break continue return goto sizeof typedef struct union enum static extern const volatile inline register auto restrict namespace class public private protected virtual override final template typename using new delete this try catch throw operator friend explicit constexpr noexcept nullptr true false NULL define include ifdef ifndef endif elif undef pragma error";
  var CTY = "void int char short long float double signed unsigned bool size_t ssize_t uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t uintptr_t intptr_t ptrdiff_t FILE string vector map set wchar_t";
  var JVKW = "abstract assert break case catch class const continue default do else enum extends final finally for goto if implements import instanceof interface native new package private protected public return static strictfp super switch synchronized this throw throws transient try volatile while true false null var val fun when object is in as override open data sealed lateinit companion init typealias suspend inline internal operator";
  var JVTY = "boolean byte char double float int long short void String Object Integer Long Boolean List Map Set Int Unit Any Nothing Array Double Float Byte Short Char";
  var JSKW = "async await break case catch class const continue debugger default delete do else export extends finally for from function if import in instanceof let new of return static super switch this throw try typeof var void while with yield true false null undefined interface type enum implements namespace declare readonly abstract as is keyof private protected public";
  var JSTY = "Array Object String Number Boolean Map Set Promise Symbol Date RegExp Error JSON Math console window document string number boolean any unknown never void";
  var PYKW = "and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield match case True False None self";
  var PYTY = "int str float bool list dict set tuple bytes object type print len range open super Exception ValueError";
  var SHKW = "if then else elif fi for while until do done case esac in function select time return exit break continue local export readonly declare unset shift set source alias echo cd test true false";
  var SHTY = "ls cat grep sed awk git make cp mv rm mkdir curl wget chmod chown tar find xargs sudo printf read eval exec trap";
  var GOKW = "break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var true false nil iota";
  var GOTY = "int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 uintptr float32 float64 complex64 complex128 string bool byte rune error any make new len cap append copy delete panic recover print println";
  var RSKW = "as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while macro_rules";
  var RSTY = "i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option Result Box Some None Ok Err";
  var MKKW = "ifeq ifneq ifdef ifndef else endif include sinclude -include define endef export unexport override vpath";

  var DQ = ['"'], DQS = ['"', "'"];
  var LANGS = {
    c: { lc: ["//"], bc: ["/*", "*/"], q: DQ, chr: 1, kw: S(CKW), ty: S(CTY), pre: 1 },
    java: { lc: ["//"], bc: ["/*", "*/"], q: DQ, ml: ['"""'], chr: 1, kw: S(JVKW), ty: S(JVTY), cap: 1 },
    js: { lc: ["//"], bc: ["/*", "*/"], q: DQS, ml: ["`"], kw: S(JSKW), ty: S(JSTY), cap: 1 },
    json: { q: DQ, kw: S("true false null"), keys: 1 },
    py: { lc: ["#"], q: DQS, ml: ['"""', "'''"], kw: S(PYKW), ty: S(PYTY), cap: 1 },
    sh: { lc: ["#"], q: DQS, kw: S(SHKW), ty: S(SHTY), extra: "shvar" },
    go: { lc: ["//"], bc: ["/*", "*/"], q: DQ, ml: ["`"], chr: 1, kw: S(GOKW), ty: S(GOTY), cap: 1 },
    rust: { lc: ["//"], bc: ["/*", "*/"], q: DQ, chr: 1, kw: S(RSKW), ty: S(RSTY), cap: 1 },
    css: { bc: ["/*", "*/"], lc: ["//"], q: DQS, extra: "css", kw: S("important none auto inherit initial block flex grid inline absolute relative fixed solid dashed transparent") },
    yaml: { lc: ["#"], q: DQS, kw: S("true false null yes no on off True False"), keys: 1, extra: "yaml" },
    toml: { lc: ["#"], q: DQS, ml: ['"""', "'''"], kw: S("true false"), extra: "ini" },
    ini: { lc: ["#", ";"], q: DQS, kw: S("true false yes no on off"), extra: "ini" },
    make: { lc: ["#"], q: DQS, kw: S(MKKW), extra: "make" },
    html: { special: "html" },
    md: { special: "md" },
    diff: { special: "diff" }
  };

  var EXT = {
    c: "c", h: "c", cc: "c", cpp: "c", cxx: "c", hpp: "c", hh: "c", hxx: "c", ino: "c", m: "c",
    java: "java", kt: "java", kts: "java", scala: "java", cs: "java", dart: "java", swift: "java",
    js: "js", mjs: "js", cjs: "js", jsx: "js", ts: "js", tsx: "js",
    json: "json", jsonc: "json", json5: "json", webmanifest: "json",
    py: "py", pyw: "py", pyi: "py",
    sh: "sh", bash: "sh", zsh: "sh", ksh: "sh", fish: "sh", bats: "sh",
    go: "go", rs: "rust",
    html: "html", htm: "html", xhtml: "html", xml: "html", svg: "html", xsl: "html", xsd: "html", plist: "html", vue: "html",
    css: "css", scss: "css", less: "css",
    md: "md", markdown: "md",
    yml: "yaml", yaml: "yaml", toml: "toml",
    ini: "ini", cfg: "ini", conf: "ini", properties: "ini", gitconfig: "ini", editorconfig: "ini", desktop: "ini",
    mk: "make", mak: "make",
    diff: "diff", patch: "diff"
  };
  var NAMES = { makefile: "make", gnumakefile: "make", dockerfile: "sh", ".bashrc": "sh", ".profile": "sh", ".zshrc": "sh",
    ".gitconfig": "ini", ".gitignore": "", ".editorconfig": "ini" };

  /* language id from the path (and the first line for a shebang); "" when unknown */
  function hlDetect(path, first) {
    var base = String(path || "").split("/").pop().toLowerCase();
    if (Object.prototype.hasOwnProperty.call(NAMES, base) && NAMES[base]) return NAMES[base];
    var dot = base.lastIndexOf(".");
    if (dot >= 0) {
      var e = base.slice(dot + 1);
      if (Object.prototype.hasOwnProperty.call(EXT, e)) return EXT[e];
    }
    var m = /^#!\s*(\S+)(?:\s+(\S+))?/.exec(first || "");
    if (m) {
      var cmd = (m[1].split("/").pop() === "env" ? m[2] || "" : m[1].split("/").pop());
      if (/^(ba|z|da|k|a)?sh$/.test(cmd)) return "sh";
      if (/^python[\d.]*$/.test(cmd)) return "py";
      if (/^(node|deno|bun)$/.test(cmd)) return "js";
    }
    return "";
  }
  HLX.langs = LANGS;
  HLX.detect = hlDetect;
  /*HL-END*/
})();
