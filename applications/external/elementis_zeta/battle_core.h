#pragma once
// Battle core: mechanics, integer math, deterministic. PURE C: no furi.h,
// no storage. Data files (reg.bin / moves.bin / trainers.bin, contract section 7) are read
// through a small callback so the same code runs on the host (tools/host_battle) and on the Flipper.
//
// Typical use (UI side):
//   BcIo io = {my_read, my_size, ctx};           // wrappers over read_at()/file_size()
//   BcData data; bc_load(&data, &io);
//   Party mine; bc_party_clear(&mine); bc_mon_create(&data, &mine.mon[0], SP_EMBERCUB, 5, &rng); mine.count = 1;
//   Party foe;  bc_trainer_party(&data, TR_RIVAL_LAB, mine.mon[0].species, &foe, &rng);
//   Battle* b = malloc(sizeof(Battle));          // ~0.7 KB (BC_MAX_EVENTS 48), never copy it by value
//   bc_init(b, &data, &mine, &foe, true, TR_RIVAL_LAB, seed);   // fills b->ev with the send-out events
//   loop: animate b->ev[0..b->n_ev) ; if(b->phase == BP_OVER) break;
//         bc_choose(b, (BcAction){ACT_MOVE, slot}) ; bc_step(b);   // (or ACT_RUN / ACT_SWITCH)
//         when phase == BP_SWITCH the player's creature fainted: bc_choose(ACT_SWITCH, idx); bc_step(b).
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gen/reg_gen.h"
#include "battle_features.h"

#define BC_MAX_PARTY 6
#define BC_MAX_LEARN 12
#define BC_MAX_EVENTS \
    24 // events of one bc_init / bc_step; 8 B each in Battle. Measured max: 10 in the rival simulation, 16 in 3,000 random battles of the multi-turn / copy moves, 17 in the worst step there is (a trainer's creature falls with the Reparto Total, five level-1 creatures on the bench: contract 79 says a creature that is not in play levels up in ONE event); ev() drops the excess but keeps the last 4 slots for EV_SENDOUT / EV_MONEY / EV_NEED_SWITCH / EV_END (was 32, 48, 128: -640 B of the battle heap in all)
#define BC_STAT_CAP  999
#define BC_PRIZE_MAX 999999u // money cap; the prize of a trainer is capped here too
#define AI_SWITCH \
    255 // contract 82: the item id of a trainer AI rule that withdraws the creature instead of using an item (ai.bin, tools/regc.py)

// ---- data (parsed from the .bin files) -------------------------------------------------------
typedef struct {
    uint8_t type1, type2; // type2 == type1 for single-type species
    uint8_t base[5]; // hp atk def spd spc
    uint8_t growth, base_exp, catch_rate, evo_level, evo_to, n_learn;
    uint8_t learn_lv[BC_MAX_LEARN], learn_mv[BC_MAX_LEARN];
} SpeciesDef;

typedef struct {
    uint8_t type, power, acc, pp, effect, arg0;
    int8_t arg1;
    uint8_t chance; // 0 = always, else effect when rand8 < chance
    int8_t priority;
    uint8_t flags; // MF_*
} MoveDef;

typedef struct {
    uint8_t ai_mod, flags, n_party, n_counter; // flags bit0 = counter_starter
    uint16_t money;
    uint8_t party_sp[BC_MAX_PARTY], party_lv[BC_MAX_PARTY];
    uint8_t ctr_from[3], ctr_to[3];
} TrainerDef;

typedef struct {
    uint8_t
        kind; // IK_CATCH (arg = BALL_*), IK_HEAL (arg = HP restored, 255 = all), IK_CURE (arg = SK_* status it cures),
    // IK_KEY (arg 0; never usable), C2-S1: IK_REPEL (arg = steps), IK_ROPE (arg 0), IK_DISC (arg = the move it teaches;
    // the three are used from the bag outside battle only, never in battle), C4-B (contract 36): IK_STONE (arg = ST_* index),
    // IK_VITAMIN (arg = the Mon.statexp[] index it raises): also bag-only; 0 = none. Contract 64: IK_REVIVE (arg 255 = full HP, 0 = half; a FAINTED target; bag and battle),
    // IK_RESTORE (Vital Draught: full HP + status cured; bag and battle), IK_CANDY (Growth Seed, bag only), IK_ELIXIR (arg = PP to every move, 255 = all; bag only)
    uint8_t arg;
} ItemDef; // 2 B per item (contract 64: 103 items): the shop price is NOT kept, the shop reads it from items.bin (the u16 at byte 4 of the record; bc_item_price() for the host tests)

typedef struct {
    bool (*read)(void* ctx, const char* file, uint32_t off, void* buf, uint32_t len);
    int32_t (*size)(void* ctx, const char* file); // -1 if missing
    void* ctx;
} BcIo;

// Memory reserve (post-M5, contract 27): the species, move and trainer RECORDS stay on the card (reg.bin / moves.bin / trainers.bin are read
// as before; their layout did not change) and are fetched on demand through a few small LRU caches, so the resident bytes no longer grow with the
// number of species / moves / trainers. What stays in the heap is what the battle scans on every hit: the type table, the chart and the items.
// bc_species / bc_move / bc_trainer return a pointer into a cache entry: it is valid until BC_CACHE_xx - 1 further lookups of the SAME kind (every
// caller holds at most two species pointers and one move pointer at a time; the host builds with -DBC_CACHE_SP=2 -DBC_CACHE_MV=1 -DBC_CACHE_TR=1
// prove it). An unknown id (0, >= n) or a card that fails returns an all-zero record (the "unused" id 0), never NULL.
#ifndef BC_CACHE_SP
#define BC_CACHE_SP \
    2 // (was 6, then 4, contract 64; contract 67: 2 = the two creatures in play. Every caller holds at most two species pointers at a time (the host stress build runs the whole battle suite with 2: tools/host_battle `make stress`), and a battle only asks for the player's and the enemy's species: -82 B of heap in every mode)
