/*
 * engine/battle/core.asm: battle flow, turn order, residual effects,
 * fainting, switching and party/battle-mon setup (link-battle paths only;
 * wild, contest, mobile and Battle Tower branches are dropped).
 */
#include <string.h>

#include "engine_i.h"

static void BattleTurn(PkbEngine* E);
static void HandleBetweenTurnEffects(PkbEngine* E);
static bool CheckFaint_PlayerThenEnemy(PkbEngine* E);
static bool CheckFaint_EnemyThenPlayer(PkbEngine* E);
static void HandleBerserkGene(PkbEngine* E);
static bool EnemyTriesToFlee(PkbEngine* E);
static bool DetermineMoveOrder(PkbEngine* E);
static bool CheckPlayerLockedIn(PkbEngine* E);
static bool ParsePlayerAction(PkbEngine* E);
static void HandleEncore(PkbEngine* E);
static void Battle_EnemyFirst(PkbEngine* E);
static void Battle_PlayerFirst(PkbEngine* E);
static bool ResidualDamage(PkbEngine* E);
static void HandlePerishSong(PkbEngine* E);
static void HandleWrap(PkbEngine* E);
static void HandleLeftovers(PkbEngine* E);
static void HandleMysteryberry(PkbEngine* E);
static void HandleFutureSight(PkbEngine* E);
static void HandleDefrost(PkbEngine* E);
static void HandleSafeguard(PkbEngine* E);
static void HandleScreens(PkbEngine* E);
static void HandleWeather(PkbEngine* E);
static void HandleEnemyMonFaint(PkbEngine* E);
static void HandlePlayerMonFaint(PkbEngine* E);
static void WildFled_EnemyFled_LinkBattleCanceled(PkbEngine* E);
static void LostBattle(PkbEngine* E);
static bool HandleEnemySwitch(PkbEngine* E);
static void EnemyPartyMonEntrance(PkbEngine* E, bool set_mode);
static void EnemySwitch(PkbEngine* E);
static bool PursuitSwitch(PkbEngine* E);
static void HandleHealingItems(PkbEngine* E);
static void HandleStatBoostingHeldItems(PkbEngine* E);
static bool BattleMenu(PkbEngine* E);
static bool PlayerSwitch(PkbEngine* E);
static void ParseEnemyAction(PkbEngine* E);
static bool CheckEnemyLockedIn(PkbEngine* E);
static void AI_Switch(PkbEngine* E);
static void SendOutPlayerMon(PkbEngine* E);
static void InitBattleMon(PkbEngine* E);
static void InitEnemyMon(PkbEngine* E);
static uint8_t CheckPlayerPartyForFitMon(PkbEngine* E);
static bool CheckIfCurPartyMonIsFitToFight(PkbEngine* E);
static void AddBattleParticipant(PkbEngine* E);
static void UpdateFaintedPlayerMon(PkbEngine* E);
static void ForcePlayerMonChoice(PkbEngine* E, uint8_t* c);
static void ResetEnemyBattleVars(PkbEngine* E);
static void LoadEnemyMonToSwitchTo(PkbEngine* E, uint8_t b);
static void ShowSetEnemyMonAndSendOutAnimation(PkbEngine* E);
static void PlayerPartyMonEntrance(PkbEngine* E);
static void DoubleSwitch(PkbEngine* E);
static void ChangeHappiness(PkbEngine* E, uint8_t c);
static void SendOutMonText(PkbEngine* E);

/* SmallFarFlagAction on a one-byte flag array */
static void flag_set(PkbEngine* E, uint16_t hl, uint8_t bit, bool on) {
    if(on) M(hl) |= (uint8_t)(1u << bit);
    else M(hl) &= (uint8_t)~(1u << bit);
}

static void set_battle_result(PkbEngine* E, uint8_t result) {
    W(wBattleResult) = (uint8_t)((W(wBattleResult) & BATTLERESULT_BITMASK) + result);
}

/* ---- DoBattle ---- */

PKB_TEXT2 void DoBattle(PkbEngine* E) {
    W(wBattleParticipantsNotFainted) = 0;
    W(wBattleParticipantsIncludingFainted) = 0;
    W(wBattlePlayerAction) = 0;
    W(wBattleEnded) = 0;
    W(wBattleHasJustStarted) = 1;
    uint8_t d = BATTLEACTION_SWITCH1 - 1;
    for(uint16_t hl = wOTPartyMon1HP;; hl += PARTYMON_STRUCT_LENGTH) {
        d++;
        if(M(hl) | M(hl + 1)) break;
    }
    W(wBattleAction) = d;

    if(!is_master(E)) {
        /* .not_linked: the slave sends out the enemy first */
        W(wEnemySwitchMonIndex) = 0;
        NewEnemyMonStatus(E);
        ResetEnemyStatLevels(E);
        BreakAttraction(E);
        EnemySwitch(E);
    }
    /* .player_2 */
    if(!CheckPlayerPartyForFitMon(E)) {
        LostBattle(E);
        return;
    }
    W(wCurPartyMon) = 0;
    while(!CheckIfCurPartyMonIsFitToFight(E)) W(wCurPartyMon)++;
    W(wLastPlayerMon) = W(wCurBattleMon);
    W(wCurBattleMon) = W(wCurPartyMon);
    W(wCurPartySpecies) = W(wPartySpecies + W(wCurPartyMon));
    W(wTempBattleMonSpecies) = W(wCurPartySpecies);
    ResetBattleParticipants(E);
    InitBattleMon(E);
    ResetPlayerStatLevels(E);
    NewBattleMonStatus(E);
    BreakAttraction(E);
    SendOutPlayerMon(E);
    SetPlayerTurn(E);
    SpikesDamage(E);
    if(is_master(E)) {
        W(wEnemySwitchMonIndex) = 0;
        NewEnemyMonStatus(E);
        ResetEnemyStatLevels(E);
        BreakAttraction(E);
        EnemySwitch(E);
        SetEnemyTurn(E);
        SpikesDamage(E);
    }
    BattleTurn(E);
}

static void WildFled_EnemyFled_LinkBattleCanceled(PkbEngine* E) {
    set_battle_result(E, DRAW);
    /* linked: the result is WIN (the other side forfeited) */
    W(wBattleResult) &= BATTLERESULT_BITMASK;
    SetPlayerTurn(E);
    W(wBattleEnded) = 1;
}

static void BattleTurn(PkbEngine* E) {
    for(;;) {
        if(E->aborted) return;
        if(E->io->on_turn) E->io->on_turn(E->io->ctx, E);
        W(wPlayerIsSwitching) = 0;
        W(wEnemyIsSwitching) = 0;
        E->peer_enemy_switching = E->peer_player_switching = false;
        W(wBattleHasJustStarted) = 0;
        W(wPlayerJustGotFrozen) = 0;
        W(wEnemyJustGotFrozen) = 0;
        W(wCurDamage) = 0;
        W(wCurDamage + 1) = 0;

        HandleBerserkGene(E);
        UpdateBattleMonInParty(E);
        /* AIChooseMove: returns at once in link battles */

        if(!CheckPlayerLockedIn(E)) {
            for(;;) { /* .loop1 */
                if(BattleMenu(E)) return;
                if(W(wBattleEnded)) return;
                if(W(wForcedSwitch)) return;
                if(!ParsePlayerAction(E)) break;
            }
        } else {
            ParsePlayerAction(E); /* .skip_iteration; locked in never returns nz */
        }

        if(EnemyTriesToFlee(E)) return;
        if(DetermineMoveOrder(E)) Battle_PlayerFirst(E);
        else Battle_EnemyFirst(E);

        if(W(wForcedSwitch)) return;
        if(W(wBattleEnded)) return;
        HandleBetweenTurnEffects(E);
        if(W(wBattleEnded)) return;
    }
}

static void HandleBetweenTurnEffects(PkbEngine* E) {
    bool (*check)(PkbEngine*) = H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK ?
                                    CheckFaint_EnemyThenPlayer :
                                    CheckFaint_PlayerThenEnemy;
    if(check(E)) return;
    HandleFutureSight(E);
    if(check(E)) return;
    HandleWeather(E);
    if(check(E)) return;
    HandleWrap(E);
    if(check(E)) return;
    HandlePerishSong(E);
    if(check(E)) return;
    HandleLeftovers(E);
    HandleMysteryberry(E);
    HandleDefrost(E);
    HandleSafeguard(E);
    HandleScreens(E);
    HandleStatBoostingHeldItems(E);
    HandleHealingItems(E);
    UpdateBattleMonInParty(E);
    HandleEncore(E);
}

/* returns carry: the battle is over */
static bool CheckFaint_PlayerThenEnemy(PkbEngine* E) {
    if(HasPlayerFainted(E)) {
        HandlePlayerMonFaint(E);
        if(W(wBattleEnded)) return true;
    }
    if(HasEnemyFainted(E)) {
        HandleEnemyMonFaint(E);
        if(W(wBattleEnded)) return true;
    }
    return false;
}

static bool CheckFaint_EnemyThenPlayer(PkbEngine* E) {
    if(HasEnemyFainted(E)) {
        HandleEnemyMonFaint(E);
        if(W(wBattleEnded)) return true;
    }
    if(HasPlayerFainted(E)) {
        HandlePlayerMonFaint(E);
        if(W(wBattleEnded)) return true;
    }
    return false;
}

