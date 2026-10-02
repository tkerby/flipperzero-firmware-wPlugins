# Flipper Zero — "20Q" Offline Guessing Game
## Implementation Specification v1.0

**Audience:** Claude Code (or any competent C developer) building this app from scratch.
**Deliverable:** a Flipper Zero `.fap` plus an offline Python data-build pipeline.
**Read this whole file before writing code.** Section 14 lists the milestones in build order.

---

## 0. TL;DR

The user thinks of a thing. The Flipper asks up to 20 yes/no questions and guesses what it is.

There is **no neural network running on the Flipper** and there is no internet. What runs on-device is
**Bayesian belief updating over a precomputed entity x question probability table** stored on the SD card —
this is what Akinator and 20Q.net actually do under the hood, and it is the only approach that fits in
196 KB of RAM at 64 MHz.

The "neural network" part lives **offline, on your dev machine**: a Python pipeline pulls commonsense
knowledge from Hugging Face datasets, optionally densifies gaps with an LLM, and compiles everything into
one compact binary blob (`20q.dat`) that ships with the app.

```
  Hugging Face datasets  ──►  Python pipeline  ──►  20q.dat  ──►  Flipper reads it
  (McRae norms,               (build_dataset.py)     ~200 KB      (streams columns
   Quasimodo, ConceptNet)                                          from SD, Bayes)
```

---

## 1. Hard constraints (verified — do not design around guesses)

| Constraint | Value | Source |
|---|---|---|
| MCU | STM32WB55RG, Cortex-M4F @ 64 MHz, hardware FPU (FPv4-SP-D16, **single precision only**) | docs.flipper.net tech-specs |
| RAM | 256 KB total, shared with the BLE core. Linker `RAM1` = **196,600 bytes**, and firmware `.data`/`.bss` eat into that before your app loads | `targets/f7/stm32wb55xx_flash.ld` |
| Usable heap for a FAP | **No official number exists.** It is link-time derived and changes per firmware build. Measure with the `free` CLI command or `memmgr_get_free_heap()`. Budget conservatively: assume **< 40 KB** is safely yours. | `furi/core/memmgr.h` |
| `malloc` failure | **Crashes the system immediately.** `malloc()` never returns NULL on Flipper. You cannot recover. Plan memory statically. | `furi/core/memmgr_heap.c` |
| Stack overflow | **Not detected at runtime** (`configCHECK_FOR_STACK_OVERFLOW = 0`). Undefined behaviour. | `targets/f7/inc/FreeRTOSConfig.h` |
| Screen | 128 x 64, 1 bit per pixel, monochrome | `gui_i.h` |
| Compiler | `-Werror -Wall -Wextra -Wdouble-promotion -fsingle-precision-constant` | `site_scons/cc.scons` |
| `float` | Hardware-accelerated, use it freely. **`double` is software-emulated — never use it.** | ditto |
| `printf("%f", x)` | **Fails to compile** without an explicit `(double)` cast, because of `-Wdouble-promotion -Werror`. Firmware idiom: `printf("%f", (double)x);` | `lfrfid_cli.c:342` |
| Math funcs | Only `powf` and `roundf` are firmware exports; everything else (`logf`, `sqrtf`, `expf`) links statically from `-lm`, which **inflates the .fap and therefore RAM at load**. Avoid transcendentals in hot loops. | `site_scons/extapps.scons:47` |
| Linking | Symbol-table based, not dynamic. Only symbols marked `+` in `targets/f7/api_symbols.csv` resolve. | `documentation/AppsOnSDCard.md` |
| API version | FAP embeds `{major, minor}`; a major mismatch refuses to load. Current: `release` 1.4.3 -> API 87.1, `dev` -> API 88.2. | `targets/f7/api_symbols.csv` |

### The single most important design consequence

> **Never load the dataset into RAM. Stream it from the SD card.**

Everything in section 5 follows from this.

---

## 2. Data sources (verified to exist on Hugging Face)

You are building the table offline. These are the raw inputs. **Verify licences before shipping** —
see section 12.

### 2.1 Primary: `metaeval/semantic-feature-production-norms`

The McRae et al. feature-production norms. Human participants were shown a concept and asked to list its
properties; the dataset records how many of 30 people listed each property. That count **is** a
probability estimate, which is exactly what this app needs.

- **Schema** (47 columns; you need these):

  | Column | Type | Use |
  |---|---|---|
  | `Concept` | string | entity name, e.g. `accordion` |
  | `Feature` | string | property, e.g. `has_keys`, `a_musical_instrument`, `requires_air` |
  | `Prod_Freq` | int64 | how many of 30 participants produced it -> **`P(yes) ~ Prod_Freq / 30`** |
  | `BR_Label` | string | feature taxonomy: `taxonomic`, `visual-form_and_surface`, `visual-colour`, `visual-motion`, `functional`, `encyclopaedic`, `sound`, `taste`, `smell`, `tactile` |
  | `WB_Label` | string | finer type: `superordinate`, `external_component`, `contingency`, ... |
  | `Distinct` | float | distinctiveness — useful for weighting question quality |

- **Real rows** (verified via the HF datasets-server):
  ```
  accordion | a_musical_instrument | superordinate      | taxonomic                 | Prod_Freq=28
  accordion | has_keys             | external_component | visual-form_and_surface   | Prod_Freq=17
  accordion | requires_air         | contingency        | encyclopaedic             | Prod_Freq=11
  ```
- **Coverage:** ~541 concrete concepts x ~2,526 distinct features. Small but *dense and trustworthy*.
- **Role in the pipeline:** the gold-standard seed. Use it to (a) design the question bank, (b) calibrate,
  (c) sanity-check the noisier sources.

### 2.2 Breadth: `Aunsiels/Quasimodo`

Commonsense assertions mined from search-engine query logs and QA forums, with plausibility scores.

- **Schema:** `subject` (str), `predicate` (str), `object` (str), `modality` (str),
  `is_negative` (int64), `score` (float64), `sentences source` (str), `typicality` (float64),
  `saliency` (float64)
