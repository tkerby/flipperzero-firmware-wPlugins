#include "driver.h"

#include <string.h>

#define MAP_GROUP_CABLE_CLUB 20
#define MAP_POKECENTER_2F    1
#define MAP_COLOSSEUM        3
/* Console seats (maps/Colosseum.asm): x 3 faces right, x 6 faces left. */
#define SEAT_LEFT_X          3
#define SEAT_RIGHT_X         6
#define SEAT_Y               4
#define USING_INTERNAL_CLOCK 2
#define PARTY_MON_LEN        48
#define MON_HP               34 /* MON_HP offset in party_struct */

#define ACTION_SWITCH1 4
#define ACTION_SWITCH6 9
#define ACTION_FORFEIT 15 /* BATTLEACTION_FORFEIT: RUN in a link battle */

static bool is_switch(uint8_t n) {
    return n >= ACTION_SWITCH1 && n <= ACTION_SWITCH6;
}

static uint8_t random_move(GbDriver* d) {
    uint8_t usable[4];
    unsigned n = 0;
    uint8_t disabled = gbh_rd(d->h, "wDisabledMove", 0);
    for(unsigned i = 0; i < 4; i++) {
        uint8_t move = gbh_rd(d->h, "wBattleMonMoves", i);
        if(move && (gbh_rd(d->h, "wBattleMonPP", i) & 0x3F) && move != disabled)
            usable[n++] = (uint8_t)i;
    }
    return n ? usable[pkb_policy_rng_below(&d->rng, n)] : 0;
}

static uint8_t random_switch(GbDriver* d) {
    uint8_t ok[6];
    unsigned n = 0;
    uint8_t count = gbh_rd(d->h, "wPartyCount", 0);
    uint8_t active = gbh_rd(d->h, "wCurBattleMon", 0);
    for(uint8_t i = 0; i < count && i < 6; i++) {
        unsigned hp = gbh_rd16be(d->h, "wPartyMon1HP", i * PARTY_MON_LEN);
        if(hp && i != active) ok[n++] = i;
    }
    return (uint8_t)(ACTION_SWITCH1 + (n ? ok[pkb_policy_rng_below(&d->rng, n)] : 0));
}

static bool can_switch(GbDriver* d) {
    if(gbh_rd(d->h, "wPlayerWrapCount", 0)) return false;
    if(gbh_rd(d->h, "wEnemySubStatus5", 0) & 0x80) return false; /* SUBSTATUS_CANT_RUN */
    uint8_t count = gbh_rd(d->h, "wPartyCount", 0), active = gbh_rd(d->h, "wCurBattleMon", 0);
    for(uint8_t i = 0; i < count && i < 6; i++) {
        if(i != active && gbh_rd16be(d->h, "wPartyMon1HP", i * PARTY_MON_LEN)) return true;
    }
    return false;
}

/* The action for the upcoming exchange (stable across the menus of one turn). */
static uint8_t plan(GbDriver* d) {
    if(d->planned_for != (int64_t)d->exchanges) {
        d->planned_for = d->exchanges;
        if(d->battles == 0 && d->exchanges < d->p->script_len) {
            d->planned = d->p->script[d->exchanges];
        } else if(
            d->switch_one_in && pkb_policy_rng_below(&d->rng, d->switch_one_in) == 0 &&
            can_switch(d)) {
            d->planned = random_switch(d);
        } else {
            d->planned = random_move(d);
        }
    }
    return d->planned;
}

static void hook_battle_menu(Gbh* h, void* ctx) {
    GbDriver* d = ctx;
    uint8_t p = plan(d);
    gbh_wr(
        h,
        "wBattleMenuCursorPosition",
        0,
        p == ACTION_FORFEIT ? 4 :
        is_switch(p)        ? 2 :
                              1); /* RUN, PKMN, FIGHT */
}

static void hook_move_select(Gbh* h, void* ctx) {
    GbDriver* d = ctx;
    uint8_t p = plan(d);
    if(gbh_rd(h, "wMoveSelectionMenuType", 0) == 0 && p < 4) gbh_wr(h, "wCurMoveNum", 0, p);
}

static void hook_party_menu(Gbh* h, void* ctx) {
    GbDriver* d = ctx;
    if(!gbh_rd(h, "wBattleMode", 0)) return;
    uint8_t p = plan(d);
    if(!is_switch(p)) d->planned = p = random_switch(d); /* forced replacement */
    gbh_wr(h, "wPartyMenuCursor", 0, (uint8_t)(p - ACTION_SWITCH1 + 1));
}

static void hook_exchange(Gbh* h, void* ctx) {
    GbDriver* d = ctx;
    (void)h;
    d->exchanges++;
}

