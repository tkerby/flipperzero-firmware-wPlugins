// IR LINK games (contract section 45): the trade with a two-phase lg_commit and the team exchange of the exhibition battle. See link_game.h.
#include <string.h>
#include "link_game.h"
#include "gen/reg_gen.h" // ST_TRADE

enum { N_INFO = 1, N_OFFER = 2, N_VOTE = 4, N_COMMIT = 8, N_DONE = 16, N_BYE = 32, N_TEAM = 64 };
#define LG_FLUSH_MS 1500u   // how long a closing side waits for its last message to be acknowledged
#define LG_COMMIT_MS 9000u  // both said YES: the COMMIT / DONE must follow within this long, else nothing changes
#define LG_LINGER_MS 3000u  // a finished slave keeps answering this long, waiting for the master's BYE

static inline uint32_t lg_now(const LinkGame* g) {
    return g->s->ch->now_ms(g->s->ch->ctx);
}
static inline int32_t lg_tdiff(uint32_t a, uint32_t b) {
    return (int32_t)(a - b);
}

// ---------------------------------------------------------------------------------------------------------------- wire formats
static void lg_w16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static uint16_t lg_r16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

void lg_mon_pack(const Mon* m, uint8_t o[LG_MON_BYTES]) {
    o[0] = m->species;
    o[1] = m->level;
    o[2] = m->status;
    for(int i = 0; i < 4; i++) o[3 + i] = m->dv[i];
    for(int i = 0; i < 5; i++) lg_w16(o + 7 + 2 * i, m->statexp[i]);
    for(int i = 0; i < 4; i++) o[17 + i] = (uint8_t)(m->exp >> (8 * i));
    lg_w16(o + 21, m->hp);
    lg_w16(o + 23, m->max_hp);
    for(int i = 0; i < 4; i++) lg_w16(o + 25 + 2 * i, m->stat[i]);
    for(int i = 0; i < 4; i++) o[33 + i] = m->moves[i];
    for(int i = 0; i < 4; i++) o[37 + i] = m->pp[i];
}

void lg_mon_unpack(Mon* m, const uint8_t* p) {
    memset(m, 0, sizeof(*m));
    m->xf = MX_TRADED; // contract 65: a creature that arrives over the link is not the player's own (original trainer cleared); the wire carries neither xf nor the friendship: it starts at the default 70 with its new trainer
    m->species = p[0];
    m->level = p[1];
    m->status = p[2];
    for(int i = 0; i < 4; i++) m->dv[i] = p[3 + i];
    for(int i = 0; i < 5; i++) m->statexp[i] = lg_r16(p + 7 + 2 * i);
    m->exp = (uint32_t)p[17] | ((uint32_t)p[18] << 8) | ((uint32_t)p[19] << 16) | ((uint32_t)p[20] << 24);
    m->hp = lg_r16(p + 21);
    m->max_hp = lg_r16(p + 23);
    for(int i = 0; i < 4; i++) m->stat[i] = lg_r16(p + 25 + 2 * i);
    for(int i = 0; i < 4; i++) m->moves[i] = p[33 + i];
    for(int i = 0; i < 4; i++) m->pp[i] = p[37 + i];
}

bool lg_mon_check(const BcData* d, const Mon* m) {
    if(!d || m->species == 0 || m->species >= d->n_species || m->level < 1 || m->level > 100) return false;
    if(m->moves[0] == 0) return false; // every creature knows at least one move
    for(int i = 0; i < 4; i++) {
        if(m->moves[i] >= d->n_moves) return false;
        if(m->dv[i] > 15) return false;
    }
    return m->max_hp >= 1 && m->hp <= m->max_hp;
}

void lg_mon_fix(const BcData* d, Mon* m) {
    const uint8_t gr = bc_species(d, m->species)->growth;
    const uint32_t lo = bc_exp_for_level(gr, m->level), cap = bc_exp_for_level(gr, 100);
    if(m->exp < lo) m->exp = lo;
    if(m->exp > cap) m->exp = cap;
    bc_mon_recalc(d, m); // the stats come from the species / DVs / stat exp / level of THIS game's data; HP is clamped
    for(int i = 0; i < 4; i++) {
        if(!m->moves[i]) m->pp[i] = 0;
        else { // contract 77: the PP Plus count (top bits) is kept; the PP left cannot exceed what that count allows
            const uint8_t mx = bc_pp_max(bc_move(d, m->moves[i])->pp, m->pp[i]);
            if((m->pp[i] & BC_PP_LEFT) > mx) m->pp[i] = (uint8_t)((m->pp[i] & ~BC_PP_LEFT) | mx);
        }
    }
}