- **Real rows:**
  ```
  man      | have         | nipple    | is_negative=0 | score=1.0 | typicality=1.0
  location | show up on   | instagram | is_negative=1 | score~1.0 | typicality=1.0
  ```
- **Coverage:** millions of triples over tens of thousands of subjects.
- **Caveats:** web-scraped, so it is noisy and contains junk subjects (`mutual friend`, `location`) and
  occasional offensive content. **Filter aggressively** — see 4.3.
- **Role:** extends the entity vocabulary far beyond McRae's 541 concepts.
- Companion: `Aunsiels/Quasimodo-GenT` (generated/typed variant) — optional.

### 2.3 Structure: `conceptnet5/conceptnet5`

The classic commonsense graph. Relations you care about: `/r/IsA`, `/r/HasA`, `/r/PartOf`, `/r/MadeOf`,
`/r/UsedFor`, `/r/CapableOf`, `/r/HasProperty`, `/r/AtLocation`, `/r/HasContext`.

- Alternatives if the canonical repo is awkward to load:
  - `peandrew/conceptnet_en_nomalized` — normalised English subset
  - `peandrew/conceptnet_en_simple`
  - `openworld-domains/conceptnet-full-en-essentials` — pre-trimmed English essentials
  - `research-backup/conceptnet_high_confidence` — pre-filtered by confidence
- **Role:** taxonomy backbone (`is it an animal / a tool / a food`) and the `AtLocation` / `UsedFor`
  question families, which McRae covers poorly.
- **Licence:** CC BY-SA 4.0 — this is share-alike and **will infect your shipped data file**. See 12.

### 2.4 Vocabulary sanity check: `jtv199/Entity-deduction-arena-20-questions`

Entity lists from the "Entity Deduction Arena" 20-questions benchmark (arXiv 2310.01468). Small.
Use it purely to validate that your entity vocabulary contains the kinds of things people actually think
of when playing 20 questions, and as a held-out evaluation set (section 11).

### 2.5 Optional densification pass

The above sources are **sparse**: most (entity, question) cells will have no evidence. Section 4.5
describes an optional offline LLM pass to fill them. This is where "the neural network" genuinely helps —
but it happens on your laptop, once, at build time. The Flipper never sees a model.

---

## 3. Game design

### 3.1 Scope

Match 20Q.net's breadth: **anything concrete**. The classic four top-level buckets are used only as an
optional opening hint, not as separate datasets:

| Bucket | Examples | Target share of entity set |
|---|---|---|
| Animal | dog, octopus, hummingbird | 30% |
| Vegetable (living / organic / food) | oak tree, banana, bread, cheese | 25% |
| Mineral (man-made / inanimate) | hammer, piano, skyscraper, sword | 40% |
| Other (concepts, places) | rainbow, desert, birthday | 5% |

**v1 target: 2,048 entities x 192 questions.** This is a *deliberate* starting point, not a limit — the
binary format stores counts in the header, so scaling to 8,192 x 256 later requires no code change
(see 5.6). Ship v1 small so you can iterate on data quality, which matters far more than raw entity count.

### 3.2 Flow

```
Splash -> [optional category hint] -> Question loop (up to 20)
   -> Guess #1 -> right? -> Win screen
                -> wrong? -> up to 5 more questions -> Guess #2
                          -> wrong? -> Guess #3
                          -> wrong? -> Learn screen (teach it) -> Save
```

### 3.3 Answer set

Five answers, mapped onto a single "truthiness" weight `w`:

| Button / label | `w` | Notes |
|---|---|---|
| Yes | 1.00 | |
| Probably | 0.75 | |
| Don't know | 0.50 | no belief update at all — skip the multiply, but **do** mark the question asked |
| Probably not | 0.25 | |
| No | 0.00 | |

Also available at any time: **Back** = undo last answer (restore previous belief snapshot),
**Long-press Back** = quit.

### 3.4 Guess trigger

After each answer, guess if **either**:

- `b_top >= 0.85` **and** `b_top / b_second >= 6.0`, **or**
- `questions_asked >= 20`

Cap at 3 guesses. Between guesses, allow 5 extra questions and set the just-rejected entity's belief to 0.

---

## 4. Offline data pipeline (`tools/build_dataset.py`)

Runs on the dev machine. Python 3.10+. Output: `files/20q.dat`.

```
tools/
  build_dataset.py        # orchestrator, CLI
  sources/
    mcrae.py              # metaeval/semantic-feature-production-norms loader
    quasimodo.py          # Aunsiels/Quasimodo loader + filters
    conceptnet.py         # conceptnet5 loader + relation extraction
  questions.yaml          # THE QUESTION BANK — hand-authored, see 4.2
  entities.yaml           # curated entity vocabulary + aliases + category
  mapping.yaml            # source-predicate -> question-id rules
  densify.py              # optional LLM fill pass
  pack.py                 # writes the binary 20q.dat
  eval.py                 # offline simulator, see section 11
```

### 4.1 Stages

1. **Build entity vocabulary** -> `entities.yaml`
   Seed from McRae's 541 concepts + top-N Quasimodo subjects by frequency + a hand-curated list of
   obvious 20Q answers (animals, household objects, foods, vehicles, instruments, body parts, jobs,
   places). Normalise to singular lower-case. Record for each: `id`, `display_name` (<= 22 chars, ASCII
   only — the Flipper fonts are `_tr` reduced-ASCII), `category` (0-3), `prior_weight`
   (how likely a person is to pick it — a hummingbird is a less likely answer than a dog).

2. **Author the question bank** -> `questions.yaml` (see 4.2)

3. **Map source assertions onto questions** -> per-cell evidence list, via `mapping.yaml`

4. **Aggregate evidence -> `P(yes | entity, question)`** (see 4.4)

5. **Densify empty cells** (see 4.5)

6. **Quantise + pack** -> `20q.dat` (see 4.6, format in section 5)

7. **Evaluate** with the offline simulator (section 11). Iterate on 2/3/5 until win-rate is acceptable.

### 4.2 The question bank

**This is the highest-leverage artefact in the whole project. Hand-author it. Do not auto-generate it.**