#endif
#ifndef BC_CACHE_MV
#define BC_CACHE_MV 8 // (was 12: contract 64 -- the 4 + 4 moves of the two creatures in play)
#endif
#ifndef BC_CACHE_TR
#define BC_CACHE_TR 1
#endif
typedef struct BcCache BcCache;

typedef struct {
    uint8_t n_species, n_moves, n_types, n_chart, n_trainers,
        n_items; // counts INCLUDE id 0 where applicable
    uint32_t phys; // bit t = type t is physical; n_types <= 32
    uint8_t*
        chart; // contract 83: the type chart, the reference row order, NULL until bc_chart_load (only the battle reads it, contract 27): n_chart bytes (attacking type | defending type << 4), then ceil(n_chart / 4) bytes of 2-bit multiplier codes m (tenths = 5 m m: 0 / 5 / 20); 103 B instead of 246
    ItemDef* items; // [n_items], index = item id (0 unused); items.bin
    BcCache* cache; // the record caches (one heap block)
    BcIo io; // where the records are read from (a copy)
    uint32_t off_moves; // file offset of moves.bin's record 0
    uint32_t off_chart; // ... and of the chart
#ifndef NUCLEO_DEVICE
    // host tests only (bc_data_resident): complete in-memory copies the tests can patch; used instead of the card when set
    SpeciesDef* species;
    MoveDef* moves;
    TrainerDef* trainers;
#endif
} BcData;

#ifndef NUCLEO_DEVICE
// host tests: the shop price of item `id` (items.bin, u16 at byte 4 of its record)
static inline uint16_t bc_item_price(const BcData* d, uint8_t id) {
    uint8_t r[2] = {0, 0};
    d->io.read(d->io.ctx, "items.bin", 12u + id * 8u + 4, r, 2);
    return (uint16_t)(r[0] | r[1] << 8);
}
#endif

typedef enum {
    BC_OK = 0,
    BC_E_OPEN,
    BC_E_MAGIC,
    BC_E_VERSION,
    BC_E_SIZE,
    BC_E_MEM,
    BC_E_DATA,
} BcErr;

BcErr bc_load(
    BcData* d,
    const BcIo*
        io); // validates "reg.bin", "moves.bin", "trainers.bin", "items.bin" (records are streamed, none is kept) and keeps the small tables
// The type chart (82 rows, 103 B packed since contract 83) is only read by the battle (bc_type_eff / bc_damage): it is loaded when a battle opens and freed when it is over
// (party_chart(), battle_open.c); with no chart loaded every type matchup is neutral. bc_chart_load is true if the chart is there (the rows are validated).
bool bc_chart_load(BcData* d);
void bc_chart_free(BcData* d);
static inline bool bc_phys(const BcData* d, uint8_t type) {
    return (d->phys >> (type & 31)) & 1;
} // physical move type (false: special)
const SpeciesDef* bc_species(const BcData* d, uint8_t id);
const MoveDef* bc_move(const BcData* d, uint8_t id);
const TrainerDef* bc_trainer(const BcData* d, uint8_t id);
#ifndef NUCLEO_DEVICE
void bc_data_resident(
    BcData*
        d); // host tests: copy every record into patchable arrays (d->species / moves / trainers); the caches are bypassed from then on
void bc_data_flush(
    BcData* d); // host tests: forget the cached records (after the test changed the filter below)
extern void (*bc_record_hook)(
    int kind,
    uint8_t id,
    void* rec); // host tests: called on every record read from the card (kind 0 species, 1 move, 2 trainer)
#endif
void bc_free(BcData* d);
const char* bc_err_str(BcErr e);

// ---- RNG: xorshift32; EVERY random draw of the core goes through these -----------------------
void bc_rng_seed(uint32_t* s, uint32_t seed); // scrambles the seed (any value, 0 allowed)
uint32_t bc_rng_next(uint32_t* s);
uint8_t bc_rand8(uint32_t* s); // 0..255 (top byte of the next state)
uint32_t bc_rand_range(uint32_t* s, uint32_t lo, uint32_t hi); // lo..hi inclusive

// ---- creatures ------------------------------------------------------------------------------
#define BS_SLP_MASK 0x07 // sleep counter (turns left)
#define BS_PSN      0x08
#define BS_BRN      0x10
#define BS_FRZ      0x20
#define BS_PAR      0x40

typedef struct {
    uint8_t species, level, status;
    uint8_t dv[4]; // atk def spd spc (0..15); the HP DV comes from their low bits
    uint8_t
        xf; // contract 65: MX_* flags (was padding: always 0 in every older save, box.bin and trade payload)
    uint16_t statexp[5]; // hp atk def spd spc
    uint16_t fr; // contract 65: friendship, biased (see bc_friend; was padding, 0 = the default 70)
    uint32_t exp;
    uint16_t hp, max_hp;
    uint16_t stat[4]; // atk def spd spc (unmodified)
    uint8_t moves[4]; // move ids, 0 = empty slot
    uint8_t pp[4];
} Mon;
_Static_assert(
    sizeof(Mon) == 44 && offsetof(Mon, xf) == 7 && offsetof(Mon, fr) == 18 &&
        offsetof(Mon, exp) == 20,
    "Mon is saved / boxed verbatim: xf and fr are the old padding bytes (contract 65)");

