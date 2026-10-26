// Party and game state (contract section 9). See party.h.
#ifdef NUCLEO_DEVICE
#pragma GCC optimize( \
    "no-move-loop-invariants,no-expensive-optimizations") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "party.h"
#include "assets.h"
#include "text.h"
#include "bag.h"
#include "ui_host.h"
#include "gen/reg_gen.h"
#include <string.h>

static PartyState g_ps;
static NickRow
    g_nick_c[BC_MAX_PARTY]; // the nicknames as codes of the active language (derived by names_apply)
static BcData g_data;
static bool g_loaded;
static uint32_t g_rng = 0x9E3779B9u;
PartyPending
    g_pending; // M5-C: the captured creature waiting for the PC (contract 25; the accessors are inline in party.h)

// ---- BcIo adapter over the asset files (read_at opens and closes the file in every call: no handle is kept)
static bool io_read(void* ctx, const char* file, uint32_t off, void* buf, uint32_t len) {
    (void)ctx;
    return read_at(file, off, buf, len);
}
static int32_t io_size(void* ctx, const char* file) {
    (void)ctx;
    return file_size(file);
}

void party_seed(uint32_t seed) {
    bc_rng_seed(&g_rng, seed);
}

bool party_init(uint32_t seed) {
    party_shutdown();
    party_pending_clear();
    memset(&g_ps, 0, sizeof(g_ps));
    memset(g_nick_c, 0, sizeof(g_nick_c));
    g_ps.name_player = g_ps.name_rival = PARTY_NAME_NONE;
    party_seed(seed);
    BcIo io = {io_read, io_size, NULL};
    g_loaded = ui_bc_load(&g_data, &io) ==
               BC_OK; // battle_load.c lives in the UI plugin (contract 38.1)
    return g_loaded;
}

void party_shutdown(void) {
    if(g_loaded) bc_free(&g_data);
    g_loaded = false;
    memset(&g_data, 0, sizeof(g_data));
}

bool party_chart(bool load) {
    if(!g_loaded) return false;
    if(!load) bc_chart_free(&g_data);
    return !load || bc_chart_load(&g_data);
}

void party_new_game(void) {
    party_pending_clear();
    memset(&g_ps, 0, sizeof(g_ps));
    g_ps.name_player = g_ps.name_rival = PARTY_NAME_NONE;
    g_ps.gid =
        (uint16_t)(g_rng >> 8) |
        1; // contract 81: a draw of the creature RNG (it keeps running between two new games), never 0
    bc_party_clear(&g_ps.party);
    memset(g_nick_c, 0, sizeof(g_nick_c));
    text_set_names(NULL, NULL);
}

const BcData* party_data(void) {
    return g_loaded ? &g_data : NULL;
}
PartyState* party_state(void) {
    return &g_ps;
}
Party* party_player(void) {
    return &g_ps.party;
}
uint32_t* party_rng(void) {
    return &g_rng;
}
uint8_t party_starter(void) {
    return g_ps.starter;
}
uint8_t party_rival_species(void) {
    return g_ps.rival_starter;
}

int party_give(uint8_t species, uint8_t level) {
    Party* p = &g_ps.party;
    if(!g_loaded || species == 0 || species >= g_data.n_species || p->count >= BC_MAX_PARTY)
        return -1;
    int slot = p->count;
    bc_mon_create(&g_data, &p->mon[slot], species, level, &g_rng);
    bc_friend_set(
        &p->mon[slot],
        BC_FR_GIFT); // contract 65: a starter / a gift (a creature caught in the wild keeps the default 70)
    p->count++;
    g_ps.nick_u[slot][0] = g_nick_c[slot][0] =
        0; // (a capture in a battle appends the slot itself: slots >= count are always empty)
    reg_set_caught(species);
    if(!g_ps.starter) g_ps.starter = species;
    return slot;
}

uint8_t party_rival_pick(uint8_t mine) {
    uint8_t out = 0;
    if(g_loaded && TR_RIVAL_LAB < g_data.n_trainers) {
        const TrainerDef* t = bc_trainer(&g_data, TR_RIVAL_LAB);
        for(uint8_t k = 0; k < t->n_counter && k < 3; k++)
            if(t->ctr_from[k] == mine) out = t->ctr_to[k];
    }
    if(out) g_ps.rival_starter = out;
    return out;
}

void party_heal_all(void) {
    if(!g_loaded) return;
    Party* p = &g_ps.party;
    for(uint8_t k = 0; k < p->count; k++) {
        Mon* m = &p->mon[k];
        m->hp = m->max_hp;
        m->status = 0;
        for(int s = 0; s < 4; s++) {
            uint8_t mv = m->moves[s];
            m->pp[s] =
                (uint8_t)((m->pp[s] & ~BC_PP_LEFT) |
                          bc_pp_max(
                              bc_move(&g_data, mv)->pp,
                              m->pp[s])); // 0 / unknown id: the all-zero record; contract 77: the PP Plus bits are kept and count
        }
    }
}

// ---- names (task K, contract 9.2): the Unicode names live in PartyState, their typecast (names.c) in the UI plugin
NickRow* party_nick_cache(void) {
    return g_nick_c;
}

void party_apply_names(void) {
    const uint16_t* v =
        &g_ps.name_u[0][0]; // name_u and nick_u are contiguous: any name or nickname at all?
    uint16_t n = 0;
    for(uint16_t i = 0; i < 2 * PARTY_NAME_CAP + BC_MAX_PARTY * PARTY_NICK_CAP; i++)
        n |= v[i];
    if(n)
        ui_apply_names(); // (the UI plugin: names.c)
    else
        text_set_names(NULL, NULL);
}

const tchar* party_nick(uint8_t slot) {
    return (slot < BC_MAX_PARTY && g_nick_c[slot][0]) ? g_nick_c[slot] : NULL;
}

int party_mon_name(uint8_t slot, tchar* out, int cap) {
    const tchar* n = party_nick(slot);
    int len = 0;
    if(n) {
        while(n[len] && len < cap - 1) {
            out[len] = n[len];
            len++;
        }
        out[len] = 0;
        return len;
    }
    const uint8_t sp = slot < g_ps.party.count ? g_ps.party.mon[slot].species : 0;
    len = text_load(reg_sp_txt(sp), out, cap);
    if(len < 0) out[0] = len = 0;
    return len;
}