192 questions, English, phrased so that a player who is thinking of the entity can answer without
ambiguity, and **short enough to fit in 3 lines of 128 px `FontPrimary`** — approx. **60 characters max**,
hard limit 72. Group by family so the pipeline can enforce coverage:

| Family | Count | Examples |
|---|---|---|
| Top-level taxonomy | 12 | `Is it alive?` `Is it man-made?` `Is it an animal?` `Is it a plant?` |
| Animal traits | 28 | `Does it have fur?` `Can it fly?` `Does it lay eggs?` `Is it a pet?` `Does it live in water?` |
| Size & shape | 18 | `Is it bigger than a car?` `Can you hold it in one hand?` `Is it round?` `Is it flat?` |
| Material | 14 | `Is it made of metal?` `Is it made of wood?` `Is it made of glass?` |
| Colour | 10 | `Is it usually green?` `Is it usually red?` |
| Location | 20 | `Would you find it in a kitchen?` `Is it found outdoors?` `Is it found in an office?` |
| Function & use | 26 | `Is it used for cooking?` `Do you wear it?` `Is it a tool?` `Is it used for transport?` |
| Food | 14 | `Can you eat it?` `Is it sweet?` `Is it a fruit?` `Is it cooked before eating?` |
| Sensory | 12 | `Does it make a sound?` `Does it have a strong smell?` `Is it soft?` `Is it heavy?` |
| Behaviour / motion | 12 | `Does it move on its own?` `Does it need electricity?` `Does it have moving parts?` |
| Social / cultural | 14 | `Would a child recognise it?` `Is it expensive?` `Is it found in most homes?` |
| Misc / tiebreakers | 12 | `Does it have a handle?` `Does it have a screen?` `Does it come in a pair?` |

Each entry in `questions.yaml`:

```yaml
- id: 37
  text: "Can you hold it in one hand?"     # <= 60 chars, ASCII only
  family: size
  negate_ok: true          # can be rendered as a negation for variety (unused in v1)
  default_by_category:     # fallback P(yes) when there is zero evidence, per category 0..3
    animal: 0.30
    vegetable: 0.55
    mineral: 0.45
    other: 0.20
```

`default_by_category` is what stops sparse data from being catastrophic. It is a **prior**, and it is
what the engine falls back to for any cell the sources never touched.

### 4.3 Source filtering (Quasimodo especially)

Quasimodo is web-mined and will happily tell you things you do not want on a game device.
Apply, in order:

1. Drop rows where `score < 0.6` or `typicality < 0.5`.
2. Drop rows whose `subject` is not in `entities.yaml`.
3. Drop rows whose `predicate` does not appear in `mapping.yaml`.
4. Run subject/predicate/object through a **blocklist** (slurs, sexual content, self-harm, violence
   against people). Maintain `tools/blocklist.txt`. Also blocklist entities themselves — the entity
   vocabulary is curated in step 1, so this is mostly belt-and-braces.
5. Log everything dropped to `build/dropped.tsv` so you can audit it.

Do the same, more lightly, for ConceptNet (drop `weight < 1.0`, drop non-English, drop `/c/en/x/n`
sense-tagged duplicates by collapsing to the lemma).

### 4.4 Evidence aggregation

For a cell `(e, q)`, you may have zero or many pieces of evidence. Each piece is
`(p_hat, confidence)`:

| Source | `p_hat` | `confidence` |
|---|---|---|
| McRae | `Prod_Freq / 30` | `1.0` (human-produced, high trust) |
| Quasimodo | `score` if `is_negative == 0`, else `1 - score` | `0.45 * typicality` |
| ConceptNet | `min(1.0, weight / 3.0)` | `0.6` |
| LLM densification | model's stated probability | `0.35` |
| Hand override (`overrides.yaml`) | as written | `2.0` (wins everything) |

Combine as a **confidence-weighted Beta posterior**:

```
alpha = alpha0 + sum_k conf_k * p_hat_k * K
beta  = beta0  + sum_k conf_k * (1 - p_hat_k) * K
P(yes | e, q) = alpha / (alpha + beta)
```

where `K = 8` (evidence strength scale) and `alpha0, beta0` come from the category prior:
`alpha0 = 2 * default_by_category[cat(e)]`, `beta0 = 2 * (1 - default_by_category[cat(e)])`.

This gives you three things for free: sparse cells fall back to the prior; a single weak source cannot
push a cell to 0.99; and conflicting sources average sensibly.

### 4.5 Optional LLM densification

After aggregation, cells whose total evidence weight is below a threshold are still effectively
at the prior. For the ~10-20% of cells that matter most (high-`Distinct` questions x high-`prior_weight`
entities), you can fill them properly with an offline LLM pass:

- Batch by entity: one prompt per entity, asking for `P(yes)` on all 192 questions at once as JSON.
- Force structured output, validate the schema, clamp to `[0.02, 0.98]`.
- **Cache aggressively** to `build/llm_cache/<entity_id>.json` so re-runs are free.
- Feed results back through 4.4 with `confidence = 0.35` so human data still outranks the model.
- Make this a flag: `python build_dataset.py --densify` — the build must work without it.

This is entirely optional. Build and evaluate without it first, so you can measure whether it helps.

### 4.6 Quantisation

Store each `P(yes)` as a **4-bit index** into a 16-entry lookup table. The table is non-linear, denser
near the ends where discrimination matters:

```python
PYES_LUT = [0.02, 0.05, 0.10, 0.17, 0.25, 0.33, 0.42, 0.48,
            0.52, 0.58, 0.67, 0.75, 0.83, 0.90, 0.95, 0.98]
```

Quantise by nearest value. Note index 7/8 straddle 0.5, so "no information" naturally lands there.
Emit the LUT into the file header so the C code never hardcodes it.

---

## 5. Binary data format — `20q.dat`

One file, little-endian, ships as a FAP file asset. Read-only on device.

### 5.1 Why column-major

The engine does exactly two kinds of read:

1. **Belief update** — after the user answers question `j`, it needs `P(yes | e, j)` for **every** entity.
   That is one *column*.
2. **Question selection** — it needs the same thing for a shortlist of candidate questions.
   That is a handful of *columns*.

