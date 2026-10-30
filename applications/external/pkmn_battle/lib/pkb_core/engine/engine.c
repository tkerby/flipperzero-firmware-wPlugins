/*
 * Engine entry points and the home-bank routines the battle code relies on
 * (home/battle.asm, home/battle_vars.asm, home/math.asm, engine/math/math.asm,
 * _BattleRandom, _LinkBattleSendReceiveAction).
 */
#include <string.h>

#include "engine_i.h"

#ifdef PKB_HOST
#include <stdio.h>
#include <stdlib.h>
#endif

uint8_t* pkb_mem(PkbEngine* E, uint16_t addr) {
    if(addr >= PB_WRAM_START && addr < PB_WRAM_START + PB_WRAM_SIZE) return &E->wram[addr - PB_WRAM_START];
    if(addr >= PB_HRAM_START) return &E->hram[addr - PB_HRAM_START];
#ifdef PKB_HOST
    /* After a game desync or an unported path the other side's actions need
     * not make sense here (a replacement can name a party slot that does not
     * exist). */
    if(!(E->flags & (PKB_ENGINE_GAME_DESYNC | PKB_ENGINE_UNSUPPORTED))) {
        fprintf(stderr, "engine: access outside WRAM/HRAM at $%04X\n", addr);
        fflush(stdout); /* keep a host trace */
        abort();
    }
#endif
    static uint8_t dummy;
    return &dummy;
}

uint8_t pkb_engine_peek(const PkbEngine* E, uint16_t addr) {
    return *pkb_mem((PkbEngine*)E, addr);
}

uint16_t pkb_engine_peek16(const PkbEngine* E, uint16_t addr) {
    return (uint16_t)(pkb_engine_peek(E, addr) << 8 | pkb_engine_peek(E, (uint16_t)(addr + 1)));
}

void game_desync(PkbEngine* E, uint8_t kind) {
    if(!(E->flags & PKB_ENGINE_GAME_DESYNC)) E->game_desync = kind;
    E->flags |= PKB_ENGINE_GAME_DESYNC;
}

void unsupported(PkbEngine* E, uint8_t effect, uint8_t command) {
    /* after a game desync the other side's actions can lead anywhere: the
     * battle is already explained by the game bug */
    if(E->flags & PKB_ENGINE_GAME_DESYNC) return;
    if(!(E->flags & PKB_ENGINE_UNSUPPORTED)) {
        E->unsupported_effect = effect;
        E->unsupported_command = command;
    }
    E->flags |= PKB_ENGINE_UNSUPPORTED;
#if defined(PKB_HOST) && defined(PKB_ENGINE_STRICT)
    fprintf(stderr, "engine: unsupported effect $%02X command $%02X\n", effect, command);
    abort();
#endif
}

/* ---- _BattleRandom (engine/battle/core.asm), link-battle path ---- */

uint8_t BattleRandom(PkbEngine* E, const char* site) {
    uint8_t i = W(wLinkBattleRNCount);
    uint8_t a = W(wLinkBattleRNs + i);
    W(wLinkBattleRNCount) = (uint8_t)(i + 1);
    if((uint8_t)(i + 1) >= SERIAL_RNS_LENGTH - 1) {
        W(wLinkBattleRNCount) = 0;
        for(int k = 0; k < SERIAL_RNS_LENGTH; k++) {
            uint8_t r = W(wLinkBattleRNs + k);
            W(wLinkBattleRNs + k) = (uint8_t)(r * 5 + 1);
        }
    }
    E->random_calls++;
    if(E->io && E->io->on_random) E->io->on_random(E->io->ctx, site, a);
    return a;
}

/* ---- _Multiply / _Divide (engine/math/math.asm), on the HRAM bytes ---- */

