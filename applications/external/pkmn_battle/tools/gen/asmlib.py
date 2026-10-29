"""Minimal RGBDS evaluator for pokecrystal's constant and data files.

Handles what the constants/ and macros/ trees use: DEF (EQU, =, +=, -=,
EQUS, rb/rw/rl), REDEF, rsreset/rsset, MACRO bodies with \\1..\\9, _NARG,
shift and {fmt:sym} interpolation, if/elif/else/endc, charmap/newcharmap/
pushc/popc and INCLUDE. Anything else (for/rept loops, SECTION, data
directives, labels) is skipped. Lines that fail to evaluate are recorded
in `self.errors` rather than aborting, so generators must assert that the
symbols they need exist.
"""

import pathlib
import re

_TOKEN = re.compile(
    r"""
    (?P<ws>\s+)
  | (?P<str>"(?:[^"\\]|\\.)*")
  | (?P<hex>\$[0-9A-Fa-f_]+)
  | (?P<num>[0-9][0-9_]*)
  | (?P<ident>[A-Za-z_.][A-Za-z0-9_#@.]*)
  | (?P<op><<|>>|<=|>=|==|!=|&&|\|\||\*\*|[-+*/%&|^~!()<>,])
""",
    re.VERBOSE,
)

_BIN = re.compile(r"%[01_]+")


class AsmError(Exception):
    pass


def split_args(s: str):
    """Split macro arguments on top-level commas (respecting strings/parens)."""
    args, cur, depth, in_str = [], [], 0, False
    i = 0
    while i < len(s):
        c = s[i]
        if in_str:
            cur.append(c)
            if c == "\\" and i + 1 < len(s):
                cur.append(s[i + 1])
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
            cur.append(c)
        elif c == "(":
            depth += 1
            cur.append(c)
        elif c == ")":
            depth -= 1
            cur.append(c)
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    tail = "".join(cur).strip()
    if tail or args:
        args.append(tail)
    return args


def strip_comment(line: str) -> str:
    in_str = False
    for i, c in enumerate(line):
        if c == '"':
            in_str = not in_str
        elif c == ";" and not in_str:
            return line[:i]
    return line


def unquote(s: str) -> str:
    s = s.strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        return s[1:-1].replace('\\"', '"').replace("\\\\", "\\")
    raise AsmError(f"not a string literal: {s}")