So the matrix is stored **column-major**: all entities' answers to question 0, then all answers to
question 1, and so on. One question = one contiguous `ceil(N/2)`-byte read. For N=2048 that is a single
**1,024-byte** sequential read. Row-major would require 2,048 scattered 96-byte reads for the same work.

### 5.2 Layout

```
offset  size    field
------  ------  -----------------------------------------------------------
0x00    4       magic          "F20Q"  (0x46 0x32 0x30 0x51)
0x04    2       format_version u16, = 1
0x06    2       flags          u16, bit0 = has_priors, bit1 = has_categories
0x08    4       entity_count   u32   N
0x0C    4       question_count u32   Q
0x10    4       matrix_off     u32   -> matrix block
0x14    4       ent_index_off  u32   -> u32[N+1] byte offsets into entity text block
0x18    4       ent_text_off   u32   -> entity name block (NUL-free, length via index)
0x1C    4       qst_index_off  u32   -> u32[Q+1] byte offsets into question text block
0x20    4       qst_text_off   u32   -> question text block
0x24    4       prior_off      u32   -> u16[N] quantised prior weights (0 = absent)
0x28    4       category_off   u32   -> u8[N] category id 0..3 (0 = absent)
0x2C    4       qrank_off      u32   -> u16[Q] static question ranking, best first
0x30    64      pyes_lut       float32[16]  the quantisation LUT
0x70    4       crc32          u32   CRC-32 (IEEE) of the data-block region, 0xC0..EOF
0x74    4       build_epoch    u32   unix timestamp of the build
0x78    32      dataset_id     char[32]  human-readable build tag, NUL-padded
0x98    40      reserved       zero
0xC0    ...     -- blocks begin here, in any order, located by the offsets above --
```

### 5.3 Matrix block

```
Q columns, each ceil(N/2) bytes, contiguous.
column j starts at:  matrix_off + j * ((N + 1) / 2)

Within a column, entity i's nibble:
    byte_index = i >> 1
    nibble     = (i & 1) ? (b >> 4) : (b & 0x0F)      // even entity = LOW nibble
    p_yes      = pyes_lut[nibble]
```

For N=2048, Q=192: `192 * 1024 = 196,608 bytes` of matrix. Whole file lands around **235 KB**.

### 5.4 Text blocks

Strings are **not NUL-terminated**. Length is `index[i+1] - index[i]`. The index array has `N+1`
entries so the last length is computable. All text is **7-bit ASCII** — the Flipper's `FontPrimary`
(`u8g2_font_helvB08_tr`) and `FontSecondary` (`u8g2_font_haxrcorp4089_tr`) are reduced-ASCII fonts with
no glyphs above 0x7F. Enforce this in `pack.py` and fail the build on violation.

Entity names: max 22 chars. Question text: max 72 chars.

### 5.5 `qrank` — the static question ranking

`u16[Q]`, question indices sorted by global discriminative power (computed offline as expected Gini
reduction against the uniform prior). The device uses this to build its per-turn candidate shortlist
without scanning all Q columns — see 6.4. This is a pure optimisation; the game is correct without it,
just slower.

### 5.6 Scaling

Nothing in the C code may hardcode `N` or `Q`. Read them from the header, compute stride at runtime,
allocate the belief array from the heap based on `N`. Then `2048 x 192` today and `8192 x 256` later is
a data change only. Guard rails: refuse to load if `N > 16384` or `Q > 1024` or
`N * ((N+1)/2)` would overflow, and show a clean error rather than crashing.

### 5.7 Learned-entity overlay — `learned.dat`

Written by the app to `APP_DATA_PATH("learned.dat")` when a player teaches it something new.
Same-endianness, append-only, no rewriting:

```
header:  magic "F20L" | u16 version=1 | u16 record_count | u32 base_dataset_crc32
record:  u8  name_len
         char name[name_len]          // ASCII, <= 22
         u8  category
         u16 answer_count
         { u16 question_id ; u8 nibble }  x answer_count
```

`base_dataset_crc32` must match the loaded `20q.dat`'s `crc32`; if it does not, ignore the overlay
(the dataset was replaced and question IDs no longer mean the same thing) and rename it to
`learned.dat.stale`. Cap at **128 records** — refuse further learning and tell the user.

Learned entities occupy virtual indices `N .. N + record_count - 1`. Their unanswered questions use
the category prior. Keep the overlay fully in RAM: 128 records x ~64 bytes = 8 KB worst case, which is
acceptable. Sparse answers stored as a small sorted array per record, binary-searched.

---

## 6. The inference engine

Pure C, no dependencies beyond `libm`. Lives in `helpers/twentyq_engine.{c,h}` and must be
**testable off-device** — no Furi calls inside the maths. File I/O goes through a tiny abstraction
(section 6.7) so the same engine code runs under a desktop unit-test harness.

### 6.1 State

```c
typedef struct {
    Tq_Dataset* ds;          // header + open file handle
    float*      belief;      // [N + learned_count], normalised, sums to 1.0
    uint8_t*    asked;       // bitmap, ceil(Q/8) bytes
    uint16_t    asked_order[TQ_MAX_QUESTIONS_PER_GAME];  // for undo + the learn screen
    uint8_t     answers[TQ_MAX_QUESTIONS_PER_GAME];      // Tq_Answer values
    uint16_t    asked_count;
    uint16_t    rejected[TQ_MAX_GUESSES];  // entities the player already said no to
    uint8_t     rejected_count;
    uint8_t     category_hint;             // 0xFF = none
    uint8_t*    colbuf;                    // scratch, ceil(N/2) bytes
} Tq_Engine;
```

Memory for N=2048: `belief` 8,704 B + `asked` 24 B + `colbuf` 1,024 B + overlay <= 8,192 B
= **under 18 KB**. Comfortable. For N=8192 it is ~45 KB — still fine, but re-measure with `free`.

### 6.2 Initialisation

```
belief[i] = prior[i] / sum(prior)          // from prior_off, or uniform if absent
if category_hint != NONE:
    belief[i] *= (category[i] == hint) ? 1.0f : 0.05f     // soft, not a hard filter —
    renormalise                                          // players mis-categorise things
```

