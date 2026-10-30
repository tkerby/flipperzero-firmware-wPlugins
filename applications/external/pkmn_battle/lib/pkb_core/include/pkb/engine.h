#pragma once

/*
 * Bit-exact port of Pokemon Crystal's link-battle engine
 * (engine/battle/core.asm, engine/battle/effect_commands.asm and helpers,
 * pret/pokecrystal at the revision in tools/gen/pokecrystal.lock).
 *
 * The engine runs one side of a Colosseum battle exactly as a Game Boy
 * would, so it stays in lockstep with the real Game Boy on the other end of
 * the cable: the only inputs are the shared random numbers, both parties and
 * the action nibbles exchanged each turn.
 *
 * State lives in a shadow of the game's WRAM and HRAM with the real layout
 * (lib/pkb_core/gen/pb_wram.h), so every routine is a 1:1 port of its asm
 * label and the host tests can compare it byte for byte with an emulated
 * Game Boy. "Player" is our side, "enemy" the other one, as on the Game Boy
 * that runs this side.
 *
 * Effects not ported yet set PKB_ENGINE_UNSUPPORTED (and abort on the host
 * when PKB_ENGINE_STRICT is defined); see pkb_engine_effect_supported().
 *
 * A few Crystal bugs make two real Game Boys compute a turn differently.
 * The engine reproduces them (it must match the Game Boy it stands in for)
 * and sets PKB_ENGINE_GAME_DESYNC, because from then on the other Game Boy
 * sees a different battle.
 */

#include <stdbool.h>
#include <stdint.h>

#include <pkb/party_block.h>
#include <pkb/policy_rng.h>

