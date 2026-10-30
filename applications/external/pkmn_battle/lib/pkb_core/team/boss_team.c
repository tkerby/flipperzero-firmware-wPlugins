#include <string.h>
#include <pkb/team.h>
#include "pb_tables.h"
#include "pb_trainers.h"

_Static_assert(
    (int)PkbBossGym == (int)PB_BOSS_GYM && (int)PkbBossEliteFour == (int)PB_BOSS_E4 &&
        (int)PkbBossRival == (int)PB_BOSS_RIVAL && (int)PkbBossRed == (int)PB_BOSS_RED &&
        (int)PkbBossChampion == (int)PB_BOSS_CHAMPION &&
        (int)PkbBossOak == (int)PB_BOSS_OAK &&
        (int)PkbBossCategories == (int)PB_NUM_BOSS_CATEGORIES,
    "boss categories");

uint8_t pkb_boss_count(void) {
    return PB_NUM_BOSSES;
}

PkbBossCategory pkb_boss_category(uint8_t boss) {
    return boss < PB_NUM_BOSSES ? (PkbBossCategory)pb_bosses[boss].category : PkbBossCategories;
}

const char* pkb_boss_label(uint8_t boss) {
    return boss < PB_NUM_BOSSES ? pb_bosses[boss].label : "";
}

uint8_t pkb_boss_ace_level(uint8_t boss) {
    if(boss >= PB_NUM_BOSSES) return 0;
    const PbBoss* b = &pb_bosses[boss];
    uint8_t ace = 0;
    for(int i = 0; i < b->count; i++)
        if(b->mon[i].level > ace) ace = b->mon[i].level;
    return ace;
}

bool pkb_boss_team_build(PkbTeam* team, uint8_t boss, uint8_t ace_level, uint16_t trainer_id) {
    if(boss >= PB_NUM_BOSSES) return false;
    const PbBoss* b = &pb_bosses[boss];
    int shift = ace_level ? ace_level - pkb_boss_ace_level(boss) : 0;

    memset(team, 0, sizeof(*team));
    strncpy(team->name, b->name, sizeof(team->name) - 1);
    team->trainer_id = trainer_id;
    team->count = b->count;
    for(int i = 0; i < b->count; i++) {
        const PbBossMon* src = &b->mon[i];
        PkbTeamMon* m = &team->mon[i];
        int level = src->level + shift;
        if(level < PKB_LEVEL_MIN) level = PKB_LEVEL_MIN;
        if(level > PKB_LEVEL_MAX) level = PKB_LEVEL_MAX;
        m->species = src->species;
        m->level = (uint8_t)level;
        m->item = src->item;
        memcpy(m->moves, src->moves, sizeof(m->moves));
        m->dvs[0] = b->dvs[0];
        m->dvs[1] = b->dvs[1];
        m->stat_exp = 0;
        m->happiness = BASE_HAPPINESS;
        pkb_team_mon_calc(m);
    }
    return true;
}