static void HandleBerserkGene_side(PkbEngine* E) {
    uint8_t b, c;
    uint16_t hl = GetUserItem(E, &b, &c);
    W(wNamedObjectIndex) = M(hl);
    if(M(hl) != BERSERK_GENE) return;
    M(hl) = 0;
    M(player_turn(E) ? GetPartyLocation(wPartyMon1Item, W(wCurBattleMon)) :
                       GetPartyLocation(wOTPartyMon1Item, W(wCurOTMon))) = 0;
    /* the confusion count stays as it was (game bug) */
    uint16_t ss3 = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3);
    uint8_t was = M(ss3);
    M(ss3) |= 1 << SUBSTATUS_CONFUSED;
    uint16_t anim = GetBattleVarAddr(E, BATTLE_VARS_MOVE_ANIM);
    uint8_t a = M(anim);
    M(anim) = 0;
    W(wAttackMissed) = 0;
    W(wEffectFailed) = 0;
    /* BattleCommand_AttackUp2; MinimizeDropSub reads the zeroed animation */
    RaiseStat(E, 0x10 | ATTACK);
    M(anim) = a;
    if(was & (1 << SUBSTATUS_CONFUSED)) return;
    W(wBattleAfterAnim) = 0;
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3) & (1 << SUBSTATUS_FLYING | 1 << SUBSTATUS_UNDERGROUND))) {
        W(wFXAnimID) = ANIM_CONFUSED & 0xFF; /* Call_PlayBattleAnim_OnlyIfVisible */
        W(wFXAnimID + 1) = ANIM_CONFUSED >> 8;
    }
    SwitchTurnCore(E);
}

static void HandleBerserkGene(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleBerserkGene_side(E);
        SetPlayerTurn(E);
        HandleBerserkGene_side(E);
    } else {
        SetPlayerTurn(E);
        HandleBerserkGene_side(E);
        SetEnemyTurn(E);
        HandleBerserkGene_side(E);
    }
}

/* returns carry: the other side forfeited */
static bool EnemyTriesToFlee(PkbEngine* E) {
    if(W(wBattleAction) != BATTLEACTION_FORFEIT) return false;
    WildFled_EnemyFled_LinkBattleCanceled(E);
    return true;
}

/* GetMovePriority: 0-3 */
static uint8_t GetMovePriority(PkbEngine* E, uint8_t move) {
    if(move == VITAL_THROW) return 0;
    if(move == 0 || move > NUM_ATTACKS) { /* the asm reads past the Moves table */
        unsupported(E, 0xFF, 0xFF);
        return BASE_PRIORITY;
    }
    uint8_t effect = GetMoveEffect(move);
    for(const uint8_t* p = pb_move_effect_priorities;; p += 2) {
        if(p[0] == effect) return p[1];
        if(p[0] == 0xFF) return BASE_PRIORITY;
    }
}

/* returns carry: the player goes first */
static bool DetermineMoveOrder(PkbEngine* E) {
    uint8_t a = W(wBattleAction);
    bool use_move = a == BATTLEACTION_STRUGGLE || a == BATTLEACTION_SKIPTURN || a < BATTLEACTION_SWITCH1;
    if(!use_move) {
        if(W(wBattlePlayerAction) == BATTLEPLAYERACTION_SWITCH) {
            /* both switch: random order, master's view mirrored */
            uint8_t r = BattleRandom(E, "DetermineMoveOrder");
            if(!is_master(E)) return r < PERCENT(50) + 1;
            return !(r < PERCENT(50) + 1);
        }
        /* .switch: the enemy switches before anyone moves */
        AI_Switch(E);
        SetEnemyTurn(E);
        SpikesDamage(E);
        return false;
    }
    /* .use_move */
    if(W(wBattlePlayerAction) != BATTLEPLAYERACTION_USEMOVE) return true;
    uint8_t pp = GetMovePriority(E, W(wCurPlayerMove)), ep = GetMovePriority(E, W(wCurEnemyMove));
    if(ep != pp) return ep < pp; /* cp b: carry if enemy < player */

    /* .equal_priority: Quick Claw */
    uint8_t ue, up, oe, op = 0;
    SetPlayerTurn(E);
    GetUserItem(E, &ue, &up);
    GetOpponentItem(E, &oe, &op);
    if(ue == HELD_QUICK_CLAW) {
        if(oe == HELD_QUICK_CLAW) {
            if(!is_master(E)) {
                if(BattleRandom(E, "DetermineMoveOrder") < op) return false;
                if(BattleRandom(E, "DetermineMoveOrder") < up) return true;
            } else {
                if(BattleRandom(E, "DetermineMoveOrder") < up) return true;
                if(BattleRandom(E, "DetermineMoveOrder") < op) return false;
            }
        } else if(BattleRandom(E, "DetermineMoveOrder") < up) {
            return true;
        }
    } else if(oe == HELD_QUICK_CLAW) {
        if(BattleRandom(E, "DetermineMoveOrder") < op) return false;
    }
    /* .speed_check */
    uint16_t ps = rd16(E, wBattleMonSpeed), es = rd16(E, wEnemyMonSpeed);
    if(ps != es) return ps > es;
    /* .speed_tie */
    uint8_t r = BattleRandom(E, "DetermineMoveOrder");
    if(!is_master(E)) return r < PERCENT(50) + 1;
    return !(r < PERCENT(50) + 1);
}

/* returns carry: locked in (recharge, charged/rampage, rollout) */
static bool CheckPlayerLockedIn(PkbEngine* E) {
    if(W(wPlayerSubStatus4) & (1 << SUBSTATUS_RECHARGE)) return true;
    W(wEnemySubStatus3) &= (uint8_t)~(1 << SUBSTATUS_FLINCHED);
    W(wPlayerSubStatus3) &= (uint8_t)~(1 << SUBSTATUS_FLINCHED);
    if(W(wPlayerSubStatus3) & (1 << SUBSTATUS_CHARGED | 1 << SUBSTATUS_RAMPAGE)) return true;
    if(W(wPlayerSubStatus1) & (1 << SUBSTATUS_ROLLOUT)) return true;
    return false;
}

static void reset_player_counters(PkbEngine* E) {
    W(wPlayerFuryCutterCount) = 0;
    W(wPlayerProtectCount) = 0;
    W(wPlayerRageCounter) = 0;
    W(wPlayerSubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RAGE);
}

/* CheckPlayerHasUsableMoves: returns z (false) if Struggle must be used */
static bool CheckPlayerHasUsableMoves(PkbEngine* E) {
    W(wCurPlayerMove) = STRUGGLE;
    uint8_t dis = W(wPlayerDisableCount);
    uint8_t a;
    if(!dis) {
        a = (uint8_t)((W(wBattleMonPP) | W(wBattleMonPP + 1) | W(wBattleMonPP + 2) | W(wBattleMonPP + 3)) & PP_MASK);
        return a != 0;
    }
    /* .disabled: OR of the PP bytes of the other moves (whole bytes: PP Up bits count) */
    uint8_t b = (uint8_t)(((dis >> 4) | (dis << 4)) & 0x0F);
    a = 0;
    for(int i = 0; i < NUM_MOVES; i++) {
        uint8_t c = W(wBattleMonPP + i);
        if(--b != 0) a |= c;
    }
    return a != 0;
}

/* MoveSelectionScreen for the player: the controller picks the slot.
 * returns nz (true) if the menu was cancelled (never: the controller always picks). */
static bool MoveSelectionScreen(PkbEngine* E) {
    if(!CheckPlayerHasUsableMoves(E)) return false; /* ret z: use Struggle */
    uint8_t slot = E->menu_slot & 3; /* chosen at BattleMenu */
    /* MoveInfoBox, redrawn for the move under the cursor, has side effects:
     * the last redraw is for the chosen move (which is never disabled) */
    uint8_t dis = W(wPlayerDisableCount);
    if(!dis || (uint8_t)(dis >> 4) != slot + 1) {
        SetPlayerTurn(E);
        W(wCurPlayerMove) = W(wBattleMonMoves + slot);
        W(wCurPartyMon) = W(wCurBattleMon);
        W(wMonType) = WILDMON;
        UpdateMoveData(E);
    }
    W(wMenuCursorY) = slot;
    W(wCurMoveNum) = slot;
    W(wCurPlayerMove) = W(wBattleMonMoves + slot);
    return false;
}

/* returns nz (true): back to the battle menu */
static bool ParsePlayerAction(PkbEngine* E) {
    if(CheckPlayerLockedIn(E)) goto locked_in;
    if(W(wPlayerSubStatus5) & (1 << SUBSTATUS_ENCORED)) {
        W(wCurPlayerMove) = W(wLastPlayerMove);
        goto encored;
    }
    if(W(wBattlePlayerAction) == BATTLEPLAYERACTION_SWITCH) {
        reset_player_counters(E);
        return false;
    }
    if(W(wBattlePlayerAction) != BATTLEPLAYERACTION_USEMOVE) {
        W(wPlayerSubStatus3) &= (uint8_t)~(1 << SUBSTATUS_BIDE);
        goto locked_in;
    }
    if(W(wPlayerSubStatus3) & (1 << SUBSTATUS_BIDE)) goto locked_in;
    W(wMoveSelectionMenuType) = 0;
    W(wFXAnimID) = POUND;
    if(MoveSelectionScreen(E)) return true;
encored:
    SetPlayerTurn(E);
    UpdateMoveData(E);
    E->player_sent_out = false;
    W(wPlayerCharging) = 0;
    if(W(wPlayerMoveStruct + MOVE_EFFECT) != EFFECT_FURY_CUTTER) W(wPlayerFuryCutterCount) = 0;
    if(W(wPlayerMoveStruct + MOVE_EFFECT) != EFFECT_RAGE) {
        W(wPlayerSubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RAGE);
        W(wPlayerRageCounter) = 0;
    }
    if(W(wPlayerMoveStruct + MOVE_EFFECT) != EFFECT_PROTECT && W(wPlayerMoveStruct + MOVE_EFFECT) != EFFECT_ENDURE) {
        W(wPlayerProtectCount) = 0;
    }
    goto continue_protect;
locked_in:
    reset_player_counters(E);
    /* its ParseEnemyAction reloads our struct (from the same move when we are
     * locked in by a charge, a rampage, Bide or Rollout) */
    E->peer_player_anim = W(wCurPlayerMove);
continue_protect:
    ParseEnemyAction(E);
    return false;
}

static void HandleEncore_player(PkbEngine* E) {
    if(!(W(wPlayerSubStatus5) & (1 << SUBSTATUS_ENCORED))) return;
    if(--W(wPlayerEncoreCount) != 0 && (W(wBattleMonPP + W(wCurMoveNum)) & PP_MASK)) return;
    W(wPlayerSubStatus5) &= (uint8_t)~(1 << SUBSTATUS_ENCORED);
    SetEnemyTurn(E);
}

