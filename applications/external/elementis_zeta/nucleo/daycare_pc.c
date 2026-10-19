// Day care (C3, contract section 33), PC plugin side: deposit, withdraw (the lazy level-ups), the levels gained and the price. The resident part (daycare.c) counts the steps:
// the creature in care is a Mon as it was DEPOSITED whose `exp` has grown by one per step; here its level is derived from that exp and settled when it is withdrawn.
// no evolution in care (the species is never touched), level 100 is the cap, a 5th move replaces the oldest (no question). The party <-> box moves of box.c are reused:
// a day-care creature is a BoxRec (the Mon and its Unicode nickname) and they keep the nickname code cache in step. No UI-plugin call is possible from here (one plugin at a time).
#include "pc_host.h"
#include "box.h"
#include "party.h"

// The level the exp in care has reached (the species' cap applied), never below the deposited level; 0 when empty or the data is not loaded.
static uint8_t level_now(const Mon* m) {
    const BcData* d = party_data();
    if(!m->species || !d) return 0;
    const uint8_t gr = bc_species(d, m->species)->growth;
    uint8_t l = m->level;
    while(l < 100 && m->exp >= bc_exp_for_level(gr, (uint8_t)(l + 1))) l++;
    return l;
}

static uint32_t gained(void) {
    const Mon* m = &party_state()->dc.mon;
    const uint8_t l = level_now(m);
    return l ? (uint32_t)(l - m->level) : 0;
}

static bool deposit(uint8_t slot) {
    DayCare* dc = &party_state()->dc;
    Party* p = party_player();
    if(dc->mon.species || slot >= p->count || p->count < 2) return false; // care is full / no such creature / the LAST creature stays
    BoxRec r;
    box_from_party(&r, slot);
    box_party_drop(slot); // the later ones move up; the code cache of the nicknames follows
    dc->mon = r.mon;
    memcpy(dc->nick_u, r.nick, sizeof(dc->nick_u));
    return true;
}

static bool withdraw(void) {
    DayCare* dc = &party_state()->dc;
    const BcData* d = party_data();
    Mon* m = &dc->mon;
    if(!m->species || !d || party_player()->count >= BC_MAX_PARTY) return false;
    const SpeciesDef* s = bc_species(d, m->species); // (valid through the lookups below: every caller holds at most two species records)
    const uint32_t cap = bc_exp_for_level(s->growth, 100);
    if(m->exp > cap) m->exp = cap;
    const uint8_t to = level_now(m);
    while(m->level < to) { // the level-ups the steps earned, one by one like a battle's award_exp: HP rises by the max-HP delta, the level's moves are learned
        const uint16_t old_max = m->max_hp;
        m->level++;
        bc_mon_recalc(d, m);
        if(m->hp) m->hp = (uint16_t)(m->hp + m->max_hp - old_max);
        for(uint8_t i = 0; i < s->n_learn; i++) {
            if(s->learn_lv[i] != m->level || bc_learn_move(d, m, s->learn_mv[i], NULL) != BC_LEARN_FULL) continue;
            for(int k = 0; k < 3; k++) m->moves[k] = m->moves[k + 1], m->pp[k] = m->pp[k + 1]; // 4 known: the OLDEST goes, no question asked
            bc_move_set(d, m, 3, s->learn_mv[i]);
        }
    }
    BoxRec r;
    r.mon = *m;
    memcpy(r.nick, dc->nick_u, sizeof(r.nick));
    box_to_party(&r);
    memset(dc, 0, sizeof(*dc));
    return true;
}

uint32_t daycare_pc_op(uint8_t op, uint8_t arg) {
    switch(op) {
    case DC_DEPOSIT: return deposit(arg);
    case DC_WITHDRAW: return withdraw();
    case DC_GAINED: return gained();
    case DC_PRICE: return party_state()->dc.mon.species ? 100u + 100u * gained() : 0;
    default: return op >= DC_FEAT ? world_feat((uint8_t)(op - DC_FEAT), arg) : 0; // C5 (contract 42): the world features ride on this call (no new plugin member)
    }
}