void Multiply(PkbEngine* E) {
    uint8_t b = 8;
    H(hMultiplicand - 1) = 0;
    for(int i = 1; i <= 4; i++) H(hMathBuffer + i) = 0;
    for(;;) {
        uint8_t m = H(hMultiplier);
        H(hMultiplier) = m >> 1;
        if(m & 1) {
            unsigned carry = 0;
            for(int k = 0; k < 4; k++) { /* MathBuffer+4..+1 += Multiplicand+2..-1 */
                unsigned s = H(hMathBuffer + 4 - k) + H(hMultiplicand + 2 - k) + carry;
                H(hMathBuffer + 4 - k) = (uint8_t)s;
                carry = s >> 8;
            }
        }
        if(--b == 0) break;
        unsigned carry = 0;
        for(int k = 0; k < 4; k++) { /* hMultiplicand-1..+2 <<= 1 */
            unsigned v = (unsigned)(H(hMultiplicand + 2 - k) << 1) | carry;
            H(hMultiplicand + 2 - k) = (uint8_t)v;
            carry = v >> 8;
        }
    }
    for(int k = 0; k < 4; k++) H(hProduct + k) = H(hMathBuffer + 1 + k);
}

void Divide(PkbEngine* E, uint8_t b) {
    for(int i = 0; i <= 4; i++) H(hMathBuffer + i) = 0;
    uint8_t e = 9;
    for(;;) {
        /* .loop: subtract (hDivisor:hMathBuffer+0) from hDividend+0..1 while it fits */
        unsigned lo = (unsigned)H(hDividend + 1) - H(hMathBuffer + 0);
        uint8_t d = (uint8_t)lo;
        unsigned borrow = (lo >> 8) & 1;
        unsigned hi = (unsigned)H(hDividend + 0) - H(hDivisor) - borrow;
        if(!(hi & 0x100)) {
            H(hDividend + 0) = (uint8_t)hi;
            H(hDividend + 1) = d;
            H(hMathBuffer + 4) = (uint8_t)(H(hMathBuffer + 4) + 1);
            continue;
        }
        /* .next */
        if(b == 1) break;
        unsigned carry = 0;
        for(int k = 4; k >= 1; k--) {
            unsigned v = (unsigned)(H(hMathBuffer + k) << 1) | carry;
            H(hMathBuffer + k) = (uint8_t)v;
            carry = v >> 8;
        }
        if(--e == 0) {
            e = 8;
            H(hDivisor) = H(hMathBuffer + 0);
            H(hMathBuffer + 0) = 0;
            H(hDividend + 0) = H(hDividend + 1);
            H(hDividend + 1) = H(hDividend + 2);
            H(hDividend + 2) = H(hDividend + 3);
        }
        /* .next2 */
        if(e == 1) b--;
        /* .okay: hDivisor:hMathBuffer+0 >>= 1 */
        uint8_t div = H(hDivisor);
        H(hDivisor) = div >> 1;
        H(hMathBuffer + 0) = (uint8_t)((H(hMathBuffer + 0) >> 1) | ((div & 1) << 7));
    }
    H(hRemainder) = H(hDividend + 1);
    for(int k = 0; k < 4; k++) H(hQuotient + k) = H(hMathBuffer + 1 + k);
}

/* ---- home/battle_vars.asm ---- */

static const uint16_t battle_var_locations[NUM_BATTLE_VARS][2] = {
    {wPlayerSubStatus1, wEnemySubStatus1},
    {wPlayerSubStatus2, wEnemySubStatus2},
    {wPlayerSubStatus3, wEnemySubStatus3},
    {wPlayerSubStatus4, wEnemySubStatus4},
    {wPlayerSubStatus5, wEnemySubStatus5},
    {wEnemySubStatus1, wPlayerSubStatus1},
    {wEnemySubStatus2, wPlayerSubStatus2},
    {wEnemySubStatus3, wPlayerSubStatus3},
    {wEnemySubStatus4, wPlayerSubStatus4},
    {wEnemySubStatus5, wPlayerSubStatus5},
    {wBattleMonStatus, wEnemyMonStatus},
    {wEnemyMonStatus, wBattleMonStatus},
    {wPlayerMoveStructAnimation, wEnemyMoveStructAnimation},
    {wPlayerMoveStructEffect, wEnemyMoveStructEffect},
    {wPlayerMoveStructPower, wEnemyMoveStructPower},
    {wPlayerMoveStructType, wEnemyMoveStructType},
    {wCurPlayerMove, wCurEnemyMove},
    {wLastPlayerCounterMove, wLastEnemyCounterMove},
    {wLastEnemyCounterMove, wLastPlayerCounterMove},
    {wLastPlayerMove, wLastEnemyMove},
    {wLastEnemyMove, wLastPlayerMove},
};

