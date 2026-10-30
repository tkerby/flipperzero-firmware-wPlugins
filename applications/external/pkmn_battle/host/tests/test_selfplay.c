/*
 * Self-play: two engines, one as the clock master and one as the slave,
 * battle each other with random parties and random choices. At the start
 * of every turn and at the end their states must mirror each other
 * (host/sim/mirror.c), they must make the same exchanges and agree on the
 * result. The slave side is checked against the real game by the SameBoy
 * harness; this keeps both roles consistent without a ROM.
 *
 *   test_selfplay [battles] [seed] [first]
 *
 * first skips to that battle number (the parties and choices are the same as
 * in a full run) to reproduce one failure; with battles = first + 1 it also
 * prints every turn and exchange of that battle. PKB_SP_PROGRESS=1 prints
 * each battle number (to find one that aborts). PKB_SP_NO_ITEMS=1 gives the
 * parties no held items (the battles the seeds gave before held items were added). PKB_SP_BEAT_UP=1
 * adds Beat Up to the parties' move pool.
 *
 * PKB_SP_AI=1 lets the Flipper's AI (pkb_ai) play side 0 against the
 * random policy; the mirror checks then also prove the AI never changes the
 * engine state. PKB_SP_TEAMS=lo-hi gives both sides generated random-battle
 * teams at levels lo-hi instead of fuzzed parties. PKB_SP_AI_MIN=n fails
 * unless side 0 wins at least n% of the battles.
 */
#include <pthread.h>
#include <stdlib.h>

#include "fuzz_party.h"
#include "mirror.h"
#include "pkb_test.h"
#include <pkb/ai.h>
#include <pkb/engine.h>
#include "host_io.h"
#include "pb_const.h"
#include "pb_wram.h"

#define MAX_EXCHANGES 4096

static uint32_t battles = 10000;
static uint64_t seed = 1;
static uint32_t first;
static bool verbose; /* one battle: print every turn */
static bool ai_side0;
static uint8_t team_lo, team_hi; /* PKB_SP_TEAMS; 0 = fuzzed parties */
static HostBlob pools_blob;
static unsigned side0_wins, side0_battles;

static uint16_t rd16(const PkbEngine* E, uint16_t a) {
    return pkb_engine_peek16(E, a);
}

typedef struct Match Match;

/* What a side is blocked on, so that a battle whose sides went out of step
 * (one in an exchange, the other at the start of a turn) ends instead of
 * hanging. */
typedef enum {
    WaitNone,
    WaitExchange,
    WaitTurn,
    WaitCompare,
} Wait;

typedef struct {
    PkbEngine E;
    PkbEngineIo io;
    Match* m;
    int side;
    PkbPolicyRng rng;
    PkbAi ai;
    PkbBattleResult result;
} Side;

struct Match {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    uint8_t sent[2][MAX_EXCHANGES];
    uint32_t nsent[2];
    uint32_t turns[2];
    uint32_t compared;
    bool done[2];
    Wait wait[2];
    uint32_t wait_for[2];
    bool stuck; /* both sides were blocked on each other */
    unsigned divergences;
    char report[2048];
    Side side[2];
};

static bool ready(const Match* m, int side) {
    int o = !side;
    uint32_t n = m->wait_for[side];
    switch(m->wait[side]) {
    case WaitExchange:
        return m->nsent[o] > n || m->done[o];
    case WaitTurn:
        return m->turns[o] >= n || m->done[o];
    case WaitCompare:
        return m->compared >= n || m->done[o];
    default:
        return true;
    }
}

/* with m->mu held: waits until w is done for n; false if the sides are stuck */
static bool block(Match* m, int side, Wait w, uint32_t n) {
    m->wait[side] = w;
    m->wait_for[side] = n;
    while(!ready(m, side) && !m->stuck) {
        if(!ready(m, !side)) {
            m->stuck = true;
            pthread_cond_broadcast(&m->cv);
            break;
        }
        pthread_cond_wait(&m->cv, &m->mu);
    }
    m->wait[side] = WaitNone;
    return !m->stuck;
}