static void HandleEncore_enemy(PkbEngine* E) {
    if(!(W(wEnemySubStatus5) & (1 << SUBSTATUS_ENCORED))) return;
    if(--W(wEnemyEncoreCount) != 0 && (W(wEnemyMonPP + W(wCurEnemyMoveNum)) & PP_MASK)) return;
    W(wEnemySubStatus5) &= (uint8_t)~(1 << SUBSTATUS_ENCORED);
    SetPlayerTurn(E);
}

static void HandleEncore(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        HandleEncore_enemy(E);
        HandleEncore_player(E);
    } else {
        HandleEncore_player(E);
        HandleEncore_enemy(E);
    }
}

/* ---- turn execution ---- */

static void EndOpponentProtectEndureDestinyBond(PkbEngine* E) {
    M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1_OPP)) &= (uint8_t)~(1 << SUBSTATUS_PROTECT | 1 << SUBSTATUS_ENDURE);
    M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS5_OPP)) &= (uint8_t)~(1 << SUBSTATUS_DESTINY_BOND);
}

static void EndUserDestinyBond(PkbEngine* E) {
    M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS5)) &= (uint8_t)~(1 << SUBSTATUS_DESTINY_BOND);
}

static void PlayerTurn_EndOpponentProtectEndureDestinyBond(PkbEngine* E) {
    SetPlayerTurn(E);
    EndUserDestinyBond(E);
    DoPlayerTurn(E);
    EndOpponentProtectEndureDestinyBond(E);
}

static void EnemyTurn_EndOpponentProtectEndureDestinyBond(PkbEngine* E) {
    SetEnemyTurn(E);
    EndUserDestinyBond(E);
    DoEnemyTurn(E);
    EndOpponentProtectEndureDestinyBond(E);
}

static void Battle_EnemyFirst(PkbEngine* E) {
    /* TryEnemyFlee: wild only */
    SetEnemyTurn(E);
    W(wEnemyGoesFirst) = 1;
    /* AI_SwitchOrTryItem returns nc in link battles */
    EnemyTurn_EndOpponentProtectEndureDestinyBond(E);
    if(W(wForcedSwitch)) return;
    if(HasPlayerFainted(E)) {
        HandlePlayerMonFaint(E);
        return;
    }
    if(HasEnemyFainted(E)) {
        HandleEnemyMonFaint(E);
        return;
    }
    /* .switch_item */
    SetEnemyTurn(E);
    if(ResidualDamage(E)) {
        HandleEnemyMonFaint(E);
        return;
    }
    PlayerTurn_EndOpponentProtectEndureDestinyBond(E);
    if(W(wForcedSwitch)) return;
    if(HasEnemyFainted(E)) {
        HandleEnemyMonFaint(E);
        return;
    }
    if(HasPlayerFainted(E)) {
        HandlePlayerMonFaint(E);
        return;
    }
    SetPlayerTurn(E);
    if(ResidualDamage(E)) {
        HandlePlayerMonFaint(E);
        return;
    }
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEMOVE;
}

static void Battle_PlayerFirst(PkbEngine* E) {
    W(wEnemyGoesFirst) = 0;
    SetEnemyTurn(E);
    /* AI_SwitchOrTryItem: nc in link battles */
    PlayerTurn_EndOpponentProtectEndureDestinyBond(E);
    if(W(wForcedSwitch)) return;
    if(HasEnemyFainted(E)) {
        HandleEnemyMonFaint(E);
        return;
    }
    if(HasPlayerFainted(E)) {
        HandlePlayerMonFaint(E);
        return;
    }
    SetPlayerTurn(E);
    if(ResidualDamage(E)) {
        HandlePlayerMonFaint(E);
        return;
    }
    EnemyTurn_EndOpponentProtectEndureDestinyBond(E);
    if(W(wForcedSwitch)) return;
    if(HasPlayerFainted(E)) {
        HandlePlayerMonFaint(E);
        return;
    }
    if(HasEnemyFainted(E)) {
        HandleEnemyMonFaint(E);
        return;
    }
    /* .switched_or_used_item */
    SetEnemyTurn(E);
    if(ResidualDamage(E)) {
        HandleEnemyMonFaint(E);
        return;
    }
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEMOVE;
}

bool HasUserFainted(PkbEngine* E) {
    return player_turn(E) ? HasPlayerFainted(E) : HasEnemyFainted(E);
}

bool HasEnemyFainted(PkbEngine* E) {
    return (W(wEnemyMonHP) | W(wEnemyMonHP + 1)) == 0;
}

bool HasPlayerFainted(PkbEngine* E) {
    return (W(wBattleMonHP) | W(wBattleMonHP + 1)) == 0;
}

/* returns z (true) if the user fainted before or because of residual damage */
static bool ResidualDamage(PkbEngine* E) {
    if(HasUserFainted(E)) return true;
    uint8_t status = GetBattleVar(E, BATTLE_VARS_STATUS);
    if(status & (1 << PSN | 1 << BRN)) {
        uint16_t bc = GetEighthMaxHP(E);
        uint16_t de = player_turn(E) ? wPlayerToxicCount : wEnemyToxicCount;
        if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS5) & (1 << SUBSTATUS_TOXIC)) {
            bc = GetSixteenthMaxHP(E);
            uint8_t n = (uint8_t)(M(de) + 1);
            M(de) = n;
            uint16_t hl = 0;
            do {
                hl = (uint16_t)(hl + bc);
            } while(--n);
            bc = hl;
        }
        SubtractHPFromUser(E, bc);
    }
    if(HasUserFainted(E)) return true;
    if(M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS4)) & (1 << SUBSTATUS_LEECH_SEED)) {
        uint16_t bc = GetEighthMaxHP(E);
        bc = SubtractHP(E, bc);
        RestoreHP(E, bc);
    }
    if(HasUserFainted(E)) return true;
    if(M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1)) & (1 << SUBSTATUS_NIGHTMARE)) {
        SubtractHPFromUser(E, GetQuarterMaxHP(E));
    }
    if(HasUserFainted(E)) return true;
    if(M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1)) & (1 << SUBSTATUS_CURSE)) {
        SubtractHPFromUser(E, GetQuarterMaxHP(E));
    }
    return HasUserFainted(E);
}

static void HandlePerishSong_do(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerPerishCount : wEnemyPerishCount;
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1) & (1 << SUBSTATUS_PERISH))) return;
    if(--M(hl) != 0) return;
    M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1)) &= (uint8_t)~(1 << SUBSTATUS_PERISH);
    if(player_turn(E)) {
        wr16(E, wBattleMonHP, 0);
        wr16(E, GetPartyLocation(wPartyMon1HP, W(wCurBattleMon)), 0);
    } else {
        wr16(E, wEnemyMonHP, 0);
        if(W(wBattleMode) == WILD_BATTLE) return;
        wr16(E, GetPartyLocation(wOTPartyMon1HP, W(wCurOTMon)), 0);
    }
}

static void HandlePerishSong(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandlePerishSong_do(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        HandlePerishSong_do(E);
        SetEnemyTurn(E);
    }
    HandlePerishSong_do(E);
}

static void HandleWrap_do(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerWrapCount : wEnemyWrapCount;
    uint16_t de = player_turn(E) ? wPlayerTrappingMove : wEnemyTrappingMove;
    if(!M(hl)) return;
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_SUBSTITUTE)) return;
    W(wNamedObjectIndex) = M(de);
    W(wFXAnimID) = M(de);
    if(--M(hl) == 0) return; /* released */
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3) & (1 << SUBSTATUS_FLYING | 1 << SUBSTATUS_UNDERGROUND))) {
        W(wBattleAfterAnim) = 0;
        W(wFXAnimID + 1) = 0;
    }
    SubtractHPFromUser(E, GetSixteenthMaxHP(E));
}

static void HandleWrap(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleWrap_do(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        HandleWrap_do(E);
        SetEnemyTurn(E);
    }
    HandleWrap_do(E);
}

void SwitchTurnCore(PkbEngine* E) {
    H(hBattleTurn) ^= 1;
}

static void HandleLeftovers_do(PkbEngine* E) {
    uint8_t b, c;
    uint16_t hl = GetUserItem(E, &b, &c);
    W(wNamedObjectIndex) = M(hl);
    if(b != HELD_LEFTOVERS) return;
    uint16_t hp = player_turn(E) ? wBattleMonHP : wEnemyMonHP;
    if(M(hp) == M(hp + 2) && M(hp + 1) == M(hp + 3)) return;
    uint16_t bc = GetSixteenthMaxHP(E);
    SwitchTurnCore(E);
    RestoreHP(E, bc);
}

static void HandleLeftovers(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleLeftovers_do(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        HandleLeftovers_do(E);
        SetEnemyTurn(E);
    }
    HandleLeftovers_do(E);
}

static void HandleMysteryberry_do(PkbEngine* E) {
    uint8_t b, c;
    GetUserItem(E, &b, &c);
    if(b != HELD_RESTORE_PP) return;
    uint16_t de = GetPartyLocation(wPartyMon1PP, W(wCurBattleMon));
    uint16_t hl = GetPartyLocation(wPartyMon1Moves, W(wCurBattleMon));
    if(!player_turn(E)) {
        if(W(wBattleMode) == WILD_BATTLE) {
            de = wWildMonPP;
            hl = wWildMonMoves;
        } else {
            de = GetPartyLocation(wOTPartyMon1PP, W(wCurOTMon));
            hl = GetPartyLocation(wOTPartyMon1Moves, W(wCurOTMon));
        }
    }
    uint8_t i = 0;
    for(;; i++) { /* the first move with no PP left in the party struct */
        if(i == NUM_MOVES || !M(hl + i)) return;
        if(!(M(de + i) & PP_MASK)) break;
    }
    uint8_t amount = M(hl + i) == SKETCH ? 1 : 5; /* "lousy hack" */
    M(de + i) += amount;
    W(wTempByteValue) = M(hl + i);
    uint16_t pp = (uint16_t)((player_turn(E) ? wBattleMonPP : wEnemyMonPP) + i);
    uint16_t move = (uint16_t)((player_turn(E) ? wBattleMonMoves : wEnemyMonMoves) + i);
    uint8_t ss5 = player_turn(E) ? W(wPlayerSubStatus5) : W(wEnemySubStatus5);
    if(W(wTempByteValue) == M(move) && !(ss5 & (1 << SUBSTATUS_TRANSFORMED))) M(pp) += amount;
    hl = GetUserItem(E, &b, &c);
    W(wNamedObjectIndex) = M(hl);
    M(hl) = 0;
    if(player_turn(E)) {
        M(GetPartyLocation(wPartyMon1Item, W(wCurBattleMon))) = 0;
    } else if(W(wBattleMode) != WILD_BATTLE) {
        M(GetPartyLocation(wOTPartyMon1Item, W(wCurOTMon))) = 0;
    }
    SwitchTurnCore(E);
    ItemRecoveryAnim(E);
    SwitchTurnCore(E);
}

