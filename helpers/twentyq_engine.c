#include "twentyq_engine.h"

#include <stdlib.h>
#include <string.h>

#define TQ_LEARNED_PRIOR 0.5f
#define TQ_CATEGORY_MISS 0.05f /* soft, never a hard filter (SPEC 6.2) */
#define TQ_TINY          1e-6f

const char* tq_answer_label(Tq_Answer a) {
    switch(a) {
    case TqAnswerNo:
        return "No";
    case TqAnswerProbablyNot:
        return "Prob. not";
    case TqAnswerUnknown:
        return "Don't know";
    case TqAnswerProbably:
        return "Probably";
    case TqAnswerYes:
        return "Yes";
    default:
        return "?";
    }
}

const char* tq_category_label(uint8_t category) {
    switch(category) {
    case 0:
        return "Animal";
    case 1:
        return "Vegetable";
    case 2:
        return "Mineral";
    default:
        return "Other";
    }
}

static uint32_t tq_rand(Tq_Engine* eng) {
    /* xorshift32: deterministic in tests, seeded from furi_hal_random on device */
    uint32_t x = eng->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    eng->rng = x ? x : 0x1234567Bu;
    return eng->rng;
}

static uint32_t tq_asked_bytes(const Tq_Dataset* ds) {
    return (ds->question_count + 7u) / 8u;
}

bool tq_engine_is_asked(const Tq_Engine* eng, uint16_t qid) {
    if(qid >= eng->ds->question_count) return true;
    return (eng->asked[qid >> 3] >> (qid & 7u)) & 1u;
}

static void tq_mark_asked(Tq_Engine* eng, uint16_t qid) {
    eng->asked[qid >> 3] |= (uint8_t)(1u << (qid & 7u));
}

static uint32_t tq_learned_count(const Tq_Engine* eng) {
    return eng->learned ? eng->learned->count : 0;
}

Tq_Engine* tq_engine_alloc(const Tq_Dataset* ds, Tq_Learned* learned) {
    Tq_Engine* eng = malloc(sizeof(Tq_Engine));
    memset(eng, 0, sizeof(*eng));
    eng->ds = ds;
    eng->learned = learned;
    eng->n_total = ds->entity_count + tq_learned_count(eng);
    eng->belief = malloc(sizeof(float) * eng->n_total);
    eng->colbuf = malloc(ds->stride);
    eng->asked = malloc(tq_asked_bytes(ds));
    eng->question_limit = 20;
    eng->rng = 0x2545F491u;
    eng->category_hint = TQ_NO_CATEGORY;
    memset(eng->belief, 0, sizeof(float) * eng->n_total);
    memset(eng->asked, 0, tq_asked_bytes(ds));
    return eng;
}

void tq_engine_free(Tq_Engine* eng) {
    if(!eng) return;
    free(eng->belief);
    free(eng->colbuf);
    free(eng->asked);
    free(eng);
}

void tq_engine_set_question_limit(Tq_Engine* eng, uint16_t limit) {
    if(limit < 5) limit = 5;
    if(limit > TQ_MAX_QUESTIONS_PER_GAME - TQ_MAX_GUESSES * TQ_EXTRA_QUESTIONS_PER_GUESS) {
        limit = TQ_MAX_QUESTIONS_PER_GAME - TQ_MAX_GUESSES * TQ_EXTRA_QUESTIONS_PER_GUESS;
    }
    eng->question_limit = limit;
}

void tq_engine_seed(Tq_Engine* eng, uint32_t seed) {
    eng->rng = seed ? seed : 0x2545F491u;
}

/** Rebuilds the prior belief. Learned entities join at a flat weight. */
static bool tq_engine_load_prior(Tq_Engine* eng, uint8_t category_hint) {
    const Tq_Dataset* ds = eng->ds;
    uint32_t n = ds->entity_count;
    if(!tq_dataset_read_priors(ds, eng->belief)) {
        eng->io_error = true;
        return false;
    }
    for(uint32_t i = n; i < eng->n_total; i++)
        eng->belief[i] = TQ_LEARNED_PRIOR;

    if(category_hint < TQ_CATEGORY_COUNT) {
        for(uint32_t i = 0; i < eng->n_total; i++) {
            uint8_t cat = tq_engine_entity_category(eng, i);
            if(cat != category_hint) eng->belief[i] *= TQ_CATEGORY_MISS;
        }
    }
    for(uint8_t r = 0; r < eng->rejected_count; r++) {
        if(eng->rejected[r] < eng->n_total) eng->belief[eng->rejected[r]] = 0.0f;
    }

    float sum = 0.0f;
    for(uint32_t i = 0; i < eng->n_total; i++)
        sum += eng->belief[i];
    if(sum <= 0.0f) {
        for(uint32_t i = 0; i < eng->n_total; i++)
            eng->belief[i] = 1.0f / (float)eng->n_total;
        return true;
    }
    float inv = 1.0f / sum;
    for(uint32_t i = 0; i < eng->n_total; i++)
        eng->belief[i] *= inv;
    return true;
}

