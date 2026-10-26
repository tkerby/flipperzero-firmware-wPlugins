/* SPEC 11.1 -- unit tests for the engine, run off-device with plain cc.
 * Golden fixture: tests/golden_8x6.dat, written by tools/pack.py. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../helpers/twentyq_engine.h"
#include "tq_file_reader.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        checks++;                                         \
        if(!(cond)) {                                     \
            failures++;                                   \
            printf("  FAIL %s:%d  ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while(0)

#define CHECK_NEAR(a, b, tol, ...)                                                          \
    do {                                                                                    \
        checks++;                                                                           \
        float _a = (a), _b = (b);                                                           \
        if(fabsf(_a - _b) > (tol)) {                                                        \
            failures++;                                                                     \
            printf("  FAIL %s:%d  %f != %f  ", __FILE__, __LINE__, (double)_a, (double)_b); \
            printf(__VA_ARGS__);                                                            \
            printf("\n");                                                                   \
        }                                                                                   \
    } while(0)

static const float LUT[16] = {
    0.02f,
    0.05f,
    0.10f,
    0.17f,
    0.25f,
    0.33f,
    0.42f,
    0.48f,
    0.52f,
    0.58f,
    0.67f,
    0.75f,
    0.83f,
    0.90f,
    0.95f,
    0.98f};

/* The fixture's generating rule, mirrored from tools/pack.py:synthetic(). */
static float truth(uint32_t i, uint32_t j) {
    return LUT[(i + 2 * j) % 16];
}

static void test_header(Tq_Dataset* ds) {
    printf("header + guard rails\n");
    CHECK(ds->entity_count == 8, "N=%u", ds->entity_count);
    CHECK(ds->question_count == 6, "Q=%u", ds->question_count);
    CHECK(ds->stride == 4, "stride=%u", ds->stride);
    CHECK(strcmp(ds->dataset_id, "golden-8x6") == 0, "id=%s", ds->dataset_id);
    CHECK(ds->flags & TQ_FLAG_HAS_PRIORS, "priors flag");
    CHECK(ds->flags & TQ_FLAG_HAS_CATEGORIES, "categories flag");
    CHECK(ds->flags & TQ_FLAG_HAS_FAMILIES, "families flag");
    for(int i = 0; i < 16; i++)
        CHECK_NEAR(ds->lut[i], LUT[i], 1e-6f, "lut[%d]", i);
    /* qrank was written as [3,1,5,0,4,2] */
    CHECK(ds->qrank[0] == 3 && ds->qrank[5] == 2, "qrank");
    CHECK(ds->qrank_pos[3] == 0 && ds->qrank_pos[2] == 5, "qrank_pos inverse");
}

static void test_nibbles(Tq_Dataset* ds) {
    printf("nibble unpacking, both parities\n");
    uint8_t col[8];
    for(uint32_t j = 0; j < ds->question_count; j++) {
        CHECK(tq_dataset_read_column(ds, j, col), "read column %u", j);
        for(uint32_t i = 0; i < ds->entity_count; i++) {
            CHECK_NEAR(tq_pyes(ds, col, i), truth(i, j), 1e-6f, "cell (%u,%u)", i, j);
        }
    }
    CHECK(!tq_dataset_read_column(ds, ds->question_count, col), "out-of-range column refused");
}

static void test_text(Tq_Dataset* ds) {
    printf("text blocks + binary search\n");
    static const char* names[8] = {"ant", "boat", "cake", "dog", "eel", "fig", "gate", "hat"};
    char buf[64];
    for(uint32_t i = 0; i < 8; i++) {
        CHECK(tq_dataset_entity_name(ds, i, buf, sizeof(buf)), "name %u", i);
        CHECK(strcmp(buf, names[i]) == 0, "name %u = %s", i, buf);
    }
    CHECK(tq_dataset_question_text(ds, 4, buf, sizeof(buf)), "qtext");
    CHECK(strcmp(buf, "Question number 4?") == 0, "qtext = %s", buf);

    uint32_t idx = 999;
    CHECK(tq_dataset_find_entity(ds, "dog", &idx) && idx == 3, "find dog -> %u", idx);
    CHECK(tq_dataset_find_entity(ds, "HAT", &idx) && idx == 7, "find HAT -> %u", idx);
    CHECK(!tq_dataset_find_entity(ds, "narwhal", &idx), "absent name not found");

    /* short buffer must truncate, not overflow */
    char tiny[4];
    CHECK(tq_dataset_entity_name(ds, 1, tiny, sizeof(tiny)), "truncating read");
    CHECK(strcmp(tiny, "boa") == 0, "truncated to %s", tiny);
}