// ---- contract 65: variocolor (shiny), friendship, original trainer. All of it lives in the Mon's old padding (xf at offset 7, fr at 18..19): no save / box / link format change; the helpers are header-inline (no symbol, no resident code).
// VARIOCOLOR: a creature is shiny when Def == Spd == Spc == 10 (any Atk): exactly 16 of the 65,536 DV sets = 1 / 4096, the wild draw (4 random DVs) keeps that chance. dv[] = atk def spd spc.
static inline bool bc_shiny(const Mon* m) {
    return m->dv[1] == 10 && m->dv[2] == 10 && m->dv[3] == 10;
}
// FRIENDSHIP (amistad) 0..255 with a 1/256 fraction: `fr` = (friendship * 256 + fraction) - BC_FR_BIAS (mod 65536), so the all-zero padding of every older Mon / save / box record IS the default 70.0. One step of
// walking adds 1 to `fr` (256 steps = +1 friendship, daycare.c); everything else goes through bc_friend_add. The 16-bit value is a bijection of 0..65535, so every stored value is valid.
#define BC_FR_DEFAULT  70u
#define BC_FR_BIAS     (BC_FR_DEFAULT << 8)
#define BC_FR_STEP_MAX 0xB9FFu // `fr` of friendship 255 + 255/256: the walking counter stops here
#define BC_FR_GIFT \
    120u // a starter / gift (party_give); a creature caught in the wild starts at BC_FR_DEFAULT
#define BC_FR_EVO 220u // friendship evolution: level-up with at least this much
static inline uint8_t bc_friend(const Mon* m) {
    return (uint8_t)((uint16_t)(m->fr + BC_FR_BIAS) >> 8);
}
static inline void bc_friend_set(Mon* m, uint8_t v) {
    m->fr = (uint16_t)((uint16_t)v * 256u - BC_FR_BIAS);
}
static inline void
    bc_friend_add(Mon* m, int d) { // +d / -d, clamped to 0..255; the fraction is kept
    const uint16_t x = (uint16_t)(m->fr + BC_FR_BIAS);
    int v = (x >> 8) + d;
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    m->fr = (uint16_t)(((uint16_t)v << 8 | (x & 255u)) - BC_FR_BIAS);
}
static inline uint8_t bc_friend_tier(
    uint8_t
        f) { // the evaluator's five answers (Club de Fans): 0 cold < 50 <= 1 wary < 100 <= 2 friendly < 150 <= 3 fond < 220 <= 4 devoted
    return f < 50 ? 0 : f < 100 ? 1 : f < 150 ? 2 : f < BC_FR_EVO ? 3 : 4;
}
static inline int
    bc_friend_level_gain(const Mon* m) { // what a level-up adds: 3 below 100, 2 below 200, 1 above
    const uint8_t f = bc_friend(m);
    return f < 100 ? 3 : f < 200 ? 2 : 1;
}
// ORIGINAL TRAINER: MX_TRADED is set when the creature arrived over the IR link (link_game.c); every creature the player caught, was given, found, hatched or bought has it clear. bc_mon_is_ot() = "the player is the
// original trainer" (the Eclipse Form of contract 66 asks for it). Old saves: every creature counts as the player's own (the link is newer than every old save).
#define MX_TRADED 1
static inline bool bc_mon_is_ot(const Mon* m) {
    return !(m->xf & MX_TRADED);
}

typedef struct {
    Mon mon[BC_MAX_PARTY];
    uint8_t count;
} Party;

void bc_party_clear(Party* p);
uint8_t bc_hp_dv(const uint8_t dv[4]);
uint16_t bc_calc_stat(uint8_t base, uint8_t dv, uint16_t statexp, uint8_t level, bool is_hp);
void bc_mon_recalc(const BcData* d, Mon* m); // recompute max_hp/stats; hp unchanged (clamped to max)
void bc_mon_create(
    const BcData* d,
    Mon* m,
    uint8_t species,
    uint8_t level,
    uint32_t* rng); // random DVs
uint32_t bc_exp_for_level(uint8_t growth, uint8_t level); // total exp needed to BE this level
uint8_t bc_level_for_exp(uint8_t growth, uint32_t exp);
uint32_t bc_exp_gain(uint8_t base_exp, uint8_t enemy_level, bool trainer);
// learn a move: the first empty slot (returns 0, *slot_out = it), 0xFF if the move is already known (nothing done), BC_LEARN_FULL when
// the 4 slots are taken: NOTHING changes and the caller decides.
#define BC_LEARN_FULL 0xFE
uint8_t bc_learn_move(const BcData* d, Mon* m, uint8_t move, uint8_t* slot_out);
void bc_move_set(
    const BcData* d,
    Mon* m,
    uint8_t slot,
    uint8_t move); // puts `move` in `slot` with full PP (forgetting what was there)