void lg_mon_heal(const BcData* d, Mon* m) {
    m->hp = m->max_hp;
    m->status = 0;
    for(int i = 0; i < 4; i++) m->pp[i] = m->moves[i] ? (uint8_t)((m->pp[i] & ~BC_PP_LEFT) | bc_pp_max(bc_move(d, m->moves[i])->pp, m->pp[i])) : 0;
}

void lg_name_clean(uint16_t* dst, const uint16_t* src, int cap) {
    int i = 0;
    for(; i < cap - 1 && src[i]; i++) dst[i] = src[i] < 0x20 ? (uint16_t)'?' : src[i];
    for(; i < cap; i++) dst[i] = 0;
}

uint16_t lg_trainer_id(const PartyState* ps) {
    uint8_t b[PARTY_NAME_CAP * 2 + 2];
    for(int i = 0; i < PARTY_NAME_CAP; i++) lg_w16(b + 2 * i, ps->name_u[0][i]);
    b[PARTY_NAME_CAP * 2] = ps->starter;
    b[PARTY_NAME_CAP * 2 + 1] = ps->rival_starter;
    return lk_crc16(b, sizeof(b)); // stable per save (no per-save random id exists yet: contract 32.5)
}

void lg_offer_make(const LgStore* st, uint8_t slot, LgOffer* o) {
    memset(o, 0, sizeof(*o));
    o->mon = st->ps->party.mon[slot];
    lg_name_clean(o->nick, st->ps->nick_u[slot], PARTY_NICK_CAP);
    o->ot_id = lg_trainer_id(st->ps);
    lg_name_clean(o->ot_name, st->ps->name_u[0], PARTY_NAME_CAP);
}

int lg_offer_pack(const LgOffer* o, uint8_t out[LG_OFFER_BYTES]) {
    lg_mon_pack(&o->mon, out);
    uint8_t* p = out + LG_MON_BYTES;
    for(int i = 0; i < LG_NICK_CHARS; i++) lg_w16(p + 2 * i, o->nick[i]);
    p += 2 * LG_NICK_CHARS;
    lg_w16(p, o->ot_id);
    p += 2;
    for(int i = 0; i < LG_NAME_CHARS; i++) lg_w16(p + 2 * i, o->ot_name[i]);
    return LG_OFFER_BYTES;
}

bool lg_offer_unpack(const BcData* d, LgOffer* o, const uint8_t* in, uint8_t len) {
    if(len != LG_OFFER_BYTES) return false;
    memset(o, 0, sizeof(*o));
    lg_mon_unpack(&o->mon, in);
    if(!lg_mon_check(d, &o->mon)) return false;
    lg_mon_fix(d, &o->mon);
    const uint8_t* p = in + LG_MON_BYTES;
    uint16_t raw[PARTY_NICK_CAP]; // (the larger of the two caps)
    memset(raw, 0, sizeof(raw));
    for(int i = 0; i < LG_NICK_CHARS; i++) raw[i] = lg_r16(p + 2 * i);
    raw[LG_NICK_CHARS] = 0;
    lg_name_clean(o->nick, raw, PARTY_NICK_CAP);
    p += 2 * LG_NICK_CHARS;
    o->ot_id = lg_r16(p);
    p += 2;
    memset(raw, 0, sizeof(raw));
    for(int i = 0; i < LG_NAME_CHARS; i++) raw[i] = lg_r16(p + 2 * i);
    lg_name_clean(o->ot_name, raw, PARTY_NAME_CAP);
    return true;
}

uint16_t lg_offer_hash(const LgOffer* a, const LgOffer* b) {
    uint8_t buf[2 * LG_OFFER_BYTES];
    lg_offer_pack(a, buf);
    lg_offer_pack(b, buf + LG_OFFER_BYTES);
    return lk_crc16(buf, sizeof(buf));
}