static void test_belief_update(Tq_Dataset* ds) {
    printf("belief update against hand-computed values\n");
    Tq_Engine* eng = tq_engine_alloc(ds, NULL);
    CHECK(tq_engine_reset(eng, TQ_NO_CATEGORY), "reset");

    /* priors were 1..8, so belief[i] = (i+1)/36 */
    float before[8];
    for(uint32_t i = 0; i < 8; i++) {
        before[i] = eng->belief[i];
        CHECK_NEAR(before[i], (float)(i + 1) / 36.0f, 1e-5f, "prior %u", i);
    }

    /* Answer question 2 with "Yes" (w = 1): L = eps + (1-2eps)*p */
    CHECK(tq_engine_answer(eng, 2, TqAnswerYes), "answer");
    float expect[8], sum = 0.0f;
    for(uint32_t i = 0; i < 8; i++) {
        float p = truth(i, 2);
        expect[i] = before[i] * (TQ_EPS + (1.0f - 2.0f * TQ_EPS) * p);
        sum += expect[i];
    }
    for(uint32_t i = 0; i < 8; i++) {
        CHECK_NEAR(eng->belief[i], expect[i] / sum, 1e-5f, "posterior %u", i);
    }
    CHECK(eng->asked_count == 1 && tq_engine_is_asked(eng, 2), "asked bookkeeping");

    /* "No" is the mirror: L = eps + (1-2eps)*(1-p) */
    float snapshot[8];
    memcpy(snapshot, eng->belief, sizeof(snapshot));
    CHECK(tq_engine_answer(eng, 3, TqAnswerNo), "answer no");
    sum = 0.0f;
    for(uint32_t i = 0; i < 8; i++) {
        float p = truth(i, 3);
        expect[i] = snapshot[i] * (TQ_EPS + (1.0f - 2.0f * TQ_EPS) * (1.0f - p));
        sum += expect[i];
    }
    for(uint32_t i = 0; i < 8; i++)
        CHECK_NEAR(eng->belief[i], expect[i] / sum, 1e-5f, "no %u", i);

    /* "Don't know" must not move the belief at all (SPEC 3.3) */
    memcpy(snapshot, eng->belief, sizeof(snapshot));
    CHECK(tq_engine_answer(eng, 1, TqAnswerUnknown), "answer unknown");
    for(uint32_t i = 0; i < 8; i++)
        CHECK_NEAR(eng->belief[i], snapshot[i], 1e-7f, "dk %u", i);
    CHECK(tq_engine_is_asked(eng, 1), "unknown still marks asked");

    tq_engine_free(eng);
}

static void test_normalisation_stability(Tq_Dataset* ds) {
    printf("normalisation stability over 30 turns\n");
    Tq_Engine* eng = tq_engine_alloc(ds, NULL);
    tq_engine_reset(eng, TQ_NO_CATEGORY);
    /* Deliberately hammer the same two columns with the worst-case answer. */
    for(int turn = 0; turn < 30; turn++) {
        uint16_t qid = (uint16_t)(turn % 6);
        eng->asked[qid >> 3] &= (uint8_t) ~(1u << (qid & 7u)); /* allow re-asking */
        eng->asked_count = 0;
        tq_engine_answer(eng, qid, (turn & 1) ? TqAnswerNo : TqAnswerYes);
        float sum = 0.0f;
        for(uint32_t i = 0; i < 8; i++) {
            CHECK(isfinite(eng->belief[i]), "belief finite at turn %d", turn);
            CHECK(eng->belief[i] >= 0.0f, "belief non-negative at turn %d", turn);
            sum += eng->belief[i];
        }
        CHECK_NEAR(sum, 1.0f, 1e-4f, "sums to 1 at turn %d", turn);
    }
    tq_engine_free(eng);
}