// Contract 77 (PP Plus): a PP byte is `ups << 6 | PP left`: the top two bits count the PP Extra used on that move (0 .. 3, each adds base / 5 to its PP), bits 0 .. 5 are the PP left.
// EVERY read of a PP byte that means "PP left" is `& BC_PP_LEFT`; a decrement is safe as it is (a move with PP left has a non-zero low part); a move put in a slot (bc_move_set) starts with 0 ups.
// bc_pp_max(base, byte) = the most PP of a move whose PP byte is `byte` (base + ups * (base / 5), at most BC_PP_CAP, 61). Resident (battle_core.c), imported by the plugins.
#define BC_PP_LEFT 0x3F
#define BC_PP_UP   0x40
#define BC_PP_CAP  61
uint8_t bc_pp_max(uint8_t base, uint8_t ppbyte);
// C4-B (contract 36): the species `species` evolves into with the stone `stone` (ST_* of reg_gen.h), 0 = it does not. evostone.bin: 12 B header ("NEVS", version, n, rec 4,
// 0, size), n records {species, stone, target, 0}; read on demand in 8-record chunks (nothing stays in memory). Header-inline: the menu plugin and the UI plugin use it
// without a symbol (and the resident app without a byte of code). A missing / short file answers 0.
static inline uint8_t bc_stone_evo(const BcData* d, uint8_t species, uint8_t stone) {
    uint8_t h[12], r[32];
    if(!d->io.read(d->io.ctx, "evostone.bin", 0, h, 12) || h[6] != 4) return 0;
    for(uint32_t i = 0, k; i < h[5]; i += k) {
        k = h[5] - i < 8 ? h[5] - i : 8;
        if(!d->io.read(d->io.ctx, "evostone.bin", 12 + i * 4, r, k * 4)) return 0;
        for(uint32_t j = 0; j < k; j++)
            if(r[j * 4] == species && r[j * 4 + 1] == stone)
                return r[j * 4 + 2] < d->n_species ? r[j * 4 + 2] : 0;
    }
    return 0;
}
// Contract 65: the BY-LEVEL evolution of `m` now (the species it becomes, 0 = none): species.toml `evolve = [level, ..]`, or `evolve_friend = [level, ..]`, which reg.bin writes as evo_level = level | EVO_FRIEND (0x80): that
// one also needs friendship >= BC_FR_EVO. Callers ask it after a level-up (the post-battle chain / the Growth Seed; a stone or a trade evolution never comes here). The resident chain (natives.c evo_ready) only masks the
// bit (it calls any friendship species level-ready at its floor); the plugins that open the evolution scene ask this.
static inline uint8_t bc_level_evo(const SpeciesDef* s, const Mon* m) {
    const uint8_t lv = s->evo_level & 127;
    return s->evo_to && lv && m->level >= lv &&
                   (!(s->evo_level & 128) || bc_friend(m) >= BC_FR_EVO) ?
               s->evo_to :
               0;
}
uint8_t bc_evolution(const BcData* d, const Mon* m); // species it should evolve into now, 0 = none
void bc_evolve(const BcData* d, Mon* m); // applies bc_evolution (HP rises by the max-HP delta)
// Builds a trainer party. For counter_starter trainers slot 0 becomes the starter that beats `player_species`.
bool bc_trainer_party(
    const BcData* d,
    uint8_t trainer,
    uint8_t player_species,
    Party* out,
    uint32_t* rng);

// ---- formula building blocks (also used by the tests) ----------------------------------------
uint16_t bc_stage_apply(uint16_t v, int stage); // v*num/den, min 1, cap 999
void bc_stage_ratio(int stage, uint8_t* num, uint8_t* den); // -6..+6 table
uint8_t bc_type_eff(
    const BcData* d,
    uint8_t move_type,
    uint8_t t1,
    uint8_t t2); // tenths: 0,2,5,10,20,40...

typedef struct {
    uint8_t level, power, move_type; // move_type TY_NONE = typeless (no STAB / chart / random)
    uint8_t atk_t1, atk_t2, def_t1, def_t2;
    uint16_t a, d; // final attack / defence (stages, status, screens already applied)
    uint8_t crit;
    uint8_t rnd; // 217..255
} BcDmgIn;
// damage with the FIXES (clamp A,D >= 1 after /4; min 1 unless immune). *eff_out = total effectiveness.
uint16_t bc_damage(const BcData* d, const BcDmgIn* in, uint8_t* eff_out);
uint8_t
    bc_crit_threshold(uint8_t base_speed, bool high_crit, bool focus); // crit if rand8 < result
uint16_t bc_hit_threshold(
    uint8_t acc,
    int acc_stage,
    int eva_stage,
    bool never_miss); // hit if rand8 < result; 256 = always
uint16_t bc_run_threshold(
    uint16_t player_spd,
    uint16_t enemy_spd,
    uint8_t attempts); // escape if rand8 < result; 256 = always

// trainer prize money: the trainer's base money x the level of its LAST creature (cap BC_PRIZE_MAX). Contract 16.
uint32_t bc_prize(uint16_t base_money, uint8_t last_level);
// True when the creature has the major status SK_SLEEP / SK_POISON / SK_BURN / SK_FREEZE / SK_PARALYSIS (what an IK_CURE item cures; 0 = any of them: Cure-All, contract 73).
// Header-inline: the battle plugin and the menu plugin (party list in the bag screen) use it without a symbol.
static inline bool bc_has_status(const Mon* m, uint8_t sk) {
    switch(sk) {
    case SK_SLEEP:
        return (m->status & BS_SLP_MASK) != 0;
    case SK_POISON:
        return (m->status & BS_PSN) != 0;
    case SK_BURN:
        return (m->status & BS_BRN) != 0;
    case SK_FREEZE:
        return (m->status & BS_FRZ) != 0;
    case SK_PARALYSIS:
        return (m->status & BS_PAR) != 0;
    case 0:
        return m->status != 0; // contract 73: Cure-All (any status)
    default:
        return false;
    }
}