static void HandleMysteryberry(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleMysteryberry_do(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        HandleMysteryberry_do(E);
        SetEnemyTurn(E);
    }
    HandleMysteryberry_do(E);
}

static void HandleFutureSight_do(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerFutureSightCount : wEnemyFutureSightCount;
    if(!M(hl)) return;
    uint8_t a = (uint8_t)(M(hl) - 1);
    M(hl) = a;
    if(a != 1) return;
    uint16_t move = GetBattleVarAddr(E, BATTLE_VARS_MOVE);
    uint8_t saved = M(move);
    M(move) = FUTURE_SIGHT;
    UpdateMoveData(E);
    W(wAttackMissed) = 0;
    W(wAlreadyDisobeyed) = 0;
    W(wTypeModifier) = EFFECTIVE;
    DoMove(E);
    W(wCurDamage) = 0;
    W(wCurDamage + 1) = 0;
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE)) = saved;
    UpdateBattleMonInParty(E);
    UpdateEnemyMonInParty(E);
}

static void HandleFutureSight(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleFutureSight_do(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        HandleFutureSight_do(E);
        SetEnemyTurn(E);
    }
    HandleFutureSight_do(E);
}

static void HandleDefrost_player(PkbEngine* E) {
    if(!(W(wBattleMonStatus) & (1 << FRZ))) return;
    if(W(wPlayerJustGotFrozen)) return;
    if(BattleRandom(E, "HandleDefrost") >= PERCENT(10)) return;
    W(wBattleMonStatus) = 0;
    M(GetPartyLocation(wPartyMon1Status, W(wCurBattleMon))) = 0;
    SetEnemyTurn(E);
}

static void HandleDefrost_enemy(PkbEngine* E) {
    if(!(W(wEnemyMonStatus) & (1 << FRZ))) return;
    if(W(wEnemyJustGotFrozen)) return;
    if(BattleRandom(E, "HandleDefrost") >= PERCENT(10)) return;
    W(wEnemyMonStatus) = 0;
    if(W(wBattleMode) != WILD_BATTLE) M(GetPartyLocation(wOTPartyMon1Status, W(wCurOTMon))) = 0;
    SetPlayerTurn(E);
}

static void HandleDefrost(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        HandleDefrost_enemy(E);
        HandleDefrost_player(E);
    } else {
        HandleDefrost_player(E);
        HandleDefrost_enemy(E);
    }
}

static void HandleSafeguard_player(PkbEngine* E) {
    uint8_t a = W(wPlayerScreens);
    if(!(a & (1 << SCREENS_SAFEGUARD))) return;
    if(--W(wPlayerSafeguardCount) != 0) return;
    W(wPlayerScreens) = (uint8_t)(a & ~(1 << SCREENS_SAFEGUARD));
    H(hBattleTurn) = 0;
}

static void HandleSafeguard_enemy(PkbEngine* E) {
    uint8_t a = W(wEnemyScreens);
    if(!(a & (1 << SCREENS_SAFEGUARD))) return;
    if(--W(wEnemySafeguardCount) != 0) return;
    W(wEnemyScreens) = (uint8_t)(a & ~(1 << SCREENS_SAFEGUARD));
    H(hBattleTurn) = 1;
}

static void HandleSafeguard(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        HandleSafeguard_enemy(E);
        HandleSafeguard_player(E);
    } else {
        HandleSafeguard_player(E);
        HandleSafeguard_enemy(E);
    }
}

static void HandleScreens_tick(PkbEngine* E, uint16_t hl, uint16_t de) {
    if(M(hl) & (1 << SCREENS_LIGHT_SCREEN)) {
        if(--M(de) == 0) M(hl) &= (uint8_t)~(1 << SCREENS_LIGHT_SCREEN);
    }
    if(M(hl) & (1 << SCREENS_REFLECT)) {
        if(--M(de + 1) == 0) M(hl) &= (uint8_t)~(1 << SCREENS_REFLECT);
    }
}

static void HandleScreens(PkbEngine* E) {
    /* .Copy writes "Your"/"Enemy" to wStringBuffer1 (text only) */
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleScreens_tick(E, wEnemyScreens, wEnemyLightScreenCount);
    }
    SetPlayerTurn(E);
    HandleScreens_tick(E, wPlayerScreens, wPlayerLightScreenCount);
    if(H(hSerialConnectionStatus) != USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleScreens_tick(E, wEnemyScreens, wEnemyLightScreenCount);
    }
}

static void SandstormDamage(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3) & (1 << SUBSTATUS_UNDERGROUND)) return;
    uint16_t hl = player_turn(E) ? wBattleMonType1 : wEnemyMonType1;
    for(int i = 0; i < 2; i++) {
        uint8_t t = M(hl + i);
        if(t == ROCK || t == GROUND || t == STEEL) return;
    }
    W(wBattleAfterAnim) = 0;
    SubtractHPFromUser(E, GetEighthMaxHP(E));
}

static void HandleWeather(PkbEngine* E) {
    if(W(wBattleWeather) == WEATHER_NONE) return;
    if(--W(wWeatherCount) == 0) {
        W(wBattleWeather) = WEATHER_NONE;
        return;
    }
    if(W(wBattleWeather) != WEATHER_SANDSTORM) return;
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        SandstormDamage(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        SandstormDamage(E);
        SetEnemyTurn(E);
    }
    SandstormDamage(E);
}

/* ---- HP ---- */

void SubtractHPFromTarget(PkbEngine* E, uint16_t bc) {
    SubtractHP(E, bc);
}

void SubtractHPFromUser(PkbEngine* E, uint16_t bc) {
    SubtractHP(E, bc);
}

/* returns bc: the amount actually subtracted (the old HP if it underflowed) */
uint16_t SubtractHP(PkbEngine* E, uint16_t bc) {
    uint16_t hl = player_turn(E) ? wBattleMonHP : wEnemyMonHP;
    uint16_t hp = rd16(E, hl);
    W(wHPBuffer2) = M(hl + 1);
    W(wHPBuffer2 + 1) = M(hl);
    uint16_t nhp = (uint16_t)(hp - bc);
    wr16(E, hl, nhp);
    W(wHPBuffer3) = (uint8_t)nhp;
    W(wHPBuffer3 + 1) = (uint8_t)(nhp >> 8);
    if(bc <= hp) return bc;
    wr16(E, hl, 0);
    W(wHPBuffer3) = 0;
    W(wHPBuffer3 + 1) = 0;
    return hp;
}

uint16_t GetMaxHP(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
    W(wHPBuffer1 + 1) = M(hl);
    W(wHPBuffer1) = M(hl + 1);
    return rd16(E, hl);
}

uint16_t GetQuarterMaxHP(PkbEngine* E) {
    uint16_t bc = (uint16_t)(GetMaxHP(E) >> 2);
    if(!(bc & 0xFF)) bc++; /* "at least 1" checks only c */
    return bc;
}

uint16_t GetEighthMaxHP(PkbEngine* E) {
    uint16_t bc = GetQuarterMaxHP(E);
    uint8_t c = (uint8_t)((bc & 0xFF) >> 1);
    if(!c) c++;
    return (uint16_t)((bc & 0xFF00) | c);
}

uint16_t GetSixteenthMaxHP(PkbEngine* E) {
    uint16_t bc = GetQuarterMaxHP(E);
    uint8_t c = (uint8_t)((bc & 0xFF) >> 2);
    if(!c) c++;
    return (uint16_t)((bc & 0xFF00) | c);
}

uint16_t GetHalfMaxHP(PkbEngine* E) {
    uint16_t bc = (uint16_t)(GetMaxHP(E) >> 1);
    if(!bc) bc++;
    return bc;
}

/* RestoreHP: heals the opponent of the current turn (hl starts at the
 * other side's max HP) by bc, capping at max HP. */
void RestoreHP(PkbEngine* E, uint16_t bc) {
    uint16_t hl = player_turn(E) ? wEnemyMonMaxHP : wBattleMonMaxHP;
    uint16_t max = rd16(E, hl), hp = rd16(E, (uint16_t)(hl - 2));
    W(wHPBuffer1 + 1) = (uint8_t)(max >> 8);
    W(wHPBuffer1) = (uint8_t)max;
    W(wHPBuffer2) = (uint8_t)hp;
    W(wHPBuffer2 + 1) = (uint8_t)(hp >> 8);
    uint16_t nhp = (uint16_t)(hp + bc);
    wr16(E, (uint16_t)(hl - 2), nhp);
    W(wHPBuffer3) = (uint8_t)nhp;
    W(wHPBuffer3 + 1) = (uint8_t)(nhp >> 8);
    if(nhp >= max) { /* sub c / sbc b without carry: cap */
        wr16(E, (uint16_t)(hl - 2), max);
        W(wHPBuffer3 + 1) = (uint8_t)(max >> 8);
        W(wHPBuffer3) = (uint8_t)max;
    }
}

/* ---- fainting ---- */

static void HandleEnemyMonFaint_tail(PkbEngine* E);