static void lg_info_pack(const LgInfo* i, uint8_t out[LG_INFO_BYTES]) {
    out[0] = i->proto;
    out[1] = i->mode;
    out[2] = i->n_species;
    out[3] = i->n_moves;
    lg_w16(out + 4, i->tid);
    for(int k = 0; k < LG_NAME_CHARS; k++) lg_w16(out + 6 + 2 * k, i->name[k]);
}
static void lg_info_unpack(LgInfo* i, const uint8_t* in) {
    memset(i, 0, sizeof(*i));
    i->proto = in[0];
    i->mode = in[1];
    i->n_species = in[2];
    i->n_moves = in[3];
    i->tid = lg_r16(in + 4);
    uint16_t raw[PARTY_NAME_CAP];
    memset(raw, 0, sizeof(raw));
    for(int k = 0; k < LG_NAME_CHARS; k++) raw[k] = lg_r16(in + 6 + 2 * k);
    lg_name_clean(i->name, raw, PARTY_NAME_CAP);
}

// ---------------------------------------------------------------------------------------------------------------- the game
void lg_init(LinkGame* g, LinkSess* s, const LgStore* st, uint8_t mode) {
    memset(g, 0, sizeof(*g));
    g->s = s;
    g->st = *st;
    g->mode = mode;
    g->phase = LP_CONNECT;
    g->slot = 0xFF;
    g->evo_slot = 0xFF;
    g->need = N_INFO;
    g->t_phase = lg_now(g);
    g->mine_i.proto = LK_PROTO_VER;
    g->mine_i.mode = mode;
    g->mine_i.n_species = st->d->n_species;
    g->mine_i.n_moves = st->d->n_moves;
    g->mine_i.tid = lg_trainer_id(st->ps);
    lg_name_clean(g->mine_i.name, st->ps->name_u[0], PARTY_NAME_CAP);
}

bool lg_over(const LinkGame* g) {
    return g->phase == LP_END;
}

// Decides the outcome. wait_flush: the last message (a BYE, a NO vote) is given a moment to reach the friend before the session is closed.
static void lg_finish(LinkGame* g, uint8_t result, uint8_t why, bool wait_flush) {
    if(g->t_end) return; // already finishing
    g->result = result;
    g->why = why;
    g->t_end = lg_now(g) + (wait_flush ? LG_FLUSH_MS : 0u);
    if(!g->t_end) g->t_end = 1;
}

static bool lg_both_offers(const LinkGame* g) {
    return g->mode == LMODE_TRADE && g->slot != 0xFF && g->u.tr.theirs.mon.species != 0;
}

static void lg_compute_hash(LinkGame* g) { // over the two offers as they crossed the wire, the master's first (both sides get the same number)
    uint8_t buf[2 * LG_OFFER_BYTES], mine[LG_OFFER_BYTES];
    lg_offer_pack(&g->u.tr.mine, mine);
    const bool m = lk_is_master(g->s);
    memcpy(buf, m ? mine : g->u.tr.theirs_wire, LG_OFFER_BYTES);
    memcpy(buf + LG_OFFER_BYTES, m ? g->u.tr.theirs_wire : mine, LG_OFFER_BYTES);
    g->hash = lk_crc16(buf, sizeof(buf));
}

// THE commit: replaces our creature by the friend's in the same slot, in one local step that cannot fail halfway. Called by the master when the slave's DONE arrives and by the slave
// when the master's COMMIT arrives (contract 45.4).
static bool lg_commit(LinkGame* g) {
    PartyState* ps = g->st.ps;
    const uint8_t k = g->slot;
    LgOffer* t = &g->u.tr.theirs;
    if(g->committed) return true;
    if(k >= ps->party.count || memcmp(&ps->party.mon[k], &g->u.tr.mine.mon, sizeof(Mon)) != 0) return false; // our party changed under the trade: refuse
    ps->party.mon[k] = t->mon;
    memcpy(ps->nick_u[k], t->nick, sizeof(ps->nick_u[k]));
    ps->nick_u[k][PARTY_NICK_CAP - 1] = 0;
    const uint8_t sp = t->mon.species;
    ps->reg_seen[sp >> 3] |= (uint8_t)(1u << (sp & 7));
    ps->reg_caught[sp >> 3] |= (uint8_t)(1u << (sp & 7));
    const uint8_t to = bc_stone_evo(g->st.d, sp, ST_TRADE); // a trade evolution (species.toml evolve_trade): the scene's native opens the evolution scene afterwards
    if(to && to < g->st.d->n_species) {
        g->evo_slot = k;
        g->evo_to = to;
    }
    g->committed = true;
    if(g->st.slot_changed) g->st.slot_changed(g->st.ctx, k);
    return true;
}