// ---- capture (contract 14.2) -----------------------------------------------
// `n` is the first draw (0..bc_ball_range(ball)), `m` the second (0..255). Returns true when the creature is caught;
// *shakes (nullable) = the wobbles shown: 3 when caught, else 0..3 by the "Z" value.
bool bc_catch(
    uint8_t ball,
    uint8_t catch_rate,
    uint16_t max_hp,
    uint16_t hp,
    uint8_t status,
    uint8_t n,
    uint8_t m,
    uint8_t* shakes);
uint8_t bc_ball_range(
    uint8_t ball); // largest first draw: 255 crystal, 200 great, 150 ultra (master: 0)

// ---- battle -------------------------------------------------------------------------------
enum {
    VF_CONF = 1,
    VF_FLINCH = 2,
    VF_FOCUS = 4,
    VF_REFLECT = 8,
    VF_LSCREEN = 16,
    VF_MIST = 32,
    VF_SEED = 64,
    VF_TRAP = 128
}; // MIST / SEED / TRAP: contract 64 (Mist, Sap Seed, held by a Bind-like move)

typedef struct {
    // contract 77: fields ordered by how often the code touches them (a byte / half-word / word below offset 32 / 64 / 124 is a 16-bit Thumb access, above it a 32-bit one): hot small fields first, big blocks last
    int8_t stage[6]; // STI_ATK..STI_EVA
    uint8_t vol; // VF_*
    uint8_t conf_turns;
    uint8_t active; // index into the party
    uint8_t acted; // already moved this turn
    uint8_t fainted_done; // the faint of the active creature was already processed
    uint8_t run_attempts;
    // contract 64 (the moves): the multi-turn moves and the memory a few of them need. All of it is cleared when a creature is sent out (side_reset).
    uint8_t
        lock; // turns still to come of a charge / Rampage / Bind / Hold Fast (0 = none); bit 7 = out of reach (Fly)
    uint8_t lmove; // the move that goes on while `lock` is set
    uint8_t last; // the move this side used last (Parrot)
    uint8_t sub; // HP of the decoy (Substitute), 0 = none
    uint8_t
        ecl; // contract 67: the Eclipse Form of the creature in play (1 .. 3, bc_ecl_form), 0 = none; cleared with the rest when a creature is sent out (side_reset): the form ends on a faint or a switch
    // contract 73: the state of the moves that is not a status. Everything but `bdg` is cleared with the rest when a creature is sent out. Fields of a feature that is switched off in
    // battle_features.h do not exist (the heap of a battle is what the shipped features need).
    uint8_t fl; // FL_* (recharge, toxic, types overridden)
    uint8_t ty[2]; // the types in force while FL_TY is set (Typecast, Transform, the Eclipse Form)
    uint8_t
        bdg; // badge boosts, bit k = stat k x 9/8 (the player's side only, set once per battle; kept when a creature is sent out)
#if BF_TOXIC
    uint8_t tox; // the toxic counter
#endif
#if BF_DISABLE
    uint8_t dis; // Disable: (slot + 1) << 4 | turns left, 0 = none
#endif
#if BF_MOVE_COPY || BF_TRANSFORM
    uint8_t
        ovm; // bit k: move slot k is overridden by mv[k] (a Echoing copy in one slot, or Transform's four: 0xF); the other slots are the creature's own moves
    uint8_t mv[4]; // the overriding moves
#endif
#if BF_TRANSFORM
    uint8_t
        tf; // Transform: the species this creature looks like (0 = none); pp5 / tst are valid while it is set
    uint8_t pp5[4]; // Transform: the PP of those moves (5 each)
    uint16_t tst[4]; // Transform: atk def spd spc (the target's, before stages)
#endif
    uint16_t hurt; // damage a Normal / Fighting move did to this side this turn (Counter)
    uint16_t store; // damage taken while biding (Hold Fast)
} Side;
enum {
    FL_RECH = 1,
    FL_TOX = 2,
    FL_TY = 4
};
enum {
    LOCK_RAGE = 0x7F
}; // Side.lock of a creature that is raging (Rampage / Hold Fast / a charge hold 1 .. 4, bit 7 = out of reach): it never runs down
// contract 79: side `s` has no choice this turn: it is locked into a multi-turn move (Rage, Rampage, Hold Fast, a binding move, Fly / Dig's second turn) or it must recharge (Mega Beam). The scene does not open the menu for the
// player (bc_forced_go sets ACT_MOVE 0xFF and the turn runs), bc_choose accepts nothing but a move (it becomes 0xFF), and the foe's choice is 0xFF too (a Dash Strike in its list must not change the order of a move that goes on).
#define BC_FORCED(b, s) ((b)->side[s].lock || ((b)->side[s].fl & FL_RECH))

uint16_t bc_eff_stat(
    const Mon* m,
    const Side* sd,
    int stat); // ATK..SPC: stages, then burn/paralysis penalty

typedef enum {
    ACT_NONE = 0,
    ACT_MOVE,
    ACT_RUN,
    ACT_SWITCH,
    ACT_ITEM,
    ACT_ROCK,
    ACT_BAIT
} ActKind; // ACT_ROCK / ACT_BAIT: only in a Reserva Salvaje (Reserve) battle, Battle.reserve (contract 43)
typedef struct {
    uint8_t kind; // ActKind
    uint8_t arg; // move slot 0..3 / party index / item id (ACT_ITEM)
    uint8_t arg2; // ACT_ITEM: target party index of a healing item (ignored by a catch item)
} BcAction;