static void UpdateBattleStateAndExperienceAfterEnemyFaint(PkbEngine* E) {
    UpdateBattleMonInParty(E);
    if(W(wBattleMode) != WILD_BATTLE) wr16(E, GetPartyLocation(wOTPartyMon1HP, W(wCurOTMon)), 0);
    W(wPlayerSubStatus3) &= (uint8_t)~(1 << SUBSTATUS_IN_LOOP);
    W(wEnemyDamageTaken) = 0;
    W(wEnemyDamageTaken + 1) = 0;
    NewEnemyMonStatus(E);
    BreakAttraction(E);
    if(!HasPlayerFainted(E) || W(wWhichMonFaintedFirst)) {
        /* .player_mon_did_not_faint */
    } else {
        UpdateFaintedPlayerMon(E);
    }
    if(!CheckPlayerPartyForFitMon(E)) return;
    W(wBattleResult) &= BATTLERESULT_BITMASK; /* WIN */
    /* Exp. Share halving of wEnemyMonBaseStats and GiveExperiencePoints
     * (which returns at once in link battles) */
    uint8_t holders = 0;
    for(uint8_t i = 0; i < W(wPartyCount); i++) {
        uint16_t mon = GetPartyLocation(wPartyMon1, i);
        if((M(mon + MON_HP) | M(mon + MON_HP + 1)) && M(mon + MON_ITEM) == EXP_SHARE) holders++;
    }
    if(holders) {
        for(uint16_t hl = wEnemyMonBaseStats; hl < wEnemyMonEnd; hl++) M(hl) >>= 1;
    }
    for(uint16_t i = 0; i < wEnemyMonEnd - wEnemyMonBaseStats; i++) {
        W(wBackupEnemyMonBaseStats + i) = W(wEnemyMonBaseStats + i);
    }
    W(wGivingExperienceToExpShareHolders) = 0;
    if(holders) {
        for(uint16_t i = 0; i < wEnemyMonEnd - wEnemyMonBaseStats; i++) {
            W(wEnemyMonBaseStats + i) = W(wBackupEnemyMonBaseStats + i);
        }
        W(wGivingExperienceToExpShareHolders) = 1;
    }
}

static bool CheckEnemyTrainerDefeated(PkbEngine* E) { /* returns z: all fainted */
    uint8_t a = 0;
    for(uint8_t i = 0; i < W(wOTPartyCount); i++) {
        uint16_t hl = GetPartyLocation(wOTPartyMon1HP, i);
        a |= M(hl) | M(hl + 1);
    }
    return a == 0;
}

static void WinTrainerBattle(PkbEngine* E) {
    W(wLowHealthAlarm) = 0;
    W(wBattleLowHealthAlarm) = 1;
    W(wBattleEnded) = 1;
}

static void HandleEnemyMonFaint(PkbEngine* E) {
    /* FaintEnemyPokemon, then FaintYourPokemon if ours fainted too (UI) */
    if(HasPlayerFainted(E)) W(wLowHealthAlarm) = 0; /* StopDangerSound */
    W(wWhichMonFaintedFirst) = 0;
    UpdateBattleStateAndExperienceAfterEnemyFaint(E);
    if(!CheckPlayerPartyForFitMon(E)) {
        LostBattle(E);
        return;
    }
    HandleEnemyMonFaint_tail(E);
}

static void HandleEnemyMonFaint_tail(PkbEngine* E) {
    if(CheckEnemyTrainerDefeated(E)) {
        WinTrainerBattle(E);
        return;
    }
    if(HasPlayerFainted(E)) {
        /* AskUseNextPokemon returns nc in trainer battles */
        uint8_t c;
        ForcePlayerMonChoice(E, &c);
        W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEITEM;
        if(HandleEnemySwitch(E)) {
            WildFled_EnemyFled_LinkBattleCanceled(E);
            return;
        }
        DoubleSwitch(E);
        return;
    }
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEITEM;
    if(HandleEnemySwitch(E)) {
        WildFled_EnemyFled_LinkBattleCanceled(E);
        return;
    }
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEMOVE;
}

static void DoubleSwitch(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        uint8_t cur = W(wCurPartyMon);
        EnemyPartyMonEntrance(E, true);
        W(wCurPartyMon) = cur;
        PlayerPartyMonEntrance(E);
    } else {
        PlayerPartyMonEntrance(E);
        EnemyPartyMonEntrance(E, true);
    }
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEMOVE;
}

/* returns z (true): the other side forfeited */
static bool HandleEnemySwitch(PkbEngine* E) {
    LinkBattleSendReceiveAction(E);
    if(W(wBattleAction) == BATTLEACTION_FORFEIT) return true;
    if(!HasPlayerFainted(E)) {
        EnemyPartyMonEntrance(E, false);
        return false; /* inc a: nz */
    }
    return false; /* inc a from 0: nz */
}

static void EnemyPartyMonEntrance(PkbEngine* E, bool set_mode) {
    W(wEnemySwitchMonIndex) = 0;
    NewEnemyMonStatus(E);
    ResetEnemyStatLevels(E);
    BreakAttraction(E);
    if(set_mode) EnemySwitch_SetMode(E);
    else EnemySwitch(E);
    ResetBattleParticipants(E);
    SetEnemyTurn(E);
    SpikesDamage(E);
    W(wEnemyMoveStruct + MOVE_ANIM) = 0;
    W(wBattlePlayerAction) = 0;
}

static void HandlePlayerMonFaint(PkbEngine* E) {
    W(wLowHealthAlarm) = 0; /* FaintYourPokemon: StopDangerSound */
    W(wWhichMonFaintedFirst) = 1;
    UpdateFaintedPlayerMon(E);
    if(!CheckPlayerPartyForFitMon(E)) {
        LostBattle(E);
        return;
    }
    if(HasEnemyFainted(E)) {
        UpdateBattleStateAndExperienceAfterEnemyFaint(E);
        if(CheckEnemyTrainerDefeated(E)) {
            WinTrainerBattle(E);
            return;
        }
    }
    /* .notfainted: AskUseNextPokemon returns nc in trainer battles */
    uint8_t c;
    ForcePlayerMonChoice(E, &c);
    if(c) return;
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEITEM;
    if(HandleEnemySwitch(E)) {
        WildFled_EnemyFled_LinkBattleCanceled(E);
        return;
    }
    DoubleSwitch(E);
}

static void UpdateFaintedPlayerMon(PkbEngine* E) {
    flag_set(E, wBattleParticipantsNotFainted, W(wCurBattleMon), false);
    W(wEnemySubStatus3) &= (uint8_t)~(1 << SUBSTATUS_IN_LOOP);
    W(wLowHealthAlarm) = 0;
    W(wPlayerDamageTaken) = 0;
    W(wPlayerDamageTaken + 1) = 0;
    W(wBattleMonStatus) = 0;
    UpdateBattleMonInParty(E);
    uint8_t c = HAPPINESS_FAINTED;
    if(W(wEnemyMonLevel) >= (uint8_t)(W(wBattleMonLevel) + 30)) c = HAPPINESS_BEATENBYSTRONGFOE;
    W(wCurPartyMon) = W(wCurBattleMon);
    ChangeHappiness(E, c);
    set_battle_result(E, LOSE);
}

/* ChangeHappiness (engine/events/happiness_egg.asm) for the two changes
 * battles make (data/events/happiness_changes.asm) */
static void ChangeHappiness(PkbEngine* E, uint8_t c) {
    static const int8_t fainted[3] = {-1, -1, -1}, strong_foe[3] = {-5, -5, -10};
    if(W(wPartySpecies + W(wCurPartyMon)) == EGG) return;
    uint16_t de = GetPartyLocation(wPartyMon1Happiness, W(wCurPartyMon));
    uint8_t a = M(de);
    uint8_t e = a < HAPPINESS_THRESHOLD_1 ? 0 : a < HAPPINESS_THRESHOLD_2 ? 1 : 2;
    uint8_t delta = (uint8_t)(c == HAPPINESS_FAINTED ? fainted[e] : strong_foe[e]);
    unsigned sum = (unsigned)a + delta;
    if(delta >= 0x64) a = sum > 0xFF ? (uint8_t)sum : 0; /* .negative */
    else a = sum > 0xFF ? 0xFF : (uint8_t)sum;
    M(de) = a;
    if(!W(wBattleMode)) return;
    /* compares the party index with the 1-based party menu cursor (as the asm) */
    if(W(wPartyMenuCursor) != W(wCurPartyMon)) return;
    W(wBattleMonHappiness) = a;
}

/* ForcePlayerMonChoice: c = 1 if our new mon was sent out, 0 if the enemy
 * mon has fainted too (both are replaced by DoubleSwitch) */
static void ForcePlayerMonChoice(PkbEngine* E, uint8_t* c) {
    /* SetUpBattlePartyMenu -> InitPartyMenuWithCancel; PartyMenuSelect sets
     * wCurPartyMon and wPartyMenuCursor */
    uint8_t slot = E->io->choose_replacement(E->io->ctx, E);
    W(wCurPartyMon) = slot;
    W(wPartyMenuCursor) = (uint8_t)(slot + 1);
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEITEM;
    LinkBattleSendReceiveAction(E);
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEMOVE;
    if(HasEnemyFainted(E)) {
        *c = 0;
        return;
    }
    W(wLastPlayerMon) = W(wCurBattleMon);
    W(wCurBattleMon) = W(wCurPartyMon);
    AddBattleParticipant(E);
    InitBattleMon(E);
    ResetPlayerStatLevels(E);
    SendOutMonText(E);
    NewBattleMonStatus(E);
    BreakAttraction(E);
    SendOutPlayerMon(E);
    SetPlayerTurn(E);
    SpikesDamage(E);
    *c = 1;
}

static void SendOutMonText(PkbEngine* E) {
    if(W(wBattleHasJustStarted)) return;
    if(HasEnemyFainted(E)) return;
    W(wEnemyHPAtTimeOfPlayerSwitch) = W(wEnemyMonHP);
    W(wEnemyHPAtTimeOfPlayerSwitch + 1) = W(wEnemyMonHP + 1);
}