#ifdef __cplusplus
extern "C" {
#endif

/* pkb_engine_run() results */
typedef enum {
    PkbBattleUnknown,
    PkbBattleWin,
    PkbBattleLose,
    PkbBattleDraw,
    PkbBattleAborted, /* exchange() failed: link lost or user abort */
} PkbBattleResult;

/* PkbEngine.flags */
#define PKB_ENGINE_UNSUPPORTED (1u << 0) /* an effect/item path not ported yet was hit */
#define PKB_ENGINE_GAME_DESYNC (1u << 1) /* a game bug made the two Game Boys disagree */

/* PkbEngine.game_desync: which game bug (the first one seen) */
#define PKB_DESYNC_CONFUSION_CRIT 1 /* confusion self-hit with wCriticalHit left set (failed Snore) */
#define PKB_DESYNC_SWAGGER_MINIMIZE 2 /* Swagger read a stale Minimize from a switched-in target */
#define PKB_DESYNC_PURSUIT_BATON_PASS 3 /* Pursuit doubled on one side only after a Baton Pass */
#define PKB_DESYNC_STALE_MOVE_STRUCT 4 /* a locked-in Fly/Dig user's stale move struct (Future Sight) */
#define PKB_DESYNC_BEAT_UP 5 /* Beat Up's user branch read the wrong status (wCurBattleMon vs HP) */
#define PKB_DESYNC_BEAT_UP_OVERRUN 6 /* Beat Up ran past the end of the party (IN_LOOP left set) */

/* What the player does at the battle menu. */
typedef struct {
    bool switch_mon; /* false: use move `slot` (0-3); true: switch to party slot `slot` */
    uint8_t slot;
} PkbChoice;

typedef struct PkbEngine PkbEngine;

typedef struct {
    /* Exchange action nibbles with the other Game Boy (blocking). Return
     * false to abort the battle (the engine unwinds and returns Aborted). */
    bool (*exchange)(void* ctx, uint8_t mine, uint8_t* theirs);
    /* Battle menu (once per turn the player chooses): the move must have
     * PP and not be disabled; the switch target must be alive, not out, and
     * switching must be allowed (not trapped). */
    void (*choose_action)(void* ctx, const PkbEngine* E, PkbChoice* choice);
    /* Replacement after our Pokemon fainted, or the target of our Baton
     * Pass: an alive party slot other than the one out. */
    uint8_t (*choose_replacement)(void* ctx, const PkbEngine* E);
    /* Optional (host): every BattleRandom call with the calling routine. */
    void (*on_random)(void* ctx, const char* site, uint8_t value);
    /* Optional (host): before each exchange. */
    void (*on_exchange)(void* ctx, const PkbEngine* E, uint8_t mine);
    /* Optional (host): at the start of every BattleTurn iteration. */
    void (*on_turn)(void* ctx, const PkbEngine* E);
    void* ctx;
} PkbEngineIo;

struct PkbEngine {
    uint8_t wram[0x2000]; /* $C000-$DFFF, WRAMX bank 1 at $D000 */
    uint8_t hram[0x80]; /* $FF80-$FFFF */
    const PkbEngineIo* io;
    uint32_t flags;
    uint8_t unsupported_effect; /* first unported move effect seen, 0xFF = none */
    uint8_t unsupported_command; /* first unported battle command seen */
    uint8_t game_desync; /* PKB_DESYNC_*, 0 = none */
    uint32_t random_calls;
    bool aborted;
    /* Z80 b, c, d, e as kept between battle commands (damagestats ->
     * damagecalc) */
    uint8_t reg_b, reg_c, reg_d, reg_e;
    uint8_t menu_slot; /* move slot chosen at the battle menu */
    bool leave_do_move; /* a command popped its return address: DoMove returns */
    /* For PKB_DESYNC_SWAGGER_MINIMIZE: whose move is running, and what the
     * other Game Boy holds in its copy of each move struct's animation. Its
     * SendOutPlayerMon clears its player struct when our enemy comes in;
     * our player struct was cleared while its enemy copy kept the old one. */
    uint8_t move_user; /* hBattleTurn when DoTurn started */
    bool enemy_sent_out; /* until ParseEnemyAction loads the enemy struct */
    bool player_sent_out; /* until ParsePlayerAction loads the player struct */
    uint8_t player_prev_anim; /* the player struct's animation before the send-out */
    /* For PKB_DESYNC_PURSUIT_BATON_PASS: the other Game Boy's switching flags
     * this turn, in our terms (its wPlayerIsSwitching is about our enemy).
     * Its EnemySwitch_SetMode sets wEnemyIsSwitching when our mon is passed
     * in, but our PassedBattleMonEntrance sets nothing. */
    bool peer_enemy_switching;
    bool peer_player_switching;
    /* For PKB_DESYNC_STALE_MOVE_STRUCT: the other Game Boy's copy of each
     * move struct's animation, in our terms. Its ParsePlayerAction skips
     * UpdateMoveData for a locked-in move while its ParseEnemyAction always
     * reloads, so after a Future Sight hit (which leaves FUTURE_SIGHT in the
     * user's struct) the copies differ until the mon's DoTurn. Send-outs are
     * not modelled: they clear the Fly/Dig bits CantMove looks at anyway. */
    uint8_t peer_player_anim;
    uint8_t peer_enemy_anim;
};

/*
 * Set up the state the Game Boy has when Gen2ToGen2LinkComms hands over to
 * StartBattle: both parties as exchanged, the master's 10 random numbers
 * and which side clocks the link (master = USING_INTERNAL_CLOCK).
 */
void pkb_engine_init(
    PkbEngine* E,
    bool master,
    const uint8_t rns[PKB_RNS_LEN],
    const PkbLinkPartyData* ours,
    const PkbLinkPartyData* theirs);

/* Run DoBattle to the end. */
PkbBattleResult pkb_engine_run(PkbEngine* E, const PkbEngineIo* io);

/* True if move effect `effect` (EFFECT_*) is fully ported. */
bool pkb_engine_effect_supported(uint8_t effect);
/* Filter for pkb_team_generate: moves whose effect is supported. */
bool pkb_engine_move_filter(void* ctx, uint8_t move);

/* Controller helpers (engine/controller.c). The random policy picks a random
 * usable move every turn and a random fit replacement. */
void pkb_engine_random_move(const PkbEngine* E, PkbPolicyRng* rng, PkbChoice* c);
uint8_t pkb_engine_random_replacement(const PkbEngine* E, PkbPolicyRng* rng);
bool pkb_engine_can_switch(const PkbEngine* E);
void pkb_engine_random_switch(const PkbEngine* E, PkbPolicyRng* rng, PkbChoice* c);

/* CRC-32 of the main battle state (both battle mons, substatus, stat
 * levels, screens, weather, random numbers, active mons), logged with every
 * exchange so host replays can check device battles step by step. */
uint32_t pkb_engine_state_crc(const PkbEngine* E);

/* The active mons for display: ours (enemy = false) or the other side's.
 * Returns the species (0 before the first send-out). */
uint8_t pkb_engine_active_mon(const PkbEngine* E, bool enemy, uint16_t* hp, uint16_t* max_hp);

/* One side as the battle screen shows it. */
typedef struct {
    uint8_t species; /* active mon, 0 before the first send-out */
    uint16_t hp, max_hp;
    uint8_t status; /* status byte: sleep counter in bits 0-2, then PSN, BRN, FRZ, PAR */
    uint8_t last_move; /* the last move it used (0: none since it came in) */
    uint8_t count; /* party size */
    uint8_t alive; /* bit i: party mon i has HP left */
} PkbSideView;
void pkb_engine_side_view(const PkbEngine* E, bool enemy, PkbSideView* v);
/* "SLP", "PSN", "BRN", "FRZ", "PAR" or "" for a status byte */
const char* pkb_status_abbrev(uint8_t status);

#ifdef PKB_HOST
/* Tests only: damage of `move` between the battle mons (DamageStats,
 * DamageCalc, Stab; no critical hit, no random roll). Changes E. */
uint16_t pkb_engine_test_damage(PkbEngine* E, bool player, uint8_t move);
#endif

/* Read-only views used by controllers and tests. */
uint8_t pkb_engine_peek(const PkbEngine* E, uint16_t addr);
uint16_t pkb_engine_peek16(const PkbEngine* E, uint16_t addr); /* big endian */

#ifdef __cplusplus
}
#endif