uint16_t GetBattleVarAddr(PkbEngine* E, uint8_t var) {
    return battle_var_locations[var][player_turn(E) ? 0 : 1];
}

uint8_t GetBattleVar(PkbEngine* E, uint8_t var) {
    return M(GetBattleVarAddr(E, var));
}

/* ---- home/battle.asm ---- */

void SetPlayerTurn(PkbEngine* E) {
    H(hBattleTurn) = 0;
}

void SetEnemyTurn(PkbEngine* E) {
    H(hBattleTurn) = 1;
}

void ResetDamage(PkbEngine* E) {
    W(wCurDamage) = 0;
    W(wCurDamage + 1) = 0;
}

uint16_t GetPartyParamLocation(PkbEngine* E, uint8_t attr) {
    return GetPartyLocation((uint16_t)(wPartyMons + attr), W(wCurPartyMon));
}

uint16_t BattlePartyAttr(PkbEngine* E, uint8_t attr) {
    return GetPartyLocation((uint16_t)(wPartyMons + attr), W(wCurBattleMon));
}

uint16_t OTPartyAttr(PkbEngine* E, uint8_t attr) {
    return GetPartyLocation((uint16_t)(wOTPartyMon1Species + attr), W(wCurOTMon));
}

uint16_t UserPartyAttr(PkbEngine* E, uint8_t attr) {
    return player_turn(E) ? BattlePartyAttr(E, attr) : OTPartyAttr(E, attr);
}

void UpdateBattleMon(PkbEngine* E, uint8_t index) {
    uint16_t de = GetPartyLocation(wPartyMon1Level, index);
    for(uint16_t i = 0; i < wBattleMonMaxHP - wBattleMonLevel; i++) M(de + i) = W(wBattleMonLevel + i);
}

void UpdateBattleMonInParty(PkbEngine* E) {
    UpdateBattleMon(E, W(wCurBattleMon));
}

void UpdateEnemyMonInParty(PkbEngine* E) {
    if(W(wBattleMode) == WILD_BATTLE) return;
    uint16_t de = GetPartyLocation(wOTPartyMon1Level, W(wCurOTMon));
    for(uint16_t i = 0; i < wEnemyMonMaxHP - wEnemyMonLevel; i++) M(de + i) = W(wEnemyMonLevel + i);
}

void UpdateUserInParty(PkbEngine* E) {
    if(player_turn(E)) UpdateBattleMonInParty(E);
    else UpdateEnemyMonInParty(E);
}

void UpdateOpponentInParty(PkbEngine* E) {
    if(player_turn(E)) UpdateEnemyMonInParty(E);
    else UpdateBattleMonInParty(E);
}

/* ---- items (effect_commands.asm) ---- */

void GetItemHeldEffect(PkbEngine* E, uint8_t item, uint8_t* b, uint8_t* c) {
    (void)E;
    if(!item) { /* "ret z": b = 0, c unchanged by the asm; callers only test b */
        *b = 0;
        return;
    }
    *b = pb_item_held_effect[item - 1][0];
    *c = pb_item_held_effect[item - 1][1];
}

uint16_t GetUserItem(PkbEngine* E, uint8_t* b, uint8_t* c) {
    uint16_t hl = player_turn(E) ? wBattleMonItem : wEnemyMonItem;
    GetItemHeldEffect(E, M(hl), b, c);
    return hl;
}

