#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pkb/policy_rng.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKB_PARTY_LENGTH 6
#define PKB_NUM_MOVES 4
#define PKB_NUM_SPECIES 251
#define PKB_MOVEPOOL_MAX 255
#define PKB_LEVEL_MIN 2
#define PKB_LEVEL_MAX 100
#define PKB_TRAINER_NAME_LEN 11 /* Game Boy name buffer incl. terminator */
/* Random teams skip a not-fully-evolved species more than this many levels
 * past its evolve_level. */
#define PKB_NFE_LEVEL_SLACK 10

/* Random teams are "trained": DVs 15, stat exp 65535 in every stat. */
#define PKB_TEAM_DV 15
#define PKB_TEAM_STAT_EXP 0xFFFF
#define PKB_TEAM_HAPPINESS 255

/* stat indices into PkbTeamMon.stats (party struct order after max HP) */
enum { PKB_STAT_ATK, PKB_STAT_DEF, PKB_STAT_SPD, PKB_STAT_SAT, PKB_STAT_SDF, PKB_NUM_BATTLE_STATS };

typedef struct {
    uint8_t species;
    uint8_t level;
    uint8_t item;
    uint8_t moves[PKB_NUM_MOVES];
    uint8_t pp[PKB_NUM_MOVES];
    uint8_t dvs[2]; /* party struct order: atk << 4 | def, spd << 4 | spc */
    uint16_t stat_exp; /* the same in all five stats */
    uint8_t happiness;
    /* derived by pkb_team_mon_calc */
    uint32_t exp;
    uint16_t max_hp;
    uint16_t stats[PKB_NUM_BATTLE_STATS];
} PkbTeamMon;

typedef struct {
    uint8_t count;
    uint16_t trainer_id;
    char name[PKB_TRAINER_NAME_LEN]; /* ASCII; trainer and OT name */
    PkbTeamMon mon[PKB_PARTY_LENGTH];
} PkbTeam;

/* Random-access reader for files/movepools.bin (SD card on the device). */
typedef struct {
    bool (*read)(void* ctx, uint32_t offset, void* buf, uint32_t len);
    void* ctx;
} PkbBlobReader;

/* Returns true if the move may be used on the Flipper's team. */
typedef bool (*PkbMoveFilter)(void* ctx, uint8_t move);

/* movepools.bin access */
bool pkb_movepool_check(const PkbBlobReader* r);
typedef struct {
    uint8_t min_level; /* lowest level at which the species can exist */
    uint8_t evolve_level; /* level by which it would normally have evolved; 0 = fully evolved */
} PkbSpeciesInfo;

/* Writes the moves `species` can know at `level` (ascending move ids) to out;
 * returns their number, or 0 if the species cannot exist at that level, or -1
 * on error. If info is not NULL it receives the species' record header. */
int pkb_movepool_read(
    const PkbBlobReader* r,
    uint8_t species,
    uint8_t level,
    uint8_t out[PKB_MOVEPOOL_MAX],
    PkbSpeciesInfo* info);

/*
 * Port of CalcMonStatC (engine/pokemon/move_mon.asm):
 *   ((base + dv) * 2 + ceil_sqrt(stat_exp) / 4) * level / 100 + 5
 * plus level + 10 instead of 5 for HP, capped at 999. ceil_sqrt follows
 * GetSquareRoot (capped at 255). Stat exp is ignored if !use_stat_exp.
 */
uint16_t pkb_calc_stat(uint8_t base, uint8_t dv, uint16_t stat_exp, bool use_stat_exp,
                       uint8_t level, bool is_hp);

/* engine/pokemon/experience.asm: CalcExpAtLevel (level >= 2) */
uint32_t pkb_exp_at_level(uint8_t growth_rate, uint8_t level);

/* HP DV from the other four (party struct DV bytes). */
uint8_t pkb_hp_dv(const uint8_t dvs[2]);