static void lg_bail(LinkGame* g, uint8_t why) { // bad data / version: tell the friend (BYE reason = the LW_* code, >= 2), end with an error
    g->bye_reason = why;
    g->need = N_BYE;
    lg_finish(g, LR_ERROR, why, true);
}

static void lg_on_message(LinkGame* g, uint8_t type, const uint8_t* p, uint8_t len) {
    LinkSess* s = g->s;
    const BcData* d = g->st.d;
    if(g->t_end) return; // finishing: nothing else matters
    switch(type) {
    case LM_INFO:
        if(len != LG_INFO_BYTES) return lg_bail(g, LW_PROTOCOL);
        lg_info_unpack(&g->peer_i, p);
        if(g->peer_i.proto != LK_PROTO_VER || g->peer_i.mode != g->mode || g->peer_i.n_species != d->n_species || g->peer_i.n_moves != d->n_moves) return lg_bail(g, LW_VERSION);
        g->have_info = true;
        if(g->mode == LMODE_BATTLE) g->need |= N_TEAM;
        break;
    case LM_OFFER:
        if(g->mode != LMODE_TRADE || !g->have_info || g->u.tr.theirs.mon.species) return lg_bail(g, LW_PROTOCOL);
        if(!lg_offer_unpack(d, &g->u.tr.theirs, p, len)) return lg_bail(g, LW_BAD_DATA);
        memcpy(g->u.tr.theirs_wire, p, LG_OFFER_BYTES);
        if(g->slot != 0xFF) lg_compute_hash(g);
        break;
    case LM_VOTE:
        if(g->mode != LMODE_TRADE || len != 3 || !lg_both_offers(g)) return lg_bail(g, LW_PROTOCOL);
        lg_compute_hash(g);
        if(p[0] == 1 && lg_r16(p + 1) != g->hash) return lg_bail(g, LW_PROTOCOL); // the friend voted on other offers than the ones we hold
        g->peer_vote = p[0] == 1 ? 1 : 2;
        if(g->peer_vote == 2) lg_finish(g, LR_CANCEL, LW_PEER_NO, false);
        break;
    case LM_COMMIT: // slave: the master decided (both voted YES): apply, then tell the master
        if(g->mode != LMODE_TRADE || lk_is_master(s) || len != 2 || g->my_vote != 1 || g->peer_vote != 1 || lg_r16(p) != g->hash) return lg_bail(g, LW_PROTOCOL);
        if(!lg_commit(g)) return lg_bail(g, LW_PROTOCOL);
        g->need |= N_DONE;
        g->local_done = true;
        g->t_phase = lg_now(g);
        break;
    case LM_DONE: // master: the slave has applied it: so do we
        if(g->mode != LMODE_TRADE || !lk_is_master(s) || len != 2 || lg_r16(p) != g->hash || !g->commit_sent) return lg_bail(g, LW_PROTOCOL);
        if(!lg_commit(g)) return lg_bail(g, LW_PROTOCOL);
        g->local_done = true;
        g->bye_reason = 0;
        g->need |= N_BYE;
        break;
    case LM_TEAM:
        if(g->mode != LMODE_BATTLE || !g->have_info || len < 3) return lg_bail(g, LW_PROTOCOL);
        {
            const uint8_t total = p[0], idx = p[1], k = p[2];
            if(total < 1 || total > BC_MAX_PARTY || k < 1 || idx != g->got || idx + k > total || len != 3 + k * LG_MON_BYTES) return lg_bail(g, LW_PROTOCOL);
            if(g->total && g->total != total) return lg_bail(g, LW_PROTOCOL);
            g->total = total;
            for(uint8_t i = 0; i < k; i++) {
                Mon* m = &g->u.foe.party.mon[idx + i];
                lg_mon_unpack(m, p + 3 + i * LG_MON_BYTES);
                if(!lg_mon_check(d, m)) return lg_bail(g, LW_BAD_DATA);
                lg_mon_fix(d, m);
                lg_mon_heal(d, m); // the exhibition starts everybody fresh
            }
            g->got = (uint8_t)(g->got + k);
            g->u.foe.party.count = g->got;
        }
        break;
    case LM_BYE:
        if(len != 1) return lg_bail(g, LW_PROTOCOL);
        if(p[0] == 0) {
            if(g->local_done) lg_finish(g, LR_DONE, LW_NONE, true);
            else lg_finish(g, LR_ERROR, LW_PROTOCOL, false);
        } else if(p[0] == 1) {
            lg_finish(g, LR_CANCEL, LW_PEER, false);
        } else {
            lg_finish(g, LR_ERROR, p[0] == LW_VERSION ? LW_VERSION : LW_PROTOCOL, false); // the friend refused us: different versions, or it did not like our data
        }
        break;
    default: lg_bail(g, LW_PROTOCOL); break;
    }
}