Never hard-zero on the category hint. People genuinely disagree about whether a mushroom is a vegetable.

### 6.3 Belief update

The user answered question `j` with weight `w` (section 3.3).

```c
// Likelihood of the observed answer given that the entity is e:
//   L = eps + (1 - 2*eps) * ( w*p + (1-w)*(1-p) )
// eps is a noise floor absorbing both bad data and player error.
#define TQ_EPS 0.05f

if (w == 0.5f) { mark_asked(j); return; }   // "Don't know" -> no information

read_column(j, colbuf);
float sum = 0.0f;
for (uint32_t i = 0; i < n_total; i++) {
    float p = pyes_of(colbuf, i);                       // learned entities: overlay lookup
    float L = TQ_EPS + (1.0f - 2.0f*TQ_EPS) * (w*p + (1.0f - w)*(1.0f - p));
    belief[i] *= L;
    sum += belief[i];
}
for (uint32_t i = 0; i < n_total; i++) belief[i] *= (1.0f / sum);   // renormalise every turn
mark_asked(j);
```

Renormalising every turn keeps `max(belief)` near 1.0, so float32 never underflows. Minimum
multiplier is `TQ_EPS = 0.05`; even 25 consecutive worst-case answers gives `0.05^25 ~ 3e-33`, still
inside float32's normal range, and normalisation resets it anyway.

`TQ_EPS` is the single most important tuning knob. Too low and one bad data cell permanently kills the
right answer; too high and 20 questions carry too little information. Start at 0.05, tune with the
offline simulator (section 11).

### 6.4 Question selection

Maximise expected purity. **Use Gini impurity, not Shannon entropy** — entropy needs a `logf` per cell,
which at 192 x 2048 cells is ~400k transcendental calls per turn and will take seconds. Gini needs only
multiply-add.

For a candidate question `j`:

```
  s_yes = sum_i  b_i * p_ij                       (probability the answer is "yes")
  s_no  = 1 - s_yes
  A     = sum_i (b_i * p_ij)^2
  B     = sum_i (b_i * (1 - p_ij))^2

  purity(j) = A / max(s_yes, 1e-6) + B / max(s_no, 1e-6)      // higher is better
```

This is `1 - E[Gini after asking j]`, so maximising it minimises expected remaining confusion.
It naturally prefers questions that split the surviving mass near 50/50 **and** penalises questions
where the data is all near 0.5 (uninformative). Six flops per entity per candidate.

**Bounding the cost.** Evaluating all Q columns means reading the whole matrix every turn
(196 KB on SD, 20 times a game). Instead, build a **candidate shortlist** of
`TQ_CANDIDATES = 40` questions per turn:

- 28 taken in order from `qrank[]`, skipping already-asked ones
- 8 chosen by highest current variance heuristic — cheap proxy: the 8 unasked questions from families
  not yet represented in `asked_order` (keeps the questions feeling varied rather than 5 colour
  questions in a row)
- 4 chosen pseudo-randomly from the unasked set (`furi_hal_random_get()`), so repeat games differ

Cost per turn: 40 columns x 1,024 B = **40 KB read**, 40 x 2,048 x 6 = **~490k flops**. On a 64 MHz M4F
that is roughly 30-60 ms of compute; SD read dominates. Show the loading view (6.6) if it exceeds
~150 ms measured.

Tie-break: among questions within 2% of the best purity, prefer the one from the least-used family,
then the lower `qrank` index.

**Never ask the same question twice** — check the `asked` bitmap before scoring.

### 6.5 Guessing

```c
// find top-2 by belief, skipping entities in rejected[]
if (b_top >= 0.85f && b_top >= 6.0f * b_second) -> guess now
if (asked_count >= 20)                          -> guess now (best available)
```

On a rejected guess: `belief[rejected] = 0`, renormalise, push onto `rejected[]`, allow 5 more questions.

### 6.6 Performance budget

Everything above happens **on the app's own thread inside a scene**, not in the draw callback.
Budget per turn: **< 400 ms**. If a turn exceeds ~150 ms measured on real hardware, push the work into a
`FuriThread` and show `gui/modules/loading.h`'s spinner via the ViewDispatcher, so the UI never blocks.
Measure with `furi_get_tick()` and log with `FURI_LOG_D`. Do not guess at timings — instrument it.

### 6.7 Off-device testability

```c
typedef struct {
    bool (*read_at)(void* ctx, uint32_t offset, void* buf, size_t len);
    void* ctx;
} Tq_Reader;
```

On Flipper, `ctx` is a `File*` and `read_at` does `storage_file_seek` + `storage_file_read`.
In tests, `ctx` is a `FILE*` or an in-memory buffer. **The engine must never `#include` a Furi header.**
This lets you run the section 11 simulator against the exact C code that ships.

---

## 7. Flipper application structure

### 7.1 Layout

```
flipper-20q/
  application.fam
  twentyq.png                 # 10x10, 1-bit PNG (see 7.2)
  twentyq_app.c               # entry point, app alloc/free, ViewDispatcher + SceneManager wiring
  twentyq_app_i.h             # app struct, scene/view/event enums
  scenes/
    twentyq_scene_config.h    # X-macro list of scenes
    twentyq_scene.c/.h        # generated handler tables
    twentyq_scene_start.c     # main menu
    twentyq_scene_category.c  # optional category hint
    twentyq_scene_question.c  # the question/answer screen
    twentyq_scene_thinking.c  # loading spinner while selecting a question
    twentyq_scene_guess.c     # "Are you thinking of a HAMMER?"
    twentyq_scene_result.c    # win / lose
    twentyq_scene_learn_name.c   # text input for the unknown entity
    twentyq_scene_learn_confirm.c
    twentyq_scene_stats.c
    twentyq_scene_settings.c
    twentyq_scene_about.c
  views/
    twentyq_question_view.c/.h   # custom View for the Q&A screen (see 8.2)
  helpers/
    twentyq_engine.c/.h       # pure C, no Furi
    twentyq_dataset.c/.h      # 20q.dat open/parse/read_column
    twentyq_learned.c/.h      # learned.dat overlay
    twentyq_storage.c/.h      # Furi Storage <-> Tq_Reader bridge, settings save/load
  files/
    20q.dat                   # shipped via fap_file_assets
  images/                     # optional icon assets
  tools/                      # the Python pipeline (section 4)
  tests/                      # desktop harness for the engine
  README.md
  docs/changelog.md
```