static void PlayerPartyMonEntrance(PkbEngine* E) {
    W(wLastPlayerMon) = W(wCurBattleMon);
    W(wCurBattleMon) = W(wCurPartyMon);
    AddBattleParticipant(E);
    InitBattleMon(E);
    ResetPlayerStatLevels(E);
    SendOutMonText(E);
    NewBattleMonStatus(E);
    BreakAttraction(E);
    SendOutPlayerMon(E);
    SetPlayerTurn(E);
    SpikesDamage(E);
}

/* ---- Baton Pass and forced switches (Roar, Whirlwind) ---- */

uint8_t ForcePickSwitchMonInBattle(PkbEngine* E) {
    /* SetUpBattlePartyMenu; the controller picks a fit mon that is not out
     * (SwitchMonAlreadyOut), and PartyMenuSelect sets these */
    uint8_t slot = E->io->choose_replacement(E->io->ctx, E);
    W(wCurPartyMon) = slot;
    W(wPartyMenuCursor) = (uint8_t)(slot + 1);
    return slot;
}

void PassedBattleMonEntrance(PkbEngine* E) {
    E->peer_player_switching = true; /* its EnemySwitch_SetMode */
    W(wCurBattleMon) = W(wCurPartyMon);
    AddBattleParticipant(E);
    InitBattleMon(E);
    W(wApplyStatLevelMultipliersToEnemy) = 0;
    ApplyStatLevelMultiplierOnAllStats(E);
    SendOutPlayerMon(E);
    SetPlayerTurn(E);
    SpikesDamage(E);
}

bool SwitchPlayerMon(PkbEngine* E) {
    W(wLastPlayerMon) = W(wCurBattleMon);
    W(wCurBattleMon) = W(wCurPartyMon);
    AddBattleParticipant(E);
    InitBattleMon(E);
    ResetPlayerStatLevels(E);
    NewBattleMonStatus(E);
    BreakAttraction(E);
    SendOutPlayerMon(E);
    return (W(wEnemyMonHP) | W(wEnemyMonHP + 1)) != 0;
}

void ForceEnemySwitch(PkbEngine* E) {
    ResetEnemyBattleVars(E);
    LoadEnemyMonToSwitchTo(E, (uint8_t)(W(wEnemySwitchMonIndex) - 1));
    NewEnemyMonStatus(E);
    ResetEnemyStatLevels(E);
    ShowSetEnemyMonAndSendOutAnimation(E);
    BreakAttraction(E);
    ResetBattleParticipants(E);
}

/* engine/battle/ai/switch.asm */
bool FindAliveEnemyMons(PkbEngine* E) {
    if(W(wOTPartyCount) < 2) return true;
    for(uint8_t e = 0; e < W(wOTPartyCount); e++) {
        if(e == W(wCurOTMon)) continue;
        uint16_t hl = GetPartyLocation(wOTPartyMon1HP, e);
        if(M(hl) | M(hl + 1)) return false;
    }
    return true;
}

static void LostBattle(PkbEngine* E) {
    W(wBattleEnded) = 1;
    /* .LostLinkBattle */
    UpdateEnemyMonInParty(E);
    if(CheckEnemyTrainerDefeated(E)) set_battle_result(E, DRAW);
}

/* ---- enemy switching ---- */

static void ResetEnemyBattleVars(PkbEngine* E) {
    W(wLastPlayerCounterMove) = 0;
    W(wLastEnemyCounterMove) = 0;
    W(wLastEnemyMove) = 0;
    W(wCurEnemyMove) = 0;
    W(wEnemyItemState) = 0xFF;
    W(wPlayerWrapCount) = 0;
}

void ResetBattleParticipants(PkbEngine* E) {
    W(wBattleParticipantsNotFainted) = 0;
    W(wBattleParticipantsIncludingFainted) = 0;
    AddBattleParticipant(E);
}

static void AddBattleParticipant(PkbEngine* E) {
    flag_set(E, wBattleParticipantsNotFainted, W(wCurBattleMon), true);
    flag_set(E, wBattleParticipantsIncludingFainted, W(wCurBattleMon), true);
}

static void LoadEnemyMon(PkbEngine* E) {
    for(uint16_t hl = wEnemyMonSpecies; hl < wEnemyMonEnd; hl++) M(hl) = 0;
    InitEnemyMon(E);
}

static void LoadEnemyMonToSwitchTo(PkbEngine* E, uint8_t b) {
    W(wCurPartyMon) = b;
    W(wCurPartyLevel) = M(GetPartyLocation(wOTPartyMon1Level, b));
    W(wTempEnemyMonSpecies) = W(wOTPartySpecies + b);
    W(wCurPartySpecies) = W(wTempEnemyMonSpecies);
    LoadEnemyMon(E);
    W(wEnemyHPAtTimeOfPlayerSwitch) = W(wEnemyMonHP);
    W(wEnemyHPAtTimeOfPlayerSwitch + 1) = W(wEnemyMonHP + 1);
}

/* engine/gfx/color.asm CheckShininess: returns carry */
static bool CheckShininess(PkbEngine* E, uint16_t dvs) {
    if(!(M(dvs) & (0x2 << 4))) return false; /* SHINY_ATK_MASK */
    if((M(dvs) & 0x0F) != 10) return false;
    if((M(dvs + 1) & 0xF0) != 10 << 4) return false;
    return (M(dvs + 1) & 0x0F) == 10;
}

static uint16_t GetEnemyMonDVs(PkbEngine* E) {
    if(!(W(wEnemySubStatus5) & (1 << SUBSTATUS_TRANSFORMED))) return wEnemyMonDVs;
    return GetPartyLocation(wOTPartyMon1DVs, W(wCurOTMon)); /* not a wild battle */
}

static uint16_t GetPartyMonDVs(PkbEngine* E) {
    if(!(W(wPlayerSubStatus5) & (1 << SUBSTATUS_TRANSFORMED))) return wBattleMonDVs;
    return GetPartyLocation(wPartyMon1DVs, W(wCurBattleMon));
}

static void ShowSetEnemyMonAndSendOutAnimation(PkbEngine* E) {
    W(wCurPartySpecies) = W(wTempEnemyMonSpecies);
    W(wCurSpecies) = W(wTempEnemyMonSpecies);
    GetBaseData(E, W(wTempEnemyMonSpecies));
    W(wBattleAfterAnim) = 0;
    W(wBattleAnimParam) = 0;
    SetEnemyTurn(E);
    if(CheckShininess(E, GetEnemyMonDVs(E))) W(wBattleAnimParam) = 1; /* the shiny animation */
}

void EnemySwitch_SetMode(PkbEngine* E) {
    ResetEnemyBattleVars(E);
    /* CheckWhetherSwitchmonIsPredetermined: link battles use the action */
    LoadEnemyMonToSwitchTo(E, (uint8_t)(W(wBattleAction) - BATTLEACTION_SWITCH1));
    W(wEnemyIsSwitching) = 1;
    ShowSetEnemyMonAndSendOutAnimation(E);
}

static void EnemySwitch(PkbEngine* E) {
    /* CheckWhetherToAskSwitch returns nc in link battles */
    EnemySwitch_SetMode(E);
}

void NewEnemyMonStatus(PkbEngine* E) {
    W(wLastPlayerCounterMove) = 0;
    W(wLastEnemyCounterMove) = 0;
    W(wLastEnemyMove) = 0;
    for(int i = 0; i < 5; i++) W(wEnemySubStatus1 + i) = 0;
    W(wEnemyDisableCount) = 0;
    W(wEnemyFuryCutterCount) = 0;
    W(wEnemyProtectCount) = 0;
    W(wEnemyRageCounter) = 0;
    W(wEnemyDisabledMove) = 0;
    W(wEnemyMinimized) = 0;
    W(wPlayerWrapCount) = 0;
    W(wEnemyWrapCount) = 0;
    W(wEnemyTurnsTaken) = 0;
    W(wPlayerSubStatus5) &= (uint8_t)~(1 << SUBSTATUS_CANT_RUN);
}

void ResetEnemyStatLevels(PkbEngine* E) {
    for(int i = 0; i < NUM_LEVEL_STATS; i++) W(wEnemyStatLevels + i) = BASE_STAT_LEVEL;
}

static uint8_t CheckPlayerPartyForFitMon(PkbEngine* E) {
    uint8_t a = 0;
    for(uint8_t i = 0; i < W(wPartyCount); i++) {
        uint16_t hl = GetPartyLocation(wPartyMon1HP, i);
        a |= M(hl) | M(hl + 1);
    }
    return a;
}

/* returns nz (true) if wCurPartyMon has HP */
static bool CheckIfCurPartyMonIsFitToFight(PkbEngine* E) {
    uint16_t hl = GetPartyLocation(wPartyMon1HP, W(wCurPartyMon));
    return (M(hl) | M(hl + 1)) != 0;
}

/* ---- battle mons ---- */

void GetBaseData(PkbEngine* E, uint8_t species) {
    /* wCurBaseData: the fields this port has tables for */
    const PbBaseStats* b = &pb_base_stats[species - 1];
    W(wBaseDexNo) = species;
    for(int i = 0; i < 6; i++) W(wBaseStats + i) = b->stats[i];
    W(wBaseType1) = b->types[0];
    W(wBaseType2) = b->types[1];
    W(wBaseGender) = b->gender;
    W(wBaseGrowthRate) = b->growth_rate;
}

static void InitBattleMon(PkbEngine* E) {
    uint16_t hl = GetPartyParamLocation(E, MON_SPECIES);
    for(int i = 0; i < MON_OT_ID; i++) W(wBattleMonSpecies + i) = M(hl + i);
    for(int i = 0; i < MON_POKERUS - MON_DVS; i++) W(wBattleMonDVs + i) = M(hl + MON_DVS + i);
    for(int i = 0; i < PARTYMON_STRUCT_LENGTH - MON_LEVEL; i++) W(wBattleMonLevel + i) = M(hl + MON_LEVEL + i);
    W(wTempBattleMonSpecies) = W(wBattleMonSpecies);
    W(wCurPartySpecies) = W(wBattleMonSpecies);
    W(wCurSpecies) = W(wBattleMonSpecies);
    GetBaseData(E, W(wBattleMonSpecies));
    W(wBattleMonType1) = W(wBaseType1);
    W(wBattleMonType2) = W(wBaseType2);
    for(int i = 0; i < MON_NAME_LENGTH; i++) {
        W(wBattleMonNickname + i) = W(wPartyMonNicknames + MON_NAME_LENGTH * W(wCurBattleMon) + i);
    }
    for(int i = 0; i < PARTYMON_STRUCT_LENGTH - MON_ATK; i++) W(wPlayerStats + i) = W(wBattleMonAttack + i);
    ApplyStatusEffectOnPlayerStats(E);
    /* BadgeStatBoosts: none in link battles */
}