static void test_gini_against_reference(Tq_Dataset* ds) {
    printf("Gini purity against a brute-force reference\n");
    Tq_Engine* eng = tq_engine_alloc(ds, NULL);
    tq_engine_reset(eng, TQ_NO_CATEGORY);
    tq_engine_answer(eng, 0, TqAnswerProbably);

    /* Reference: score every question the slow, obvious way and confirm the
     * engine picks one of the argmax questions (allowing its 2% tie-break). */
    float best = -1.0f, chosen_score = -1.0f;
    uint16_t picked = 0xFFFF;
    CHECK(tq_engine_select_question(eng, &picked), "select");
    for(uint32_t j = 0; j < ds->question_count; j++) {
        if(tq_engine_is_asked(eng, j)) continue;
        float s_yes = 0.0f, a = 0.0f, b = 0.0f;
        for(uint32_t i = 0; i < 8; i++) {
            float y = eng->belief[i] * truth(i, j);
            float n = eng->belief[i] - y;
            s_yes += y;
            a += y * y;
            b += n * n;
        }
        float s_no = 1.0f - s_yes;
        float purity = a / (s_yes < 1e-6f ? 1e-6f : s_yes) + b / (s_no < 1e-6f ? 1e-6f : s_no);
        if(purity > best) best = purity;
        if(j == picked) chosen_score = purity;
    }
    CHECK(picked != 0, "never re-asks question 0");
    CHECK(
        chosen_score >= best * 0.98f,
        "picked %u scores %f vs best %f",
        picked,
        (double)chosen_score,
        (double)best);
    tq_engine_free(eng);
}

static void test_undo_is_replay(Tq_Dataset* ds) {
    printf("undo by replay reproduces the earlier belief exactly\n");
    Tq_Engine* eng = tq_engine_alloc(ds, NULL);
    tq_engine_reset(eng, TQ_NO_CATEGORY);
    tq_engine_answer(eng, 0, TqAnswerYes);
    tq_engine_answer(eng, 4, TqAnswerProbablyNot);
    float snapshot[8];
    memcpy(snapshot, eng->belief, sizeof(snapshot));

    tq_engine_answer(eng, 2, TqAnswerNo);
    CHECK(eng->asked_count == 3, "three asked");
    CHECK(tq_engine_undo(eng), "undo");
    CHECK(eng->asked_count == 2, "back to two");
    CHECK(!tq_engine_is_asked(eng, 2), "undone question is askable again");
    for(uint32_t i = 0; i < 8; i++)
        CHECK_NEAR(eng->belief[i], snapshot[i], 1e-6f, "undo %u", i);
    CHECK(tq_engine_undo(eng) && tq_engine_undo(eng), "undo to empty");
    CHECK(!tq_engine_undo(eng), "undo past the start refused");
    tq_engine_free(eng);
}

static void test_category_hint(Tq_Dataset* ds) {
    printf("category hint is soft, never a hard filter\n");
    Tq_Engine* eng = tq_engine_alloc(ds, NULL);
    tq_engine_reset(eng, 0); /* animal: entities 0, 3, 4 */
    for(uint32_t i = 0; i < 8; i++)
        CHECK(eng->belief[i] > 0.0f, "entity %u survives hint", i);
    float animal = eng->belief[0] + eng->belief[3] + eng->belief[4];
    CHECK(animal > 0.5f, "hinted category dominates (%f)", (double)animal);
    tq_engine_free(eng);
}