uint16_t GetOpponentItem(PkbEngine* E, uint8_t* b, uint8_t* c) {
    uint16_t hl = player_turn(E) ? wEnemyMonItem : wBattleMonItem;
    GetItemHeldEffect(E, M(hl), b, c);
    return hl;
}

/* ---- moves ---- */

void GetMoveData(PkbEngine* E, uint8_t move_index, uint16_t dest) {
    const PbMove* m = &pb_moves[move_index];
    M(dest + MOVE_ANIM) = (uint8_t)(move_index + 1);
    M(dest + MOVE_EFFECT) = m->effect;
    M(dest + MOVE_POWER) = m->power;
    M(dest + MOVE_TYPE) = m->type;
    M(dest + MOVE_ACC) = m->accuracy;
    M(dest + MOVE_PP) = m->pp;
    M(dest + MOVE_CHANCE) = m->effect_chance;
    /* both Game Boys load the same struct, except in the per-turn reloads
     * that ParsePlayerAction and ParseEnemyAction correct */
    if(dest == wPlayerMoveStruct) E->peer_player_anim = (uint8_t)(move_index + 1);
    if(dest == wEnemyMoveStruct) E->peer_enemy_anim = (uint8_t)(move_index + 1);
}

uint8_t GetMoveEffect(uint8_t move) {
    if(move && move <= NUM_ATTACKS) return pb_moves[move - 1].effect;
    /* past the Moves table: the bytes the game reads there (Pursuit
     * checks the move of a side that is switching, 0 or CANNOT_MOVE) */
    switch(move) {
    case 0x00: return 0x21;
    case 0xFC: return 0xE8;
    case 0xFD: return 0x4F;
    case 0xFE: return 0xEA;
    default: return 0xE5; /* CANNOT_MOVE */
    }
}

/* ---- _LinkBattleSendReceiveAction (mobile/mobile_40.asm) ---- */

static uint8_t StageForSend(PkbEngine* E) {
    uint8_t a;
    if(W(wBattlePlayerAction) != BATTLEPLAYERACTION_USEMOVE) {
        a = (uint8_t)(W(wCurPartyMon) + BATTLEACTION_SWITCH1);
    } else if(W(wCurPlayerMove) == STRUGGLE) {
        a = BATTLEACTION_STRUGGLE;
    } else if(W(wCurPlayerMove) == 0xFF) {
        a = BATTLEACTION_SKIPTURN;
    } else {
        a = W(wCurMoveNum);
    }
    return a & 0x0F;
}

/* Returns false if the link was lost; the received action is then FORFEIT,
 * which ends the battle through the game's own forfeit paths. */
bool LinkBattleSendReceiveAction(PkbEngine* E) {
    uint8_t mine = StageForSend(E), theirs = BATTLEACTION_FORFEIT;
    W(wLinkBattleSentAction) = mine;
    if(E->io->on_exchange) E->io->on_exchange(E->io->ctx, E, mine);
    if(E->aborted || !E->io->exchange(E->io->ctx, mine, &theirs)) {
        E->aborted = true;
        theirs = BATTLEACTION_FORFEIT;
    }
    W(wOtherPlayerLinkAction) = theirs & 0x0F;
    W(wBattleAction) = theirs & 0x0F;
    return !E->aborted;
}

/* ---- state CRC for battle logs ---- */