bool tq_engine_reset(Tq_Engine* eng, uint8_t category_hint) {
    uint32_t want_total = eng->ds->entity_count + tq_learned_count(eng);
    if(want_total != eng->n_total) { /* a word was taught since the last game */
        free(eng->belief);
        eng->n_total = want_total;
        eng->belief = malloc(sizeof(float) * eng->n_total);
    }
    memset(eng->asked, 0, tq_asked_bytes(eng->ds));
    memset(eng->family_used, 0, sizeof(eng->family_used));
    eng->asked_count = 0;
    eng->rejected_count = 0;
    eng->io_error = false;
    eng->category_hint = category_hint;
    return tq_engine_load_prior(eng, category_hint);
}

/**
 * P(yes | entity i, current column). Dataset entities read their nibble;
 * learned entities use their own answer if they have one, else the mean over
 * dataset entities of the same category -- which is exactly the category
 * prior for this question, computed from the column we already hold.
 */
static void tq_category_means(const Tq_Engine* eng, const uint8_t* col, float* means) {
    uint32_t counts[TQ_CATEGORY_COUNT] = {0};
    float sums[TQ_CATEGORY_COUNT] = {0.0f};
    const Tq_Dataset* ds = eng->ds;
    for(uint32_t i = 0; i < ds->entity_count; i++) {
        uint8_t c = ds->category[i];
        sums[c] += tq_pyes(ds, col, i);
        counts[c]++;
    }
    for(uint8_t c = 0; c < TQ_CATEGORY_COUNT; c++) {
        means[c] = counts[c] ? (sums[c] / (float)counts[c]) : 0.5f;
    }
}

static inline float tq_cell(
    const Tq_Engine* eng,
    const uint8_t* col,
    uint32_t i,
    uint16_t qid,
    const float* cat_means) {
    const Tq_Dataset* ds = eng->ds;
    if(i < ds->entity_count) return tq_pyes(ds, col, i);
    uint16_t rec = (uint16_t)(i - ds->entity_count);
    float p;
    if(eng->learned && tq_learned_pyes(eng->learned, rec, qid, ds->lut, &p)) return p;
    uint8_t cat = eng->learned ? eng->learned->records[rec]->category : (TQ_CATEGORY_COUNT - 1);
    return cat_means[cat];
}

static bool tq_apply(Tq_Engine* eng, uint16_t qid, Tq_Answer answer) {
    float w = tq_answer_weight(answer);
    if(answer == TqAnswerUnknown) { /* no information -- skip the multiply */
        tq_mark_asked(eng, qid);
        return true;
    }
    if(!tq_dataset_read_column(eng->ds, qid, eng->colbuf)) {
        eng->io_error = true;
        return false;
    }
    float cat_means[TQ_CATEGORY_COUNT] = {0.5f, 0.5f, 0.5f, 0.5f};
    if(eng->n_total > eng->ds->entity_count) tq_category_means(eng, eng->colbuf, cat_means);

    const float k = 1.0f - 2.0f * TQ_EPS;
    float sum = 0.0f;
    for(uint32_t i = 0; i < eng->n_total; i++) {
        float p = tq_cell(eng, eng->colbuf, i, qid, cat_means);
        float l = TQ_EPS + k * (w * p + (1.0f - w) * (1.0f - p));
        eng->belief[i] *= l;
        sum += eng->belief[i];
    }
    if(sum > 0.0f) {
        float inv = 1.0f / sum;
        for(uint32_t i = 0; i < eng->n_total; i++)
            eng->belief[i] *= inv;
    }
    tq_mark_asked(eng, qid);
    return true;
}

bool tq_engine_answer(Tq_Engine* eng, uint16_t qid, Tq_Answer answer) {
    if(qid >= eng->ds->question_count) return false;
    if(eng->asked_count >= TQ_MAX_QUESTIONS_PER_GAME) return false;
    if(!tq_apply(eng, qid, answer)) return false;
    eng->asked_order[eng->asked_count] = qid;
    eng->answers[eng->asked_count] = (uint8_t)answer;
    eng->asked_count++;
    uint8_t fam = tq_dataset_family(eng->ds, qid);
    if(fam < TQ_FAMILY_COUNT && eng->family_used[fam] < 255) eng->family_used[fam]++;
    return true;
}