static bool sp_exchange(void* ctx, uint8_t mine, uint8_t* theirs) {
    Side* s = ctx;
    Match* m = s->m;
    int o = !s->side;
    pthread_mutex_lock(&m->mu);
    uint32_t k = m->nsent[s->side];
    if(k >= MAX_EXCHANGES) {
        pthread_mutex_unlock(&m->mu);
        return false;
    }
    m->sent[s->side][k] = mine;
    m->nsent[s->side]++;
    pthread_cond_broadcast(&m->cv);
    block(m, s->side, WaitExchange, k);
    bool ok = m->nsent[o] > k;
    if(ok) *theirs = m->sent[o][k];
    if(ok && verbose)
        fprintf(stderr, "exchange %u side %d: sent $%02X got $%02X\n", k, s->side, mine, *theirs);
    pthread_mutex_unlock(&m->mu);
    return ok;
}

static void sp_choose(void* ctx, const PkbEngine* E, PkbChoice* c) {
    Side* s = ctx;
    if(pkb_policy_rng_below(&s->rng, 6) == 0 && pkb_engine_can_switch(E))
        pkb_engine_random_switch(E, &s->rng, c);
    else
        pkb_engine_random_move(E, &s->rng, c);
}

static uint8_t sp_replace(void* ctx, const PkbEngine* E) {
    Side* s = ctx;
    return pkb_engine_random_replacement(E, &s->rng);
}

/* side 0 with the AI: same ctx (the Side), so sp_exchange keeps working */
static void sp_ai_choose(void* ctx, const PkbEngine* E, PkbChoice* c) {
    pkb_ai_choose_action(&((Side*)ctx)->ai, E, c);
}

static uint8_t sp_ai_replace(void* ctx, const PkbEngine* E) {
    return pkb_ai_choose_replacement(&((Side*)ctx)->ai, E);
}

static void sp_on_turn(void* ctx, const PkbEngine* E) {
    (void)E;
    Side* s = ctx;
    Match* m = s->m;
    int o = !s->side;
    pthread_mutex_lock(&m->mu);
    uint32_t t = ++m->turns[s->side];
    pthread_cond_broadcast(&m->cv);
    if(block(m, s->side, WaitTurn, t) && m->turns[o] >= t) {
        if(s->side == 0) {
            unsigned n = mirror_compare(
                m->side[0].E.wram,
                m->side[1].E.wram,
                m->divergences ? NULL : m->report,
                sizeof(m->report),
                12);
            m->divergences += n;
            m->compared = t;
            if(verbose) {
                for(int i = 0; i < 2; i++) {
                    const PkbEngine* E = &m->side[i].E;
                    printf(
                        "%sturn %u side %d: mon %3u hp %3u last %3u sub %02X %02X %02X %02X %02X | enemy %3u hp %3u "
                        "last %3u sub %02X %02X %02X %02X %02X\n",
                        E->game_desync ? "D " : "",
                        t,
                        i,
                        E->wram[wBattleMonSpecies - 0xC000],
                        rd16(E, wBattleMonHP),
                        E->wram[wLastPlayerMove - 0xC000],
                        E->wram[wPlayerSubStatus1 - 0xC000],
                        E->wram[wPlayerSubStatus2 - 0xC000],
                        E->wram[wPlayerSubStatus3 - 0xC000],
                        E->wram[wPlayerSubStatus4 - 0xC000],
                        E->wram[wPlayerSubStatus5 - 0xC000],
                        E->wram[wEnemyMonSpecies - 0xC000],
                        rd16(E, wEnemyMonHP),
                        E->wram[wLastEnemyMove - 0xC000],
                        E->wram[wEnemySubStatus1 - 0xC000],
                        E->wram[wEnemySubStatus2 - 0xC000],
                        E->wram[wEnemySubStatus3 - 0xC000],
                        E->wram[wEnemySubStatus4 - 0xC000],
                        E->wram[wEnemySubStatus5 - 0xC000]);
                }
                if(n) {
                    char r[2048];
                    mirror_compare(m->side[0].E.wram, m->side[1].E.wram, r, sizeof(r), 40);
                    printf("%s", r);
                }
            }
            pthread_cond_broadcast(&m->cv);
        } else {
            block(m, s->side, WaitCompare, t);
        }
    }
    pthread_mutex_unlock(&m->mu);
}