### 7.2 `application.fam`

```python
App(
    appid="twentyq",
    name="20 Questions",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="twentyq_app",
    stack_size=2 * 1024,
    fap_category="Games",
    fap_icon="twentyq.png",
    fap_icon_assets="images",
    fap_file_assets="files",
    fap_description="Think of anything. Your Flipper guesses it in 20 questions. Fully offline.",
    fap_author="<your name>",
    fap_weburl="https://github.com/<you>/flipper-20q",
    fap_version=(1, 0),
)
```

Notes:
- `appid` must match `^[a-z0-9_]+$` and determines `/ext/apps_data/twentyq/` and
  `/ext/apps_assets/twentyq/`.
- `name` is truncated at 32 chars in FAP metadata.
- `fap_category` must be exactly one of the 11 catalog categories; for this app it is **`Games`**.
- `fap_icon` must be 10x10 px PNG and, for catalog acceptance, **every pixel must be pure black
  `(0,0,0)` or fully transparent** — no greys, no white pixels.
- `fap_file_assets="files"` unpacks `files/` to `/ext/apps_assets/twentyq/` on first run and re-extracts
  whenever the content hash changes. Read it with `APP_ASSETS_PATH("20q.dat")`, **never** a raw
  `/ext/...` path.
- Start `stack_size` at `2 * 1024` and verify with the `top` CLI command. Raise only if the watermark
  warning appears on thread exit.

### 7.3 Architecture

**ViewDispatcher + SceneManager** — this is the idiomatic choice for a multi-screen app and gives you a
free back-navigation stack. Note that `gui/scene_manager.h` is not listed in the GUI service's
`sdk_headers` but every `scene_manager_*` symbol is exported and `view_dispatcher.h` includes it
transitively; include it directly, as all in-tree apps do.

Use `ViewDispatcherTypeFullscreen` for the question view so you get all 128x64 rather than
128x51 under the status bar.

---

## 8. UI specification

Screen is **128 x 64, 1-bit**. `canvas_draw_str` takes a **baseline** y, not a top y.
Font metrics: `FontPrimary` = `helvB08_tr`, height 8, leading 12. `FontSecondary` = `haxrcorp4089_tr`,
height 7, leading 11.

### 8.1 Main menu (`Submenu`)

```
  20 QUESTIONS
  > Play
    How it works
    Stats
    Settings
```

### 8.2 Question screen — custom `View`

Stock `DialogEx` gives you only three buttons; you need five answers plus a progress indicator.
Write a custom `View` with its own draw + input callbacks and a `with_view_model` state block.

```
+--------------------------------------------------------------+
|  Q 7/20                                     [||||||......]   |   y=0..8   FontSecondary + progress bar
|                                                              |
|     Can you hold it in one                                   |   y=16..40 FontPrimary,
|     hand?                                                    |            elements_multiline_text_aligned
|                                                              |            AlignCenter/AlignCenter at (64,28)
|                                                              |
|   No    Probably not   ?   Probably    Yes                   |   y=48..63 selector row
|                       ^^^                                     |
+--------------------------------------------------------------+
```

- Answer selector: **Left / Right** move through the 5 options, **OK** confirms.
  Selected option drawn with `canvas_draw_rbox` + `canvas_set_color(ColorWhite)` for inverted text.
  Default selection resets to the middle ("?") each question — or, better, to "Yes", which is the most
  common answer; make it a setting.
- **Up** = shortcut for "Yes", **Down** = shortcut for "No". Power users will find these; mention them
  in the How-it-works screen.
- **Back (short)** = undo the last answer. **Implement undo by replay, not by snapshot.** Snapshotting
  the belief vector costs 8.7 KB per level at N=2048 and does not scale; instead, reset the belief to the
  prior and replay `asked_order[]`/`answers[]` minus the last entry. That costs one column read per
  replayed question — at most 20 x 1 KB = 20 KB of sequential SD reads, well inside the 400 ms turn
  budget, and zero extra RAM.
- **Back (long)** = confirm-and-quit popup.
- Progress bar: `elements_progress_bar(canvas, x, y, w, (float)asked / 20.0f)`.
- Notification on each answer: a short `sequence_blink_blue_10` or a click, gated by the sound setting.

Question text longer than fits: use `elements_text_box` with wrapping, or pre-wrap in the pipeline.
Since `pack.py` enforces <= 72 chars, three lines of `FontPrimary` at ~24 chars/line always fits.

### 8.3 Thinking screen

Only shown if question selection measures slower than ~150 ms. Use `gui/modules/loading.h`, or draw a
1-frame-per-100 ms animated "..." with a `FuriTimer`. Do **not** do heavy work inside the draw callback.

### 8.4 Guess screen (`DialogEx` is fine here)

```
        I'm thinking of...

           HAMMER

  [ No ]              [ Yes! ]
```

`FontBigNumbers` is digits-only (`_tn`), so use `FontPrimary` for the entity name. If the name is wider
than 120 px (`canvas_string_width`), fall back to `FontSecondary`, then `elements_string_fit_width`.

### 8.5 Result screens

- **Win:** "Got it in 12 questions!" + `sequence_success` + green LED. Show a running win-rate.
- **Loss after 3 guesses:** "You win! What was it?" -> Learn flow.

### 8.6 Learn flow

1. `text_input` (`gui/modules/text_input.h`), header "What was it?", 22-char limit, with a
   `TextInputValidatorCallback` rejecting non-ASCII, empty, and names already in the dataset
   (binary-search the entity index — names must therefore be sorted in `pack.py`, or ship a sorted
   index; simplest is to require `pack.py` to emit entity names in sorted order).
2. Category picker (Animal / Vegetable / Mineral / Other).
3. Confirm screen: "Save NARWHAL so I remember next time?" -> append to `learned.dat`.
4. Optionally re-ask 3 of the questions the game got most wrong, to sharpen the new record. Skippable.