/* A random-team mon: DVs PKB_TEAM_DV, stat exp PKB_TEAM_STAT_EXP, happiness
 * PKB_TEAM_HAPPINESS, no item; then pkb_team_mon_calc. */
void pkb_team_mon_init(
    PkbTeamMon* mon,
    uint8_t species,
    uint8_t level,
    const uint8_t moves[PKB_NUM_MOVES]);

/* Fill PP (base PP), exp and stats from species, level, moves, DVs and stat exp. */
void pkb_team_mon_calc(PkbTeamMon* mon);

/* set_builder.c: fill mon->moves with 4 distinct moves (fewer if n < 4) from
 * `pool`, picked for a sensible set (see the file comment). Uses species,
 * level and DVs (Hidden Power) of mon. */
void pkb_set_build(PkbTeamMon* mon, const uint8_t* pool, int n, PkbPolicyRng* rng);
/* A held item for a built set. *leftovers_left caps the Leftovers per team. */
uint8_t pkb_set_item(const PkbTeamMon* mon, uint8_t* leftovers_left, PkbPolicyRng* rng);

typedef enum {
    PkbTeamOk,
    PkbTeamErrIo,
    PkbTeamErrNotEnough, /* fewer than `count` species have 4 allowed moves */
} PkbTeamResult;

/*
 * Draw `count` distinct species (partial Fisher-Yates) that can exist at
 * level_max, each at a level uniform in [max(level_min, its min level),
 * level_max], with a set of 4 distinct moves legal at that level that pass
 * `filter` (NULL = all), built by pkb_set_build, and a held item
 * (pkb_set_item, at most 2 Leftovers). Species with
 * fewer than 4 passing moves are skipped and another is drawn. The team name
 * is "FLIPPER".
 */
PkbTeamResult pkb_team_generate(
    PkbTeam* team,
    uint8_t count,
    uint8_t level_min,
    uint8_t level_max,
    const PkbBlobReader* pools,
    PkbMoveFilter filter,
    void* filter_ctx,
    PkbPolicyRng* rng);

/*
 * Boss teams (gen/pb_trainers.h: Crystal's gym leaders, Elite Four, Champion
 * Lance, the final rival, Red, and Prof. Oak, plus hand-picked held items). Species, levels,
 * moves and class DVs are the game's; stat exp 0 and BASE_HAPPINESS as for
 * any trainer mon. ace_level 0 keeps the game's levels; otherwise every level
 * moves by the same amount so the highest becomes ace_level (clamped to
 * PKB_LEVEL_MIN..PKB_LEVEL_MAX). Returns false for an unknown boss.
 */
bool pkb_boss_team_build(PkbTeam* team, uint8_t boss, uint8_t ace_level, uint16_t trainer_id);
/* The level the boss's strongest mon has in the game. */
uint8_t pkb_boss_ace_level(uint8_t boss);

/* The boss list for menus (in gen/pb_trainers.h order). */
typedef enum {
    PkbBossGym,
    PkbBossEliteFour, /* Will, Koga, Bruno, Karen */
    PkbBossRival, /* the final rival battle, one per rival starter */
    PkbBossRed,
    PkbBossChampion, /* Lance */
    PkbBossOak, /* Prof. Oak */
    PkbBossCategories,
} PkbBossCategory;
uint8_t pkb_boss_count(void);
PkbBossCategory pkb_boss_category(uint8_t boss);
const char* pkb_boss_label(uint8_t boss); /* "Falkner", "Lt. Surge", "Meganium"... */

/* Names in ASCII for the UI. Item names exist only for items with a held
 * effect (others give ""). */
void pkb_species_name(uint8_t species, char* out, size_t size);
void pkb_move_name(uint8_t move, char* out, size_t size);
void pkb_item_name(uint8_t item, char* out, size_t size);
/* Species name in the Game Boy charset (not terminated); returns its length. */
size_t pkb_species_name_gb(uint8_t species, const uint8_t** name);

#ifdef __cplusplus
}
#endif