static void* side_thread(void* arg) {
    Side* s = arg;
    s->result = pkb_engine_run(&s->E, &s->io);
    pthread_mutex_lock(&s->m->mu);
    s->m->done[s->side] = true;
    pthread_cond_broadcast(&s->m->cv);
    pthread_mutex_unlock(&s->m->mu);
    return NULL;
}

static const PkbBattleResult mirrored[] = {
    [PkbBattleUnknown] = PkbBattleUnknown,
    [PkbBattleWin] = PkbBattleLose,
    [PkbBattleLose] = PkbBattleWin,
    [PkbBattleDraw] = PkbBattleDraw,
    [PkbBattleAborted] = PkbBattleAborted,
};

/* returns the number of exchanges, -1 if the sides diverged, or -2 if a game
 * bug made them disagree (PKB_ENGINE_GAME_DESYNC: two real Game Boys would
 * desync the same way) */
static int
    play(Match* m, PkbPolicyRng* rng, uint32_t battle, unsigned* unsupported, unsigned* beat_up) {
    PkbLinkPartyData party[2];
    uint8_t rns[PKB_RNS_LEN];
    if(team_lo) {
        PkbBlobReader pools = host_movepools(&pools_blob);
        for(int i = 0; i < 2; i++) {
            PkbTeam t;
            uint8_t block[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
            CHECK_EQ(
                pkb_team_generate(&t, PKB_PARTY_LENGTH, team_lo, team_hi, &pools, NULL, NULL, rng),
                PkbTeamOk);
            CHECK(pkb_party_block_build(&t, t.name, block, patch));
            CHECK(pkb_party_block_parse(block, sizeof(block), patch, sizeof(patch), &party[i]));
        }
    } else {
        fuzz_party(rng, &party[0]);
        fuzz_party(rng, &party[1]);
    }
    for(int i = 0; i < PKB_RNS_LEN; i++)
        rns[i] = (uint8_t)pkb_policy_rng_below(rng, PKB_SERIAL_PREAMBLE_BYTE);
    if(battle < first) {
        for(int i = 0; i < 2; i++)
            pkb_policy_rng_next(rng);
        return 0;
    }

    memset(m->nsent, 0, sizeof(m->nsent));
    memset(m->turns, 0, sizeof(m->turns));
    memset(m->done, 0, sizeof(m->done));
    memset(m->wait, 0, sizeof(m->wait));
    m->stuck = false;
    m->compared = 0;
    m->divergences = 0;
    m->report[0] = 0;
    pthread_t th[2];
    for(int i = 0; i < 2; i++) {
        Side* s = &m->side[i];
        s->m = m;
        s->side = i;
        pkb_policy_rng_seed(&s->rng, pkb_policy_rng_next(rng) | (uint64_t)battle << 32);
        pkb_engine_init(&s->E, i == 0, rns, &party[i], &party[!i]);
        s->io = (PkbEngineIo){
            .exchange = sp_exchange,
            .choose_action = sp_choose,
            .choose_replacement = sp_replace,
            .on_turn = sp_on_turn,
            .ctx = s,
        };
        if(i == 0 && ai_side0) {
            pkb_ai_init(&s->ai, s->rng.s[0]);
            s->io.choose_action = sp_ai_choose;
            s->io.choose_replacement = sp_ai_replace;
        }
    }
    for(int i = 0; i < 2; i++)
        pthread_create(&th[i], NULL, side_thread, &m->side[i]);
    for(int i = 0; i < 2; i++)
        pthread_join(th[i], NULL);

    uint32_t flags = m->side[0].E.flags | m->side[1].E.flags;
    if(flags & PKB_ENGINE_UNSUPPORTED) {
        for(int i = 0; i < 2; i++) {
            const PkbEngine* E = &m->side[i].E;
            if(!(E->flags & PKB_ENGINE_UNSUPPORTED)) continue;
            printf(
                "battle %u: side %d hit an unported path (effect $%02X, command $%02X)\n",
                battle,
                i,
                E->unsupported_effect,
                E->unsupported_command);
        }
        (*unsupported)++;
    }
    if(m->side[0].E.game_desync >= PKB_DESYNC_BEAT_UP ||
       m->side[1].E.game_desync >= PKB_DESYNC_BEAT_UP) {
        (*beat_up)++;
    }
    /* At the end the sides legitimately differ (the winner's NewEnemyMonStatus
     * resets the last-move and turn counters the loser keeps): compare the
     * outcome and the battle mons' HP. */
    unsigned end = 0;
    if(rd16(&m->side[0].E, wBattleMonHP) != rd16(&m->side[1].E, wEnemyMonHP)) end++;
    if(rd16(&m->side[0].E, wEnemyMonHP) != rd16(&m->side[1].E, wBattleMonHP)) end++;
    bool same = !m->stuck && m->divergences == 0 && end == 0 && m->nsent[0] == m->nsent[1] &&
                m->side[1].result == mirrored[m->side[0].result] &&
                m->side[0].result != PkbBattleAborted;
    if(flags & PKB_ENGINE_GAME_DESYNC) {
        printf(
            "battle %u: game desync %u/%u (%s%s)\n",
            battle,
            m->side[0].E.game_desync,
            m->side[1].E.game_desync,
            same ? "no difference" : "diverged",
            m->stuck ? ", out of step" : "");
        return -2;
    }
    if(!same) {
        fprintf(
            stderr,
            "battle %u: %u turn differences, %u at the end, exchanges %u/%u, results %d/%d%s\n%s",
            battle,
            m->divergences,
            end,
            m->nsent[0],
            m->nsent[1],
            m->side[0].result,
            m->side[1].result,
            m->stuck ? ", out of step" : "",
            m->report);
        return -1;
    }
    side0_battles++;
    side0_wins += m->side[0].result == PkbBattleWin;
    return (int)m->nsent[0];
}

static void test_selfplay_mirrors(void) {
    static Match m;
    PkbPolicyRng rng;
    unsigned unsupported = 0, beat_up = 0, diverged = 0, game_desyncs = 0;
    uint64_t exchanges = 0;
    pthread_mutex_init(&m.mu, NULL);
    pthread_cond_init(&m.cv, NULL);
    pkb_policy_rng_seed(&rng, seed);
    if(getenv("PKB_SP_NO_ITEMS")) fuzz_party_items = false;
    if(getenv("PKB_SP_BEAT_UP")) fuzz_party_beat_up = true;
    if(getenv("PKB_SP_AI")) ai_side0 = true;
    const char* teams = getenv("PKB_SP_TEAMS");
    if(teams) {
        unsigned lo = 50, hi = 50;
        sscanf(teams, "%u-%u", &lo, &hi);
        team_lo = (uint8_t)lo;
        team_hi = (uint8_t)hi;
    }
    for(uint32_t b = 0; b < battles; b++) {
        if(getenv("PKB_SP_PROGRESS")) fprintf(stderr, "battle %u\n", b);
        int n = play(&m, &rng, b, &unsupported, &beat_up);
        if(n == -2) {
            game_desyncs++;
        } else if(n < 0) {
            if(++diverged >= 3) break;
        } else {
            exchanges += (uint64_t)n;
        }
    }
    printf(
        "%u battles, %llu exchanges, %u diverged, %u desynced by game bugs, %u hit unsupported paths, %u of the "
        "desyncs by Beat Up\n",
        battles,
        (unsigned long long)exchanges,
        diverged,
        game_desyncs,
        unsupported,
        beat_up);
    CHECK_EQ(diverged, 0);
    CHECK_EQ(unsupported, 0);
    if(side0_battles) {
        printf(
            "side 0 (%s) won %u of %u completed battles (%u%%)\n",
            ai_side0 ? "AI" : "random",
            side0_wins,
            side0_battles,
            side0_wins * 100 / side0_battles);
    }
    const char* min = getenv("PKB_SP_AI_MIN");
    if(min && side0_battles) CHECK(side0_wins * 100 / side0_battles >= (unsigned)atoi(min));
    pthread_mutex_destroy(&m.mu);
    pthread_cond_destroy(&m.cv);
}

int main(int argc, char** argv) {
    if(argc > 1) battles = (uint32_t)strtoul(argv[1], NULL, 0);
    if(argc > 2) seed = strtoull(argv[2], NULL, 0);
    if(argc > 3) first = (uint32_t)strtoul(argv[3], NULL, 0);
    verbose = argc > 3 && battles == first + 1;
    RUN(test_selfplay_mirrors);
    return TEST_REPORT();
}