bool tq_engine_undo(Tq_Engine* eng) {
    if(eng->asked_count == 0) return false;
    uint16_t keep = (uint16_t)(eng->asked_count - 1);
    uint16_t order[TQ_MAX_QUESTIONS_PER_GAME];
    uint8_t ans[TQ_MAX_QUESTIONS_PER_GAME];
    memcpy(order, eng->asked_order, sizeof(uint16_t) * keep);
    memcpy(ans, eng->answers, keep);

    memset(eng->asked, 0, tq_asked_bytes(eng->ds));
    memset(eng->family_used, 0, sizeof(eng->family_used));
    eng->asked_count = 0;
    if(!tq_engine_load_prior(eng, eng->category_hint)) return false;
    for(uint16_t k = 0; k < keep; k++) {
        if(!tq_engine_answer(eng, order[k], (Tq_Answer)ans[k])) return false;
    }
    return true;
}

/* --- question selection (SPEC 6.4) ------------------------------------- */

static bool tq_cand_push(uint16_t* cand, uint8_t* count, uint8_t* seen, uint16_t qid) {
    if(*count >= TQ_CANDIDATES) return false;
    if((seen[qid >> 3] >> (qid & 7u)) & 1u) return true;
    seen[qid >> 3] |= (uint8_t)(1u << (qid & 7u));
    cand[(*count)++] = qid;
    return true;
}

static uint8_t tq_build_shortlist(Tq_Engine* eng, uint16_t* cand) {
    const Tq_Dataset* ds = eng->ds;
    uint32_t q = ds->question_count;
    uint8_t seen[(TQ_MAX_QUESTIONS + 7) / 8];
    memset(seen, 0, (q + 7u) / 8u);
    uint8_t count = 0;

    /* 28 from the static ranking, best first */
    for(uint32_t k = 0; k < q && count < 28; k++) {
        uint16_t qid = ds->qrank[k];
        if(tq_engine_is_asked(eng, qid)) continue;
        tq_cand_push(cand, &count, seen, qid);
    }
    /* 8 from families this game has not touched yet -- keeps the run varied */
    uint8_t target = (uint8_t)(count + 8);
    if(target > TQ_CANDIDATES) target = TQ_CANDIDATES;
    for(uint32_t k = 0; k < q && count < target; k++) {
        uint16_t qid = ds->qrank[k];
        if(tq_engine_is_asked(eng, qid)) continue;
        uint8_t fam = tq_dataset_family(ds, qid);
        if(fam < TQ_FAMILY_COUNT && eng->family_used[fam] != 0) continue;
        tq_cand_push(cand, &count, seen, qid);
    }
    /* 4 at random so repeat games differ */
    uint32_t guard = 0;
    while(count < TQ_CANDIDATES && guard++ < 64) {
        uint16_t qid = (uint16_t)(tq_rand(eng) % q);
        if(tq_engine_is_asked(eng, qid)) continue;
        tq_cand_push(cand, &count, seen, qid);
    }
    return count;
}

bool tq_engine_select_question(Tq_Engine* eng, uint16_t* out_qid) {
    uint16_t cand[TQ_CANDIDATES];
    uint8_t n_cand = tq_build_shortlist(eng, cand);
    if(n_cand == 0) return false;

    float purity[TQ_CANDIDATES];
    float best = -1.0f;
    for(uint8_t c = 0; c < n_cand; c++) {
        if(!tq_dataset_read_column(eng->ds, cand[c], eng->colbuf)) {
            eng->io_error = true;
            return false;
        }
        float cat_means[TQ_CATEGORY_COUNT] = {0.5f, 0.5f, 0.5f, 0.5f};
        if(eng->n_total > eng->ds->entity_count) {
            tq_category_means(eng, eng->colbuf, cat_means);
        }
        float s_yes = 0.0f, a = 0.0f, b = 0.0f;
        for(uint32_t i = 0; i < eng->n_total; i++) {
            float bi = eng->belief[i];
            if(bi <= 0.0f) continue;
            float p = tq_cell(eng, eng->colbuf, i, cand[c], cat_means);
            float y = bi * p;
            float nno = bi - y;
            s_yes += y;
            a += y * y;
            b += nno * nno;
        }
        float s_no = 1.0f - s_yes;
        if(s_yes < TQ_TINY) s_yes = TQ_TINY;
        if(s_no < TQ_TINY) s_no = TQ_TINY;
        /* Gini, not Shannon: no logf in a 40 x N loop (SPEC 6.4). */
        purity[c] = a / s_yes + b / s_no;
        if(purity[c] > best) best = purity[c];
    }

    /* Tie-break: within 2% of best, prefer the least-used family, then the
     * better static rank. */
    uint16_t chosen = cand[0];
    uint8_t best_fam_use = 255;
    uint16_t best_rank = 0xFFFFu;
    float cutoff = best * 0.98f;
    for(uint8_t c = 0; c < n_cand; c++) {
        if(purity[c] < cutoff) continue;
        uint8_t fam = tq_dataset_family(eng->ds, cand[c]);
        uint8_t use = (fam < TQ_FAMILY_COUNT) ? eng->family_used[fam] : 0;
        uint16_t rank = eng->ds->qrank_pos[cand[c]];
        if(use < best_fam_use || (use == best_fam_use && rank < best_rank)) {
            best_fam_use = use;
            best_rank = rank;
            chosen = cand[c];
        }
    }
    *out_qid = chosen;
    return true;
}