**Do not** let the app trigger any browser-style modal; that is not a risk here, but do keep every
storage write inside a scene and check the `FS_Error` — an SD card can be removed mid-game.

### 8.7 Settings (`VariableItemList`)

| Setting | Values | Default |
|---|---|---|
| Question limit | 15 / 20 / 25 | 20 |
| Category hint | Ask / Skip | Ask |
| Sound | On / Off | On |
| Vibration | On / Off | Off |
| Default answer | Yes / Middle | Yes |
| Reset learned words | action | — |
| Reset stats | action | — |

Persist to `APP_DATA_PATH("settings.bin")` — a small versioned struct, not a text format.
Handle a missing/corrupt file by writing defaults.

### 8.8 Stats

Games played, wins, win-rate, average questions to a correct guess, best game.
Store in `APP_DATA_PATH("stats.bin")`. Write after each game, and check `storage_file_sync()`.

---

## 9. Error handling

`malloc` cannot fail gracefully on Flipper, so the only recoverable errors are I/O and data errors.
Every one of these must produce a readable screen, not a crash:

| Condition | Behaviour |
|---|---|
| `20q.dat` missing | Popup: "Data file missing. Reinstall the app." + how to fix. Exit cleanly. |
| Bad magic / version / CRC | Popup: "Data file corrupt." Offer to continue with learned words only? No — just exit. |
| `N` or `Q` out of guard-rail range | Popup: "Unsupported data file." Exit. |
| SD card removed mid-game | `storage_file_read` returns short; detect, popup "SD card removed", exit to menu. |
| `learned.dat` CRC mismatch with base dataset | Silently rename to `learned.dat.stale`, continue. |
| `learned.dat` full (128 records) | Popup on save attempt: "Memory full — reset learned words in Settings." |
| Settings/stats file corrupt | Overwrite with defaults, no user-facing error. |

Always call `storage_file_close()` even when `storage_file_open()` failed — the Storage API explicitly
requires this.

---

## 10. Coding standards

- C11 (`-std=gnu11` per firmware defaults). No C++.
- `-Werror` is on with `-Wall -Wextra -Wdouble-promotion -Wredundant-decls -Wundef`.
  Unused params need `UNUSED(p)`.
- **Never use `double`.** Every float literal is already single-precision thanks to
  `-fsingle-precision-constant`, but write `1.0f` anyway. Cast explicitly for any `printf`-family
  call: `FURI_LOG_D(TAG, "b=%f", (double)b);`
- Avoid `sqrtf`/`expf`/`logf` in hot paths — they link statically from `-lm` and bloat the `.fap`,
  which costs RAM at load. The Gini formulation in 6.4 exists precisely to avoid them.
- Prefix everything `twentyq_` / `Tq_`. One translation unit per scene.
- `FURI_LOG_*` with `#define TAG "TwentyQ"`. Strip debug logs to `FURI_LOG_D`/`T` so they compile out.
- Use `FuriString` for anything dynamic; never `strcpy` into a fixed buffer without a bound.
- `furi_check`/`furi_assert` for invariants that indicate a bug; graceful handling for anything a
  user or a bad SD card can cause.

---

## 11. Testing

### 11.1 Desktop engine tests (`tests/`)

Because the engine has no Furi dependency (6.7), compile it with plain `gcc` and run:

- **Unit:** nibble unpacking on both parities; header parsing; column offset maths; belief update
  against hand-computed values; normalisation stability over 30 turns; Gini purity against a
  brute-force reference.
- **Golden file:** a tiny synthetic `20q.dat` (8 entities x 6 questions) checked into the repo with
  expected outputs.

### 11.2 Offline simulator (`tools/eval.py` + the C engine)

This is how you tune the data, and it is not optional. For every entity in a held-out set:

1. Pick entity `e`.
2. Play a full game where the simulated player answers question `j` by sampling from the *true*
   `P(yes | e, j)` — and, in a second mode, with 10% of answers deliberately corrupted to model a
   confused human.
3. Record: win-rate, mean questions to a correct first guess, top-3 accuracy.

Targets for v1 on the honest-player setting: **>= 80% win-rate within 20 questions**, mean
**<= 15 questions**. Below 70% means the data is too sparse — go back to 4.5.

Sweep `TQ_EPS` (0.02 / 0.05 / 0.10), the guess threshold, and `TQ_CANDIDATES` (20 / 40 / 80) and
report a table. Use `jtv199/Entity-deduction-arena-20-questions` entities as an external sanity set:
how many of them does your vocabulary even contain?

### 11.3 On-device checks

- `ufbt launch` then `ufbt cli` -> run `free` before and after opening the app; record the delta.
- Run `top` during a game; confirm the stack watermark stays above 256 bytes.
- Time a full turn with `furi_get_tick()` and log it. Confirm < 400 ms.
- Pull the SD card mid-game and confirm the error screen, not a crash.
- Play 10 real games by hand. This catches bad question wording, which no simulator will.

---

## 12. Licensing and content

**Check this before you ship — it constrains the whole project.**

- **ConceptNet** is **CC BY-SA 4.0**. Share-alike. If `20q.dat` is derived from ConceptNet, the derived
  data is arguably share-alike too, and you must attribute. The Flipper App Catalog requires an
  open-source licence anyway, so this is compatible — but you must (a) state it in `README.md`,
  (b) licence the *data file* under CC BY-SA 4.0 even if the *code* is MIT, and (c) say so explicitly.
- **Quasimodo** — verify the licence on the HF dataset card and the original project before shipping.
  Do not assume.
- **McRae / `metaeval/semantic-feature-production-norms`** — feature-norm data is typically released for
  research use. **Verify the terms on the dataset card.** If redistribution is restricted, either drop it
  from the shipped file or use it only to calibrate and design the question bank (which produces no
  redistributable derivative).
- If any source turns out to be non-redistributable, fall back to: use it offline for question design
  and calibration only, and generate the shipped cell values via 4.5 densification, which you own.