typedef enum {
    BP_CHOOSE = 0,
    BP_SWITCH,
    BP_OVER
} BcPhase;
typedef enum {
    BR_NONE = 0,
    BR_WIN,
    BR_LOSE,
    BR_RAN,
    BR_CAUGHT
} BcResult;

// Events (the UI animates them in order). side: 0 = player, 1 = enemy.
//  type            side  a                      b                       v                  w
typedef enum {
    EV_NONE = 0,
    EV_SENDOUT, // side  party index
    EV_USE_MOVE, // user  move id             slot (0xFF struggle)
    EV_MISS, // user  (attack missed)
    EV_DAMAGE, // target crit (0/1)         effectiveness in tenths   damage dealt      hp after
        //                           (0 immune, <10 not very effective, 10, >10 super)
    EV_STAT, // target stat STI_*         0                        delta (int16)      new stage (int16); delta 0 = nothing happened
    EV_STATUS, // target SK_* kind          0 inflicted / 1 failed (already has one / immune)
    EV_CURED, // side  SK_* kind (woke up, thawed, confusion over)
    EV_CANT_MOVE, // side  CM_* reason
    EV_CONF_TURN, // side  ("is confused")
    EV_SELF_HIT, // side  0                   0                       damage             hp after (confusion)
    EV_RESIDUAL, // side  SK_POISON/SK_BURN   0                       damage             hp after
    EV_FOCUS, // side  (getting pumped)
    EV_SCREEN, // side  SCR_*
    EV_RECOIL, // side  0                   0                       damage             hp after (struggle)
    EV_FAILED, // user  ("but it failed")
    EV_FAINT, // side
    EV_EXP, // 0     party index         0                       exp gained
    EV_LEVELUP, // 0     party index         new level
    EV_LEARN, // 0     move id             slot                    0 learned in `slot`, BC_LEARN_FULL = 4 moves known (nothing changed: the scene asks, contract 23)
    EV_RUN_OK, // 0
    EV_RUN_FAIL, // 0     1 = trainer battle (cannot flee)
    EV_THROW, // 0     item id             (a catch item is thrown at the wild creature)
    EV_SHAKE, // 0     wobbles 0..3        (the capsule wobbles `a` times before the verdict; 3 when caught)
    EV_CAUGHT, // 0     party slot, 0xFF = the party was full (the scene hands the creature to the pending slot, contract 23)
    EV_BROKE, // 0 which 0..3 (it broke free after `which` wobbles: four messages)
    EV_HEAL, // 0     party index         item id                  HP restored        hp after
    EV_CURE, // 0     party index         item id                  SK_* cured         (a status-cure item; M4-A)
    EV_MONEY, // 0     (trainer beaten)    0                        prize low 16 bits  prize high 16 bits (before EV_END)
    EV_NEED_SWITCH, // 0   (the player's creature fainted: choose ACT_SWITCH)
    EV_END, // 0     BR_* result
    EV_RESERVE, // 0     SF_*                (Reserva Salvaje, contract 43: after EV_END so no older number moves)
    EV_MSG, // side  MSG_* (contract 64)  number (MSG_HITS)  - a line of the new moves: the scene's MSG_TXT table
} BcEventType;

// EV_MSG.a: which line (the scene keeps the text ids in this order, scene_battle.c MSG_TXT); EV_MSG.b = the number the line shows ({v2}, MSG_HITS). The charge lines are the `arg1` of the charge moves.
// Contract 73: MSG_RAGE "is building", MSG_UNDIS "is no longer disabled" (b = 0), the lines that name a MOVE in `b` (MSG_DISABLED "'s move was disabled", MSG_MOVE_COPY "learned"), the lines
// that name a SPECIES in `b` (MSG_TRANSFORM "transformed into", MSG_CONVERT "changed its type to match") and the five disobedience lines (the creature of the player's side).
enum {
    MSG_HITS = 0,
    MSG_OHKO,
    MSG_CHG_WIND,
    MSG_CHG_FLY,
    MSG_CHG_BASH,
    MSG_CHG_SKY,
    MSG_FLED,
    MSG_STAT_RESET,
    MSG_MIST,
    MSG_SEEDED,
    MSG_STORE,
    MSG_UNLEASH,
    MSG_SUBHIT,
    MSG_SUBEND,
    MSG_RAGE,
    MSG_UNDIS,
    MSG_DISABLED,
    MSG_MOVE_COPY,
    MSG_TRANSFORM,
    MSG_CONVERT,
    MSG_LOAF,
    MSG_NOOBEY,
    MSG_TURNED,
    MSG_IGNORED,
    MSG_NAP,
    // contract 82 (agent): Dig's charging line (a move's arg1, 25 in moves.toml), Pilfer's coins (b = how many), the trainer AI's item (side 1, b = the item id) and withdrawal (the old creature's name), the Flute in battle (played / everybody that slept woke / no effect)
    MSG_DIG,
    MSG_COINS,
    MSG_AIUSE,
    MSG_AIOUT,
    MSG_FLUTE,
    MSG_FLUTE0,
    MSG_WOKE,
    MSG_COUNT
};
// the scene's MSG_TXT table stops at the last line of a feature that is switched on (the entries past it would be zeros)
#define MSG_N \
    MSG_COUNT // (contract 82: the lines of the trainer AI, Dig, Pilfer and the Flute come after the optional ones of contract 73, so the table runs to the end)
