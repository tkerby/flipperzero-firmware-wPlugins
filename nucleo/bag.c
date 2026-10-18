// Bag, money and Registro bits over PartyState (bag.h, contract section 14).
#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-tree-loop-optimize") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "bag.h"
#include "party.h"
#include "gen/reg_gen.h"

static PartyState* ps(void) {
    return party_state();
}

uint8_t bag_count(uint8_t item) {
    const PartyState* s = ps();
    for(uint8_t i = 0; i < s->n_bag; i++)
        if(s->bag[i].item == item) return s->bag[i].count;
    return 0;
}

uint8_t bag_add(uint8_t item, uint8_t n) {
    PartyState* s = ps();
    if(!item) return 0;
    uint8_t i = 0;
    while(i < s->n_bag && s->bag[i].item != item) i++;
    if(i == s->n_bag) {
        if(i >= BAG_MAX_KINDS) return 0;
        s->bag[i].item = item;
        s->bag[i].count = 0;
        s->n_bag++;
    }
    uint8_t room = (uint8_t)(BAG_MAX_COUNT - s->bag[i].count);
    if(n > room) n = room;
    s->bag[i].count = (uint8_t)(s->bag[i].count + n);
    if(!s->bag[i].count) { // nothing was added to a fresh slot (n == 0)
        s->n_bag--;
        s->bag[i].item = 0;
    }
    return n;
}

bool bag_take(uint8_t item) {
    PartyState* s = ps();
    for(uint8_t i = 0; i < s->n_bag; i++) {
        if(s->bag[i].item != item) continue;
        if(--s->bag[i].count == 0) {
            for(uint8_t k = i; k + 1 < s->n_bag; k++) s->bag[k] = s->bag[k + 1];
            s->n_bag--;
            s->bag[s->n_bag].item = 0;
        }
        return true;
    }
    return false;
}

uint8_t bag_kinds(void) {
    return ps()->n_bag;
}

bool bag_get(uint8_t i, uint8_t* item, uint8_t* count) {
    const PartyState* s = ps();
    if(i >= s->n_bag) return false;
    *item = s->bag[i].item;
    *count = s->bag[i].count;
    return true;
}

// ---- Registro
void reg_set_seen(uint8_t sp) {
    if(sp) ps()->reg_seen[sp >> 3] |= (uint8_t)(1u << (sp & 7));
}
void reg_set_caught(uint8_t sp) {
    if(!sp) return;
    ps()->reg_caught[sp >> 3] |= (uint8_t)(1u << (sp & 7));
    reg_set_seen(sp);
}
bool reg_seen(uint8_t sp) {
    return sp && (ps()->reg_seen[sp >> 3] >> (sp & 7)) & 1;
}
bool reg_caught(uint8_t sp) {
    return sp && (ps()->reg_caught[sp >> 3] >> (sp & 7)) & 1;
}
static uint8_t count_bits(const uint8_t* a) {
    uint8_t n = 0;
    for(int i = 0; i < REG_BYTES; i++)
        for(uint8_t v = a[i]; v; v &= (uint8_t)(v - 1)) n++;
    return n; // at most 255: species 0 is never set
}
uint8_t reg_caught_count(void) {
    return count_bits(ps()->reg_caught);
}
uint8_t reg_seen_count(void) {
    return count_bits(ps()->reg_seen);
}

bool bag_debug_give(void) {
    uint8_t a = bag_add(IT_CRYSTAL, 5);
    uint8_t b = bag_add(IT_SALVE, 2);
    return a || b;
}

// ---------------------------------------------------------------- money
uint32_t money_get(void) {
    return party_state()->money;
}

uint32_t money_add(uint32_t n) {
    PartyState* s = party_state();
    uint32_t room = s->money >= MONEY_MAX ? 0 : MONEY_MAX - s->money;
    if(n > room) n = room;
    s->money += n;
    return n;
}

bool money_take(uint32_t n) {
    PartyState* s = party_state();
    if(s->money < n) return false;
    s->money -= n;
    return true;
}

uint32_t money_lose_half(void) {
    PartyState* s = party_state();
    uint32_t lost = s->money / 2;
    s->money -= lost;
    return lost;
}
