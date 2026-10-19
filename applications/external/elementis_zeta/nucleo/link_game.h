#pragma once
// IR LINK, the games played over the session (contract section 45): the TRADE (two-phase commit) and the BATTLE team exchange (the exhibition battle
// against a copy of the friend's team). Pure logic over a LinkSess and a LgStore (a party + names + Registro): no scenes, no globals, no furi.h, so the host tests run two
// "Flippers" with two PartyStates in one process. The scene (scene_link.c) is the only user of the real globals.
#include <stdint.h>
#include <stdbool.h>
#include "battle_core.h"
#include "party.h"
#include "link_proto.h"

// Result codes written to the script variable (VAR) by the `link` native.
enum { LR_OPEN_FAIL = 0, LR_DONE, LR_CANCEL, LR_NOPARTNER, LR_ERROR };
enum { LMODE_TRADE = 1, LMODE_BATTLE = 2 };
// Why a session ended with LR_CANCEL / LR_ERROR (for the message the scene shows)
enum { LW_NONE = 0, LW_ME, LW_PEER, LW_PEER_NO, LW_VERSION, LW_BAD_DATA, LW_LOST, LW_PROTOCOL };
// Phases the UI follows
enum { LP_CONNECT = 0, LP_PICK, LP_WAIT_OFFER, LP_REVIEW, LP_WAIT_VOTE, LP_COMMIT, LP_XCHG, LP_END };
// Application messages (LK_MT_APP + ...)
enum { LM_INFO = 0x10, LM_OFFER, LM_VOTE, LM_COMMIT, LM_DONE, LM_BYE, LM_TEAM };

#define LG_MON_BYTES 41
#define LG_NICK_CHARS (PARTY_NICK_CAP - 1)
#define LG_NAME_CHARS (PARTY_NAME_CAP - 1)
#define LG_OFFER_BYTES (LG_MON_BYTES + 2 * LG_NICK_CHARS + 2 + 2 * LG_NAME_CHARS) // 77
#define LG_INFO_BYTES (4 + 2 + 2 * LG_NAME_CHARS)                                  // 20

// What a Flipper's end of the game touches: its party (creatures + nickname rows), its names and its Registro.
typedef struct {
    const BcData* d;
    PartyState* ps;
    void (*slot_changed)(void* ctx, uint8_t slot); // after a trade replaced party slot `slot` (the nickname code cache is derived data of the scene's world)
    void* ctx;
} LgStore;

typedef struct {
    uint8_t proto, mode, n_species, n_moves;
    uint16_t tid;
    uint16_t name[PARTY_NAME_CAP]; // the player's name, END-terminated
} LgInfo;

typedef struct {
    Mon mon;
    uint16_t nick[PARTY_NICK_CAP];   // END-terminated, empty = the species name
    uint16_t ot_id;                  // trainer id of the original trainer (the trade payload carries it; the save has no field for it yet, contract 32.5 / 45)
    uint16_t ot_name[PARTY_NAME_CAP];
} LgOffer;

// The friend's team as the exhibition battle receives it (heap block, owned by the app: link_host.h LinkFoe)
typedef struct {
    Party party;
    uint16_t name[PARTY_NAME_CAP];
    uint16_t tid;
} LgTeam;

// ---- wire formats (field by field, little endian: no struct padding on the wire)
void lg_mon_pack(const Mon* m, uint8_t out[LG_MON_BYTES]);
void lg_mon_unpack(Mon* m, const uint8_t* in);
// A received creature must be a creature this game could have made: false = refuse it. lg_mon_fix then recomputes what the data decides (stats, max HP) and clamps the rest.
bool lg_mon_check(const BcData* d, const Mon* m);
void lg_mon_fix(const BcData* d, Mon* m);
void lg_mon_heal(const BcData* d, Mon* m); // full HP / PP, no status (the exhibition battle starts everybody fresh)
void lg_name_clean(uint16_t* dst, const uint16_t* src, int cap); // copies up to cap - 1 characters, control codes become '?', END-terminated
uint16_t lg_trainer_id(const PartyState* ps);
void lg_offer_make(const LgStore* st, uint8_t slot, LgOffer* o);
int lg_offer_pack(const LgOffer* o, uint8_t out[LG_OFFER_BYTES]);
bool lg_offer_unpack(const BcData* d, LgOffer* o, const uint8_t* in, uint8_t len);
uint16_t lg_offer_hash(const LgOffer* master, const LgOffer* slave);

typedef struct LinkGame {
    LinkSess* s;
    LgStore st;
    uint8_t mode, phase, result, why, need, bye_reason, my_vote, peer_vote, slot, evo_slot, evo_to, sent, got, total;
    bool have_info, local_done, committed, commit_sent, agreed;
    uint16_t hash;
    uint32_t t_phase, t_end;
    LgInfo mine_i, peer_i;
    union {
        struct {
            LgOffer mine, theirs;
            uint8_t theirs_wire[LG_OFFER_BYTES]; // the friend's offer exactly as received (the vote's hash covers the bytes that crossed, not our normalised copy)
        } tr;
        LgTeam foe;
    } u;
} LinkGame;

// Starts a game of `mode` on an (already initialised, still discovering) session.
void lg_init(LinkGame* g, LinkSess* s, const LgStore* st, uint8_t mode);
// Call after every lk_poll(). Drives the messages; sets phase / result.
void lg_step(LinkGame* g);
// UI actions. lg_pick: trade, in LP_PICK (false = not now / bad slot). lg_vote: LP_REVIEW. lg_abort: leave now (allowed until the vote is YES; later it is ignored: false).
bool lg_pick(LinkGame* g, uint8_t slot);
bool lg_vote(LinkGame* g, bool yes);
bool lg_abort(LinkGame* g);
bool lg_over(const LinkGame* g);
