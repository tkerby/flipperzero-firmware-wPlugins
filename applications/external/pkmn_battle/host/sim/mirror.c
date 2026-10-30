#include "mirror.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pb_const.h"
#include "pb_wram.h"

typedef struct {
    uint16_t ours, theirs, len;
    const char* name;
} MirrorField;

/* each pair is compared both ways (ours->theirs and theirs->ours) */
static const MirrorField pairs[] = {
    {wBattleMon, wEnemyMon, wBattleMonStructEnd - wBattleMon, "battle mon"},
    {wPlayerSubStatus1, wEnemySubStatus1, 5, "substatus"},
    {wPlayerRolloutCount,
     wEnemyRolloutCount,
     wPlayerProtectCount + 1 - wPlayerRolloutCount,
     "counters"},
    {wPlayerDamageTaken, wEnemyDamageTaken, 2, "damage taken"},
    {wPlayerStats, wEnemyStats, 2 * NUM_BATTLE_STATS, "unboosted stats"},
    {wPlayerStatLevels, wEnemyStatLevels, NUM_LEVEL_STATS, "stat levels"},
    {wPlayerTurnsTaken, wEnemyTurnsTaken, 1, "turns taken"},
    {wPlayerSubstituteHP, wEnemySubstituteHP, 1, "substitute HP"},
    {wDisabledMove, wEnemyDisabledMove, 1, "disabled move"},
    {wLastPlayerCounterMove, wLastEnemyCounterMove, 1, "last counter move"},
    {wPlayerMinimized, wEnemyMinimized, 1, "minimized"},
    {wPlayerScreens, wEnemyScreens, 1, "screens"},
    {wPlayerSafeguardCount, wEnemySafeguardCount, 3, "screen counts"},
    {wLastPlayerMove, wLastEnemyMove, 1, "last move"},
    {wPlayerFutureSightCount, wEnemyFutureSightCount, 1, "future sight count"},
    {wPlayerFutureSightDamage, wEnemyFutureSightDamage, 2, "future sight damage"},
    {wPlayerRageCounter, wEnemyRageCounter, 1, "rage counter"},
    {wPlayerTrappingMove, wEnemyTrappingMove, 1, "trapping move"},
    {wPlayerWrapCount, wEnemyWrapCount, 1, "wrap count"},
    {wPlayerCharging, wEnemyCharging, 1, "charging"},
    {wCurBattleMon, wCurOTMon, 1, "current mon"},
    {wPartyCount, wOTPartyCount, 1, "party count"},
};

/* compared as they are */
static const MirrorField shared[] = {
    {wLinkBattleRNs, wLinkBattleRNs, 10, "random numbers"},
    {wLinkBattleRNCount, wLinkBattleRNCount, 1, "random count"},
    {wBattleWeather, wBattleWeather, 2, "weather"},
};

static bool skipped(uint16_t addr_ours, uint16_t base) {
    (void)addr_ours;
    (void)base;
    return false;
}

/* A party against the other side's copy of it. Not compared:
 * - happiness: ChangeHappiness only updates the fainting side's own party;
 * - status and HP of the active mon: a Game Boy copies its own battle mon
 *   into its party every turn (UpdateBattleMonInParty) but the other side's
 *   only when it switches or faints, so that copy lags until then (the
 *   battle mons themselves are compared);
 * - status of a fainted mon: its own side clears it, the other side's
 *   copy only gets HP 0 (UpdateBattleStateAndExperienceAfterEnemyFaint). */
static unsigned cmp_party(
    const uint8_t* own,
    const uint8_t* copy,
    uint16_t own_base,
    uint16_t copy_base,
    uint8_t active,
    char* report,
    size_t size,
    unsigned* lines,
    unsigned max_lines) {
    unsigned n = 0;
    for(uint16_t i = 0; i < PARTY_LENGTH * PARTYMON_STRUCT_LENGTH; i++) {
        uint16_t field = i % PARTYMON_STRUCT_LENGTH;
        if(field == MON_HAPPINESS) continue;
        uint16_t mon = (uint16_t)(own_base + i - field);
        bool fainted = !(own[mon + MON_HP - 0xC000] | own[mon + MON_HP + 1 - 0xC000]);
        if(i / PARTYMON_STRUCT_LENGTH == active && field >= MON_STATUS && field < MON_MAXHP)
            continue;
        if(fainted && field == MON_STATUS) continue;
        uint8_t x = own[own_base + i - 0xC000], y = copy[copy_base + i - 0xC000];
        if(x == y) continue;
        n++;
        if(report && *lines < max_lines) {
            size_t used = strlen(report);
            snprintf(
                report + used,
                size - used,
                "  party mon %u+%u ($%04X/$%04X): $%02X vs $%02X\n",
                i / PARTYMON_STRUCT_LENGTH,
                field,
                own_base + i,
                copy_base + i,
                x,
                y);
            (*lines)++;
        }
    }
    return n;
}

static unsigned cmp_field(
    const uint8_t* ours,
    const uint8_t* theirs,
    uint16_t a,
    uint16_t b,
    uint16_t len,
    const char* name,
    char* report,
    size_t size,
    unsigned* lines,
    unsigned max_lines) {
    unsigned n = 0;
    for(uint16_t i = 0; i < len; i++) {
        if(skipped((uint16_t)(a + i), a)) continue;
        uint8_t x = ours[a + i - 0xC000], y = theirs[b + i - 0xC000];
        if(x == y) continue;
        n++;
        if(report && *lines < max_lines) {
            size_t used = strlen(report);
            snprintf(
                report + used,
                size - used,
                "  %s+%u ($%04X/$%04X): ours $%02X theirs $%02X\n",
                name,
                i,
                a + i,
                b + i,
                x,
                y);
            (*lines)++;
        }
    }
    return n;
}

unsigned mirror_compare(
    const uint8_t* ours,
    const uint8_t* theirs,
    char* report,
    size_t report_size,
    unsigned max_lines) {
    unsigned n = 0, lines = 0;
    if(report && report_size) report[0] = 0;
    for(size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        const MirrorField* f = &pairs[i];
        n += cmp_field(
            ours,
            theirs,
            f->ours,
            f->theirs,
            f->len,
            f->name,
            report,
            report_size,
            &lines,
            max_lines);
        n += cmp_field(
            ours,
            theirs,
            f->theirs,
            f->ours,
            f->len,
            f->name,
            report,
            report_size,
            &lines,
            max_lines);
    }
    n += cmp_party(
        ours,
        theirs,
        wPartyMons,
        wOTPartyMons,
        ours[wCurBattleMon - 0xC000],
        report,
        report_size,
        &lines,
        max_lines);
    n += cmp_party(
        theirs,
        ours,
        wPartyMons,
        wOTPartyMons,
        theirs[wCurBattleMon - 0xC000],
        report,
        report_size,
        &lines,
        max_lines);
    for(size_t i = 0; i < sizeof(shared) / sizeof(shared[0]); i++) {
        const MirrorField* f = &shared[i];
        n += cmp_field(
            ours,
            theirs,
            f->ours,
            f->theirs,
            f->len,
            f->name,
            report,
            report_size,
            &lines,
            max_lines);
    }
    return n;
}
