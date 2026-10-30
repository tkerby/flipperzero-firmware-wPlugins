#include <string.h>
#include <pkb/team.h>
#include <pkb/charset.h>
#include "pb_tables.h"

PkbTeamResult pkb_team_generate(
    PkbTeam* team,
    uint8_t count,
    uint8_t level_min,
    uint8_t level_max,
    const PkbBlobReader* pools,
    PkbMoveFilter filter,
    void* filter_ctx,
    PkbPolicyRng* rng) {
    uint8_t species[NUM_POKEMON];
    uint8_t pool[PKB_MOVEPOOL_MAX];
    uint32_t n = NUM_POKEMON;
    uint8_t leftovers_left = 2;

    if(count < 1 || count > PKB_PARTY_LENGTH) return PkbTeamErrNotEnough;
    if(level_max > PKB_LEVEL_MAX) level_max = PKB_LEVEL_MAX;
    if(level_min < PKB_LEVEL_MIN) level_min = PKB_LEVEL_MIN;
    if(level_min > level_max) level_min = level_max;
    for(uint32_t s = 0; s < n; s++) species[s] = (uint8_t)(s + 1);

    memset(team, 0, sizeof(*team));
    strcpy(team->name, "FLIPPER");
    team->trainer_id = (uint16_t)pkb_policy_rng_next(rng);

    for(uint32_t i = 0; i < n && team->count < count; i++) {
        uint32_t j = i + pkb_policy_rng_below(rng, n - i);
        uint8_t tmp = species[i];
        species[i] = species[j];
        species[j] = tmp;

        PkbSpeciesInfo info;
        int pool_len = pkb_movepool_read(pools, species[i], level_max, pool, &info);
        if(pool_len < 0) return PkbTeamErrIo;
        if(pool_len == 0) continue; /* cannot exist at level_max */
        uint8_t lo = info.min_level > level_min ? info.min_level : level_min;
        uint8_t level = (uint8_t)(lo + pkb_policy_rng_below(rng, level_max - lo + 1u));
        /* legal but odd: a mon far past the level it would have evolved at */
        if(info.evolve_level && level > info.evolve_level + PKB_NFE_LEVEL_SLACK) continue;
        if(level < level_max) {
            pool_len = pkb_movepool_read(pools, species[i], level, pool, NULL);
            if(pool_len < 0) return PkbTeamErrIo;
        }

        uint32_t usable = 0;
        for(int k = 0; k < pool_len; k++) {
            if(!filter || filter(filter_ctx, pool[k])) pool[usable++] = pool[k];
        }
        if(usable < PKB_NUM_MOVES) continue;

        static const uint8_t no_moves[PKB_NUM_MOVES] = {0};
        PkbTeamMon* mon = &team->mon[team->count++];
        pkb_team_mon_init(mon, species[i], level, no_moves);
        pkb_set_build(mon, pool, (int)usable, rng);
        mon->item = pkb_set_item(mon, &leftovers_left, rng);
        pkb_team_mon_calc(mon);
    }
    return team->count == count ? PkbTeamOk : PkbTeamErrNotEnough;
}

size_t pkb_species_name_gb(uint8_t species, const uint8_t** name) {
    if(species < 1 || species > NUM_POKEMON) {
        *name = NULL;
        return 0;
    }
    uint16_t a = pb_species_name_off[species - 1], b = pb_species_name_off[species];
    *name = &pb_species_names[a];
    return b - a;
}

void pkb_species_name(uint8_t species, char* out, size_t size) {
    const uint8_t* gb;
    size_t len = pkb_species_name_gb(species, &gb);
    if(!gb) {
        if(size) out[0] = '\0';
        return;
    }
    pkb_gb_decode(gb, len, out, size);
}

void pkb_move_name(uint8_t move, char* out, size_t size) {
    if(move < 1 || move > NUM_ATTACKS) {
        if(size) out[0] = '\0';
        return;
    }
    uint16_t a = pb_move_name_off[move - 1], b = pb_move_name_off[move];
    pkb_gb_decode(&pb_move_names[a], b - a, out, size);
}

void pkb_item_name(uint8_t item, char* out, size_t size) {
    if(!item) {
        if(size) out[0] = '\0';
        return;
    }
    uint16_t a = pb_item_name_off[item - 1], b = pb_item_name_off[item];
    pkb_gb_decode(&pb_item_names[a], b - a, out, size);
}