// One queued message goes out when the session can take it (they leave in a fixed order, which is also the order the friend expects them).
static void lg_send_one(LinkGame* g) {
    LinkSess* s = g->s;
    if(!g->need || !lk_can_send(s)) return;
    uint8_t buf[LK_MAX_PAYLOAD];
    if(g->need & N_INFO) {
        lg_info_pack(&g->mine_i, buf);
        if(lk_send(s, LM_INFO, buf, LG_INFO_BYTES)) g->need &= (uint8_t)~N_INFO;
    } else if(g->need & N_OFFER) {
        lg_offer_pack(&g->u.tr.mine, buf);
        if(lk_send(s, LM_OFFER, buf, LG_OFFER_BYTES)) g->need &= (uint8_t)~N_OFFER;
    } else if(g->need & N_VOTE) {
        buf[0] = g->my_vote;
        lg_w16(buf + 1, g->hash);
        if(lk_send(s, LM_VOTE, buf, 3)) g->need &= (uint8_t)~N_VOTE;
    } else if(g->need & N_COMMIT) {
        lg_w16(buf, g->hash);
        if(lk_send(s, LM_COMMIT, buf, 2)) g->need &= (uint8_t)~N_COMMIT;
    } else if(g->need & N_DONE) {
        lg_w16(buf, g->hash);
        if(lk_send(s, LM_DONE, buf, 2)) g->need &= (uint8_t)~N_DONE;
    } else if(g->need & N_TEAM) {
        const Party* p = &g->st.ps->party;
        const uint8_t k = (uint8_t)(p->count - g->sent < 2 ? p->count - g->sent : 2);
        buf[0] = p->count;
        buf[1] = g->sent;
        buf[2] = k;
        for(uint8_t i = 0; i < k; i++) lg_mon_pack(&p->mon[g->sent + i], buf + 3 + i * LG_MON_BYTES);
        if(lk_send(s, LM_TEAM, buf, (uint8_t)(3 + k * LG_MON_BYTES))) {
            g->sent = (uint8_t)(g->sent + k);
            if(g->sent >= p->count) g->need &= (uint8_t)~N_TEAM;
        }
    } else if(g->need & N_BYE) {
        buf[0] = g->bye_reason;
        if(lk_send(s, LM_BYE, buf, 1)) g->need &= (uint8_t)~N_BYE;
    }
}

bool lg_pick(LinkGame* g, uint8_t slot) {
    if(g->mode != LMODE_TRADE || g->phase != LP_PICK || slot >= g->st.ps->party.count || g->slot != 0xFF) return false;
    g->slot = slot;
    lg_offer_make(&g->st, slot, &g->u.tr.mine);
    g->need |= N_OFFER;
    if(g->u.tr.theirs.mon.species) lg_compute_hash(g);
    return true;
}

bool lg_vote(LinkGame* g, bool yes) {
    if(g->phase != LP_REVIEW || g->my_vote) return false;
    lg_compute_hash(g);
    g->my_vote = yes ? 1 : 2;
    g->need |= N_VOTE;
    g->t_phase = lg_now(g);
    return true;
}