static void hook_prep_party(Gbh* h, void* ctx) {
    GbDriver* d = ctx;
    bool master = gbh_rd(h, "hSerialConnectionStatus", 0) == USING_INTERNAL_CLOCK;
    DrvProfile* prof = &d->prof[master ? DRV_AS_MASTER : DRV_AS_SLAVE];
    d->p = prof;
    const PkbLinkPartyData* p = &prof->party;
    if(prof->make_party) prof->make_party(prof->make_party_ctx, &prof->party);
    if(!prof->inject_party) return;
    if(d->p->inject_trainer) {
        for(unsigned i = 0; i < sizeof(p->player_name); i++)
            gbh_wr(h, "wPlayerName", i, p->player_name[i]);
        gbh_wr(h, "wPlayerID", 0, p->player_id[0]);
        gbh_wr(h, "wPlayerID", 1, p->player_id[1]);
    }
    gbh_wr(h, "wPartyCount", 0, p->count);
    for(unsigned i = 0; i < sizeof(p->species); i++)
        gbh_wr(h, "wPartySpecies", i, p->species[i]);
    for(unsigned i = 0; i < sizeof(p->mons); i++)
        gbh_wr(h, "wPartyMons", i, ((const uint8_t*)p->mons)[i]);
    for(unsigned i = 0; i < sizeof(p->ot_names); i++)
        gbh_wr(h, "wPartyMonOTs", i, ((const uint8_t*)p->ot_names)[i]);
    for(unsigned i = 0; i < sizeof(p->nicknames); i++) {
        gbh_wr(h, "wPartyMonNicknames", i, ((const uint8_t*)p->nicknames)[i]);
    }
}

static void hook_rns(Gbh* h, void* ctx) {
    GbDriver* d = ctx;
    if(!d->p->inject_rns) return;
    for(unsigned i = 0; i < PKB_RNS_LEN; i++)
        gbh_wr(h, "wLinkBattleRNs", i, d->p->rns[i]);
}

/* Walk from the door (4, 7) to our seat and use the console. The clock
 * master sits left (CableClubCheckWhichChris shows the friend on the right).
 * Directions are tapped, one step per tap, so we never overshoot. */
static uint8_t colosseum_keys(Gbh* h, uint32_t t) {
    uint8_t x = gbh_rd(h, "wXCoord", 0);
    uint8_t y = gbh_rd(h, "wYCoord", 0);
    bool master = gbh_rd(h, "hSerialConnectionStatus", 0) == USING_INTERNAL_CLOCK;
    uint8_t seat = master ? SEAT_LEFT_X : SEAT_RIGHT_X;
    uint32_t phase = t % 24;
    uint8_t dir;
    if(y > SEAT_Y + 1)
        dir = GBH_KEY_UP;
    else if(x != seat)
        dir = x < seat ? GBH_KEY_RIGHT : GBH_KEY_LEFT;
    else if(y > SEAT_Y)
        dir = GBH_KEY_UP;
    else {
        /* seated: face the table, then A */
        if(phase < 2) return master ? GBH_KEY_RIGHT : GBH_KEY_LEFT;
        return phase >= 12 && phase < 14 ? GBH_KEY_A : 0;
    }
    return phase < 2 ? dir : 0;
}

void drv_init(GbDriver* d, Gbh* h, int battles, uint64_t seed) {
    memset(d, 0, sizeof(*d));
    d->h = h;
    d->target_battles = battles;
    d->planned_for = -1;
    d->p = &d->prof[DRV_AS_MASTER];
    pkb_policy_rng_seed(&d->rng, seed);
    gbh_hook(h, "BattleMenu", hook_battle_menu, d);
    /* past the Struggle check: with no usable moves the game returns before
     * the cursor, and a player has no way to change wCurMoveNum */
    gbh_hook(h, "MoveSelectionScreen.got_menu_type", hook_move_select, d);
    gbh_hook(h, "InitPartyMenuWithCancel", hook_party_menu, d);
    gbh_hook(h, "InitPartyMenuNoCancel", hook_party_menu, d);
    gbh_hook(h, "LinkBattleSendReceiveAction", hook_exchange, d);
    gbh_hook(h, "Link_PrepPartyData_Gen2", hook_prep_party, d);
    gbh_hook(h, "Gen2ToGen2LinkComms.player_1", hook_rns, d);
}

void drv_frame(GbDriver* d) {
    Gbh* h = d->h;
    uint8_t keys = 0;
    uint32_t t = d->tick++;
    bool pulse_a = t % 6 < 2;

    if(t < (uint32_t)d->start_delay) {
        gbh_set_keys(h, 0);
        return;
    }
    bool battle = gbh_rd(h, "wBattleMode", 0) != 0;
    if(battle && !d->in_battle) {
        d->in_battle = true;
        d->exchanges = 0;
        d->planned_for = -1;
    } else if(!battle && d->in_battle) {
        d->in_battle = false;
        d->battles++;
    }
    bool colosseum = gbh_rd(h, "wMapGroup", 0) == MAP_GROUP_CABLE_CLUB &&
                     gbh_rd(h, "wMapNumber", 0) == MAP_COLOSSEUM;

    if(battle) {
        keys = pulse_a ? GBH_KEY_A : 0;
    } else if(colosseum) {
        if(d->battles >= d->target_battles) {
            d->done = ++d->idle > 120;
        } else {
            keys = colosseum_keys(h, t);
        }
    } else if(d->battles >= d->target_battles && d->target_battles > 0) {
        d->done = ++d->idle > 120;
    } else {
        keys = pulse_a ? GBH_KEY_A : 0;
    }
    gbh_set_keys(h, keys);
}
