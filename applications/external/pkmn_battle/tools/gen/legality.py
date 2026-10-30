"""Parse pokecrystal species/move data and apply this project's legality rules.

Rules for random-team movepools:
- A species is ineligible at level L if it cannot exist at L (Dragonite and
  Tyranitar evolve at 55) or has fewer than 4 moves legal at L.
- Legal moves: level-up moves of the species and all of its pre-evolutions,
  TM/HM and Crystal's 3 move tutor moves those species can learn, and the egg
  moves of the chain. No event or Gen 1 tradeback moves.
- Each move has an earliest legal level: a level-up move of chain member m
  learned at x needs max(x, min_level(m)); TM/HM/tutor moves need
  min_level(m); egg moves need EGG_HATCH_LEVEL.
- Beat Up is never used (it desyncs even two real Game Boys).
"""

import pathlib
import re

import asmlib
import fetch_pokecrystal

TEAM_LEVEL = 50
EGG_HATCH_LEVEL = 5  # Gen 2 eggs hatch at level 5
# not-fully-evolved species that evolve by stone, trade or happiness count as
# "should have evolved" from this level on (random teams)
NFE_STONE_TRADE_LEVEL = 30
BANNED_MOVES = ("BEAT_UP",)

# The spec's expected excluded list at level 50; generators assert the computed
# set matches.
EXPECTED_EXCLUDED = (
    "CATERPIE",
    "METAPOD",
    "WEEDLE",
    "KAKUNA",
    "MAGIKARP",
    "DITTO",
    "UNOWN",
    "SMEARGLE",
    "DRAGONITE",
    "TYRANITAR",
)


def _statements(path: pathlib.Path):
    """Yield (directive, argstring) for each non-empty, comment-free line."""
    for raw in path.read_text(encoding="utf-8").splitlines():
        s = asmlib.strip_comment(raw).strip()
        if not s:
            continue
        if s.endswith(":") and " " not in s:
            yield ("label", s[:-1].rstrip(":"))
            continue
        word, _, rest = s.partition(" ")
        if "\t" in word:
            word, _, rest2 = word.partition("\t")
            rest = rest2 + " " + rest
        yield (word.strip(), rest.strip())


