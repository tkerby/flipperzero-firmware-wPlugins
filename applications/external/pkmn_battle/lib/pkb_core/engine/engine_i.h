#pragma once

/*
 * Engine internals. Every function named like an asm label is a port of that
 * label (engine/battle/core.asm, effect_commands.asm, home bank, ...); flag
 * results are returned as bool and documented as "returns carry" / "returns z".
 * Text, sound, animation and HUD calls are dropped; everything they do to
 * game state is kept (e.g. UsedMoveText updates the last-move variables).
 */

#include <pkb/engine.h>
#include <pkb/section.h>

#include "pb_battle.h"
#include "pb_const.h"
#include "pb_tables.h"
#include "pb_wram.h"

/* "n percent" and "n out_of d" as rgbds evaluates them */
#define PERCENT(n) ((n) * 0xFF / 100)

#define W(a) (E->wram[(uint16_t)((a) - PB_WRAM_START)])
#define H(a) (E->hram[(uint8_t)((a) - PB_HRAM_START)])

/* Any WRAM/HRAM byte by bus address (routines that walk hl). */
uint8_t* pkb_mem(PkbEngine* E, uint16_t addr);
#define M(a) (*pkb_mem(E, (uint16_t)(a)))

static inline uint16_t rd16(PkbEngine* E, uint16_t a) {
    return (uint16_t)(M(a) << 8 | M(a + 1));
}
static inline void wr16(PkbEngine* E, uint16_t a, uint16_t v) {
    M(a) = (uint8_t)(v >> 8);
    M(a + 1) = (uint8_t)v;
}

/* hl + PARTYMON_STRUCT_LENGTH * index */
static inline uint16_t GetPartyLocation(uint16_t base, uint8_t index) {
    return (uint16_t)(base + PARTYMON_STRUCT_LENGTH * index);
}

static inline bool in_list(const uint8_t* p, uint8_t a) { /* IsInArray on a $FF-terminated list */
    for(; *p != 0xFF; p++) {
        if(*p == a) return true;
    }
    return false;
}

static inline bool player_turn(PkbEngine* E) {
    return H(hBattleTurn) == 0;
}
static inline bool is_master(PkbEngine* E) {
    return H(hSerialConnectionStatus) == USING_INTERNAL_CLOCK;
}

/* CheckBattleScene: returns carry if battle animations are turned off.
 * The substitute animations set wBattleAnimParam only when they are on. */
static inline bool CheckBattleScene(PkbEngine* E) {
    return (W(wOptions) & (1 << BATTLE_SCENE)) != 0;
}

/* ---- engine.c: home routines ---- */
uint8_t BattleRandom(PkbEngine* E, const char* site);
void Multiply(PkbEngine* E);
void Divide(PkbEngine* E, uint8_t b);
uint16_t GetBattleVarAddr(PkbEngine* E, uint8_t var);
uint8_t GetBattleVar(PkbEngine* E, uint8_t var);
void SetPlayerTurn(PkbEngine* E);
void SetEnemyTurn(PkbEngine* E);
void ResetDamage(PkbEngine* E);
void UpdateBattleMonInParty(PkbEngine* E);
void UpdateBattleMon(PkbEngine* E, uint8_t index);
void UpdateEnemyMonInParty(PkbEngine* E);
void UpdateUserInParty(PkbEngine* E);
void UpdateOpponentInParty(PkbEngine* E);
uint16_t BattlePartyAttr(PkbEngine* E, uint8_t attr);
uint16_t OTPartyAttr(PkbEngine* E, uint8_t attr);
uint16_t UserPartyAttr(PkbEngine* E, uint8_t attr);
uint16_t GetPartyParamLocation(PkbEngine* E, uint8_t attr);
/* GetItemHeldEffect: b = effect, c = parameter */
void GetItemHeldEffect(PkbEngine* E, uint8_t item, uint8_t* b, uint8_t* c);
/* GetUserItem / GetOpponentItem: effect, parameter, address of the item */
uint16_t GetUserItem(PkbEngine* E, uint8_t* b, uint8_t* c);
uint16_t GetOpponentItem(PkbEngine* E, uint8_t* b, uint8_t* c);
void GetMoveData(PkbEngine* E, uint8_t move_index, uint16_t dest); /* move_index = move - 1 */
uint8_t GetMoveEffect(uint8_t move);
void unsupported(PkbEngine* E, uint8_t effect, uint8_t command);
/* the other Game Boy computes this differently (a game bug); kind = PKB_DESYNC_* */
void game_desync(PkbEngine* E, uint8_t kind);
bool LinkBattleSendReceiveAction(PkbEngine* E);

