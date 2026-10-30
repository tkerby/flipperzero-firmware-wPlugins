# 20 Questions for Flipper Zero

Think of anything real. Your Flipper asks up to 20 yes/no questions and tries to name it.
No network, no phone, no cloud — everything runs on the device.

```
  Q 7/20                    [||||||......]

        Can you hold it in
            one hand?

   No   no?   ?   yes?   Yes
                        ^^^^^
```

## How it actually works

There is no neural network on the Flipper. What runs on-device is **Bayesian belief
updating over a precomputed entity x question probability table** streamed from the SD
card — the same trick Akinator and 20Q.net use, and the only one that fits in a
64 MHz microcontroller with a few tens of KB of usable heap.

- `files/20q.dat` holds **835 things x 192 questions** as 4-bit quantised probabilities
  (96 KB). It is **never loaded into RAM**; the engine reads one 418-byte column per
  question it considers.
- After each answer the engine multiplies every candidate's belief by the likelihood of
  that answer and renormalises (`helpers/twentyq_engine.c`).
- To choose the next question it scores 80 candidates by **Gini purity**, not Shannon
  entropy — entropy would need a `logf` per cell, and 80 x 835 transcendental calls per
  turn is seconds of compute on this chip. Gini needs only multiply-add.
- When one candidate is far enough ahead, it guesses. Three guesses, then it asks you to
  teach it the word, and stores it in `learned.dat`.

Measured against the shipping dataset with `tools/eval.py` (800 simulated games):

| Player | Win rate | First-guess | Mean questions to a win |
|---|---|---|---|
| Answers honestly | **96.4%** | 78.2% | 18.0 |
| 10% of answers wrong | **88.4%** | 57.8% | 20.9 |

## Build and install

```bash
python3 -m venv venv && ./venv/bin/pip install ufbt pyyaml
export UFBT_HOME="$PWD/.ufbt"        # dot-directory on purpose, see Notes
./venv/bin/ufbt update
./venv/bin/ufbt                      # builds dist/twentyq.fap
./venv/bin/ufbt launch               # build, upload and run on a connected Flipper
```

Built and verified against firmware **1.4.3 / API 87.1**, target 7.

## Repository layout

```
application.fam            app manifest
twentyq_app.c              entry point, ViewDispatcher + SceneManager wiring
scenes/                    one translation unit per screen
views/                     the custom five-answer question view
helpers/
  twentyq_engine.c         belief update, question choice, guessing  (pure C, no Furi)
  twentyq_dataset.c        20q.dat parsing and column streaming      (pure C, no Furi)
  twentyq_learned.c        learned-word overlay                      (pure C, no Furi)
  twentyq_storage.c        the only file that talks to Furi Storage
files/20q.dat              shipped dataset, unpacked to /ext/apps_assets/twentyq/
tools/                     the offline data pipeline (see below)
tests/                     desktop harness: unit tests + game simulator
```

The engine includes no Furi headers, so `tests/` compiles the **exact code that ships**
with plain `cc` and runs it on the desktop.

```bash
make -C tests test     # 718 assertions
make -C tests sim      # play 400 simulated games
```

## The data pipeline

```bash
./venv/bin/python tools/build_dataset.py          # writes files/20q.dat
SOURCE_DATE_EPOCH=1750000000 ./venv/bin/python tools/build_dataset.py   # byte-reproducible
./venv/bin/python tools/eval.py                   # sweeps TQ_EPS x TQ_CANDIDATES
./venv/bin/python tools/dump_dataset.py files/20q.dat dog banana   # inspect cells
./venv/bin/python tools/make_icon.py              # regenerate the 10x10 icon
```

| File | What it is |
|---|---|
| `tools/questions.yaml` | the 192 questions, hand-written, with per-category priors and tag rules |
| `tools/entities.yaml` | 835 curated things: name, category, how likely a player is to pick it, tags |
| `tools/ontology.yaml` | 265 tags and what they imply (`mammal` implies `animal` implies `alive`) |
| `tools/pack.py` | writes the binary format |
| `tools/sources/` | optional Hugging Face evidence loaders (off by default) |

A cell's probability comes from the entity's expanded tag set matched against the
question's rules (`y`/`n`/`l`/`u`), combined with the question's category prior through a
confidence-weighted Beta posterior. 56% of the 160,320 cells are set by an explicit rule;
the rest fall back to the category prior, which is exactly what stops sparse data from
being catastrophic.