static void test_guessing(Tq_Dataset* ds) {
    printf("guess trigger, rejection and question budget\n");
    Tq_Engine* eng = tq_engine_alloc(ds, NULL);
    tq_engine_set_question_limit(eng, 20);
    tq_engine_reset(eng, TQ_NO_CATEGORY);

    Tq_Candidate top[3];
    CHECK(tq_engine_top(eng, top, 3) == 3, "top-3");
    CHECK(top[0].belief >= top[1].belief && top[1].belief >= top[2].belief, "sorted");
    CHECK(top[0].index == 7, "highest prior wins on turn 0");

    uint32_t first = top[0].index;
    tq_engine_reject(eng, first);
    CHECK(eng->belief[first] == 0.0f, "rejected entity zeroed");
    CHECK(tq_engine_question_budget(eng) == 25, "budget grew by 5");
    tq_engine_top(eng, top, 3);
    CHECK(top[0].index != first, "rejected entity is not offered again");
    float sum = 0.0f;
    for(uint32_t i = 0; i < 8; i++)
        sum += eng->belief[i];
    CHECK_NEAR(sum, 1.0f, 1e-5f, "renormalised after rejection");

    /* Drive one entity to near-certainty and confirm the trigger fires. */
    tq_engine_reset(eng, TQ_NO_CATEGORY);
    for(uint32_t i = 0; i < 8; i++)
        eng->belief[i] = (i == 5) ? 0.97f : 0.03f / 7.0f;
    CHECK(tq_engine_should_guess(eng), "confident -> guess");
    for(uint32_t i = 0; i < 8; i++)
        eng->belief[i] = 0.125f;
    CHECK(!tq_engine_should_guess(eng), "flat belief -> keep asking");
    eng->asked_count = 20;
    CHECK(tq_engine_should_guess(eng), "out of questions -> guess anyway");
    tq_engine_free(eng);
}

static void test_learned_overlay(Tq_Dataset* ds) {
    printf("learned overlay: add, serialise, reload, participate in the game\n");
    Tq_Learned ln;
    tq_learned_init(&ln, ds->crc32);
    uint16_t qids[3] = {4, 0, 2};
    uint8_t nibs[3] = {15, 0, 8};
    CHECK(tq_learned_add(&ln, "narwhal", 0, qids, nibs, 3), "add");
    CHECK(ln.count == 1, "one record");
    CHECK(ln.records[0]->qid[0] == 0 && ln.records[0]->qid[2] == 4, "answers sorted");

    float p = -1.0f;
    CHECK(
        tq_learned_pyes(&ln, 0, 4, ds->lut, &p) && fabsf(p - 0.98f) < 1e-6f,
        "lookup p=%f",
        (double)p);
    CHECK(!tq_learned_pyes(&ln, 0, 5, ds->lut, &p), "unanswered question reports absent");

    size_t len = 0;
    uint8_t* blob = tq_learned_serialize(&ln, &len);
    Tq_Learned ln2;
    tq_learned_init(&ln2, ds->crc32);
    CHECK(tq_learned_load(&ln2, blob, len, ds->crc32), "reload");
    CHECK(ln2.count == 1 && strcmp(ln2.records[0]->name, "narwhal") == 0, "round trip");
    CHECK(!tq_learned_load(&ln2, blob, len, ds->crc32 ^ 1u), "wrong base crc rejected");
    CHECK(ln2.count == 0, "rejected overlay leaves nothing behind");
    /* truncated file must not read past the end */
    tq_learned_load(&ln2, blob, len, ds->crc32);
    CHECK(!tq_learned_load(&ln2, blob, len - 2, ds->crc32), "truncated file rejected");
    free(blob);

    tq_learned_load(&ln2, (uint8_t*)"", 0, ds->crc32);
    tq_learned_clear(&ln2);
    tq_learned_add(&ln2, "narwhal", 0, qids, nibs, 3);

    Tq_Engine* eng = tq_engine_alloc(ds, &ln2);
    CHECK(eng->n_total == 9, "learned entity joins the belief vector");
    CHECK(tq_engine_reset(eng, TQ_NO_CATEGORY), "reset with overlay");
    CHECK(tq_engine_answer(eng, 4, TqAnswerYes), "answer with overlay");
    char nm[32];
    CHECK(tq_engine_entity_name(eng, 8, nm, sizeof(nm)) && strcmp(nm, "narwhal") == 0, "name");
    CHECK(tq_engine_entity_category(eng, 8) == 0, "category");
    /* q4 -> p=0.98 for narwhal, so a "yes" must raise its share of the mass */
    float sum = 0.0f;
    for(uint32_t i = 0; i < 9; i++)
        sum += eng->belief[i];
    CHECK_NEAR(sum, 1.0f, 1e-5f, "still normalised with overlay");
    CHECK(
        eng->belief[8] > 1.0f / 9.0f, "learned entity gained belief (%f)", (double)eng->belief[8]);
    uint16_t qsel;
    CHECK(tq_engine_select_question(eng, &qsel), "selection works with overlay");
    tq_engine_free(eng);
    tq_learned_clear(&ln2);
    tq_learned_clear(&ln);
}

