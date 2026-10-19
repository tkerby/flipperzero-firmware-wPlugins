// Battle core: mechanics with the documented fixes. See battle_core.h and
// contract section 7. Pure C (no furi.h).
#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-tree-loop-optimize,no-thread-jumps,no-if-conversion,no-move-loop-invariants,no-ira-hoist-pressure") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "battle_core.h"
#include <stdlib.h>
#include <string.h>

#include "battle_core_priv.h"

// ============================================================================================
// RNG
// ============================================================================================
uint32_t bc_rng_next(uint32_t* s) {
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

void bc_rng_seed(uint32_t* s, uint32_t seed) {
    // murmur3 finalizer: small / sequential seeds still give well mixed first draws
    seed ^= seed >> 16;
    seed *= 0x85EBCA6Bu;
    seed ^= seed >> 13;
    seed *= 0xC2B2AE35u;
    seed ^= seed >> 16;
    *s = seed ? seed : 0x9E3779B9u;
}

uint8_t bc_rand8(uint32_t* s) {
    return (uint8_t)(bc_rng_next(s) >> 24);
}

uint32_t bc_rand_range(uint32_t* s, uint32_t lo, uint32_t hi) {
    if(hi <= lo) return lo;
    return lo + (bc_rng_next(s) >> 8) % (hi - lo + 1);
}

// ============================================================================================
// Data loading
// ============================================================================================
const char* bc_err_str(BcErr e) {
    switch(e) {
    case BC_OK: return "ok";
    case BC_E_OPEN: return "open";
    case BC_E_MAGIC: return "magic";
    case BC_E_VERSION: return "version";
    case BC_E_SIZE: return "size";
    case BC_E_MEM: return "mem";
    case BC_E_DATA: return "data";
    }
    return "?";
}

// ---- records: parsed (and validated) from their file layout; used by the load-time scan AND by the on-demand fill
static BcErr rec_species(const BcData* d, const uint8_t* r, SpeciesDef* s) {
    s->type1 = r[0];
    s->type2 = r[1];
    memcpy(s->base, r + 2, 5);
    s->growth = r[7];
    s->base_exp = r[8];
    s->catch_rate = r[9];
    s->evo_level = r[10];
    s->evo_to = r[11];
    s->n_learn = r[14];
    memcpy(s->learn_lv, r + 16, BC_MAX_LEARN);
    memcpy(s->learn_mv, r + 28, BC_MAX_LEARN);
    if(s->n_learn > BC_MAX_LEARN) return BC_E_DATA; // (the array bound: the one check the device keeps)
#if BC_SCAN
    if(s->growth > GR_SLOW || s->evo_to >= d->n_species || s->type1 >= d->n_types || s->type2 >= d->n_types) return BC_E_DATA;
    for(uint8_t k = 0; k < s->n_learn; k++)
        if(!s->learn_mv[k] || s->learn_mv[k] >= d->n_moves || !s->learn_lv[k]) return BC_E_DATA;
#else
    (void)d;
#endif
    return BC_OK;
}

static BcErr rec_move(const BcData* d, const uint8_t* r, MoveDef* m) {
    m->type = r[0];
    m->power = r[1];
    m->acc = r[2];
    m->pp = r[3];
    m->effect = r[4];
    m->arg0 = r[5];
    m->arg1 = (int8_t)r[6];
    m->chance = r[7];
    m->priority = (int8_t)r[8];
    m->flags = r[9];
    if((m->effect == EF_STAT_TGT || m->effect == EF_STAT_SELF) && m->arg0 > STI_EVA) return BC_E_DATA; // (indexes the stat stages: the one check the device keeps)
#if BC_SCAN
    if(m->type >= d->n_types || m->effect > EF_COIN_HIT) return BC_E_DATA;
    if(((m->effect == EF_RECOIL || m->effect == EF_DRAIN) && !m->arg0) || (m->effect == EF_CHARGE && (m->arg1 < 0 || m->arg1 >= MSG_COUNT))) return BC_E_DATA; // contract 64: a divisor, a charge line
    if(m->effect == EF_STATUS_TGT && (m->arg0 < SK_SLEEP || m->arg0 > SK_CONFUSION)) return BC_E_DATA;
    if(m->effect == EF_SCREEN && m->arg0 > SCR_LIGHT_SCREEN) return BC_E_DATA;
#else
    (void)d;
#endif
    return BC_OK;
}

static BcErr rec_trainer(const BcData* d, const uint8_t* r, TrainerDef* t) {
    t->ai_mod = r[0];
    t->flags = r[1];
    t->n_party = r[2];
    t->n_counter = r[3];
    t->money = rd16(r + 6);
    memcpy(t->party_sp, r + 8, BC_MAX_PARTY);
    memcpy(t->party_lv, r + 14, BC_MAX_PARTY);
    memcpy(t->ctr_from, r + 20, 3);
    memcpy(t->ctr_to, r + 23, 3);
    if(t->n_party > BC_MAX_PARTY || t->n_counter > 3) return BC_E_DATA; // (the array bounds: checks the device keeps, with the species ids below, which index the name tables)
    for(uint8_t k = 0; k < t->n_party; k++)
        if(!t->party_sp[k] || t->party_sp[k] >= d->n_species) return BC_E_DATA;
    for(uint8_t k = 0; k < t->n_counter; k++)
        if(t->ctr_to[k] >= d->n_species) return BC_E_DATA;
#if BC_SCAN
    if(!t->n_party) return BC_E_DATA;
    for(uint8_t k = 0; k < t->n_party; k++)
        if(!t->party_lv[k] || t->party_lv[k] > 100) return BC_E_DATA;
    for(uint8_t k = 0; k < t->n_counter; k++)
        if(t->ctr_from[k] >= d->n_species) return BC_E_DATA;
#endif
    return BC_OK;
}

// ---- on-demand records: a few LRU caches in ONE heap block. An empty entry has id 0 (id 0 itself is never cached).

// The all-zero record every unknown id answers (the same as the unused id 0 of the tables).
static const union {
    SpeciesDef s;
    MoveDef m;
    TrainerDef t;
} ZERO;

#ifndef NUCLEO_DEVICE
void (*bc_record_hook)(int kind, uint8_t id, void* rec);
#endif

enum { K_SP, K_MV, K_TR };

// The record `id` of table `kind`: a cache hit, else read from the card into the least recently used entry (an empty one first).
static const void* fetch(const BcData* d, uint8_t kind, uint8_t id) {
    BcCache* c = d->cache;
    uint8_t *ids, n;
    uint16_t* ages;
    uint8_t* recs;
    size_t sz;
    switch(kind) {
    case K_SP: ids = c->sp_id, ages = c->sp_age, n = BC_CACHE_SP, recs = (uint8_t*)c->sp, sz = sizeof(SpeciesDef); break;
    case K_MV: ids = c->mv_id, ages = c->mv_age, n = BC_CACHE_MV, recs = (uint8_t*)c->mv, sz = sizeof(MoveDef); break;
    default: ids = c->tr_id, ages = c->tr_age, n = BC_CACHE_TR, recs = (uint8_t*)c->tr, sz = sizeof(TrainerDef); break;
    }
    uint16_t now = ++c->clock;
    uint8_t v = 0;
    uint16_t worst = 0;
    for(uint8_t k = 0; k < n; k++) {
        if(ids[k] == id) {
            ages[k] = now;
            return recs + k * sz;
        }
        uint16_t a = ids[k] ? (uint16_t)(now - ages[k]) : 0xFFFFu;
        if(a >= worst) worst = a, v = k;
    }
    void* rec = recs + v * sz;
    uint8_t r[SPECIES_REC]; // the largest file record
    const char* file = "trainers.bin";
    uint32_t off = 12u + (uint32_t)id * TRAINER_REC;
    uint8_t len = TRAINER_REC;
    if(kind == K_SP) file = "reg.bin", off = 12u + (uint32_t)id * SPECIES_REC, len = SPECIES_REC;
    if(kind == K_MV) file = "moves.bin", off = d->off_moves + (uint32_t)id * MOVE_REC, len = MOVE_REC;
    ids[v] = 0;
    if(!d->io.read(d->io.ctx, file, off, r, len) && !d->io.read(d->io.ctx, file, off, r, len)) return NULL; // (one retry: a card that hiccups once must not turn a species into the zero record)
    BcErr e = kind == K_SP ? rec_species(d, r, rec) : kind == K_MV ? rec_move(d, r, rec) : rec_trainer(d, r, rec);
    if(e) return NULL;
#ifndef NUCLEO_DEVICE
    if(bc_record_hook) bc_record_hook(kind, id, rec);
#endif
    ids[v] = id;
    ages[v] = now;
    return rec;
}

const SpeciesDef* bc_species(const BcData* d, uint8_t id) {
#ifndef NUCLEO_DEVICE
    if(d->species && id && id < d->n_species) return &d->species[id];
#endif
    const void* r = id && id < d->n_species ? fetch(d, K_SP, id) : NULL;
    return r ? r : &ZERO.s;
}

const MoveDef* bc_move(const BcData* d, uint8_t id) {
#ifndef NUCLEO_DEVICE
    if(d->moves && id && id < d->n_moves) return &d->moves[id];
#endif
    const void* r = id && id < d->n_moves ? fetch(d, K_MV, id) : NULL;
    return r ? r : &ZERO.m;
}

const TrainerDef* bc_trainer(const BcData* d, uint8_t id) {
#ifndef NUCLEO_DEVICE
    if(d->trainers && id && id < d->n_trainers) return &d->trainers[id];
#endif
    const void* r = id && id < d->n_trainers ? fetch(d, K_TR, id) : NULL;
    return r ? r : &ZERO.t;
}

#ifndef NUCLEO_DEVICE
void bc_data_flush(BcData* d) {
    if(d->cache) memset(d->cache, 0, sizeof(*d->cache));
}

void bc_data_resident(BcData* d) {
    if(d->species) return;
    SpeciesDef* sp = calloc(d->n_species, sizeof(SpeciesDef));
    MoveDef* mv = calloc(d->n_moves, sizeof(MoveDef));
    TrainerDef* tr = calloc(d->n_trainers, sizeof(TrainerDef));
    for(uint8_t i = 1; i < d->n_species; i++) sp[i] = *bc_species(d, i);
    for(uint8_t i = 1; i < d->n_moves; i++) mv[i] = *bc_move(d, i);
    for(uint8_t i = 1; i < d->n_trainers; i++) tr[i] = *bc_trainer(d, i);
    d->species = sp;
    d->moves = mv;
    d->trainers = tr;
}
#endif

bool bc_chart_load(BcData* d) {
    if(d->chart) return true;
    const uint32_t n = CHART_BYTES(d->n_chart); // (the packed chart is read straight into its array; no row check: a row only ever COMPARES its two types with a creature's, it indexes nothing)
    uint8_t* ch = malloc(n ? n : 1);
    if(!ch) return false;
    if(n && !d->io.read(d->io.ctx, "moves.bin", d->off_chart, ch, n)) {
        free(ch);
        return false;
    }
    d->chart = ch;
    return true;
}

void bc_chart_free(BcData* d) {
    free(d->chart);
    d->chart = NULL;
}

void bc_free(BcData* d) {
    free(d->chart);
    free(d->items);
    free(d->cache);
#ifndef NUCLEO_DEVICE
    free(d->species);
    free(d->moves);
    free(d->trainers);
#endif
    memset(d, 0, sizeof(*d));
}

// ---- load: header checks + a streamed validation of every record. The code is battle_load.c (battle_load_impl.h): part of the UI PLUGIN on the device (it runs once, at boot, through
// ui_bc_load, contract 38.1), included here on the host so every harness and bc_test keep calling bc_load directly.
#ifndef NUCLEO_DEVICE
#include "battle_load_impl.h"
#endif

// ============================================================================================
// Stats, experience, creatures
// ============================================================================================
void bc_party_clear(Party* p) {
    memset(p, 0, sizeof(*p));
}

uint8_t bc_hp_dv(const uint8_t dv[4]) {
    return (uint8_t)(((dv[0] & 1) << 3) | ((dv[1] & 1) << 2) | ((dv[2] & 1) << 1) | (dv[3] & 1));
}

static uint32_t ceil_sqrt(uint32_t v) {
    uint32_t r = 0;
    while(r * r < v) r++;
    return r;
}

uint16_t bc_calc_stat(uint8_t base, uint8_t dv, uint16_t statexp, uint8_t level, bool is_hp) {
    uint32_t sq = ceil_sqrt(statexp);
    if(sq > 255) sq = 255;
    uint32_t term = sq / 4;
    uint32_t core = ((((uint32_t)base + dv) * 2 + term) * level) / 100;
    uint32_t v = is_hp ? core + level + 10 : core + 5;
    return (uint16_t)(v > BC_STAT_CAP ? BC_STAT_CAP : v);
}

void bc_mon_recalc(const BcData* d, Mon* m) {
    const SpeciesDef* s = bc_species(d, m->species);
    m->max_hp = bc_calc_stat(s->base[0], bc_hp_dv(m->dv), m->statexp[0], m->level, true);
    for(int i = 0; i < 4; i++)
        m->stat[i] = bc_calc_stat(s->base[i + 1], m->dv[i], m->statexp[i + 1], m->level, false);
    if(m->hp > m->max_hp) m->hp = m->max_hp;
}

uint32_t bc_exp_for_level(uint8_t growth, uint8_t level) {
    if(level <= 1) return 0;
    int32_t n = level, c = n * n * n, v; // 255^3 * 6 < 2^31: no 64-bit division (saves the libgcc helper)
    switch(growth) {
    case GR_MEDIUM_FAST: v = c; break;
    case GR_FAST: v = 4 * c / 5; break;
    case GR_SLOW: v = 5 * c / 4; break;
    default: v = 6 * c / 5 - 15 * n * n + 100 * n - 140; break; // medium slow
    }
    return v < 0 ? 0 : (uint32_t)v;
}

uint8_t bc_level_for_exp(uint8_t growth, uint32_t exp) {
    uint8_t l = 1;
    while(l < 100 && exp >= bc_exp_for_level(growth, l + 1)) l++;
    return l;
}

uint32_t bc_exp_gain(uint8_t base_exp, uint8_t enemy_level, bool trainer) {
    uint32_t e = (uint32_t)base_exp * enemy_level / 7;
    if(trainer) e += e / 2;
    return e;
}

void bc_move_set(const BcData* d, Mon* m, uint8_t slot, uint8_t move) {
    m->moves[slot] = move;
    m->pp[slot] = bc_move(d, move)->pp;
}

uint8_t bc_pp_max(uint8_t base, uint8_t ppbyte) { // contract 77
    const unsigned v = base + (unsigned)(ppbyte >> 6) * (base / 5u);
    return v > BC_PP_CAP ? BC_PP_CAP : (uint8_t)v;
}

uint8_t bc_learn_move(const BcData* d, Mon* m, uint8_t move, uint8_t* slot_out) {
    for(int i = 0; i < 4; i++)
        if(m->moves[i] == move) return 0xFF;
    for(int i = 0; i < 4; i++) {
        if(!m->moves[i]) {
            bc_move_set(d, m, (uint8_t)i, move);
            if(slot_out) *slot_out = (uint8_t)i;
            return 0;
        }
    }
    return BC_LEARN_FULL; // 4 known: nothing changes (the player chooses what to forget; contract 23)
}

void bc_mon_create(const BcData* d, Mon* m, uint8_t species, uint8_t level, uint32_t* rng) {
    memset(m, 0, sizeof(*m));
    const SpeciesDef* s = bc_species(d, species);
    m->species = species;
    m->level = level;
    for(int i = 0; i < 4; i++) m->dv[i] = bc_rand8(rng) & 15;
    m->exp = bc_exp_for_level(s->growth, level);
    bc_mon_recalc(d, m);
    m->hp = m->max_hp;
    for(uint8_t i = 0; i < s->n_learn; i++)
        if(s->learn_lv[i] <= level && bc_learn_move(d, m, s->learn_mv[i], NULL) == BC_LEARN_FULL) { // the newest 4
            for(int k = 0; k < 3; k++) m->moves[k] = m->moves[k + 1], m->pp[k] = m->pp[k + 1];
            bc_move_set(d, m, 3, s->learn_mv[i]);
        }
}

uint8_t bc_evolution(const BcData* d, const Mon* m) {
    const SpeciesDef* s = bc_species(d, m->species);
    return bc_level_evo(s, m);
}

void bc_evolve(const BcData* d, Mon* m) {
    uint8_t to = bc_evolution(d, m);
    if(!to) return;
    uint16_t old_max = m->max_hp;
    m->species = to;
    bc_mon_recalc(d, m);
    if(m->hp) {
        uint32_t h = (uint32_t)m->hp + (m->max_hp > old_max ? m->max_hp - old_max : 0);
        m->hp = (uint16_t)(h > m->max_hp ? m->max_hp : h);
    }
}

bool bc_trainer_party(const BcData* d, uint8_t trainer, uint8_t player_species, Party* out, uint32_t* rng) {
    if(!trainer || trainer >= d->n_trainers) return false;
    const TrainerDef* t = bc_trainer(d, trainer);
    bc_party_clear(out);
    for(uint8_t i = 0; i < t->n_party; i++) {
        uint8_t sp = t->party_sp[i];
        if(i == 0 && (t->flags & 1)) {
            for(uint8_t k = 0; k < t->n_counter; k++)
                if(t->ctr_from[k] == player_species) sp = t->ctr_to[k];
        }
        bc_mon_create(d, &out->mon[i], sp, t->party_lv[i], rng);
    }
    out->count = t->n_party;
    return true;
}