class Data:
    def __init__(self, root=None):
        self.root = pathlib.Path(root or fetch_pokecrystal.ensure())
        self.asm = asmlib.load_constants(self.root)
        a = self.asm
        self.num_pokemon = a.sym["NUM_POKEMON"]
        self.num_moves = a.sym["NUM_ATTACKS"]
        self.species = self._const_names(
            "constants/pokemon_constants.asm", 1, self.num_pokemon
        )
        self.moves_by_id = self._const_names(
            "constants/move_constants.asm", 1, self.num_moves
        )
        self._parse_base_stats()
        self._parse_moves()
        self._parse_evos_attacks()
        self._parse_egg_moves()
        # names.asm continues past NUM_POKEMON with "?????"/"EGG" entries
        self.species_names = self._parse_names("data/pokemon/names.asm", "dname")[
            : self.num_pokemon
        ]
        self.move_names = self._parse_names("data/moves/names.asm", "li")[
            : self.num_moves
        ]
        self._parse_growth_rates()

    # ---------- parsing ----------
    def _const_names(self, rel, lo, hi):
        """Ordered list (index = value - lo) of `const NAME` symbols in [lo, hi]."""
        names = {}
        for word, rest in _statements(self.root / rel):
            if word == "const":
                name = rest.split()[0]
                v = self.asm.sym[name]
                if lo <= v <= hi and v not in names:
                    names[v] = name
        out = [names[v] for v in range(lo, hi + 1)]
        return out

    def _parse_base_stats(self):
        a = self.asm
        files = [
            rest.strip('"')
            for w, rest in _statements(self.root / "data/pokemon/base_stats.asm")
            if w == "INCLUDE"
        ]
        assert len(files) == self.num_pokemon, len(files)
        tm_moves = {}  # TMNUM -> move id
        for name, v in a.sym.items():
            if name.endswith("_TMNUM"):
                tm_moves[v] = a.sym[name[: -len("_TMNUM")]]
        self.tm_moves = tm_moves
        self.base = []
        for idx, rel in enumerate(files):
            dbs, tmhm = [], []
            for word, rest in _statements(self.root / rel):
                if word in ("db", "dn"):
                    dbs.append([a.eval(x) for x in asmlib.split_args(rest)])
                elif word == "tmhm":
                    tmhm = [x for x in asmlib.split_args(rest) if x]
            species_id = dbs[0][0]
            assert species_id == idx + 1, (rel, species_id)
            stats = dbs[1]
            rec = {
                "dex": species_id,
                "stats": stats,  # hp atk def spd sat sdf
                "types": dbs[2],
                "catch_rate": dbs[3][0],
                "base_exp": dbs[4][0],
                "items": dbs[5],
                "gender": dbs[6][0] & 0xFF,
                "growth_rate": dbs[10][0],
                "tmhm": sorted(a.sym[m] for m in tmhm),
            }
            assert len(stats) == 6 and len(rec["types"]) == 2
            self.base.append(rec)

    def _parse_moves(self):
        a = self.asm
        self.moves = []
        for word, rest in _statements(self.root / "data/moves/moves.asm"):
            if word == "move":
                f = asmlib.split_args(rest)
                # the move macro emits accuracy and effect chance as "<n> percent"
                anim, effect, power, typ, pp = (a.eval(f[i]) for i in (0, 1, 2, 3, 5))
                acc, chance = a.eval(f"{f[4]} percent"), a.eval(f"{f[6]} percent")
                self.moves.append(
                    {
                        "anim": anim,
                        "effect": effect,
                        "power": power,
                        "type": typ,
                        "accuracy": acc,
                        "pp": pp,
                        "effect_chance": chance,
                    }
                )
        assert len(self.moves) == self.num_moves, len(self.moves)
        for i, m in enumerate(self.moves):
            assert m["anim"] == i + 1, f"move {i + 1} animation {m['anim']} != id"

    def _parse_evos_attacks(self):
        a = self.asm
        order = [
            rest
            for w, rest in _statements(
                self.root / "data/pokemon/evos_attacks_pointers.asm"
            )
            if w == "dw"
        ]
        assert len(order) == self.num_pokemon
        blocks = {}
        cur = None
        for word, rest in _statements(self.root / "data/pokemon/evos_attacks.asm"):
            if word == "label":
                cur = rest
                blocks[cur] = []
            elif word == "db" and cur:
                blocks[cur].append([a.eval(x) for x in asmlib.split_args(rest)])
        self.evos = []  # species index -> [(method, param, target)]
        self.learnset = []  # species index -> [(level, move)]
        for label in order:
            rows = blocks[label]
            i = 0
            evos = []
            while rows[i][0] != 0:
                r = rows[i]
                if r[0] == a.sym["EVOLVE_STAT"]:
                    evos.append((r[0], r[1], r[3]))
                else:
                    evos.append((r[0], r[1], r[2]))
                i += 1
            i += 1
            learn = []
            while rows[i][0] != 0:
                learn.append((rows[i][0], rows[i][1]))
                i += 1
            self.evos.append(evos)
            self.learnset.append(learn)

    def _parse_egg_moves(self):
        a = self.asm
        order = [
            rest
            for w, rest in _statements(self.root / "data/pokemon/egg_move_pointers.asm")
            if w == "dw"
        ]
        assert len(order) == self.num_pokemon, len(order)
        blocks, cur = {}, None
        for word, rest in _statements(self.root / "data/pokemon/egg_moves.asm"):
            if word == "label":
                cur = rest
                blocks[cur] = []
            elif word == "db" and cur:
                v = a.eval(rest)
                if v != 0xFF and v != -1:
                    blocks[cur].append(v)
        self.egg_moves = [blocks[label] for label in order]  # species index -> [move]

    def _parse_names(self, rel, macro):
        out = []
        for word, rest in _statements(self.root / rel):
            if word == macro:
                out.append(asmlib.unquote(asmlib.split_args(rest)[0]))
        return out

    def _parse_growth_rates(self):
        a = self.asm
        self.growth_rates = []
        for word, rest in _statements(self.root / "data/growth_rates.asm"):
            if word == "growth_rate":
                self.growth_rates.append(
                    tuple(a.eval(x) for x in asmlib.split_args(rest))
                )
        assert len(self.growth_rates) == a.sym["NUM_GROWTH_RATES"]

    # ---------- derived ----------
    def exp_at_level(self, growth_rate, n):
        """data/growth_rates.asm: a/b*n^3 + c*n^2 + d*n - e (CalcExpAtLevel)."""
        num, den, c, d, e = self.growth_rates[growth_rate]
        return (num * n**3) // den + c * n * n + d * n - e

    def pre_evolutions(self, sid):
        """Species ids that evolve (directly or not) into sid, nearest first."""
        parents = {}
        for p, evos in enumerate(self.evos, start=1):
            for _, _, target in evos:
                parents.setdefault(target, []).append(p)
        chain, todo = [], list(parents.get(sid, []))
        while todo:
            p = todo.pop(0)
            if p not in chain:
                chain.append(p)
                todo.extend(parents.get(p, []))
        return chain

    def min_level(self, sid, _memo=None):
        """Lowest level at which sid can legally exist (ignoring encounters)."""
        a = self.asm
        memo = _memo if _memo is not None else {}
        if sid in memo:
            return memo[sid]
        best = None
        for p, evos in enumerate(self.evos, start=1):
            for method, param, target in evos:
                if target != sid:
                    continue
                base = self.min_level(p, memo)
                if method in (a.sym["EVOLVE_LEVEL"], a.sym["EVOLVE_STAT"]):
                    lvl = max(param, base + 1)
                elif method == a.sym["EVOLVE_HAPPINESS"]:
                    lvl = base + 1
                else:  # item, trade
                    lvl = base
                best = lvl if best is None else min(best, lvl)
        memo[sid] = best if best is not None else 1
        return memo[sid]

    def evolve_level(self, sid):
        """Level by which sid would normally have evolved (0 = fully evolved):
        the level of a level-up evolution, else NFE_STONE_TRADE_LEVEL."""
        a = self.asm
        best = 0
        for method, param, _ in self.evos[sid - 1]:
            lvl = (
                param
                if method in (a.sym["EVOLVE_LEVEL"], a.sym["EVOLVE_STAT"])
                else NFE_STONE_TRADE_LEVEL
            )
            best = lvl if not best else min(best, lvl)
        return best

    def move_levels(self, sid):
        """{move: earliest level at which sid can know it} (rules in the module doc)."""
        banned = {self.asm.sym[m] for m in BANNED_MOVES}
        out = {}

        def add(mv, lvl):
            if mv not in banned and (mv not in out or lvl < out[mv]):
                out[mv] = lvl

        for m in [sid] + self.pre_evolutions(sid):
            ml = self.min_level(m)
            for lvl, mv in self.learnset[m - 1]:
                add(mv, max(lvl, ml))
            for mv in self.base[m - 1]["tmhm"]:
                add(mv, ml)
            for mv in self.egg_moves[m - 1]:
                add(mv, max(EGG_HATCH_LEVEL, ml))
        return out

    def legal_moves(self, sid, level=TEAM_LEVEL):
        return sorted(mv for mv, lvl in self.move_levels(sid).items() if lvl <= level)

    def excluded_species(self, level=TEAM_LEVEL):
        out = []
        for sid in range(1, self.num_pokemon + 1):
            if self.min_level(sid) > level or len(self.legal_moves(sid, level)) < 4:
                out.append(sid)
        return out


def check_expected_exclusions(data: Data):
    got = sorted(data.species[s - 1] for s in data.excluded_species())
    want = sorted(EXPECTED_EXCLUDED)
    if got != want:
        raise SystemExit(f"excluded species mismatch:\n  got  {got}\n  want {want}")
    return got