bool lg_abort(LinkGame* g) {
    if(g->phase == LP_END || g->t_end || g->my_vote == 1 || g->local_done || g->committed) return false; // after a YES vote the trade runs to its end
    g->bye_reason = 1;
    g->need = N_BYE;
    lg_finish(g, LR_CANCEL, LW_ME, true);
    return true;
}

static uint8_t lg_phase_of(const LinkGame* g) {
    if(!g->have_info) return LP_CONNECT;
    if(g->mode == LMODE_BATTLE) return LP_XCHG;
    if(g->slot == 0xFF) return LP_PICK;
    if(!g->u.tr.theirs.mon.species) return LP_WAIT_OFFER;
    if(!g->my_vote) return LP_REVIEW;
    if(g->my_vote == 1 && g->peer_vote == 1) return LP_COMMIT;
    return LP_WAIT_VOTE;
}

void lg_step(LinkGame* g) {
    if(g->phase == LP_END) return;
    LinkSess* s = g->s;
    const uint32_t now = lg_now(g);
    uint8_t type = 0, len = 0, buf[LK_MAX_PAYLOAD];
    // 1. the friend's messages
    while(!g->t_end && lk_recv(s, &type, buf, &len)) lg_on_message(g, type, buf, len);
    // 2. the session died under us
    if(!g->t_end && (lk_state(s) == LS_FAILED || lk_state(s) == LS_CLOSED)) {
        if(g->committed || g->local_done) lg_finish(g, LR_DONE, LW_NONE, false); // we had applied the trade (the slave) / everything was in (the battle team): it is done
        else if(!lk_established(s) && !g->have_info && s->fail == LF_NOPARTNER) lg_finish(g, LR_NOPARTNER, LW_NONE, false);
        else lg_finish(g, LR_ERROR, s->fail == LF_VERSION ? LW_VERSION : LW_LOST, false);
    }
    // 3. what the game needs next
    if(!g->t_end) {
        if(g->mode == LMODE_TRADE) {
            if(g->my_vote == 2) lg_finish(g, LR_CANCEL, LW_ME, true); // the NO vote leaves first
            else if(g->my_vote == 1 && g->peer_vote == 1) {
                if(!g->agreed) { // both said YES: from here on the trade runs to its end (two-phase lg_commit, phase 2)
                    g->agreed = true;
                    g->t_phase = now;
                }
                if(lk_is_master(s) && !g->commit_sent) { // the master decides
                    g->commit_sent = true;
                    g->need |= N_COMMIT;
                }
                if(!g->local_done && lg_tdiff(now, g->t_phase) > (int32_t)LG_COMMIT_MS) lg_finish(g, LR_ERROR, LW_LOST, false); // the decision never came / never confirmed: nothing was changed
            }
            if(g->local_done && !lk_is_master(s) && lg_tdiff(now, g->t_phase) > (int32_t)LG_LINGER_MS && lk_flushed(s)) lg_finish(g, LR_DONE, LW_NONE, false);
        } else if(g->have_info && !g->local_done && g->total && g->got == g->total && g->sent >= g->st.ps->party.count && !(g->need & N_TEAM) && lk_flushed(s)) {
            g->local_done = true; // the whole teams crossed
            g->t_phase = now;
            if(lk_is_master(s)) {
                g->bye_reason = 0;
                g->need |= N_BYE;
            }
        } else if(g->mode == LMODE_BATTLE && g->local_done && !lk_is_master(s) && lg_tdiff(now, g->t_phase) > (int32_t)LG_LINGER_MS && lk_flushed(s)) {
            lg_finish(g, LR_DONE, LW_NONE, false);
        }
    }
    if(!g->t_end && g->local_done && lk_is_master(s)) lg_finish(g, LR_DONE, LW_NONE, true); // the master leaves once its BYE (queued with local_done) is acknowledged
    lg_send_one(g);
    // 4. the phase for the screen, and the end
    if(!g->t_end) {
        g->phase = lg_phase_of(g);
    } else if(lk_flushed(s) && !g->need) {
        g->phase = LP_END;
        lk_close(s);
    } else if(lg_tdiff(now, g->t_end) >= 0 || lk_state(s) == LS_FAILED || lk_state(s) == LS_CLOSED) {
        g->phase = LP_END;
        lk_close(s);
    }
}