#define MSG_MV0 MSG_DISABLED // MSG_MV0 .. MSG_MOVE_COPY: `b` is a move id
#define MSG_SP0 MSG_TRANSFORM // MSG_TRANSFORM: `b` is a species id
// EV_RECOIL.a: the cause of the HP change of `side` (v = the HP, w = the HP after): RC_RECOIL "hit with recoil", RC_ABSORB "sucked up health" (drain moves, the seed's receiver), RC_REGAIN (Recover),
// RC_SEED (the seeded creature), RC_NAP (Rest), RC_SUB (Substitute: the HP paid for the decoy)
enum {
    RC_RECOIL = 0,
    RC_ABSORB,
    RC_REGAIN,
    RC_SEED,
    RC_NAP,
    RC_SUB,
    RC_CRASH
}; // RC_CRASH (contract 73): Jump Kick / High Kick missed, the user takes 1 HP

// EV_RESERVE.a: the Reserve battle's own lines. SF_ROCK / SF_BAIT: the player threw one; SF_ANGRY / SF_EATING: the creature's state this turn; SF_FLED: it ran away (a BR_RAN result follows).
enum {
    SF_ROCK = 1,
    SF_BAIT,
    SF_ANGRY,
    SF_EATING,
    SF_FLED
};

enum {
    CM_SLEEP = 1,
    CM_FROZEN,
    CM_FLINCH,
    CM_PARALYZED,
    CM_TRAPPED,
    CM_RECHARGE
}; // EV_CANT_MOVE.a (CM_TRAPPED: held by Bind / Wrap / Clamp / Cinder Spin, contract 64; CM_RECHARGE: after Mega Beam, contract 73)

typedef struct {
    uint8_t type, side, a, b;
    uint16_t v, w;
} BcEvent;

typedef struct Battle {
    // contract 77: ordered by use, not by topic: the small hot fields first (a byte / half-word / word below offset 32 / 64 / 124 is a 16-bit Thumb access), the big blocks (side, enemy, ev) last. Keep new fields in that spirit.
    BcAction act[2];
    uint8_t phase; // BcPhase
    uint8_t result; // BcResult
    uint8_t n_ev;
    uint8_t is_trainer, trainer_id, ai_mod;
    uint8_t
        no_reward; // IR LINK exhibition battle (contract 45): no experience, no prize money, no Registro entries (set by the scene right after bc_init)
    uint8_t
        reserve; // C5-B (contract 43): a Reserva Salvaje battle (set by the scene): bc_step's turn has no enemy attack, only the chance that the creature runs away; the scene also sets side[0].run_attempts = 255 (Run always works)
#if BF_OBEY
    uint8_t
        cap; // contract 73: the level above which a creature of another trainer may disobey (badges: 10 / 30 / 50 / 70, 255 = never); the scene sets it with Side.bdg
#endif
    uint8_t
        sf_rate; // the creature's CURRENT catch rate: its species' rate (set by the scene), doubled by every rock (max 255), halved by every bait (min 1)
    int8_t
        sf_mood; // > 0: angry for that many more turns (a rock: flee chance x2), < 0: eating (bait: flee chance / 4), 0 normal; 1..5 turns
    uint8_t
        ecl_used; // contract 67: the player's Eclipse Form was taken in this battle (once per battle, even when the form has ended)
    uint8_t
        share; // contract 74: the Reparto Total is in the bag (set by the scene right after bc_init): the experience of a fallen foe is halved for the creature in play and the other half split among the living party (battle_run.c award_exp)
    uint8_t turn;
    uint8_t
        part; // contract 82: bit k = party creature k of the player took part in the fight against the enemy creature in play (set when it is sent out, cleared when it faints, reset to the creature in play when the enemy sends one out): the experience is shared among them
    uint8_t
        ai_st; // contract 82: bits 0 .. 3 = the trainer AI's uses left for the enemy creature in play + 1 (0 = its row of ai.bin was not read yet), bits 4 .. 5 = the enemy turns it has had (0, 1, 2 +): the AI modifier 2 acts on the second
    uint16_t
        pay; // contract 82: the coins Pilfer scattered in this battle, 2 x the user's level at every hit, whoever used it; paid (EV_MONEY, with a trainer's prize) at the end of a won or caught battle
    uint32_t rng;
    const BcData* data;
    Party* party[2]; // [0] = the player's party (caller owned), [1] = &enemy
    Side side[2];
    Party enemy;
    BcEvent ev[BC_MAX_EVENTS]; // events produced by the last bc_init / bc_step
#if BF_AI_ITEMS || BF_AI_SWITCH
    uint8_t ai_row
        [7]; // contract 85: the trainer AI class's row of files/ai.bin (uses, then 2 rules of 3 bytes), read once per battle by ai_turn (battle_ai.c); [3] = the first rule's item, never 0 once read. Last: cold, and nothing above moves
#endif
} Battle;

