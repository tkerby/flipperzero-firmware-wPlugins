"""Writes the binary 20q.dat described in SPEC.md section 5.

Pure stdlib. Little-endian throughout. Import `write_dataset` from
build_dataset.py, or run this file directly to emit the synthetic
8x6 golden file used by tests/.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path

MAGIC = b"F20Q"
FORMAT_VERSION = 1
HEADER_SIZE = 0xC0

FLAG_HAS_PRIORS = 1 << 0
FLAG_HAS_CATEGORIES = 1 << 1
# Addition to SPEC 5.2: a u8[Q] family block, located by a u32 at 0x98 (the
# first reserved word). The device needs question families for the "keep the
# questions varied" shortlist rule in SPEC 6.4, which is otherwise
# unimplementable -- the spec never gave families a home in the file.
FLAG_HAS_FAMILIES = 1 << 2

FAMILIES = (
    "taxonomy", "animal", "size", "material", "colour", "location",
    "function", "food", "sensory", "motion", "social", "misc",
)

MAX_ENTITY_NAME = 22
MAX_QUESTION_TEXT = 72

# Section 4.6. Non-linear, denser near the ends. Indices 7/8 straddle 0.5,
# so "no information" quantises to the middle of the table.
PYES_LUT = [
    0.02, 0.05, 0.10, 0.17, 0.25, 0.33, 0.42, 0.48,
    0.52, 0.58, 0.67, 0.75, 0.83, 0.90, 0.95, 0.98,
]

CATEGORIES = ("animal", "vegetable", "mineral", "other")


class PackError(Exception):
    pass


@dataclass
class Entity:
    name: str
    category: int = 3
    prior: float = 1.0
    tags: list = field(default_factory=list)


@dataclass
class Question:
    text: str
    family: str = "misc"
    defaults: dict = field(default_factory=dict)

    @property
    def family_id(self) -> int:
        if self.family not in FAMILIES:
            raise PackError(f"unknown question family {self.family!r}")
        return FAMILIES.index(self.family)


def quantise(p: float) -> int:
    """Nearest-value index into PYES_LUT."""
    best_i, best_d = 0, 9.9
    for i, v in enumerate(PYES_LUT):
        d = abs(v - p)
        if d < best_d:
            best_i, best_d = i, d
    return best_i


def _check_ascii(s: str, limit: int, what: str) -> bytes:
    if not s:
        raise PackError(f"empty {what}")
    if len(s) > limit:
        raise PackError(f"{what} too long ({len(s)} > {limit}): {s!r}")
    try:
        raw = s.encode("ascii")
    except UnicodeEncodeError as exc:
        raise PackError(f"{what} is not 7-bit ASCII: {s!r}") from exc
    for b in raw:
        if b < 0x20 or b > 0x7E:
            raise PackError(f"{what} has a non-printable byte {b:#04x}: {s!r}")
    return raw


def _text_block(strings, limit, what):
    """Returns (index_bytes, text_bytes). Strings are not NUL-terminated;
    length comes from index[i+1] - index[i], hence N+1 entries."""
    text = bytearray()
    index = bytearray()
    for s in strings:
        index += struct.pack("<I", len(text))
        text += _check_ascii(s, limit, what)
    index += struct.pack("<I", len(text))
    return bytes(index), bytes(text)


def write_dataset(
    path,
    entities,
    questions,
    pmatrix,
    dataset_id: str = "20q-dev",
    qrank=None,
    build_epoch: int = 0,
):
    """pmatrix[j][i] = P(yes | entity i, question j)  -- column-major, as stored."""
    n = len(entities)
    q = len(questions)
    if n == 0 or q == 0:
        raise PackError("empty dataset")
    if n > 16384 or q > 1024:
        raise PackError(f"exceeds device guard rails: N={n} Q={q}")
    if len(pmatrix) != q:
        raise PackError(f"pmatrix has {len(pmatrix)} columns, expected {q}")

    names = [e.name for e in entities]
    for nm in names:
        if nm != nm.lower():
            # The device compares case-insensitively; keeping the file
            # lowercase makes byte order and casefold order identical.
            raise PackError(f"entity names must be lower-case: {nm!r}")
    if names != sorted(names):
        # Section 8.6: the learn flow binary-searches this list.
        raise PackError("entity names must be sorted for on-device binary search")
    if len(set(names)) != n:
        dupes = sorted({x for x in names if names.count(x) > 1})
        raise PackError(f"duplicate entity names: {dupes[:5]}")

    ent_index, ent_text = _text_block(names, MAX_ENTITY_NAME, "entity name")
    qst_index, qst_text = _text_block(
        [x.text for x in questions], MAX_QUESTION_TEXT, "question text"
    )

    # u16 quantised priors, 65535 == the largest weight in the set.
    maxprior = max(max((e.prior for e in entities), default=1.0), 1e-9)
    prior_blk = b"".join(
        struct.pack("<H", max(1, min(65535, round(e.prior / maxprior * 65535.0))))
        for e in entities
    )
    cat_blk = bytes(min(3, max(0, e.category)) for e in entities)

    if qrank is None:
        qrank = list(range(q))
    if sorted(qrank) != list(range(q)):
        raise PackError("qrank must be a permutation of 0..Q-1")
    qrank_blk = b"".join(struct.pack("<H", j) for j in qrank)
    family_blk = bytes(x.family_id for x in questions)

    stride = (n + 1) // 2
    matrix = bytearray(stride * q)
    for j in range(q):
        col = pmatrix[j]
        if len(col) != n:
            raise PackError(f"column {j} has {len(col)} rows, expected {n}")
        base = j * stride
        for i, p in enumerate(col):
            nib = quantise(p)
            off = base + (i >> 1)
            if i & 1:
                matrix[off] = (matrix[off] & 0x0F) | (nib << 4)   # odd -> high
            else:
                matrix[off] = (matrix[off] & 0xF0) | nib          # even -> low

    # Lay the blocks out after the header; any order is legal, offsets rule.
    blocks = []
    cursor = HEADER_SIZE
    offsets = {}
    for key, blk in (
        ("ent_index", ent_index), ("ent_text", ent_text),
        ("qst_index", qst_index), ("qst_text", qst_text),
        ("prior", prior_blk), ("category", cat_blk),
        ("qrank", qrank_blk), ("family", family_blk), ("matrix", bytes(matrix)),
    ):
        pad = (-cursor) % 4
        if pad:
            blocks.append(b"\0" * pad)
            cursor += pad
        offsets[key] = cursor
        blocks.append(blk)
        cursor += len(blk)

    body = b"".join(blocks)
    crc = zlib.crc32(body) & 0xFFFFFFFF

    did = _check_ascii(dataset_id, 32, "dataset_id")
    header = bytearray(HEADER_SIZE)
    struct.pack_into(
        "<4sHHIIIIIIIIII", header, 0,
        MAGIC, FORMAT_VERSION,
        FLAG_HAS_PRIORS | FLAG_HAS_CATEGORIES | FLAG_HAS_FAMILIES,
        n, q,
        offsets["matrix"], offsets["ent_index"], offsets["ent_text"],
        offsets["qst_index"], offsets["qst_text"],
        offsets["prior"], offsets["category"], offsets["qrank"],
    )
    struct.pack_into("<16f", header, 0x30, *PYES_LUT)
    struct.pack_into("<II", header, 0x70, crc, build_epoch)
    header[0x78:0x78 + len(did)] = did
    struct.pack_into("<I", header, 0x98, offsets["family"])

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(header) + body)
    return {
        "path": str(path), "entities": n, "questions": q,
        "bytes": HEADER_SIZE + len(body), "matrix_bytes": stride * q, "crc32": crc,
    }


def synthetic():
    """The 8 x 6 golden fixture from SPEC 11.1. Values are deliberately
    hand-checkable: entity i answers question j with p = LUT[(i + 2*j) % 16]."""
    names = ["ant", "boat", "cake", "dog", "eel", "fig", "gate", "hat"]
    cats = [0, 2, 1, 0, 0, 1, 2, 2]
    ents = [Entity(nm, c, 1.0 + i) for i, (nm, c) in enumerate(zip(names, cats))]
    qs = [Question(f"Question number {j}?", FAMILIES[j]) for j in range(6)]
    mtx = [[PYES_LUT[(i + 2 * j) % 16] for i in range(8)] for j in range(6)]
    return ents, qs, mtx


if __name__ == "__main__":
    import sys
    out = sys.argv[1] if len(sys.argv) > 1 else "tests/golden_8x6.dat"
    e, q, m = synthetic()
    info = write_dataset(out, e, q, m, dataset_id="golden-8x6", qrank=[3, 1, 5, 0, 4, 2])
    print(info)