/* --- guessing (SPEC 6.5) ----------------------------------------------- */

uint32_t tq_engine_top(const Tq_Engine* eng, Tq_Candidate* out, uint32_t max) {
    if(max == 0) return 0;
    uint32_t filled = 0;
    for(uint32_t i = 0; i < eng->n_total; i++) {
        float b = eng->belief[i];
        bool skip = false;
        for(uint8_t r = 0; r < eng->rejected_count; r++) {
            if(eng->rejected[r] == i) skip = true;
        }
        if(skip) continue;
        if(filled < max) {
            uint32_t pos = filled++;
            while(pos > 0 && out[pos - 1].belief < b) {
                out[pos] = out[pos - 1];
                pos--;
            }
            out[pos].index = i;
            out[pos].belief = b;
        } else if(b > out[max - 1].belief) {
            uint32_t pos = max - 1;
            while(pos > 0 && out[pos - 1].belief < b) {
                out[pos] = out[pos - 1];
                pos--;
            }
            out[pos].index = i;
            out[pos].belief = b;
        }
    }
    return filled;
}

uint16_t tq_engine_question_budget(const Tq_Engine* eng) {
    return (uint16_t)(eng->question_limit + eng->rejected_count * TQ_EXTRA_QUESTIONS_PER_GUESS);
}

bool tq_engine_out_of_questions(const Tq_Engine* eng) {
    return eng->asked_count >= tq_engine_question_budget(eng) ||
           eng->asked_count >= TQ_MAX_QUESTIONS_PER_GAME;
}

bool tq_engine_should_guess(const Tq_Engine* eng) {
    if(tq_engine_out_of_questions(eng)) return true;
    Tq_Candidate top[2];
    uint32_t got = tq_engine_top(eng, top, 2);
    if(got == 0) return true;
    if(got == 1) return true;
    return top[0].belief >= TQ_GUESS_CONFIDENCE && top[0].belief >= TQ_GUESS_RATIO * top[1].belief;
}

void tq_engine_reject(Tq_Engine* eng, uint32_t entity_index) {
    if(entity_index >= eng->n_total) return;
    if(eng->rejected_count < TQ_MAX_GUESSES) eng->rejected[eng->rejected_count++] = entity_index;
    eng->belief[entity_index] = 0.0f;
    float sum = 0.0f;
    for(uint32_t i = 0; i < eng->n_total; i++)
        sum += eng->belief[i];
    if(sum > 0.0f) {
        float inv = 1.0f / sum;
        for(uint32_t i = 0; i < eng->n_total; i++)
            eng->belief[i] *= inv;
    } else {
        for(uint32_t i = 0; i < eng->n_total; i++)
            eng->belief[i] = 1.0f / (float)eng->n_total;
    }
}

bool tq_engine_entity_name(const Tq_Engine* eng, uint32_t index, char* out, size_t out_size) {
    if(index < eng->ds->entity_count) {
        return tq_dataset_entity_name(eng->ds, index, out, out_size);
    }
    uint16_t rec = (uint16_t)(index - eng->ds->entity_count);
    if(!eng->learned || rec >= eng->learned->count) {
        if(out_size) out[0] = '\0';
        return false;
    }
    size_t len = strlen(eng->learned->records[rec]->name);
    if(len >= out_size) len = out_size - 1;
    memcpy(out, eng->learned->records[rec]->name, len);
    out[len] = '\0';
    return true;
}

uint8_t tq_engine_entity_category(const Tq_Engine* eng, uint32_t index) {
    if(index < eng->ds->entity_count) return tq_dataset_category(eng->ds, index);
    uint16_t rec = (uint16_t)(index - eng->ds->entity_count);
    if(!eng->learned || rec >= eng->learned->count) return TQ_CATEGORY_COUNT - 1;
    return eng->learned->records[rec]->category;
}

uint8_t tq_answer_to_nibble(const Tq_Dataset* ds, Tq_Answer answer) {
    float w = tq_answer_weight(answer);
    uint8_t best = 0;
    float best_d = 9.9f;
    for(uint8_t i = 0; i < TQ_LUT_SIZE; i++) {
        float d = ds->lut[i] - w;
        if(d < 0.0f) d = -d;
        if(d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}