class Asm:
    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.sym = {"_RS": 0}
        self.equs = {}
        self.macros = {}
        self.charmaps = {"main": {}}
        self.charmap_name = "main"
        self.charmap_stack = []
        self.errors = []
        self.def_file = {}  # symbol -> file that first defined it

    # ---------------- expressions ----------------
    def _subst_equs(self, expr: str, depth=0) -> str:
        if depth > 20:
            raise AsmError("EQUS recursion")
        out = []
        pos = 0
        for m in _TOKEN.finditer(expr):
            if m.start() != pos:
                raise AsmError(f"bad token in {expr!r} at {pos}")
            pos = m.end()
            tok = m.group(0)
            if m.lastgroup == "ident" and tok in self.equs:
                out.append(" " + self._subst_equs(self.equs[tok], depth + 1) + " ")
            else:
                out.append(tok)
        if pos != len(expr):
            raise AsmError(f"bad token in {expr!r} at {pos}")
        return "".join(out)

    def eval(self, expr: str) -> int:
        # %0101 binary literals (pokecrystal always spaces the modulo operator)
        expr = _BIN.sub(lambda m: str(int(m.group(0)[1:].replace("_", ""), 2)), expr)
        s = self._subst_equs(expr)
        tokens = [
            (m.lastgroup, m.group(0)) for m in _TOKEN.finditer(s) if m.lastgroup != "ws"
        ]
        py = []
        k = 0
        while k < len(tokens):
            kind, tok = tokens[k]
            if kind == "hex":
                py.append(str(int(tok[1:].replace("_", ""), 16)))
            elif kind == "num":
                py.append(str(int(tok.replace("_", ""))))
            elif kind == "str":
                py.append(repr(unquote(tok)))
            elif kind == "ident":
                up = tok.upper()
                if (
                    up in ("DEF", "HIGH", "LOW", "STRLEN", "CHARLEN")
                    and k + 1 < len(tokens)
                    and tokens[k + 1][1] == "("
                ):
                    depth, j = 0, k + 1
                    while True:
                        if tokens[j][1] == "(":
                            depth += 1
                        elif tokens[j][1] == ")":
                            depth -= 1
                            if depth == 0:
                                break
                        j += 1
                    inner = tokens[k + 2 : j]
                    if up == "DEF":
                        name = inner[0][1] if inner else ""
                        py.append(
                            "1"
                            if (
                                name in self.sym
                                or name in self.equs
                                or name in self.macros
                            )
                            else "0"
                        )
                    elif up in ("STRLEN", "CHARLEN"):
                        py.append(str(len(unquote(inner[0][1]))))
                    else:
                        v = self.eval(" ".join(t for _, t in inner))
                        py.append(str((v >> 8) & 0xFF if up == "HIGH" else v & 0xFF))
                    k = j + 1
                    continue
                if tok in self.sym:
                    py.append(str(self.sym[tok]))
                else:
                    raise AsmError(f"undefined symbol {tok}")
            else:
                py.append(
                    {
                        "&&": " and ",
                        "||": " or ",
                        "!": " not ",
                        "/": "//",
                        "**": "**",
                    }.get(tok, tok)
                    if tok != "!="
                    else "!="
                )
            k += 1
        code = " ".join(py)
        try:
            v = eval(code, {"__builtins__": {}}, {})
        except Exception as e:  # noqa: BLE001
            raise AsmError(f"eval {code!r}: {e}")
        if isinstance(v, bool):
            v = int(v)
        if not isinstance(v, int):
            raise AsmError(f"non-integer result for {expr!r}")
        return v

    # ---------------- interpolation ----------------
    def interpolate(self, line: str) -> str:
        def repl(m):
            fmt, name = m.group(1), m.group(2)
            if name in self.equs and fmt is None:
                return self.equs[name]
            v = self.sym.get(name)
            if v is None:
                if name in self.equs:
                    return self.equs[name]
                raise AsmError(f"undefined interpolation {name}")
            if fmt is None:
                return f"${v:X}"
            fm = re.fullmatch(r"(0?)(\d*)([dxXbs])", fmt)
            if not fm:
                raise AsmError(f"bad format {fmt}")
            zero, width, conv = fm.groups()
            spec = f"{zero}{width}{'b' if conv == 'b' else conv}"
            return format(v, spec)

        prev = None
        while prev != line:
            prev = line
            line = re.sub(
                r"\{(?:([0-9a-zA-Z]+):)?([A-Za-z_][A-Za-z0-9_]*)\}", repl, line
            )
        return line

    # ---------------- statements ----------------
    def define(self, name, value, op="EQU", file=None):
        if op in ("EQU", "="):
            self.sym[name] = value
        elif op == "+=":
            self.sym[name] = self.sym[name] + value
        elif op == "-=":
            self.sym[name] = self.sym[name] - value
        if file and name not in self.def_file:
            self.def_file[name] = file

    def include(self, rel):
        path = self.root / rel
        self.run_lines(path.read_text(encoding="utf-8").splitlines(), rel)

    def run_lines(self, lines, file, args=None):
        """Execute lines. args: macro argument list when expanding a macro."""
        cond_stack = []  # (active, taken)
        i = 0
        args = list(args) if args is not None else None
        while i < len(lines):
            raw = lines[i]
            i += 1
            line = strip_comment(raw)
            # line continuation
            while line.rstrip().endswith("\\") and i < len(lines):
                line = line.rstrip()[:-1] + " " + strip_comment(lines[i]).strip()
                i += 1
            if args is not None:
                line = self._subst_args(line, args)
            stripped = line.strip()
            if not stripped:
                continue
            word = stripped.split(None, 1)[0]
            lw = word.lower().rstrip("?")

            # conditionals are processed even while inactive
            if lw == "if":
                active = all(a for a, _ in cond_stack)
                val = False
                if active:
                    val = self._cond(stripped[2:], file)
                cond_stack.append((active and val, val))
                continue
            if lw == "elif":
                parent = all(a for a, _ in cond_stack[:-1])
                _, taken = cond_stack[-1]
                val = (not taken) and parent and self._cond(stripped[4:], file)
                cond_stack[-1] = (val, taken or val)
                continue
            if lw == "else":
                parent = all(a for a, _ in cond_stack[:-1])
                _, taken = cond_stack[-1]
                cond_stack[-1] = (parent and not taken, True)
                continue
            if lw == "endc":
                cond_stack.pop()
                continue
            if not all(a for a, _ in cond_stack):
                # still need to skip macro/loop bodies properly
                if lw == "macro":
                    i = self._skip_block(lines, i, ("endm",), ("macro",))
                elif lw in ("rept", "for"):
                    i = self._skip_block(lines, i, ("endr",), ("rept", "for"))
                continue

            if lw == "macro":
                name = stripped.split(None, 1)[1].strip()
                j = self._skip_block(lines, i, ("endm",), ("macro",))
                self.macros[name] = lines[i : j - 1]
                i = j
                continue
            if lw in ("rept", "for"):
                i = self._skip_block(lines, i, ("endr",), ("rept", "for"))
                continue
            if lw == "shift" and args is not None:
                n = 1
                rest = stripped.split(None, 1)
                if len(rest) > 1:
                    n = self.eval(rest[1])
                del args[:n]
                # re-substitute remaining lines lazily: handled since we substitute per line
                continue
            try:
                self._statement(stripped, file, args)
            except AsmError as e:
                self.errors.append(f"{file}: {stripped}: {e}")
            except (KeyError, IndexError, ValueError) as e:
                self.errors.append(f"{file}: {stripped}: {e!r}")

    def _skip_block(self, lines, i, ends, starts):
        depth = 1
        while i < len(lines):
            w = strip_comment(lines[i]).strip().split(None, 1)
            i += 1
            if not w:
                continue
            lw = w[0].lower().rstrip("?")
            if lw in starts:
                depth += 1
            elif lw in ends:
                depth -= 1
                if depth == 0:
                    return i
        raise AsmError(f"unterminated block {starts}")

    def _subst_args(self, line, args):
        def repl(m):
            c = m.group(1)
            if c.isdigit():
                n = int(c)
                return args[n - 1] if 0 < n <= len(args) else ""
            if c == "#":
                return ", ".join(args)
            if c == "@":
                return "_u"
            return m.group(0)

        line = re.sub(r"\\([1-9#@])", repl, line)
        return re.sub(r"\b_NARG\b", str(len(args)), line)

    def _cond(self, expr, file):
        try:
            return bool(self.eval(self.interpolate(expr.strip())))
        except AsmError as e:
            self.errors.append(f"{file}: if {expr}: {e}")
            return False

    def _statement(self, s, file, args):
        s = self.interpolate(s)
        m = re.match(r"(?i)(?:DEF|REDEF)\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.*)$", s)
        if m:
            name, rest = m.group(1), m.group(2).strip()
            m2 = re.match(r"(?i)(EQUS|EQU|RB|RW|RL)\b\s*(.*)$", rest)
            if m2:
                kw, val = m2.group(1).upper(), m2.group(2).strip()
                if kw == "EQUS":
                    self.equs[name] = unquote(val)
                    self.def_file.setdefault(name, file)
                elif kw == "EQU":
                    self.define(name, self.eval(val), "EQU", file)
                else:
                    n = self.eval(val) if val else 1
                    size = {"RB": 1, "RW": 2, "RL": 4}[kw]
                    self.define(name, self.sym["_RS"], "EQU", file)
                    self.sym["_RS"] += n * size
                return
            m2 = re.match(r"(\+=|-=|=)\s*(.*)$", rest)
            if m2:
                self.define(name, self.eval(m2.group(2)), m2.group(1), file)
                return
            raise AsmError("unknown DEF form")
        word, _, rest = s.partition(" ")
        if not rest:
            word, _, rest = s.partition("\t")
        w = word.strip()
        lw = w.lower().rstrip("?")
        rest = rest.strip()
        if lw == "rsreset":
            self.sym["_RS"] = 0
        elif lw == "rsset":
            self.sym["_RS"] = self.eval(rest)
        elif lw == "include":
            self.include(unquote(rest))
        elif lw == "charmap":
            a = split_args(rest)
            self.charmaps[self.charmap_name][unquote(a[0])] = self.eval(a[1])
        elif lw == "newcharmap":
            name = split_args(rest)[0]
            self.charmaps[name] = {}
            self.charmap_name = name
        elif lw == "setcharmap":
            self.charmap_name = rest
        elif lw == "pushc":
            self.charmap_stack.append(self.charmap_name)
        elif lw == "popc":
            self.charmap_name = self.charmap_stack.pop()
        elif lw == "purge":
            for n in split_args(rest):
                self.sym.pop(n, None)
                self.equs.pop(n, None)
                self.macros.pop(n, None)
        elif w in self.macros:
            self.run_lines(self.macros[w], file, split_args(rest))
        elif w.rstrip("?") in self.macros:
            self.run_lines(self.macros[w.rstrip("?")], file, split_args(rest))
        else:
            # labels, data directives, SECTION, assert, fail, etc.: ignore
            pass

    # ---------------- helpers for generators ----------------
    def charmap(self, name="main"):
        return self.charmaps[name]

    def encode(self, text, name="main"):
        """Greedy longest-match encode like RGBDS."""
        cm = self.charmaps[name]
        keys = sorted(cm, key=len, reverse=True)
        out = bytearray()
        i = 0
        while i < len(text):
            for k in keys:
                if text.startswith(k, i):
                    out.append(cm[k])
                    i += len(k)
                    break
            else:
                raise AsmError(f"unmapped character {text[i]!r} in {text!r}")
        return bytes(out)


def load_constants(root):
    """Evaluate includes.asm (macros + constants) and return the Asm."""
    asm = Asm(root)
    asm.include("includes.asm")
    return asm