- Record the exact source list, versions, and licences in `README.md` and in `dataset_id` in the header.
- **Content safety:** this ships to a general audience including kids. The blocklist in 4.3 is not
  optional. Manually review the final entity list — 2,048 names is one afternoon of reading, and it is
  cheaper than a catalog rejection.

---

## 13. Publishing to the Flipper Apps Catalog (optional, do last)

Requirements, all verified against `flipperdevices/flipper-application-catalog`:

- Public **GitHub** repo, open-source licence, buildable with **uFBT** against the latest
  Release or Release Candidate firmware.
- `manifest.yml` at `applications/Games/twentyq/manifest.yml` in the catalog repo. `name`, `id`, and
  `version` **must be byte-identical** to `application.fam` or the bundler errors.
- Icon: 10x10 PNG, every pixel pure black or fully transparent.
- Screenshots: **taken with qFlipper**, unmodified, landscape, exactly **512x256** or **1024x512**
  (128x64 upscaled 4x or 8x). At least one.
- `description` and `changelog` can be `"@README.md"` / `"@./docs/changelog.md"`.
- Version must be `major.minor` and strictly increasing.
- Validate locally before opening a PR:
  ```bash
  python3 -m venv venv && source venv/bin/activate
  pip install -r tools/requirements.txt
  export UFBT_HOME="$PWD/venv/ufbt" && ufbt update
  python3 tools/bundle.py --nolint applications/Games/twentyq/manifest.yml bundle.zip
  ```
- Branch name: `<username>/twentyq_<version>`.

There is currently **no Akinator/20-questions app in the Flipper catalog** — this would be the first.

---

## 14. Build order

Do these in order. Each milestone must run before starting the next.

| # | Milestone | Done when |
|---|---|---|
| 1 | **Toolchain** | `pip install --upgrade ufbt`; `ufbt create APPID=twentyq`; `ufbt` builds; `ufbt launch` shows a hello-world on device. Commit this. |
| 2 | **Format + packer** | `tools/pack.py` writes a synthetic 8x6 `20q.dat`. `tests/` parses it with the C reader and asserts every field. No Flipper involved. |
| 3 | **Engine, off-device** | `twentyq_engine.c` compiles under plain gcc. Unit tests in 11.1 pass against the synthetic file. |
| 4 | **Question bank** | `tools/questions.yaml` — all 192, hand-written, length-validated, family-balanced. This is a writing task, not a coding task. Budget real time for it. |
| 5 | **Entity vocabulary** | `tools/entities.yaml` — 2,048 curated entries with category and prior weight. |
| 6 | **Real pipeline** | `build_dataset.py` produces a real `20q.dat` from McRae + Quasimodo + ConceptNet. |
| 7 | **Simulator** | `tools/eval.py` reports win-rate. **Iterate on 4/5/6 until >= 80%.** Do not proceed to UI work with bad data. |
| 8 | **Device data loading** | App opens `APP_ASSETS_PATH("20q.dat")`, parses the header, reads one column, logs it. Verify with `ufbt cli`. |
| 9 | **Playable loop** | Scenes: start -> question -> guess -> result. Ugly is fine. Play it. |
| 10 | **Polish the UI** | Section 8 in full: progress bar, selector, undo, animations, notifications. |
| 11 | **Learning** | `learned.dat` write/read, text input, overlay merged into belief. |
| 12 | **Settings + stats** | Persistence, reset actions. |
| 13 | **Hardening** | Every row of section 9. Pull the SD card. Corrupt the file on purpose. |
| 14 | **Scale up** | Regenerate at 4,096-8,192 entities. Re-measure heap with `free` and turn time with `furi_get_tick()`. |
| 15 | **Catalog** | Section 13. |

---

## 15. Open decisions — flag these back to the human

These were left open deliberately; make a call and say which you made.

1. **Entity count for v1.** Spec says 2,048. If the pipeline can only produce ~800 *good* entities,
   ship 800 — quality beats quantity, and the format scales later.
2. **Whether to use the LLM densification pass (4.5).** Build without it first and measure. It is a
   real cost and possibly a licensing simplification, but it also introduces model-shaped errors.
3. **Learned-entity sharing.** Should `learned.dat` be exportable/importable so people can swap word
   packs? Nice feature, not v1.
4. **Question wording tone.** Neutral and plain, or a bit of personality? Personality is charming for
   three games and grating by the tenth. Recommend neutral.
5. **Whether to keep the category hint at all.** It speeds the game up meaningfully but costs a screen
   and confuses people who do not know if a virus is an animal. Default: keep it, make it skippable.

---

## 16. Reference index

| Topic | Link |
|---|---|
| ufbt | https://github.com/flipperdevices/flipperzero-ufbt |
| App manifests | https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/AppManifests.md |
| Apps on SD card / RAM | https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/AppsOnSDCard.md |
| App examples index | https://developer.flipper.net/flipperzero/doxygen/app_examples.html |
| Shipping data files with a FAP | https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/examples/example_apps_assets |
| Saving user data | https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/examples/example_apps_data |
| Scene/ViewDispatcher skeleton | https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/examples/example_number_input |
| Streaming a big dictionary off SD | https://github.com/flipperdevices/flipperzero-firmware/blob/dev/lib/toolbox/keys_dict.c |
| Best real-world SD-data reference (Hangman) | https://github.com/bolknote/Flipper-Zero-Hangman-Game |
| Closest structural analogue (decision-tree UI) | https://github.com/fgreil/mitzi-tree-ident |
| Clean scenes + SD file reading (Text Viewer) | https://github.com/xMasterX/all-the-plugins/tree/dev/base_pack/text_viewer |
| Catalog contributing | https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md |
| Catalog manifest | https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Manifest.md |
| Tech specs | https://docs.flipper.net/zero/development/hardware/tech-specs |
| McRae norms dataset | https://huggingface.co/datasets/metaeval/semantic-feature-production-norms |
| Quasimodo dataset | https://huggingface.co/datasets/Aunsiels/Quasimodo |
| ConceptNet dataset | https://huggingface.co/datasets/conceptnet5/conceptnet5 |
| 20Q benchmark entities | https://huggingface.co/datasets/jtv199/Entity-deduction-arena-20-questions |

---

*End of specification.*
