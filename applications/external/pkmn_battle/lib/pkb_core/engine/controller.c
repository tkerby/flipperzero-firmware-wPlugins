/* Choice helpers for engine controllers (random policies and tests). */
#include <pkb/engine.h>
#include <pkb/policy_rng.h>

#include "pb_const.h"
#include "pb_wram.h"

#define P(a) pkb_engine_peek(E, (uint16_t)(a))

/* MoveSelectionScreen refuses moves without PP and the Disabled one */
static bool move_usable(const PkbEngine* E, uint8_t slot) {
    if(!P(wBattleMonMoves + slot)) return false;
    if(!(P(wBattleMonPP + slot) & PP_MASK)) return false;
    uint8_t dis = P(wPlayerDisableCount);
    return !dis || (uint8_t)(dis >> 4) != slot + 1;
}

void pkb_engine_random_move(const PkbEngine* E, PkbPolicyRng* rng, PkbChoice* c) {
    uint8_t usable[NUM_MOVES];
    unsigned n = 0;
    for(uint8_t i = 0; i < NUM_MOVES; i++) {
        if(move_usable(E, i)) usable[n++] = i;
    }
    c->switch_mon = false;
    /* none usable: the engine falls back to Struggle by itself */
    c->slot = n ? usable[pkb_policy_rng_below(rng, n)] : 0;
}

static bool fit(const PkbEngine* E, uint8_t i) {
    return pkb_engine_peek16(E, (uint16_t)(wPartyMon1HP + PARTYMON_STRUCT_LENGTH * i)) != 0;
}

static unsigned others_fit(const PkbEngine* E, uint8_t out[PARTY_LENGTH]) {
    unsigned n = 0;
    for(uint8_t i = 0; i < P(wPartyCount) && i < PARTY_LENGTH; i++) {
        if(i != P(wCurBattleMon) && fit(E, i)) out[n++] = i;
    }
    return n;
}

/* after a faint (the mon out has no HP) or for Baton Pass (it has) */
uint8_t pkb_engine_random_replacement(const PkbEngine* E, PkbPolicyRng* rng) {
    uint8_t ok[PARTY_LENGTH];
    unsigned n = others_fit(E, ok);
    return n ? ok[pkb_policy_rng_below(rng, n)] : 0;
}

bool pkb_engine_can_switch(const PkbEngine* E) {
    uint8_t ok[PARTY_LENGTH];
    if(P(wPlayerWrapCount)) return false;
    if(P(wEnemySubStatus5) & (1 << SUBSTATUS_CANT_RUN)) return false;
    return others_fit(E, ok) > 0;
}

void pkb_engine_random_switch(const PkbEngine* E, PkbPolicyRng* rng, PkbChoice* c) {
    uint8_t ok[PARTY_LENGTH];
    unsigned n = others_fit(E, ok);
    c->switch_mon = n > 0;
    c->slot = n ? ok[pkb_policy_rng_below(rng, n)] : 0;
}

uint8_t pkb_engine_active_mon(const PkbEngine* E, bool enemy, uint16_t* hp, uint16_t* max_hp) {
    uint16_t mon = enemy ? wEnemyMon : wBattleMon;
    *hp = pkb_engine_peek16(E, (uint16_t)(mon + (wBattleMonHP - wBattleMon)));
    *max_hp = pkb_engine_peek16(E, (uint16_t)(mon + (wBattleMonMaxHP - wBattleMon)));
    return P(mon);
}

void pkb_engine_side_view(const PkbEngine* E, bool enemy, PkbSideView* v) {
    v->species = pkb_engine_active_mon(E, enemy, &v->hp, &v->max_hp);
    v->status = P(enemy ? wEnemyMonStatus : wBattleMonStatus);
    v->last_move = P(enemy ? wLastEnemyMove : wLastPlayerMove);
    v->count = P(enemy ? wOTPartyCount : wPartyCount);
    if(v->count > PARTY_LENGTH) v->count = PARTY_LENGTH;
    /* the party structs' HP lags behind the battle mon's */
    uint16_t hp = enemy ? wOTPartyMon1HP : wPartyMon1HP;
    uint8_t cur = P(enemy ? wCurOTMon : wCurBattleMon);
    v->alive = 0;
    for(uint8_t i = 0; i < v->count; i++) {
        bool fit = i == cur && v->species ? v->hp != 0 :
                                            pkb_engine_peek16(E, (uint16_t)(hp + PARTYMON_STRUCT_LENGTH * i)) != 0;
        if(fit) v->alive |= (uint8_t)(1 << i);
    }
}

const char* pkb_status_abbrev(uint8_t status) {
    if(status & SLP_MASK) return "SLP";
    if(status & (1 << PSN)) return "PSN";
    if(status & (1 << BRN)) return "BRN";
    if(status & (1 << FRZ)) return "FRZ";
    if(status & (1 << PAR)) return "PAR";
    return "";
}