// ---- contract 73: the moves in use and their PP (Echoing / Transform change them for the battle; the Mon is never written). Header-inline: no symbol.
#if BF_DISABLE
#define BF_DIS_OFF(b, s, slot) ((b)->side[s].dis >> 4 == (slot) + 1) // that move slot is Disabled
#define BF_DIS_ON(b, s)        ((b)->side[s].dis)
#else
#define BF_DIS_OFF(b, s, slot) 0
#define BF_DIS_ON(b, s)        0
#endif
#if BF_TRANSFORM
#define BF_TF_ON(b) ((b)->side[0].tf) // the player's creature is transformed
#else
#define BF_TF_ON(b) 0
#endif
#if BF_MOVE_COPY || BF_TRANSFORM
static __attribute__((noinline, unused)) uint8_t bc_mv(
    const Battle* b,
    int s,
    int k) { // (outlined: the plugin builds with GCC's inlining on, and the expression is large)
    return (b->side[s].ovm >> k) & 1 ? b->side[s].mv[k] :
                                       b->party[s]->mon[b->side[s].active].moves[k];
}
#else
static __attribute__((noinline, unused)) uint8_t bc_mv(const Battle* b, int s, int k) {
    return b->party[s]->mon[b->side[s].active].moves[k];
}
#endif
static __attribute__((noinline, unused)) uint8_t* bc_ppp(
    const Battle* b,
    int s,
    int k) { // the PP byte of move slot k of side s (Transform's own while it is on)
#if BF_TRANSFORM
    if(b->side[s].tf) return (uint8_t*)&b->side[s].pp5[k];
#endif
    return &b->party[s]->mon[b->side[s].active].pp[k];
}

// ---- contract 67: the Eclipse Form. Header-inline like the contract 65 helpers: no symbol, no resident code.
// Forms 1 .. 3 = the player's starter finals (species.toml `eclipse_art`, tools/monc.py ECL_SPECIES): Pyrewolf, Tidalgard, Grovestag. A form changes the types and adds base stat points (NOT HP) to the creature in play, for the
// rest of the battle or until it faints or is switched out (side_reset). Nothing is written to the Mon: its stats, HP and the save never see it; `bc_eff_stat` adds the bonus and the type reads go through `bc_ecl_ty`.
#define BC_INL                    \
    static inline __attribute__(( \
        always_inline)) // (the battle plugin builds with -fno-inline-small-functions: these tables are meant to cost no call)
BC_INL uint8_t bc_ecl_form(uint8_t species) {
    return species == SP_PYREWOLF  ? 1 :
           species == SP_TIDALGARD ? 2 :
           species == SP_GROVESTAG ? 3 :
                                     0;
}
// types of the form: Fire / Ghost, Water / Ice, Grass / Psychic
BC_INL uint8_t bc_ecl_ty(uint8_t form, int n) {
    static const uint8_t T[3][2] = {
        {TY_FIRE, TY_GHOST}, {TY_WATER, TY_ICE}, {TY_GRASS, TY_PSYCHIC}};
    return T[form - 1][n];
}
// base stat points added (atk def spd spc), +100 in all: the stat grows by points * level / 50 (what the formula gives for that many base points, DVs and stat exp unchanged)
BC_INL uint16_t bc_ecl_bonus(uint8_t form, int stat, uint8_t level) {
    static const uint8_t D[3][4] = {{20, 10, 30, 40}, {10, 40, 10, 40}, {10, 20, 30, 40}};
    return (uint16_t)(D[form - 1][stat] * level / 50);
}
// The form the creature in play of the PLAYER (side 0) could take now (0 = none): a starter final, the player's own (bc_mon_is_ot), alive, not taken yet in this battle, no form on, the choosing phase, not a
// Reserva Salvaje battle. The scene adds what the core cannot know: the Eclipse Stone in the bag. The enemy is never asked (trainers never use it).
uint8_t bc_ecl_can(const Battle* b); // battle_run.c (the plugin: the resident app never asks)
// Takes the form (no turn passes: the player still chooses an action). Returns the form, 0 when it is not allowed.
uint8_t bc_ecl_go(Battle* b);

// Starts a battle: the first healthy creature of each party is sent out (events EV_SENDOUT enemy, player).
// `enemy` is copied into b->enemy. Returns false if a party has no healthy creature.
bool bc_init(
    Battle* b,
    const BcData* d,
    Party* player,
    const Party* enemy,
    bool trainer,
    uint8_t trainer_id,
    uint32_t seed);
// Stores the player's action for the next bc_step. false = illegal (empty slot, no PP, fainted target, a catch item in a
// trainer battle, a heal item on a full-HP or fainted creature, unknown item...).
// A creature without PP anywhere may pick any slot: it uses the built-in Struggle.
bool bc_choose(Battle* b, BcAction act);
// Resolves the turn (or the forced switch) and returns the number of events in b->ev.
int bc_step(Battle* b);
// Battle style "Shift" (contract 75): swaps the player's creature for party slot `idx` WITHOUT a turn (no enemy action) and appends an EV_SENDOUT (side 0) to b->ev / n_ev.
// False (nothing changed) when the phase is not BP_CHOOSE, the slot is empty / fainted / the creature in play, or b->ev is full. The scene calls it between a trainer's creature falling and the next one coming.
bool bc_forced_go(Battle* b); // contract 79 (battle_run.c, the battle plugin only)
bool bc_shift(Battle* b, uint8_t idx);
// contract 79 (both in battle_run.c, the battle plugin only): move slot k of side s can be chosen (it holds a move with PP left and it is not the Disabled one) / the base PP of the slot's OWN move, the one its maximum comes from
// (a Transformed creature's slots: 5; a Echoing copy keeps Echoing's, not the copied move's, maximum).
bool bc_usable(const Battle* b, int s, int k);
uint8_t bc_pp_base(const Battle* b, int s, int k);

// Low-level pieces used by bc_step (exposed for tests).
const MoveDef* bc_move_def(const Battle* b, uint8_t move_id); // MV_STRUGGLE is built in
uint16_t bc_compute_damage(
    const Battle* b,
    int atk_side,
    const MoveDef* mv,
    bool crit,
    uint8_t rnd,
    uint8_t* eff_out);