static void test_bad_files(void) {
    printf("corrupt files produce errors, not crashes\n");
    Tq_Dataset ds;
    Tq_Reader r;
    uint8_t buf[TQ_HEADER_SIZE + 16];

    CHECK(tq_test_reader_open(&r, "tests/golden_8x6.dat"), "open golden");
    CHECK(tq_dataset_open(&ds, r, true) == TqOk, "crc verifies");
    tq_dataset_close(&ds);
    tq_test_reader_close(&r);

    /* magic */
    FILE* f = fopen("tests/golden_8x6.dat", "rb");
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    CHECK(n == sizeof(buf), "read header");

    struct MemCtx {
        uint8_t* data;
        uint32_t len;
    } mem = {buf, (uint32_t)n};
    r.ctx = &mem;
    r.size = (uint32_t)n;
    r.read_at = NULL;
    (void)r;

    uint8_t saved[TQ_HEADER_SIZE + 16];
    memcpy(saved, buf, sizeof(saved));

    /* Rewrite the on-disk file with each corruption and reopen. */
    const struct {
        const char* what;
        uint32_t off;
        uint8_t val;
        Tq_Error expect;
    } cases[] = {
        {"bad magic", 0, 'X', TqErrMagic},
        {"bad version", 4, 9, TqErrVersion},
        {"absurd entity count", 0x0B, 0xFF, TqErrRange},
        {"absurd question count", 0x0F, 0xFF, TqErrRange},
        {"flipped crc byte", 0x70, 0xAB, TqErrCrc},
    };
    for(size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        FILE* src = fopen("tests/golden_8x6.dat", "rb");
        FILE* dst = fopen("build/corrupt.dat", "wb");
        int ch;
        uint32_t off = 0;
        while((ch = fgetc(src)) != EOF) {
            fputc(off == cases[c].off ? cases[c].val : ch, dst);
            off++;
        }
        fclose(src);
        fclose(dst);
        Tq_Reader cr;
        CHECK(tq_test_reader_open(&cr, "build/corrupt.dat"), "open corrupt");
        Tq_Error err = tq_dataset_open(&ds, cr, true);
        CHECK(err == cases[c].expect, "%s -> %s", cases[c].what, tq_error_str(err));
        tq_dataset_close(&ds);
        tq_test_reader_close(&cr);
    }
}

int main(void) {
    Tq_Reader reader;
    if(!tq_test_reader_open(&reader, "tests/golden_8x6.dat")) {
        printf("cannot open tests/golden_8x6.dat -- run: python3 tools/pack.py\n");
        return 2;
    }
    Tq_Dataset ds;
    Tq_Error err = tq_dataset_open(&ds, reader, true);
    if(err != TqOk) {
        printf("golden file rejected: %s\n", tq_error_str(err));
        return 2;
    }

    test_header(&ds);
    test_nibbles(&ds);
    test_text(&ds);
    test_belief_update(&ds);
    test_normalisation_stability(&ds);
    test_gini_against_reference(&ds);
    test_undo_is_replay(&ds);
    test_category_hint(&ds);
    test_guessing(&ds);
    test_learned_overlay(&ds);
    tq_dataset_close(&ds);
    tq_test_reader_close(&reader);
    test_bad_files();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