static void InitEnemyMon(PkbEngine* E) {
    uint16_t hl = GetPartyLocation(wOTPartyMon1Species, W(wCurPartyMon));
    for(int i = 0; i < MON_OT_ID; i++) W(wEnemyMonSpecies + i) = M(hl + i);
    for(int i = 0; i < MON_POKERUS - MON_DVS; i++) W(wEnemyMonDVs + i) = M(hl + MON_DVS + i);
    for(int i = 0; i < PARTYMON_STRUCT_LENGTH - MON_LEVEL; i++) W(wEnemyMonLevel + i) = M(hl + MON_LEVEL + i);
    W(wCurSpecies) = W(wEnemyMonSpecies);
    GetBaseData(E, W(wEnemyMonSpecies));
    for(int i = 0; i < MON_NAME_LENGTH; i++) {
        W(wEnemyMonNickname + i) = W(wOTPartyMonNicknames + MON_NAME_LENGTH * W(wCurPartyMon) + i);
    }
    for(int i = 0; i < PARTYMON_STRUCT_LENGTH - MON_ATK; i++) W(wEnemyStats + i) = W(wEnemyMonAttack + i);
    ApplyStatusEffectOnEnemyStats(E);
    W(wEnemyMonType1) = W(wBaseType1);
    W(wEnemyMonType2) = W(wBaseType2);
    for(int i = 0; i < NUM_STATS - 1; i++) W(wEnemyMonBaseStats + i) = W(wBaseStats + i);
    W(wCurOTMon) = W(wCurPartyMon);
    E->enemy_sent_out = true; /* the other Game Boy's SendOutPlayerMon */
}

void ResetPlayerStatLevels(PkbEngine* E) {
    for(int i = 0; i < NUM_LEVEL_STATS; i++) W(wPlayerStatLevels + i) = BASE_STAT_LEVEL;
}

static void CheckAmuletCoin(PkbEngine* E) {
    uint8_t b, c;
    GetItemHeldEffect(E, W(wBattleMonItem), &b, &c);
    if(b == HELD_AMULET_COIN) W(wAmuletCoin) = 1;
}

static void SendOutPlayerMon(PkbEngine* E) {
    /* GetUnownLetter (wUnownLetter) is only used for graphics here */
    W(wBattleMenuCursorPosition) = 0;
    W(wCurMoveNum) = 0;
    W(wTypeModifier) = 0;
    if(!E->player_sent_out) E->player_prev_anim = W(wPlayerMoveStruct + MOVE_ANIM);
    E->player_sent_out = true;
    W(wPlayerMoveStruct + MOVE_ANIM) = 0;
    W(wLastPlayerCounterMove) = 0;
    W(wLastEnemyCounterMove) = 0;
    W(wLastPlayerMove) = 0;
    CheckAmuletCoin(E);
    W(wEnemyWrapCount) = 0;
    SetPlayerTurn(E);
    W(wBattleAfterAnim) = 0;
    W(wBattleAnimParam) = 0;
    if(CheckShininess(E, GetPartyMonDVs(E))) W(wBattleAnimParam) = 1; /* the shiny animation */
}

void NewBattleMonStatus(PkbEngine* E) {
    W(wLastPlayerCounterMove) = 0;
    W(wLastEnemyCounterMove) = 0;
    W(wLastPlayerMove) = 0;
    for(int i = 0; i < 5; i++) W(wPlayerSubStatus1 + i) = 0;
    for(int i = 0; i < NUM_MOVES; i++) W(wPlayerUsedMoves + i) = 0;
    W(wPlayerDisableCount) = 0;
    W(wPlayerFuryCutterCount) = 0;
    W(wPlayerProtectCount) = 0;
    W(wPlayerRageCounter) = 0;
    W(wDisabledMove) = 0;
    W(wPlayerMinimized) = 0;
    W(wEnemyWrapCount) = 0;
    W(wPlayerWrapCount) = 0;
    W(wPlayerTurnsTaken) = 0;
    W(wEnemySubStatus5) &= (uint8_t)~(1 << SUBSTATUS_CANT_RUN);
}

void BreakAttraction(PkbEngine* E) {
    W(wPlayerSubStatus1) &= (uint8_t)~(1 << SUBSTATUS_IN_LOVE);
    W(wEnemySubStatus1) &= (uint8_t)~(1 << SUBSTATUS_IN_LOVE);
}

void SpikesDamage(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerScreens : wEnemyScreens;
    uint16_t de = player_turn(E) ? wBattleMonType : wEnemyMonType;
    if(!(M(hl) & (1 << SCREENS_SPIKES))) return;
    if(M(de) == FLYING || M(de + 1) == FLYING) return;
    SubtractHPFromTarget(E, GetEighthMaxHP(E));
}

/* returns carry: the switching mon fainted to Pursuit */
static bool PursuitSwitch(PkbEngine* E) {
    if(GetMoveEffect(GetBattleVar(E, BATTLE_VARS_MOVE)) != EFFECT_PURSUIT) return false;
    uint8_t cur = W(wCurBattleMon);
    if(player_turn(E)) {
        DoPlayerTurn(E);
    } else {
        W(wCurBattleMon) = W(wLastPlayerMon);
        DoEnemyTurn(E);
    }
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE)) = CANNOT_MOVE;
    W(wCurBattleMon) = cur;
    if(player_turn(E)) {
        if(W(wEnemyMonHP) | W(wEnemyMonHP + 1)) return false;
        return true;
    }
    UpdateBattleMon(E, W(wLastPlayerMon));
    if(W(wBattleMonHP) | W(wBattleMonHP + 1)) return false;
    /* the old mon's status stays in the party (game bug) */
    flag_set(E, wBattleParticipantsNotFainted, W(wLastPlayerMon), false);
    return true;
}

/* ItemRecoveryAnim: the RECOVER animation (it changes no other state) */
void ItemRecoveryAnim(PkbEngine* E) {
    W(wFXAnimID) = RECOVER;
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
}

/* engine/battle/consume_held_item.asm: hBattleTurn 0 takes the enemy's item */
static void ConsumeHeldItem(PkbEngine* E) {
    bool theirs = !H(hBattleTurn);
    uint16_t de = theirs ? wEnemyMonItem : wBattleMonItem;
    uint8_t b, c;
    GetItemHeldEffect(E, M(de), &b, &c);
    if(!in_list(pb_held_consumables, b)) return;
    M(de) = 0;
    uint16_t hl = theirs ? GetPartyLocation(wOTPartyMon1Item, W(wCurOTMon)) :
                           GetPartyLocation(wPartyMon1Item, W(wCurBattleMon));
    if(theirs && W(wBattleMode) == WILD_BATTLE) return;
    M(hl) = NO_ITEM;
}

void UseOpponentItem(PkbEngine* E) {
    uint8_t b, c;
    W(wNamedObjectIndex) = M(GetOpponentItem(E, &b, &c));
    ConsumeHeldItem(E);
}

/* Berries: the opponent heals c HP when below half its max HP */
static void HandleHPHealingItem(PkbEngine* E) {
    uint8_t b, c;
    GetOpponentItem(E, &b, &c);
    if(b != HELD_BERRY) return;
    uint16_t hp = player_turn(E) ? wEnemyMonHP : wBattleMonHP;
    uint16_t maxhp = (uint16_t)(hp + 2);
    uint16_t cur = rd16(E, hp), max = rd16(E, maxhp);
    W(wHPBuffer2) = (uint8_t)cur;
    W(wHPBuffer2 + 1) = (uint8_t)(cur >> 8);
    if((uint16_t)(cur << 1) >= max) return;
    ItemRecoveryAnim(E);
    W(wHPBuffer1 + 1) = (uint8_t)(max >> 8);
    W(wHPBuffer1) = (uint8_t)max;
    uint16_t nhp = (uint16_t)(cur + c);
    if(nhp > max) nhp = max;
    W(wHPBuffer3) = (uint8_t)nhp;
    W(wHPBuffer3 + 1) = (uint8_t)(nhp >> 8);
    wr16(E, hp, nhp);
    W(wWhichHPBar) = H(hBattleTurn);
    UseOpponentItem(E);
}

static void HandleHealingItems_side(PkbEngine* E) {
    HandleHPHealingItem(E);
    UseHeldStatusHealingItem(E);
    UseConfusionHealingItem(E);
}

static void HandleHealingItems(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        SetEnemyTurn(E);
        HandleHealingItems_side(E);
        SetPlayerTurn(E);
    } else {
        SetPlayerTurn(E);
        HandleHealingItems_side(E);
        SetEnemyTurn(E);
    }
    HandleHealingItems_side(E);
}

static void HandleStatBoostingHeldItems_side(PkbEngine* E, uint16_t item_addr, uint8_t turn) {
    uint8_t b, c;
    H(hBattleTurn) = turn;
    GetItemHeldEffect(E, M(item_addr), &b, &c);
    for(const uint8_t* p = pb_held_stat_up; *p != 0xFF; p++) {
        if(*p == b) unsupported(E, 0xFF, 0xFF); /* "not used in-game" */
    }
}

static void HandleStatBoostingHeldItems(PkbEngine* E) {
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        HandleStatBoostingHeldItems_side(E, wEnemyMonItem, 1);
        HandleStatBoostingHeldItems_side(E, wBattleMonItem, 0);
    } else {
        HandleStatBoostingHeldItems_side(E, wBattleMonItem, 0);
        HandleStatBoostingHeldItems_side(E, wEnemyMonItem, 1);
    }
}

/* ---- the player's menu ---- */