**Watch the implications.** Tagging a banana `tree` to mean "grows on a tree" made it
inherit `plant -> alive -> wooden -> big`, and the game started asking whether the banana
needed water to survive. Ontology edges must be things that are *always* true; merely
likely facts belong in a question's `l:` list. `tools/dump_dataset.py` exists to catch this.

## Data and licensing

- **Code:** MIT (`LICENSE`).
- **`files/20q.dat` as shipped is written from scratch** by this repo's hand-authored
  ontology, question bank and entity list. It contains **no ConceptNet, Quasimodo or
  McRae data**, so it carries no share-alike obligation and is MIT like the rest.
- The loaders in `tools/sources/` can fold those datasets in as *additional* evidence
  (`--hf-mcrae`, `--hf-quasimodo`, `--hf-conceptnet`). **If you build with
  `--hf-conceptnet` you must relicense the resulting `.dat` under CC BY-SA 4.0 and
  attribute ConceptNet**, and you should verify the terms on the Quasimodo and McRae
  dataset cards before redistributing anything derived from them. That is why the default
  build does not use them.
- `tools/blocklist.txt` filters source assertions; the shipped entity list was written by
  hand and read through for a general audience, children included.

## Controls

| Key | Action |
|---|---|
| Left / Right | move through the five answers |
| OK | confirm |
| Up / Down | shortcut for Yes / No |
| Back | undo your last answer |
| Back (hold) | give up |

Undo is implemented by **replay**, not snapshots: the belief vector is rebuilt from the
prior and the remaining answers. Snapshotting would cost 3.3 KB per level of undo.

## Notes and known limits

- **`UFBT_HOME` must be a dot-directory or live outside the app tree.** scons ignores
  dot-directories, so the conventional `.ufbt` (or `~/.ufbt`) is fine — but a plain
  directory inside the app tree, such as `venv/ufbt`, gets globbed into your own build and
  scons fails with a confusing `Tried to lookup Dir 'helpers' as a File`. `application.fam`
  also pins `sources=[...]` so `tests/` is never compiled for the device.
- **Measured on a Flipper Zero, firmware 1.4.3** (3 games, ~60 turns):
  - Turn time 180–331 ms (spec budget: 400 ms).
  - Worker thread used at most ~1.1 KB of stack, so it now gets 2 KB (~950 bytes spare;
    spec floor: 256). Stack overflow is undetected on this platform — re-measure after
    any engine change. The worker logs `stack free` and `heap free` after every turn (`log debug`).
  - Heap: 124 KB free with the app closed, 102 KB at the start screen, 86 KB mid-game,
    so the app costs ~38 KB while playing against the spec's ~40 KB budget. Scaling up
    entities will need memory work.
  - Pulling the SD card mid-game shows the error screen, no crash.
  - Main thread used at most ~1.3 KB across a full lost game including the learn flow, so
    `stack_size` is now 2 KB (~750 bytes spare). Check it with `top` (`Stack Min`).
- Turn cost is 80 columns x 418 bytes = 33 KB of sequential SD reads plus ~400k flops.
  All of it runs on a worker thread behind the loading spinner, so the UI never blocks.
- Learned words are capped at 128 records and are dropped (renamed to `learned.dat.stale`)
  if `20q.dat` is replaced, because question ids would no longer mean the same thing.

## Decisions taken where the spec left them open

1. **Entity count:** shipped 835 hand-curated things rather than a padded 2,048. The
   format stores counts in the header, so growing later is a data change only.
2. **LLM densification:** not used. Density comes from the tag ontology instead, which is
   deterministic, auditable, licence-clean and needs no model at build time.
3. **Learned-word sharing:** not in v1.
4. **Question tone:** neutral and plain.
5. **Category hint:** kept, skippable, and it only tilts the prior — it never filters.
6. **Guess threshold:** the spec's 0.85 / 6:1 ran nearly every game to the 20-question
   limit. Measured 0.50 / 4:1 wins as often and guesses about four questions sooner.
7. **Question families in the file:** the spec's question-variety rule needs to know each
   question's family, which the spec's layout had nowhere to store. Added a `u8[Q]` block
   located by a `u32` at header offset `0x98` (the first reserved word), flagged by
   `flags` bit 2.