static uint32_t crc32_update(uint32_t crc, const uint8_t* p, uint16_t len) {
    while(len--) {
        crc ^= *p++;
        for(int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return crc;
}

uint32_t pkb_engine_state_crc(const PkbEngine* E) {
    static const struct {
        uint16_t addr, len;
    } fields[] = {
        {wBattleMon, wBattleMonStructEnd - wBattleMon},
        {wEnemyMon, wEnemyMonStructEnd - wEnemyMon},
        {wPlayerSubStatus1, 10}, /* player and enemy substatus 1-5 */
        {wPlayerStatLevels, 2 * NUM_LEVEL_STATS},
        {wPlayerScreens, 2},
        {wBattleWeather, 2},
        {wLinkBattleRNCount, 1},
        {wLinkBattleRNs, PKB_RNS_LEN},
        {wCurBattleMon, 1},
        {wCurOTMon, 1},
    };
    uint32_t crc = 0xFFFFFFFFu;
    for(unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        crc = crc32_update(crc, pkb_mem((PkbEngine*)E, fields[i].addr), fields[i].len);
    }
    return ~crc;
}

/* ---- setup ---- */

static void copy_in(PkbEngine* E, uint16_t dest, const void* src, uint16_t len) {
    memcpy(pkb_mem(E, dest), src, len);
}

void pkb_engine_init(
    PkbEngine* E,
    bool master,
    const uint8_t rns[PKB_RNS_LEN],
    const PkbLinkPartyData* ours,
    const PkbLinkPartyData* theirs) {
    memset(E, 0, sizeof(*E));
    E->unsupported_effect = 0xFF;
    E->unsupported_command = 0xFF;
    H(hSerialConnectionStatus) = master ? USING_INTERNAL_CLOCK : USING_EXTERNAL_CLOCK;

    /* our party (wPlayerName, wPlayerID, wPartyCount..wPartyMonNicknames) */
    copy_in(E, wPlayerName, ours->player_name, NAME_LENGTH);
    copy_in(E, wPlayerID, ours->player_id, 2);
    W(wPartyCount) = ours->count;
    copy_in(E, wPartySpecies, ours->species, PARTY_LENGTH + 1);
    copy_in(E, wPartyMons, ours->mons, sizeof(ours->mons));
    copy_in(E, wPartyMonOTs, ours->ot_names, sizeof(ours->ot_names));
    copy_in(E, wPartyMonNicknames, ours->nicknames, sizeof(ours->nicknames));

    /* the other side, as Gen2ToGen2LinkComms copies it */
    copy_in(E, wOTPlayerName, theirs->player_name, NAME_LENGTH);
    W(wOTPartyCount) = theirs->count;
    copy_in(E, wOTPartySpecies, theirs->species, PARTY_LENGTH + 1);
    copy_in(E, wOTPlayerID, theirs->player_id, 2);
    copy_in(E, wOTPartyMons, theirs->mons, sizeof(theirs->mons));
    copy_in(E, wOTPartyMonOTs, theirs->ot_names, sizeof(theirs->ot_names));
    copy_in(E, wOTPartyMonNicknames, theirs->nicknames, sizeof(theirs->nicknames));

    /* shared seed: the master keeps its own, the slave copies them */
    copy_in(E, wLinkBattleRNs, rns, PKB_RNS_LEN);

    /* Colosseum (link.asm, Colosseum and Gen2ToGen2LinkComms) */
    W(wLinkMode) = LINK_COLOSSEUM;
    /* BattleIntro: ClearBattleRAM (the wBattle area and a few others) is
     * implied by the zeroed state; InitEnemyTrainer: */
    W(wEnemyItemState) = 0xFF;
    W(wCurOTMon) = 0xFF;
    W(wBattleMode) = TRAINER_BATTLE;
    W(wBattleType) = BATTLETYPE_NORMAL;
    ResetEnemyStatLevels(E);
}

PkbBattleResult pkb_engine_run(PkbEngine* E, const PkbEngineIo* io) {
    E->io = io;
    DoBattle(E);
    if(E->aborted) return PkbBattleAborted;
    switch(W(wBattleResult) & ~BATTLERESULT_BITMASK) {
    case WIN:
        return PkbBattleWin;
    case LOSE:
        return PkbBattleLose;
    case DRAW:
        return PkbBattleDraw;
    default:
        return PkbBattleUnknown;
    }
}
