/* Session (slave) against master_sim (a model of the clock-master GB). */
#include "pkb_test.h"
#include "../sim/master_sim.h"
#include <pkb/party_block.h>
#include <pkb/policy_rng.h>
#include <pkb/session.h>
#include "pb_const.h"

#define MAX_ACTIONS 64

typedef struct {
    PblSession s;
    MasterSim m;
    /* slave "engine": posts action i once result i-1 is in and
     * post_delay[i] frames have passed */
    uint8_t actions[MAX_ACTIONS];
    uint32_t post_delay[MAX_ACTIONS];
    uint32_t n_actions;
    uint32_t next_post;
    uint32_t waited;
    uint32_t events;
} Rig;

static void rig_notify(void* ctx, uint32_t ev) {
    Rig* r = ctx;
    r->events |= ev;
    /* the engine starts a fresh battle when the blocks are in */
    if(ev & PBL_EV_BLOCKS_DONE) {
        r->next_post = 0;
        r->waited = 0;
    }
}

static void rig_on_frame(MasterSim* m, void* ctx) {
    Rig* r = ctx;
    (void)m;
    if(atomic_load(&r->s.phase) != PblPhaseBattle || r->s.auto_action != PBL_ACTION_NONE) return;
    uint32_t i = r->next_post;
    uint8_t gb;
    if(i >= r->n_actions) return;
    if(i > 0 && !pbl_battle_exchange_result(&r->s, i - 1, &gb)) return;
    if(r->waited++ < r->post_delay[i]) return;
    pbl_battle_post_action(&r->s, r->actions[i]);
    r->next_post++;
    r->waited = 0;
}

static void make_team(PkbTeam* t, const uint8_t species[6], uint16_t id) {
    static const uint8_t moves[4] = {TACKLE, POUND, SCRATCH, EMBER};
    t->count = 6;
    t->trainer_id = id;
    for(int i = 0; i < 6; i++)
        pkb_team_mon_init(&t->mon[i], species[i], 50, moves);
}

static void build_tx(
    const PkbTeam* t,
    const char* name,
    const uint8_t rns[10],
    uint8_t rn[17],
    uint8_t party[450],
    uint8_t patch[200]) {
    pkb_rn_block_build(rns, rn);
    CHECK(pkb_party_block_build(t, name, party, patch));
}

/* A master party full of bytes the link treats specially. */
static void make_nasty_master_team(PkbTeam* t) {
    static const uint8_t species[6] = {CHANSEY, BLISSEY, SNORLAX, WOBBUFFET, LAPRAS, MEW};
    make_team(t, species, 0xFEFE);
    for(int i = 0; i < 6; i++) {
        t->mon[i].exp = 0xFEFDFE;
        t->mon[i].max_hp = 0x00FE;
        for(int s = 0; s < 5; s++)
            t->mon[i].stats[s] = (uint16_t)(s & 1 ? 0xFEFF : 0xFDFE);
        t->mon[i].pp[0] = 0xFE & 0x3F;
    }
}

static void rig_init(Rig* r) {
    memset(r, 0, sizeof(*r));
    r->s.notify = rig_notify;
    r->s.notify_ctx = r;
    r->s.auto_action = PBL_ACTION_NONE;
    pbl_start(&r->s);
    ms_init(&r->m, &r->s);
    r->m.on_frame = rig_on_frame;
    r->m.hook_ctx = r;
}

static void slave_set_team(Rig* r, const PkbTeam* t) {
    uint8_t rn[17], party[450], patch[200];
    static const uint8_t rns[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}; /* ignored by the master */
    build_tx(t, "FLIPPER", rns, rn, party, patch);
    pbl_set_tx(&r->s, rn, party, patch);
}

static void check_party_matches(const uint8_t* rx, const uint8_t* patch_rx, const PkbTeam* t) {
    PkbLinkPartyData d;
    CHECK(pkb_party_block_parse(rx, PKB_PARTY_BLOCK_LEN, patch_rx, PKB_PATCH_BLOCK_LEN, &d));
    CHECK_EQ(d.count, t->count);
    CHECK_EQ((d.player_id[0] << 8) | d.player_id[1], t->trainer_id);
    for(int i = 0; i < t->count; i++) {
        uint8_t want[48];
        pkb_party_struct_build(&t->mon[i], t->trainer_id, want);
        CHECK_MEM_EQ(d.mons[i], want, 48);
    }
}