/* returns carry: the battle ended from the menu (never for FIGHT/PKMN) */
static bool BattleMenu(PkbEngine* E) {
    PkbChoice ch = {false, 0};
    E->io->choose_action(E->io->ctx, E, &ch);
    if(!ch.switch_mon) {
        E->menu_slot = ch.slot;
        W(wBattleMenuCursorPosition) = 1;
        W(wNumFleeAttempts) = 0; /* BattleMenu_Fight */
        return false;
    }
    /* BattleMenu_PKMN -> TryPlayerSwitch (the controller only picks valid targets) */
    W(wBattleMenuCursorPosition) = 2;
    W(wCurPartyMon) = ch.slot;
    W(wPartyMenuCursor) = (uint8_t)(ch.slot + 1);
    W(wLastPlayerMon) = W(wCurBattleMon);
    W(wBattlePlayerAction) = BATTLEPLAYERACTION_SWITCH;
    W(wCurBattleMon) = W(wCurPartyMon);
    return PlayerSwitch(E);
}

static void BattleMonEntrance(PkbEngine* E) {
    W(wPlayerSubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RAGE);
    SetEnemyTurn(E);
    PursuitSwitch(E);
    W(wCurPartyMon) = W(wCurBattleMon);
    AddBattleParticipant(E);
    InitBattleMon(E);
    ResetPlayerStatLevels(E);
    SendOutMonText(E);
    NewBattleMonStatus(E);
    BreakAttraction(E);
    SendOutPlayerMon(E);
    SetPlayerTurn(E);
    SpikesDamage(E);
    W(wMenuCursorY) = 2;
}

static void EnemyMonEntrance(PkbEngine* E) {
    AI_Switch(E);
    SetEnemyTurn(E);
    SpikesDamage(E);
}

/* returns carry (never) */
static bool PlayerSwitch(PkbEngine* E) {
    W(wPlayerIsSwitching) = 1;
    E->peer_player_switching = true; /* its AI_Switch */
    LinkBattleSendReceiveAction(E);
    ParseEnemyAction(E);
    uint8_t a = W(wBattleAction);
    if(a == BATTLEACTION_STRUGGLE || a == BATTLEACTION_SKIPTURN || a < BATTLEACTION_SWITCH1) {
        BattleMonEntrance(E);
        return false;
    }
    if(a == BATTLEACTION_FORFEIT) {
        WildFled_EnemyFled_LinkBattleCanceled(E);
        return false;
    }
    if(H(hSerialConnectionStatus) == USING_EXTERNAL_CLOCK) {
        EnemyMonEntrance(E);
        BattleMonEntrance(E);
    } else {
        BattleMonEntrance(E);
        EnemyMonEntrance(E);
    }
    return false;
}

static void ResetVarsForSubstatusRage(PkbEngine* E) {
    W(wEnemyFuryCutterCount) = 0;
    W(wEnemyProtectCount) = 0;
    W(wEnemyRageCounter) = 0;
    W(wEnemySubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RAGE);
}

/* returns nz (true) if locked in */
static bool CheckEnemyLockedIn(PkbEngine* E) {
    if(W(wEnemySubStatus4) & (1 << SUBSTATUS_RECHARGE)) return true;
    if(W(wEnemySubStatus3) & (1 << SUBSTATUS_CHARGED | 1 << SUBSTATUS_RAMPAGE | 1 << SUBSTATUS_BIDE)) return true;
    return (W(wEnemySubStatus1) & (1 << SUBSTATUS_ROLLOUT)) != 0;
}

static void ParseEnemyAction(PkbEngine* E) {
    if(W(wEnemyIsSwitching)) return;
    if(W(wBattlePlayerAction) == BATTLEPLAYERACTION_USEMOVE) LinkBattleSendReceiveAction(E);
    uint8_t a = W(wBattleAction);
    if(a == BATTLEACTION_STRUGGLE) {
        W(wCurEnemyMove) = STRUGGLE;
    } else if(a == BATTLEACTION_SKIPTURN) {
        W(wCurEnemyMove) = 0xFF;
    } else if(a >= BATTLEACTION_SWITCH1) {
        ResetVarsForSubstatusRage(E);
        return;
    } else {
        W(wCurEnemyMoveNum) = a;
        if(W(wEnemySubStatus1) & (1 << SUBSTATUS_ROLLOUT)) goto skip_load;
        if(W(wEnemySubStatus3) & (1 << SUBSTATUS_CHARGED | 1 << SUBSTATUS_RAMPAGE | 1 << SUBSTATUS_BIDE)) goto skip_load;
        if(W(wEnemySubStatus5) & (1 << SUBSTATUS_ENCORED)) W(wCurEnemyMove) = W(wLastEnemyMove);
        else W(wCurEnemyMove) = W(wEnemyMonMoves + a);
    }
skip_load:
    SetEnemyTurn(E);
    uint8_t peer_anim = E->peer_enemy_anim;
    UpdateMoveData(E);
    /* its ParsePlayerAction keeps its struct for a locked-in move */
    if(CheckEnemyLockedIn(E)) E->peer_enemy_anim = peer_anim;
    E->enemy_sent_out = false;
    if(!CheckEnemyLockedIn(E)) W(wEnemyCharging) = 0;
    if(W(wEnemyMoveStruct + MOVE_EFFECT) != EFFECT_FURY_CUTTER) W(wEnemyFuryCutterCount) = 0;
    if(W(wEnemyMoveStruct + MOVE_EFFECT) != EFFECT_RAGE) {
        W(wEnemySubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RAGE);
        W(wEnemyRageCounter) = 0;
    }
    if(W(wEnemyMoveStruct + MOVE_EFFECT) == EFFECT_PROTECT || W(wEnemyMoveStruct + MOVE_EFFECT) == EFFECT_ENDURE) {
        return;
    }
    W(wEnemyProtectCount) = 0;
}

/* AI_Switch (engine/battle/ai/items.asm), as used for link switches */
static void AI_Switch(PkbEngine* E) {
    W(wEnemyIsSwitching) = 1;
    E->peer_enemy_switching = true; /* its PlayerSwitch */
    W(wEnemyGoesFirst) = 1;
    W(wEnemySubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RAGE);
    H(hBattleTurn) = 0;
    PursuitSwitch(E);
    uint16_t de = GetPartyLocation(wOTPartyMon1Status, W(wCurOTMon));
    for(int i = 0; i < MON_MAXHP - MON_STATUS; i++) M(de + i) = W(wEnemyMonStatus + i);
    W(wBattleHasJustStarted) = 1;
    NewEnemyMonStatus(E);
    ResetEnemyStatLevels(E);
    W(wPlayerSubStatus1) &= (uint8_t)~(1 << SUBSTATUS_IN_LOVE);
    EnemySwitch(E);
    ResetBattleParticipants(E);
    W(wBattleHasJustStarted) = 0;
}

/* ---- stats ---- */

void ApplyPrzEffectOnSpeed(PkbEngine* E) {
    uint16_t hl;
    if(H(hBattleTurn)) {
        if(!(W(wBattleMonStatus) & (1 << PAR))) return;
        hl = wBattleMonSpeed;
    } else {
        if(!(W(wEnemyMonStatus) & (1 << PAR))) return;
        hl = wEnemyMonSpeed;
    }
    uint16_t v = (uint16_t)(rd16(E, hl) >> 2);
    M(hl) = (uint8_t)(v >> 8);
    M(hl + 1) = v ? (uint8_t)v : 1;
}

void ApplyBrnEffectOnAttack(PkbEngine* E) {
    uint16_t hl;
    if(H(hBattleTurn)) {
        if(!(W(wBattleMonStatus) & (1 << BRN))) return;
        hl = wBattleMonAttack;
    } else {
        if(!(W(wEnemyMonStatus) & (1 << BRN))) return;
        hl = wEnemyMonAttack;
    }
    uint16_t v = (uint16_t)(rd16(E, hl) >> 1);
    M(hl) = (uint8_t)(v >> 8);
    M(hl + 1) = v ? (uint8_t)v : 1;
}

/* hBattleTurn = 1 means "apply to the player" here (as the asm) */
void ApplyStatusEffectOnPlayerStats(PkbEngine* E) {
    H(hBattleTurn) = 1;
    ApplyPrzEffectOnSpeed(E);
    ApplyBrnEffectOnAttack(E);
}

void ApplyStatusEffectOnEnemyStats(PkbEngine* E) {
    H(hBattleTurn) = 0;
    ApplyPrzEffectOnSpeed(E);
    ApplyBrnEffectOnAttack(E);
}

static void ApplyStatLevelMultiplier(PkbEngine* E, uint8_t c) {
    bool enemy = W(wApplyStatLevelMultipliersToEnemy) != 0;
    uint16_t hl = enemy ? wEnemyMonAttack : wBattleMonAttack;
    uint16_t de = enemy ? wEnemyStats : wPlayerStats;
    uint16_t bc = enemy ? wEnemyAtkLevel : wPlayerAtkLevel;
    uint8_t level = M(bc + c);
    hl = (uint16_t)(hl + 2 * c);
    de = (uint16_t)(de + 2 * c);
    const uint8_t* mul = pb_stat_level_multipliers[level - 1];
    H(hMultiplicand + 0) = 0;
    H(hMultiplicand + 1) = M(de);
    H(hMultiplicand + 2) = M(de + 1);
    H(hMultiplier) = mul[0];
    Multiply(E);
    H(hDivisor) = mul[1];
    Divide(E, 4);
    unsigned q = (unsigned)(H(hQuotient + 2) << 8 | H(hQuotient + 3));
    if(q >= MAX_STAT_VALUE) {
        H(hQuotient + 2) = MAX_STAT_VALUE >> 8;
        H(hQuotient + 3) = MAX_STAT_VALUE & 0xFF;
    }
    M(hl) = H(hQuotient + 2);
    M(hl + 1) = H(hQuotient + 3);
    if(!(M(hl) | M(hl + 1))) M(hl + 1)++;
}

void ApplyStatLevelMultiplierOnAllStats(PkbEngine* E) {
    for(uint8_t c = 0; c < NUM_BATTLE_STATS; c++) ApplyStatLevelMultiplier(E, c);
}