/* ---- core.c ---- */
void DoBattle(PkbEngine* E);
bool HasPlayerFainted(PkbEngine* E); /* true if HP is zero */
bool HasEnemyFainted(PkbEngine* E);
bool HasUserFainted(PkbEngine* E);
void SwitchTurnCore(PkbEngine* E);
void SubtractHPFromUser(PkbEngine* E, uint16_t bc);
void SubtractHPFromTarget(PkbEngine* E, uint16_t bc);
uint16_t SubtractHP(PkbEngine* E, uint16_t bc);
uint16_t GetSixteenthMaxHP(PkbEngine* E);
uint16_t GetEighthMaxHP(PkbEngine* E);
uint16_t GetQuarterMaxHP(PkbEngine* E);
uint16_t GetHalfMaxHP(PkbEngine* E);
uint16_t GetMaxHP(PkbEngine* E);
void RestoreHP(PkbEngine* E, uint16_t bc);
void PursuitSwitch_none(PkbEngine* E);
void ApplyStatLevelMultiplierOnAllStats(PkbEngine* E);
/* hBattleTurn != 0 means the player's mon here (as the asm) */
void ApplyPrzEffectOnSpeed(PkbEngine* E);
void ApplyBrnEffectOnAttack(PkbEngine* E);
void ApplyStatusEffectOnPlayerStats(PkbEngine* E);
void ApplyStatusEffectOnEnemyStats(PkbEngine* E);
void NewEnemyMonStatus(PkbEngine* E);
void NewBattleMonStatus(PkbEngine* E);
void BreakAttraction(PkbEngine* E);
void ResetEnemyStatLevels(PkbEngine* E);
void ResetPlayerStatLevels(PkbEngine* E);
void SpikesDamage(PkbEngine* E);
void PassedBattleMonEntrance(PkbEngine* E);
void EnemySwitch_SetMode(PkbEngine* E);
void ResetBattleParticipants(PkbEngine* E);
void ForceEnemySwitch(PkbEngine* E);
bool SwitchPlayerMon(PkbEngine* E); /* true = nz (the enemy mon has HP) */
bool FindAliveEnemyMons(PkbEngine* E); /* true = carry (no other mon can fight) */
/* the player's party menu outside the faint path (Baton Pass): an alive
 * party slot that is not out */
uint8_t ForcePickSwitchMonInBattle(PkbEngine* E);
void ItemRecoveryAnim(PkbEngine* E);
/* wCurBaseData for species (the fields this port has tables for) */
void GetBaseData(PkbEngine* E, uint8_t species);
void UseOpponentItem(PkbEngine* E);

/* ---- effects.c ---- */
void DoPlayerTurn(PkbEngine* E);
void DoEnemyTurn(PkbEngine* E);
void DoMove(PkbEngine* E);
void UpdateMoveData(PkbEngine* E);
void EndMoveEffect(PkbEngine* E);
bool CheckSubstituteOpp(PkbEngine* E); /* true = nz (substitute up) */
void BattleCommand_SwitchTurn(PkbEngine* E);
void BattleCheckTypeMatchup(PkbEngine* E);
void CheckTypeMatchup(PkbEngine* E, uint16_t hl);
bool CheckUserIsCharging(PkbEngine* E); /* true = nz */
void SkipToBattleCommand(PkbEngine* E, uint8_t b);
void ResetTurn(PkbEngine* E);
void ClearLastMove(PkbEngine* E);
void ResetActorDisable(PkbEngine* E);
bool CheckUserMove(PkbEngine* E, uint8_t move); /* true = z (the user knows it) */
void BattleCommand_LowerSub(PkbEngine* E);
void BattleCommand_RaiseSub(PkbEngine* E);
void AnimateCurrentMove(PkbEngine* E);
void AnimateFailedMove(PkbEngine* E);
void BattleCommand_Stab(PkbEngine* E);
void BattleCommand_ResetTypeMatchup(PkbEngine* E);
void BattleCommand_StatUp(PkbEngine* E, uint8_t b);
void RaiseStat(PkbEngine* E, uint8_t b);
bool UseHeldStatusHealingItem(PkbEngine* E); /* true = nz (the item cured the opponent) */
void UseConfusionHealingItem(PkbEngine* E);
uint8_t LowerStat(PkbEngine* E, uint8_t a); /* returns wFailedMessage */
bool SafeCheckSafeguard(PkbEngine* E); /* true = nz */
bool CheckOpponentWentFirst(PkbEngine* E); /* true = nz */
bool CheckHiddenOpponent(PkbEngine* E); /* true = nz */
void CalcPlayerStats(PkbEngine* E);
void CalcEnemyStats(PkbEngine* E);
void ResetFuryCutterCount(PkbEngine* E);

/* ---- move_effects.c (engine/battle/move_effects) ---- */
bool MoveEffectCommandSupported(uint8_t cmd);
bool RunMoveEffectCommand(PkbEngine* E, uint8_t cmd); /* false: not a command of this file */

/* res/set bits of a battle variable */
static inline void res_var(PkbEngine* E, uint8_t var, uint8_t bits) {
    M(GetBattleVarAddr(E, var)) &= (uint8_t)~bits;
}
static inline void set_var(PkbEngine* E, uint8_t var, uint8_t bits) {
    M(GetBattleVarAddr(E, var)) |= bits;
}