/* One console round: blocks, then the given battle exchanges. */
static void play_round(
    Rig* r,
    const PkbTeam* master_team,
    const PkbTeam* slave_team,
    const uint8_t master_rns[10],
    const uint8_t* gb_actions,
    const uint32_t* gb_gaps,
    uint32_t n) {
    uint8_t m_rn[17], m_party[450], m_patch[200];
    uint8_t rx_rn[17], rx_party[450], rx_patch[200];

    build_tx(master_team, "GOLD", master_rns, m_rn, m_party, m_patch);
    CHECK(ms_console(&r->m, m_rn, m_party, m_patch, rx_rn, rx_party, rx_patch));
    if(r->m.error) printf("master error: %s\n", r->m.error);
    /* The master's last patch byte only reaches us in its next transfer
     * (the first battle exchange), so we may still be in XB_PATCH here. */
    PblPhase ph = atomic_load(&r->s.phase);
    CHECK(ph == PblPhaseXbPatch || ph == PblPhaseBattle);

    /* master got our party */
    check_party_matches(rx_party, rx_patch, slave_team);

    for(uint32_t i = 0; i < n; i++) {
        ms_delay_frames(&r->m, gb_gaps[i]);
        uint8_t got = ms_battle_exchange(&r->m, gb_actions[i]);
        uint8_t want = r->s.auto_action != PBL_ACTION_NONE ? r->s.auto_action : r->actions[i];
        CHECK_EQ(got, want);
        uint8_t gb = 0xFF;
        /* let the slave finish its repeats */
        for(int f = 0; f < 3 && !pbl_battle_exchange_result(&r->s, i, &gb); f++)
            ms_delay_frames(&r->m, 1);
        CHECK(pbl_battle_exchange_result(&r->s, i, &gb));
        CHECK_EQ(gb, gb_actions[i]);
        if(r->m.error) {
            printf("master error at exchange %u: %s\n", i, r->m.error);
            return;
        }
    }
    if(n) {
        /* we got the master's party and RNs */
        CHECK_EQ(atomic_load(&r->s.phase), PblPhaseBattle);
        check_party_matches(r->s.party_rx, r->s.patch_rx, master_team);
        uint8_t rns[10];
        CHECK(pbl_master_rns(&r->s, rns));
        CHECK_MEM_EQ(rns, master_rns, 10);
    }
    /* result screen, then ExitLinkCommunications */
    ms_delay_frames(&r->m, 240);
    ms_exit_link_communications(&r->m);
    ms_delay_frames(&r->m, 60);
    CHECK(pbl_battle_end_hint(&r->s, (uint32_t)(r->m.now_us / 1000)));
}

static void test_full_session_and_rematch(void) {
    static Rig r;
    static const uint8_t slave_species[6] = {
        PIKACHU, BULBASAUR, CHARMANDER, SQUIRTLE, EEVEE, ONIX};
    static const uint8_t master_rns[10] = {
        0x12, 0xFC, 0x00, 0x7F, 0x80, 0x85, 0xD2, 0x61, 0x01, 0x02};
    PkbTeam slave, master;

    rig_init(&r);
    make_team(&slave, slave_species, 0x1234);
    make_nasty_master_team(&master);
    slave_set_team(&r, &slave);

    CHECK(ms_receptionist(&r.m, 90));
    if(r.m.error) printf("master error: %s\n", r.m.error);
    CHECK_EQ(atomic_load(&r.s.phase), PblPhaseRoom);

    static const uint8_t gb_actions[] = {0, 1, 4, 2, 5, 3, 0x0F};
    static const uint32_t gb_gaps[] = {0, 120, 300, 0, 0, 45, 200};
    static const uint8_t mine[] = {3, 2, 1, 0, 6, 0x0E, 0x0D};
    static const uint32_t delays[] = {0, 0, 400, 3, 0, 90, 1};
    r.n_actions = 7;
    memcpy(r.actions, mine, sizeof(mine));
    memcpy(r.post_delay, delays, sizeof(delays));
    play_round(&r, &master, &slave, master_rns, gb_actions, gb_gaps, 7);

    /* rematch at the console with a fresh team */
    static const uint8_t slave2[6] = {MEWTWO, CELEBI, LUGIA, HO_OH, MEW, SUICUNE};
    make_team(&slave, slave2, 0x4321);
    slave_set_team(&r, &slave);
    r.events = 0;
    static const uint8_t master_rns2[10] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    play_round(&r, &master, &slave, master_rns2, gb_actions, gb_gaps, 3);
    CHECK(r.events & PBL_EV_BATTLE_END);
    CHECK_EQ(atomic_load(&r.s.battles), 1);

    /* leave the Colosseum, come back through the receptionist */
    ms_wait_for_other_player_to_exit(&r.m);
    ms_delay_frames(&r.m, 600);
    CHECK(ms_receptionist(&r.m, 10));
    CHECK_EQ(atomic_load(&r.s.phase), PblPhaseRoom);
}

static void test_auto_action_smoke_mode(void) {
    static Rig r;
    static const uint8_t one[6] = {PIKACHU, PIKACHU, PIKACHU, PIKACHU, PIKACHU, PIKACHU};
    static const uint8_t rns[10] = {0};
    PkbTeam slave, master;

    rig_init(&r);
    r.s.auto_action = 0;
    make_team(&slave, one, 1);
    slave.count = 1;
    make_nasty_master_team(&master);
    slave_set_team(&r, &slave);
    CHECK(ms_receptionist(&r.m, 0));
    static const uint8_t gb_actions[] = {0, 0, 5, 1, 2, 4};
    static const uint32_t gaps[] = {10, 0, 0, 400, 1, 2};
    play_round(&r, &master, &slave, rns, gb_actions, gaps, 6);
}

static void test_fuzz_timings(void) {
    static Rig r;
    PkbPolicyRng rng;
    static const uint8_t sp[6] = {GENGAR, ALAKAZAM, MACHAMP, GOLEM, JOLTEON, ARCANINE};
    PkbTeam slave, master;

    pkb_policy_rng_seed(&rng, 99);
    for(int run = 0; run < 150; run++) {
        rig_init(&r);
        make_team(&slave, sp, (uint16_t)pkb_policy_rng_next(&rng));
        make_nasty_master_team(&master);
        master.trainer_id = (uint16_t)pkb_policy_rng_next(&rng);
        slave_set_team(&r, &slave);
        CHECK(ms_receptionist(&r.m, pkb_policy_rng_below(&rng, 600)));

        uint8_t gb_actions[24], rns[10];
        uint32_t gaps[24];
        uint32_t n = 1 + pkb_policy_rng_below(&rng, 24);
        for(int i = 0; i < 10; i++)
            rns[i] = (uint8_t)pkb_policy_rng_below(&rng, 0xFD);
        for(uint32_t i = 0; i < n; i++) {
            static const uint32_t gap_choices[] = {0, 0, 1, 2, 17, 60, 300, 900};
            static const uint32_t delay_choices[] = {0, 0, 1, 2, 9, 11, 25, 80, 700};
            gb_actions[i] = (uint8_t)pkb_policy_rng_below(&rng, 16);
            gaps[i] = gap_choices[pkb_policy_rng_below(&rng, 8)];
            r.actions[i] = (uint8_t)pkb_policy_rng_below(&rng, 16);
            r.post_delay[i] = delay_choices[pkb_policy_rng_below(&rng, 9)];
        }
        r.n_actions = n;
        int before = pkb_test_failures;
        play_round(&r, &master, &slave, rns, gb_actions, gaps, n);
        if(pkb_test_failures != before) {
            printf("fuzz run %d failed\n", run);
            break;
        }
    }
}

static void test_link_trace_shape(void) {
    /* The very first bytes: handshake answered with $02, then $61 echoed. */
    static Rig r;
    static uint8_t trace[1 << 16];
    PkbTeam slave;
    static const uint8_t sp[6] = {PIKACHU, BULBASAUR, CHARMANDER, SQUIRTLE, EEVEE, ONIX};
    rig_init(&r);
    r.m.trace = trace;
    r.m.trace_cap = sizeof(trace);
    make_team(&slave, sp, 7);
    slave_set_team(&r, &slave);
    CHECK(ms_receptionist(&r.m, 30));
    bool saw_handshake = false, saw_61 = false, saw_d2 = false;
    for(uint32_t i = 0; i + 1 < r.m.trace_len; i += 2) {
        if(trace[i] == 0x01 && trace[i + 1] == 0x02) saw_handshake = true;
        if(trace[i] == 0x61 && trace[i + 1] == 0x61) saw_61 = true;
        if(trace[i] == 0xD2 && trace[i + 1] == 0xD2) saw_d2 = true;
    }
    CHECK(saw_handshake);
    CHECK(saw_61);
    CHECK(saw_d2);
}

int main(void) {
    RUN(test_link_trace_shape);
    RUN(test_full_session_and_rematch);
    RUN(test_auto_action_smoke_mode);
    RUN(test_fuzz_timings);
    return TEST_REPORT();
}
