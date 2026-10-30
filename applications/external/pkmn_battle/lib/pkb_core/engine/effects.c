/*
 * engine/battle/effect_commands.asm: turn checks, the move-effect script
 * interpreter (DoMove) and the battle commands ported so far.
 *
 * Commands talk to each other through the Z80 registers b, c, d and e
 * (damagestats leaves power, level, defense and attack in them for
 * damagecalc); E->reg_* keep them between commands.
 */
#include "engine_i.h"

/* registers that survive from one command to the next (DoMove keeps bc and de) */
#define reg_b (E->reg_b)
#define reg_c (E->reg_c)
#define reg_d (E->reg_d)
#define reg_e (E->reg_e)

static void BattleCommand_DamageCalc(PkbEngine* E);
static void DoEnemyDamage(PkbEngine* E, bool ignore_sub);
static void DoPlayerDamage(PkbEngine* E, bool ignore_sub);

void BattleCommand_SwitchTurn(PkbEngine* E) {
    H(hBattleTurn) ^= 1;
}

bool CheckSubstituteOpp(PkbEngine* E) {
    return (GetBattleVar(E, BATTLE_VARS_SUBSTATUS4_OPP) & (1 << SUBSTATUS_SUBSTITUTE)) != 0;
}

bool CheckUserIsCharging(PkbEngine* E) {
    return (player_turn(E) ? W(wPlayerCharging) : W(wEnemyCharging)) != 0;
}

void ResetFuryCutterCount(PkbEngine* E) {
    W(player_turn(E) ? wPlayerFuryCutterCount : wEnemyFuryCutterCount) = 0;
}

static void EndRechargeOpp(PkbEngine* E) {
    res_var(E, BATTLE_VARS_SUBSTATUS4_OPP, 1 << SUBSTATUS_RECHARGE);
}

/* ---- script interpreter ---- */

void EndMoveEffect(PkbEngine* E) {
    uint16_t hl = (uint16_t)(W(wBattleScriptBufferAddress) | W(wBattleScriptBufferAddress + 1) << 8);
    M(hl) = PB_CMD_ENDMOVE;
    M(hl + 1) = PB_CMD_ENDMOVE;
    M(hl + 2) = PB_CMD_ENDMOVE;
}

static void run_command(PkbEngine* E, uint8_t cmd);

PKB_TEXT2 void DoMove(PkbEngine* E) {
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    const uint8_t* p = &pb_move_effect_scripts[pb_move_effect_script_off[effect]];
    uint16_t de = wBattleScriptBuffer;
    uint8_t a;
    do {
        a = *p++;
        M(de++) = a;
    } while(a != PB_CMD_ENDMOVE);
    W(wBattleScriptBufferAddress) = (uint8_t)(wBattleScriptBuffer & 0xFF);
    W(wBattleScriptBufferAddress + 1) = (uint8_t)(wBattleScriptBuffer >> 8);
    for(;;) {
        uint16_t hl = (uint16_t)(W(wBattleScriptBufferAddress) | W(wBattleScriptBufferAddress + 1) << 8);
        a = M(hl++);
        W(wBattleScriptBufferAddress) = (uint8_t)hl;
        W(wBattleScriptBufferAddress + 1) = (uint8_t)(hl >> 8);
        if(a >= PB_CMD_ENDTURN) return;
        run_command(E, a);
        if(E->leave_do_move) {
            E->leave_do_move = false;
            return;
        }
    }
}

void UpdateMoveData(PkbEngine* E) {
    uint16_t de = GetBattleVarAddr(E, BATTLE_VARS_MOVE_ANIM);
    uint8_t move = GetBattleVar(E, BATTLE_VARS_MOVE);
    W(wCurSpecies) = move;
    W(wNamedObjectIndex) = move;
    if(move == 0 || move > NUM_ATTACKS) { /* reads past the Moves table */
        unsupported(E, 0xFF, 0xFF);
        return;
    }
    GetMoveData(E, (uint8_t)(move - 1), de);
}

/* ---- DoPlayerTurn / DoEnemyTurn / DoTurn ---- */

static bool CheckTurn(PkbEngine* E);

PKB_TEXT2 static void DoTurn(PkbEngine* E) {
    E->move_user = H(hBattleTurn);
    W(wTurnEnded) = 0;
    CheckTurn(E);
    if(W(wTurnEnded)) return;
    UpdateMoveData(E);
    DoMove(E);
}

void DoPlayerTurn(PkbEngine* E) {
    SetPlayerTurn(E);
    if(W(wBattlePlayerAction) != BATTLEPLAYERACTION_USEMOVE) return;
    DoTurn(E);
}

void DoEnemyTurn(PkbEngine* E) {
    SetEnemyTurn(E);
    uint8_t a = W(wBattleAction);
    if(a != BATTLEACTION_STRUGGLE && a >= BATTLEACTION_SWITCH1) return;
    DoTurn(E);
}

/* ---- CheckTurn ---- */

static void EndTurn(PkbEngine* E) {
    W(wTurnEnded) = 1;
    ResetDamage(E);
}

static void CantMove(PkbEngine* E) {
    res_var(E, BATTLE_VARS_SUBSTATUS1, 1 << SUBSTATUS_ROLLOUT);
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3);
    M(hl) &= (uint8_t)~(1 << SUBSTATUS_BIDE | 1 << SUBSTATUS_RAMPAGE | 1 << SUBSTATUS_CHARGED);
    ResetFuryCutterCount(E);
    uint8_t anim = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
    uint8_t theirs = player_turn(E) ? E->peer_player_anim : E->peer_enemy_anim;
    if((M(hl) & (1 << SUBSTATUS_UNDERGROUND | 1 << SUBSTATUS_FLYING)) &&
       (anim == FLY || anim == DIG) != (theirs == FLY || theirs == DIG)) {
        game_desync(E, PKB_DESYNC_STALE_MOVE_STRUCT);
    }
    if(anim == FLY || anim == DIG) M(hl) &= (uint8_t)~(1 << SUBSTATUS_UNDERGROUND | 1 << SUBSTATUS_FLYING);
}

static void MoveDisabled(PkbEngine* E) {
    res_var(E, BATTLE_VARS_SUBSTATUS3, 1 << SUBSTATUS_CHARGED);
    W(wNamedObjectIndex) = GetBattleVar(E, BATTLE_VARS_MOVE);
}

/* TruncateHL_BC: returns b = l and c (one pass in the Colosseum) */
static void TruncateHL_BC(PkbEngine* E, uint16_t hl, uint16_t bc, uint8_t* b, uint8_t* c) {
    for(;;) {
        if(((hl >> 8) | (bc >> 8)) == 0) {
            if(W(wLinkMode) == LINK_COLOSSEUM) break;
            break; /* h | b == 0: done either way */
        }
        bc >>= 2;
        if(!bc) bc = 1; /* "or b; jr nz; inc c" */
        hl >>= 2;
        if(!hl) hl = 1;
        if(W(wLinkMode) == LINK_COLOSSEUM) break;
        if(((hl >> 8) | (bc >> 8)) == 0) break;
    }
    *b = (uint8_t)hl;
    *c = (uint8_t)bc;
}

static void HitSelfInConfusion(PkbEngine* E) {
    ResetDamage(E);
    uint16_t hl = player_turn(E) ? wBattleMonDefense : wEnemyMonDefense;
    uint16_t de = player_turn(E) ? wPlayerScreens : wEnemyScreens;
    uint8_t level = player_turn(E) ? W(wBattleMonLevel) : W(wEnemyMonLevel);
    uint16_t bc = rd16(E, hl);
    if(M(de) & (1 << SCREENS_REFLECT)) bc = (uint16_t)(bc << 1);
    uint16_t atk = rd16(E, (uint16_t)(hl - 2));
    TruncateHL_BC(E, atk, bc, &reg_b, &reg_c);
    reg_d = 40;
    reg_e = level;
}

/* After HitSelfInConfusion, with wCriticalHit left set before the self-hit
 * (a failed Snore): the player's version clears it, the enemy's keeps it, and
 * the other Game Boy runs the other version. That only matters if the HP left
 * differs, so work out both damages (DamageCalc only touches the HRAM math
 * bytes and wCurDamage, which are put back). */
static void CheckConfusionCritDesync(PkbEngine* E, bool left_crit, uint16_t hp_addr) {
    if(!left_crit) return;
    uint8_t crit = W(wCriticalHit), cur[2] = {W(wCurDamage), W(wCurDamage + 1)};
    uint8_t math[hMathBuffer + 5 - hProduct];
    for(unsigned i = 0; i < sizeof(math); i++) math[i] = H(hProduct + i);
    uint16_t hp = rd16(E, hp_addr), left[2];
    for(int k = 0; k < 2; k++) {
        W(wCriticalHit) = (uint8_t)k;
        BattleCommand_DamageCalc(E);
        uint16_t dmg = rd16(E, wCurDamage);
        left[k] = dmg < hp ? (uint16_t)(hp - dmg) : 0;
        W(wCurDamage) = cur[0];
        W(wCurDamage + 1) = cur[1];
        for(unsigned i = 0; i < sizeof(math); i++) H(hProduct + i) = math[i];
    }
    W(wCriticalHit) = crit;
    if(left[0] != left[1]) game_desync(E, PKB_DESYNC_CONFUSION_CRIT);
}

static void HitConfusion(PkbEngine* E) {
    /* the other Game Boy runs the enemy version, which keeps wCriticalHit */
    bool left_crit = W(wCriticalHit);
    W(wCriticalHit) = 0;
    HitSelfInConfusion(E);
    CheckConfusionCritDesync(E, left_crit, wBattleMonHP);
    BattleCommand_DamageCalc(E);
    BattleCommand_LowerSub(E);
    W(wBattleAfterAnim) = 0;
    DoPlayerDamage(E, true);
    BattleCommand_RaiseSub(E);
}

static bool CheckEnemyTurn(PkbEngine* E);

/* returns: turn ended (also in wTurnEnded) */
static bool CheckTurn(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_MOVE) == CANNOT_MOVE) {
        EndTurn(E);
        return true;
    }
    W(wAttackMissed) = 0;
    W(wEffectFailed) = 0;
    W(wBattleAnimParam) = 0;
    W(wAlreadyDisobeyed) = 0;
    W(wAlreadyFailed) = 0;
    W(wSomeoneIsRampaging) = 0;
    W(wTypeModifier) = EFFECTIVE;
    if(!player_turn(E)) return CheckEnemyTurn(E);

    if(W(wPlayerSubStatus4) & (1 << SUBSTATUS_RECHARGE)) {
        W(wPlayerSubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RECHARGE);
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(W(wBattleMonStatus) & SLP_MASK) {
        uint8_t a = (uint8_t)((W(wBattleMonStatus) & SLP_MASK) - 1);
        W(wBattleMonStatus) = a;
        if(a & SLP_MASK) {
            W(wBattleAfterAnim) = 0;
            if(W(wCurPlayerMove) != SNORE && W(wCurPlayerMove) != SLEEP_TALK) {
                CantMove(E);
                EndTurn(E);
                return true;
            }
        } else {
            CantMove(E);
            UpdateBattleMonInParty(E);
            W(wPlayerSubStatus1) &= (uint8_t)~(1 << SUBSTATUS_NIGHTMARE);
        }
    }
    if((W(wBattleMonStatus) & (1 << FRZ)) && W(wCurPlayerMove) != FLAME_WHEEL && W(wCurPlayerMove) != SACRED_FIRE) {
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(W(wPlayerSubStatus3) & (1 << SUBSTATUS_FLINCHED)) {
        W(wPlayerSubStatus3) &= (uint8_t)~(1 << SUBSTATUS_FLINCHED);
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(W(wPlayerDisableCount)) {
        uint8_t a = (uint8_t)(W(wPlayerDisableCount) - 1);
        W(wPlayerDisableCount) = a;
        if(!(a & 0x0F)) {
            W(wPlayerDisableCount) = 0;
            W(wDisabledMove) = 0;
        }
    }
    if(W(wPlayerSubStatus3) & (1 << SUBSTATUS_CONFUSED)) {
        if(--W(wPlayerConfuseCount) == 0) {
            W(wPlayerSubStatus3) &= (uint8_t)~(1 << SUBSTATUS_CONFUSED);
        } else {
            W(wBattleAfterAnim) = 0;
            if(BattleRandom(E, "CheckTurn.confused") < PERCENT(50) + 1) {
                W(wPlayerSubStatus3) &= 1 << SUBSTATUS_CONFUSED;
                HitConfusion(E);
                CantMove(E);
                EndTurn(E);
                return true;
            }
        }
    }
    if(W(wPlayerSubStatus1) & (1 << SUBSTATUS_IN_LOVE)) {
        W(wBattleAfterAnim) = 0;
        if(!(BattleRandom(E, "CheckTurn.in_love") < PERCENT(50) + 1)) {
            CantMove(E);
            EndTurn(E);
            return true;
        }
    }
    if(W(wDisabledMove) && W(wDisabledMove) == W(wCurPlayerMove)) {
        MoveDisabled(E);
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(!(W(wBattleMonStatus) & (1 << PAR))) return false;
    if(BattleRandom(E, "CheckTurn.paralyzed") >= PERCENT(25)) return false;
    CantMove(E);
    EndTurn(E);
    return true;
}

static bool CheckEnemyTurn(PkbEngine* E) {
    if(W(wEnemySubStatus4) & (1 << SUBSTATUS_RECHARGE)) {
        W(wEnemySubStatus4) &= (uint8_t)~(1 << SUBSTATUS_RECHARGE);
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(W(wEnemyMonStatus) & SLP_MASK) {
        uint8_t a = (uint8_t)((W(wEnemyMonStatus) & SLP_MASK) - 1);
        W(wEnemyMonStatus) = a;
        if(a) {
            W(wBattleAfterAnim) = 0;
            if(W(wCurEnemyMove) != SNORE && W(wCurEnemyMove) != SLEEP_TALK) {
                CantMove(E);
                EndTurn(E);
                return true;
            }
        } else {
            CantMove(E);
            UpdateEnemyMonInParty(E);
            W(wEnemySubStatus1) &= (uint8_t)~(1 << SUBSTATUS_NIGHTMARE);
        }
    }
    if((W(wEnemyMonStatus) & (1 << FRZ)) && W(wCurEnemyMove) != FLAME_WHEEL && W(wCurEnemyMove) != SACRED_FIRE) {
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(W(wEnemySubStatus3) & (1 << SUBSTATUS_FLINCHED)) {
        W(wEnemySubStatus3) &= (uint8_t)~(1 << SUBSTATUS_FLINCHED);
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(W(wEnemyDisableCount)) {
        uint8_t a = (uint8_t)(W(wEnemyDisableCount) - 1);
        W(wEnemyDisableCount) = a;
        if(!(a & 0x0F)) {
            W(wEnemyDisableCount) = 0;
            W(wEnemyDisabledMove) = 0;
        }
    }
    if(W(wEnemySubStatus3) & (1 << SUBSTATUS_CONFUSED)) {
        if(--W(wEnemyConfuseCount) == 0) {
            W(wEnemySubStatus3) &= (uint8_t)~(1 << SUBSTATUS_CONFUSED);
        } else {
            W(wBattleAfterAnim) = 0;
            if(BattleRandom(E, "CheckEnemyTurn.confused") < PERCENT(50) + 1) {
                W(wEnemySubStatus3) &= 1 << SUBSTATUS_CONFUSED;
                /* unlike HitConfusion, wCriticalHit is not cleared here, so a
                 * critical flag left over (a failed Snore) doubles the damage
                 * on this side only */
                HitSelfInConfusion(E);
                CheckConfusionCritDesync(E, W(wCriticalHit), wEnemyMonHP);
                BattleCommand_DamageCalc(E);
                BattleCommand_LowerSub(E);
                W(wBattleAfterAnim) = 0;
                DoEnemyDamage(E, true);
                BattleCommand_RaiseSub(E);
                CantMove(E);
                EndTurn(E);
                return true;
            }
        }
    }
    if(W(wEnemySubStatus1) & (1 << SUBSTATUS_IN_LOVE)) {
        W(wBattleAfterAnim) = 0;
        if(!(BattleRandom(E, "CheckEnemyTurn.in_love") < PERCENT(50) + 1)) {
            CantMove(E);
            EndTurn(E);
            return true;
        }
    }
    if(W(wEnemyDisabledMove) && W(wEnemyDisabledMove) == W(wCurEnemyMove)) {
        MoveDisabled(E);
        CantMove(E);
        EndTurn(E);
        return true;
    }
    if(!(W(wEnemyMonStatus) & (1 << PAR))) return false;
    if(BattleRandom(E, "CheckEnemyTurn.paralyzed") >= PERCENT(25)) return false;
    CantMove(E);
    EndTurn(E);
    return true;
}

/* ---- commands ---- */

static void BattleCommand_CheckObedience(PkbEngine* E) {
    /* the enemy can't disobey; link battles have no obedience check */
    (void)E;
}

static void UpdateUsedMoves(PkbEngine* E, uint8_t b) {
    for(int i = 0; i < NUM_MOVES; i++) {
        uint8_t a = W(wPlayerUsedMoves + i);
        if(!a) {
            W(wPlayerUsedMoves + i) = b;
            return;
        }
        if(a == b) return;
    }
    W(wPlayerUsedMoves + 0) = W(wPlayerUsedMoves + 1);
    W(wPlayerUsedMoves + 1) = W(wPlayerUsedMoves + 2);
    W(wPlayerUsedMoves + 2) = W(wPlayerUsedMoves + 3);
    W(wPlayerUsedMoves + 3) = b;
}

/* UsedMoveText's text_asm: the last-move variables are updated here */
static void BattleCommand_UsedMoveText(PkbEngine* E) {
    if(player_turn(E)) UpdateUsedMoves(E, W(wPlayerMoveStruct + MOVE_ANIM));
    uint16_t de = GetBattleVarAddr(E, BATTLE_VARS_LAST_MOVE);
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_LAST_COUNTER_MOVE);
    uint8_t a = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
    W(wMoveGrammar) = a; /* then turned into a grammar index (text only) */
    if(CheckUserIsCharging(E)) return;
    M(hl) = a;
    M(de) = a;
}

static bool CheckMimicUsed(PkbEngine* E) { /* returns carry */
    uint8_t c = player_turn(E) ? W(wCurMoveNum) : W(wCurEnemyMoveNum);
    uint16_t hl = UserPartyAttr(E, MON_MOVES);
    if(GetBattleVar(E, BATTLE_VARS_MOVE) == MIMIC) return false;
    return M(hl + c) == MIMIC;
}

/* .consume_pp: returns b (1 = out of PP) */
static uint8_t consume_pp(PkbEngine* E, uint16_t hl) {
    uint8_t c = player_turn(E) ? W(wCurMoveNum) : W(wCurEnemyMoveNum);
    hl = (uint16_t)(hl + c);
    if(!(M(hl) & PP_MASK)) return 1; /* .out_of_pp (text only) */
    M(hl)--;
    return 0;
}

static void BattleCommand_DoTurn(PkbEngine* E) {
    if(CheckUserIsCharging(E)) return;
    uint16_t hl = player_turn(E) ? wBattleMonPP : wEnemyMonPP;
    uint16_t de = player_turn(E) ? wPlayerSubStatus3 : wEnemySubStatus3;
    uint16_t bc = player_turn(E) ? wPlayerTurnsTaken : wEnemyTurnsTaken;
    M(bc)++;
    if(GetBattleVar(E, BATTLE_VARS_MOVE) == STRUGGLE) return;
    if(M(de) & (1 << SUBSTATUS_IN_LOOP | 1 << SUBSTATUS_RAMPAGE | 1 << SUBSTATUS_BIDE)) return;
    if(consume_pp(E, hl)) {
        EndMoveEffect(E);
        return;
    }
    if(M(de + 2) & (1 << SUBSTATUS_TRANSFORMED)) return;
    uint16_t party;
    if(player_turn(E)) {
        party = GetPartyLocation(wPartyMon1PP, W(wCurBattleMon));
    } else {
        party = GetPartyLocation(wOTPartyMon1PP, W(wCurOTMon));
    }
    if(CheckMimicUsed(E)) return;
    consume_pp(E, party);
}

static void BattleCommand_Critical(PkbEngine* E) {
    W(wCriticalHit) = 0;
    if(!GetBattleVar(E, BATTLE_VARS_MOVE_POWER)) return;
    uint16_t item = player_turn(E) ? wBattleMonItem : wEnemyMonItem;
    uint8_t species = player_turn(E) ? W(wBattleMonSpecies) : W(wEnemyMonSpecies);
    uint8_t c = 0;
    if(species == CHANSEY && M(item) == LUCKY_PUNCH) {
        c = 2;
        goto tally;
    }
    if(species == FARFETCH_D && M(item) == STICK) {
        c = 2;
        goto tally;
    }
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_FOCUS_ENERGY)) c++;
    {
        uint8_t anim = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
        for(const uint8_t* p = pb_critical_hit_moves; *p != 0xFF; p++) {
            if(*p == anim) {
                c += 2;
                break;
            }
        }
        uint8_t b, ic;
        GetUserItem(E, &b, &ic);
        if(b == HELD_CRITICAL_UP) c++;
    }
tally:
    if(BattleRandom(E, "BattleCommand_Critical") < pb_critical_hit_chances[c]) W(wCriticalHit) = 1;
}

static void DoWeatherModifiers(PkbEngine* E) {
    uint8_t b = W(wBattleWeather), c = W(wCurType);
    const uint8_t* de = NULL;
    for(const uint8_t* p = pb_weather_type_modifiers; *p != 0xFF; p += 3) {
        if(p[0] == b && p[1] == c) {
            de = p;
            break;
        }
    }
    if(!de) {
        c = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
        for(const uint8_t* p = pb_weather_move_modifiers; *p != 0xFF; p += 3) {
            if(p[0] == b && p[1] == c) {
                de = p;
                break;
            }
        }
    }
    if(!de) return;
    H(hMultiplicand + 0) = 0;
    H(hMultiplicand + 1) = W(wCurDamage);
    H(hMultiplicand + 2) = W(wCurDamage + 1);
    H(hMultiplier) = de[2];
    Multiply(E);
    H(hDivisor) = 10;
    Divide(E, 4);
    uint16_t bc;
    if(H(hQuotient + 1)) bc = 0xFFFF;
    else {
        bc = (uint16_t)(H(hQuotient + 2) << 8 | H(hQuotient + 3));
        if(!bc) bc = 1;
    }
    W(wCurDamage) = (uint8_t)(bc >> 8);
    W(wCurDamage + 1) = (uint8_t)bc;
}

void BattleCommand_Stab(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_MOVE_ANIM) == STRUGGLE) return;
    uint8_t b, c, d, e;
    if(player_turn(E)) {
        b = W(wBattleMonType1), c = W(wBattleMonType2), d = W(wEnemyMonType1), e = W(wEnemyMonType2);
    } else {
        b = W(wEnemyMonType1), c = W(wEnemyMonType2), d = W(wBattleMonType1), e = W(wBattleMonType2);
    }
    W(wCurType) = GetBattleVar(E, BATTLE_VARS_MOVE_TYPE);
    DoWeatherModifiers(E);
    /* DoBadgeTypeBoosts: none in link battles */
    if(W(wCurType) == b || W(wCurType) == c) {
        uint16_t hl = rd16(E, wCurDamage);
        hl = (uint16_t)(hl + (hl >> 1));
        wr16(E, wCurDamage, hl);
        W(wTypeModifier) |= STAB_DAMAGE;
    }
    uint8_t type = GetBattleVar(E, BATTLE_VARS_MOVE_TYPE);
    for(const uint8_t* p = pb_type_matchups;;) {
        uint8_t a = *p++;
        if(a == 0xFF) break;
        if(a == 0xFE) {
            if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1_OPP) & (1 << SUBSTATUS_IDENTIFIED)) break;
            continue;
        }
        if(a == type && (p[0] == d || p[0] == e)) {
            uint8_t stab = W(wTypeModifier) & STAB_DAMAGE;
            uint8_t mult = p[1];
            if(!mult) W(wAttackMissed) = 1;
            H(hMultiplier) = mult;
            W(wTypeModifier) = (uint8_t)(mult + stab);
            H(hMultiplicand + 0) = 0;
            H(hMultiplicand + 1) = W(wCurDamage);
            H(hMultiplicand + 2) = W(wCurDamage + 1);
            Multiply(E);
            if(H(hProduct + 1) | H(hProduct + 2) | H(hProduct + 3)) {
                H(hDivisor) = 10;
                Divide(E, 4);
                if(!(H(hQuotient + 2) | H(hQuotient + 3))) H(hMultiplicand + 2) = 1;
            }
            W(wCurDamage) = H(hMultiplicand + 1);
            W(wCurDamage + 1) = H(hMultiplicand + 2);
        }
        p += 2;
    }
    BattleCheckTypeMatchup(E);
    W(wTypeModifier) = (uint8_t)((W(wTypeModifier) & STAB_DAMAGE) | W(wTypeMatchup));
}

void BattleCheckTypeMatchup(PkbEngine* E) {
    CheckTypeMatchup(E, player_turn(E) ? wEnemyMonType1 : wBattleMonType1);
}

void CheckTypeMatchup(PkbEngine* E, uint16_t hl) {
    uint8_t d = GetBattleVar(E, BATTLE_VARS_MOVE_TYPE);
    uint8_t b = M(hl), c = M(hl + 1);
    W(wTypeMatchup) = EFFECTIVE;
    for(const uint8_t* p = pb_type_matchups;;) {
        uint8_t a = *p++;
        if(a == 0xFF) return;
        if(a == 0xFE) {
            if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1_OPP) & (1 << SUBSTATUS_IDENTIFIED)) return;
            continue;
        }
        if(a == d && (p[0] == b || p[0] == c)) {
            H(hDividend + 0) = 0;
            H(hMultiplicand + 0) = 0;
            H(hMultiplicand + 1) = 0;
            H(hMultiplicand + 2) = p[1];
            H(hMultiplier) = W(wTypeMatchup);
            Multiply(E);
            H(hDivisor) = 10;
            Divide(E, 4);
            W(wTypeMatchup) = H(hQuotient + 3);
        }
        p += 2;
    }
}

static void BattleCommand_DamageVariation(PkbEngine* E) {
    if(!W(wCurDamage) && W(wCurDamage + 1) < 2) return;
    H(hMultiplicand + 0) = 0;
    H(hMultiplicand + 1) = W(wCurDamage);
    H(hMultiplicand + 2) = W(wCurDamage + 1);
    uint8_t a;
    do {
        a = BattleRandom(E, "BattleCommand_DamageVariation");
        a = (uint8_t)(a >> 1 | a << 7); /* rrca */
    } while(a < PERCENT(85) + 1);
    H(hMultiplier) = a;
    Multiply(E);
    H(hDivisor) = PERCENT(100);
    Divide(E, 4);
    W(wCurDamage) = H(hQuotient + 2);
    W(wCurDamage + 1) = H(hQuotient + 3);
}

/* .StatModifiers: writes the modified accuracy into the move struct */
static void CheckHit_StatModifiers(PkbEngine* E) {
    uint16_t hl;
    uint8_t b, c;
    if(player_turn(E)) {
        hl = wPlayerMoveStruct + MOVE_ACC;
        b = W(wPlayerAccLevel);
        c = W(wEnemyEvaLevel);
    } else {
        hl = wEnemyMoveStruct + MOVE_ACC;
        b = W(wEnemyAccLevel);
        c = W(wPlayerEvaLevel);
    }
    if(!(c < b)) { /* cp b with a = evasion */
        if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1_OPP) & (1 << SUBSTATUS_IDENTIFIED)) return;
    }
    c = (uint8_t)(MAX_STAT_LEVEL + 1 - c);
    H(hMultiplicand + 0) = 0;
    H(hMultiplicand + 1) = 0;
    H(hMultiplicand + 2) = M(hl);
    for(int d = 2; d; d--) {
        const uint8_t* mul = pb_accuracy_level_multipliers[(uint8_t)(b - 1)];
        H(hMultiplier) = mul[0];
        Multiply(E);
        H(hDivisor) = mul[1];
        Divide(E, 4);
        if(!(H(hQuotient + 3) | H(hQuotient + 2))) {
            H(hQuotient + 2) = 0;
            H(hQuotient + 3) = 1;
        }
        b = c;
    }
    M(hl) = H(hQuotient + 2) ? 0xFF : H(hQuotient + 3);
}

static void BattleCommand_CheckHit(PkbEngine* E) {
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    /* .DreamEater */
    if(effect == EFFECT_DREAM_EATER && !(GetBattleVar(E, BATTLE_VARS_STATUS_OPP) & SLP_MASK)) goto miss;
    /* .Protect */
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1_OPP) & (1 << SUBSTATUS_PROTECT)) goto miss;
    /* .DrainSub */
    if(CheckSubstituteOpp(E) && (effect == EFFECT_LEECH_HIT || effect == EFFECT_DREAM_EATER)) goto miss;
    { /* .LockOn */
        uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS5_OPP);
        bool locked = M(hl) & (1 << SUBSTATUS_LOCK_ON);
        M(hl) &= (uint8_t)~(1 << SUBSTATUS_LOCK_ON);
        if(locked) {
            uint8_t anim = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
            if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_FLYING)) ||
               !(anim == EARTHQUAKE || anim == FISSURE || anim == MAGNITUDE)) {
                return;
            }
        }
    }
    { /* .FlyDigMoves */
        uint8_t ss3 = GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_FLYING | 1 << SUBSTATUS_UNDERGROUND);
        if(ss3) {
            uint8_t anim = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
            bool hits = (ss3 & (1 << SUBSTATUS_FLYING)) ?
                            (anim == GUST || anim == WHIRLWIND || anim == THUNDER || anim == TWISTER) :
                            (anim == EARTHQUAKE || anim == FISSURE || anim == MAGNITUDE);
            if(!hits) goto miss;
        }
    }
    if(effect == EFFECT_THUNDER && W(wBattleWeather) == WEATHER_RAIN) return;
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_X_ACCURACY)) return;
    if(effect == EFFECT_ALWAYS_HIT) return;
    CheckHit_StatModifiers(E);
    {
        uint8_t b = player_turn(E) ? W(wPlayerMoveStruct + MOVE_ACC) : W(wEnemyMoveStruct + MOVE_ACC);
        uint8_t ib, ic = 0;
        GetOpponentItem(E, &ib, &ic);
        if(ib == HELD_BRIGHTPOWDER) b = b >= ic ? (uint8_t)(b - ic) : 0;
        if(b == 0xFF) return;
        if(BattleRandom(E, "BattleCommand_CheckHit") < b) return;
    }
miss:
    if(GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT) != EFFECT_JUMP_KICK) ResetDamage(E);
    W(wAttackMissed) = 1;
}

/* The substitute animations: what LoadAnim leaves behind */
static void SubstituteAnim(PkbEngine* E, uint8_t param) {
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
    W(wBattleAnimParam) = param;
    W(wFXAnimID) = SUBSTITUTE;
}

void BattleCommand_LowerSub(PkbEngine* E) {
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_SUBSTITUTE))) return;
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    bool charge_turn = false;
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3) & (1 << SUBSTATUS_CHARGED))) {
        charge_turn = effect == EFFECT_RAZOR_WIND || effect == EFFECT_SKY_ATTACK || effect == EFFECT_SKULL_BASH ||
                      effect == EFFECT_SOLARBEAM || effect == EFFECT_FLY;
    }
    if(!charge_turn) {
        /* .already_charged: .Rampage returns z (go to .charge_turn) if
         * nobody was rampaging */
        if(effect == EFFECT_ROLLOUT || effect == EFFECT_RAMPAGE) {
            charge_turn = W(wSomeoneIsRampaging) == 0;
            W(wSomeoneIsRampaging) = 0;
        }
        if(!charge_turn && CheckUserIsCharging(E)) return;
    }
    /* .charge_turn; with animations off, .mimic_anims only redraws */
    if(CheckBattleScene(E)) return;
    SubstituteAnim(E, 1);
}

void BattleCommand_RaiseSub(PkbEngine* E) {
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_SUBSTITUTE))) return;
    if(CheckBattleScene(E)) return; /* BattleCommand_RaiseSubNoAnim */
    SubstituteAnim(E, 2);
}

/* wBattleAnimParam (wBattleAfterAnim and the animation itself are display only) */
static void BattleCommand_MoveAnimNoSub(PkbEngine* E) {
    if(W(wAttackMissed)) return;
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    if(effect == EFFECT_MULTI_HIT || effect == EFFECT_CONVERSION || effect == EFFECT_DOUBLE_HIT ||
       effect == EFFECT_POISON_MULTI_HIT) {
        W(wBattleAnimParam) = (uint8_t)((W(wBattleAnimParam) & 1) ^ 1);
    } else if(effect != EFFECT_TRIPLE_KICK) {
        W(wBattleAnimParam) = 0;
    }
}

static void BattleCommand_MoveAnim(PkbEngine* E) {
    BattleCommand_LowerSub(E);
    BattleCommand_MoveAnimNoSub(E);
    BattleCommand_RaiseSub(E);
}

/* AnimateCurrentMove keeps wBattleAnimParam across the lowered substitute */
void AnimateCurrentMove(PkbEngine* E) {
    uint8_t param = W(wBattleAnimParam);
    BattleCommand_LowerSub(E);
    W(wBattleAnimParam) = param;
    /* LoadMoveAnim */
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
    BattleCommand_RaiseSub(E);
}

void AnimateFailedMove(PkbEngine* E) {
    BattleCommand_LowerSub(E);
    /* BattleCommand_MoveDelay: DelayFrames counts c down to 0, which a failed
     * Present hands to damagecalc as the attack (the damage is then 2). With
     * animations off a substitute's redraw would clobber more registers; the
     * Flipper always has them on. */
    reg_c = 0;
    BattleCommand_RaiseSub(E);
}

/* BattleCommand_FalseSwipe: returns carry if the damage was lowered */
static bool BattleCommand_FalseSwipe(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyMonHP : wBattleMonHP;
    uint16_t hp = rd16(E, hl), dmg = rd16(E, wCurDamage);
    if(dmg < hp) return false;
    W(wCurDamage) = M(hl);
    W(wCurDamage + 1) = (uint8_t)(M(hl + 1) - 1);
    if(M(hl + 1) == 0) W(wCurDamage)--;
    if(W(wCriticalHit) == 2) W(wCriticalHit) = 0;
    return true;
}

static void update_damage_taken(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4_OPP) & (1 << SUBSTATUS_SUBSTITUTE)) return;
    uint16_t de = player_turn(E) ? wEnemyDamageTaken : wPlayerDamageTaken;
    uint32_t v = (uint32_t)rd16(E, de) + rd16(E, wCurDamage);
    wr16(E, de, v > 0xFFFF ? 0xFFFF : (uint16_t)v);
}

static void BattleCommand_ApplyDamage(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1_OPP) & (1 << SUBSTATUS_ENDURE)) {
        BattleCommand_FalseSwipe(E);
    } else {
        uint8_t b, c = 0;
        GetOpponentItem(E, &b, &c);
        if(b == HELD_FOCUS_BAND) {
            if(BattleRandom(E, "BattleCommand_ApplyDamage") < c) BattleCommand_FalseSwipe(E);
        }
    }
    update_damage_taken(E);
    if(player_turn(E)) DoEnemyDamage(E, false);
    else DoPlayerDamage(E, false);
}

static void DoSubstituteDamage(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wEnemySubstituteHP : wPlayerSubstituteHP;
    bool broke = W(wCurDamage) != 0;
    if(!broke) {
        uint8_t hp = M(de), dmg = W(wCurDamage + 1);
        M(de) = (uint8_t)(hp - dmg);
        broke = M(de) == 0 || hp < dmg;
    }
    if(broke) {
        res_var(E, BATTLE_VARS_SUBSTATUS4_OPP, 1 << SUBSTATUS_SUBSTITUTE);
        uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_MOVE_EFFECT);
        uint8_t a = M(hl);
        if(a != EFFECT_MULTI_HIT && a != EFFECT_DOUBLE_HIT && a != EFFECT_POISON_MULTI_HIT && a != EFFECT_TRIPLE_KICK &&
           a != EFFECT_BEAT_UP) {
            M(hl) = 0;
        }
    }
    ResetDamage(E);
}

static void DoEnemyDamage(PkbEngine* E, bool ignore_sub) {
    if(!(W(wCurDamage) | W(wCurDamage + 1))) return;
    if(!ignore_sub && (W(wEnemySubStatus4) & (1 << SUBSTATUS_SUBSTITUTE))) {
        DoSubstituteDamage(E);
        return;
    }
    uint16_t hp = rd16(E, wEnemyMonHP), dmg = rd16(E, wCurDamage);
    W(wHPBuffer2) = (uint8_t)hp;
    W(wHPBuffer2 + 1) = (uint8_t)(hp >> 8);
    wr16(E, wEnemyMonHP, (uint16_t)(hp - dmg));
    if(dmg > hp) {
        wr16(E, wCurDamage, hp);
        wr16(E, wEnemyMonHP, 0);
    }
    uint16_t max = rd16(E, wEnemyMonMaxHP), nhp = rd16(E, wEnemyMonHP);
    W(wHPBuffer1 + 1) = (uint8_t)(max >> 8);
    W(wHPBuffer1) = (uint8_t)max;
    W(wHPBuffer3 + 1) = (uint8_t)(nhp >> 8);
    W(wHPBuffer3) = (uint8_t)nhp;
    W(wWhichHPBar) = 0;
}

static void DoPlayerDamage(PkbEngine* E, bool ignore_sub) {
    if(!(W(wCurDamage) | W(wCurDamage + 1))) return;
    if(!ignore_sub && (W(wPlayerSubStatus4) & (1 << SUBSTATUS_SUBSTITUTE))) {
        DoSubstituteDamage(E);
        return;
    }
    uint16_t hp = rd16(E, wBattleMonHP), dmg = rd16(E, wCurDamage);
    uint16_t nhp = (uint16_t)(hp - dmg);
    W(wHPBuffer2) = (uint8_t)hp;
    W(wHPBuffer2 + 1) = (uint8_t)(hp >> 8);
    wr16(E, wBattleMonHP, nhp);
    W(wHPBuffer3) = (uint8_t)nhp;
    W(wHPBuffer3 + 1) = (uint8_t)(nhp >> 8);
    if(dmg > hp) {
        wr16(E, wCurDamage, hp);
        wr16(E, wBattleMonHP, 0);
        W(wHPBuffer3) = 0;
        W(wHPBuffer3 + 1) = 0;
    }
    uint16_t max = rd16(E, wBattleMonMaxHP);
    W(wHPBuffer1 + 1) = (uint8_t)(max >> 8);
    W(wHPBuffer1) = (uint8_t)max;
    W(wWhichHPBar) = 1;
}

static void GetFailureResultText(PkbEngine* E) {
    W(wCriticalHit) = 0;
    if(GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT) != EFFECT_JUMP_KICK) return;
    if(!(W(wTypeModifier) & EFFECTIVENESS_MASK)) return;
    uint16_t d = (uint16_t)(rd16(E, wCurDamage) >> 3);
    if(!d) d = 1;
    wr16(E, wCurDamage, d);
    W(wBattleAnimParam) = 1;
    if(player_turn(E)) DoPlayerDamage(E, true);
    else DoEnemyDamage(E, true);
}

static void BattleCommand_FailureText(PkbEngine* E) {
    if(!W(wAttackMissed)) return;
    GetFailureResultText(E);
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_MOVE_ANIM);
    uint8_t anim = M(hl);
    if(anim == FLY || anim == DIG) {
        res_var(E, BATTLE_VARS_SUBSTATUS3, 1 << SUBSTATUS_UNDERGROUND | 1 << SUBSTATUS_FLYING);
    } else {
        uint8_t effect = M(hl + 1);
        if(effect == EFFECT_MULTI_HIT || effect == EFFECT_DOUBLE_HIT || effect == EFFECT_POISON_MULTI_HIT) {
            BattleCommand_RaiseSub(E);
        }
    }
    EndMoveEffect(E);
}

static void BattleCommand_CriticalText(PkbEngine* E) {
    if(W(wCriticalHit)) W(wCriticalHit) = 0;
}

static void BattleCommand_CheckFaint(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyMonHP : wBattleMonHP;
    if(M(hl) | M(hl + 1)) return;
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS5_OPP) & (1 << SUBSTATUS_DESTINY_BOND)) {
        /* the user faints too */
        uint16_t max = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
        W(wWhichHPBar) = player_turn(E) ? 1 : 0;
        W(wHPBuffer1) = M(max + 1);
        W(wHPBuffer1 + 1) = M(max);
        W(wHPBuffer2) = M(max - 1);
        M(max - 1) = 0;
        W(wHPBuffer2 + 1) = M(max - 2);
        M(max - 2) = 0;
        W(wHPBuffer3) = 0;
        W(wHPBuffer3 + 1) = 0;
        W(wBattleAfterAnim) = 0;
        W(wFXAnimID + 1) = 0;
        W(wBattleAnimParam) = 1;
    } else {
        uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
        if(effect == EFFECT_MULTI_HIT || effect == EFFECT_DOUBLE_HIT || effect == EFFECT_POISON_MULTI_HIT ||
           effect == EFFECT_TRIPLE_KICK || effect == EFFECT_BEAT_UP) {
            BattleCommand_RaiseSub(E);
        }
    }
    EndMoveEffect(E);
}

static void BattleCommand_BuildOpponentRage(PkbEngine* E) {
    if(W(wAttackMissed)) return;
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4_OPP) & (1 << SUBSTATUS_RAGE))) return;
    uint16_t de = player_turn(E) ? wEnemyRageCounter : wPlayerRageCounter;
    uint8_t a = (uint8_t)(M(de) + 1);
    if(!a) return;
    M(de) = a;
}

static void BattleCommand_HeldFlinch(PkbEngine* E) {
    if(W(wAttackMissed)) return;
    uint8_t b, c = 0;
    GetUserItem(E, &b, &c);
    if(b != HELD_FLINCH) return;
    if(CheckSubstituteOpp(E)) return;
    GetUserItem(E, &b, &c);
    if(BattleRandom(E, "BattleCommand_HeldFlinch") >= c) return;
    EndRechargeOpp(E);
    M(GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3_OPP)) |= 1 << SUBSTATUS_FLINCHED;
}

/* recoil: 1/4 of the damage dealt (at least 1) to the user */
static void BattleCommand_Recoil(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
    uint16_t bc = (uint16_t)(rd16(E, wCurDamage) >> 2);
    if(!bc) bc = 1;
    W(wHPBuffer1 + 1) = M(hl);
    W(wHPBuffer1) = M(hl + 1);
    uint16_t hp = rd16(E, (uint16_t)(hl - 2));
    W(wHPBuffer2) = (uint8_t)hp;
    W(wHPBuffer2 + 1) = (uint8_t)(hp >> 8);
    uint16_t nhp = (uint16_t)(hp - bc);
    wr16(E, (uint16_t)(hl - 2), nhp);
    W(wHPBuffer3) = (uint8_t)nhp;
    W(wHPBuffer3 + 1) = (uint8_t)(nhp >> 8);
    if(bc > hp) {
        wr16(E, (uint16_t)(hl - 2), 0);
        W(wHPBuffer3) = 0;
        W(wHPBuffer3 + 1) = 0;
    }
    W(wWhichHPBar) = player_turn(E) ? 1 : 0;
}

/* ---- damage stats and calculation ---- */

/* returns carry: use the boosted (battle mon) stats */
static bool CheckDamageStatsCritical(PkbEngine* E) {
    if(!W(wCriticalHit)) return true;
    uint8_t b, a;
    if(player_turn(E)) {
        bool special = W(wPlayerMoveStructType) >= SPECIAL;
        b = special ? W(wPlayerSAtkLevel) : W(wPlayerAtkLevel);
        a = special ? W(wEnemySDefLevel) : W(wEnemyDefLevel);
    } else {
        bool special = W(wEnemyMoveStructType) >= SPECIAL;
        b = special ? W(wEnemySAtkLevel) : W(wEnemyAtkLevel);
        a = special ? W(wPlayerSDefLevel) : W(wPlayerDefLevel);
    }
    return a < b;
}

/* SpeciesItemBoost: the stat at hl, doubled if the attacker is species
 * s1/s2 holding item `item` */
static uint16_t SpeciesItemBoost(PkbEngine* E, uint16_t hl, uint8_t s1, uint8_t s2, uint8_t item) {
    uint16_t v = rd16(E, hl);
    uint8_t species = player_turn(E) ? M(BattlePartyAttr(E, MON_SPECIES)) : W(wTempEnemyMonSpecies);
    if(species != s1 && species != s2) return v;
    uint8_t b, c;
    uint16_t it = GetUserItem(E, &b, &c);
    if(M(it) != item) return v;
    return (uint16_t)(v << 1);
}

static void DittoMetalPowder(PkbEngine* E, uint8_t* b, uint8_t* c) {
    uint8_t species = player_turn(E) ? W(wTempEnemyMonSpecies) : M(BattlePartyAttr(E, MON_SPECIES));
    if(species != DITTO) return;
    uint8_t ib, ic;
    uint16_t it = GetOpponentItem(E, &ib, &ic);
    if(M(it) != METAL_POWDER) return;
    unsigned v = (unsigned)*c + (*c >> 1);
    *c = (uint8_t)v;
    if(v < 0x100) return;
    *b >>= 1;
    if(!*b) *b = 1;
    *c = (uint8_t)(0x80 | (*c >> 1));
}

static void AttackDamage(PkbEngine* E, bool player) {
    ResetDamage(E);
    uint16_t power = player ? wPlayerMoveStructPower : wEnemyMoveStructPower;
    reg_d = M(power);
    if(!reg_d) return;
    bool special = M(power + 1) >= SPECIAL;
    uint16_t screens = player ? wEnemyScreens : wPlayerScreens;
    uint16_t def, atk, unb_def, unb_atk;
    if(!special) {
        def = player ? wEnemyMonDefense : wBattleMonDefense;
        atk = player ? wBattleMonAttack : wEnemyMonAttack;
        unb_def = player ? wEnemyDefense : wPlayerDefense;
        unb_atk = player ? wPlayerAttack : wEnemyAttack;
    } else {
        def = player ? wEnemyMonSpclDef : wBattleMonSpclDef;
        atk = player ? wBattleMonSpclAtk : wEnemyMonSpclAtk;
        unb_def = player ? wEnemySpDef : wPlayerSpDef;
        unb_atk = player ? wPlayerSpAtk : wEnemySpAtk;
    }
    uint16_t bc = rd16(E, def);
    if(M(screens) & (1 << (special ? SCREENS_LIGHT_SCREEN : SCREENS_REFLECT))) bc = (uint16_t)(bc << 1);
    uint16_t hl = atk;
    if(!CheckDamageStatsCritical(E)) {
        bc = rd16(E, unb_def);
        hl = unb_atk;
    }
    uint16_t a = special ? SpeciesItemBoost(E, hl, PIKACHU, PIKACHU, LIGHT_BALL) :
                           SpeciesItemBoost(E, hl, CUBONE, MAROWAK, THICK_CLUB);
    TruncateHL_BC(E, a, bc, &reg_b, &reg_c);
    reg_e = player ? W(wBattleMonLevel) : W(wEnemyMonLevel);
    DittoMetalPowder(E, &reg_b, &reg_c);
}

static void BattleCommand_DamageStats(PkbEngine* E) {
    AttackDamage(E, player_turn(E));
}

static void BattleCommand_DamageCalc(PkbEngine* E) {
    uint8_t b = reg_b, c = reg_c, d = reg_d, e = reg_e;
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    if(effect == EFFECT_SELFDESTRUCT) {
        c >>= 1;
        if(!c) c = 1;
    }
    if(effect != EFFECT_MULTI_HIT && effect != EFFECT_CONVERSION && !d) return;
    if(!c) c = 1;

    H(hDividend + 0) = 0;
    H(hDividend + 1) = 0;
    H(hDividend + 2) = 0;
    if(e & 0x80) H(hDividend + 2) = 1;
    H(hDividend + 3) = (uint8_t)(e << 1);
    H(hDivisor) = 5;
    Divide(E, 4);
    H(hQuotient + 3) = (uint8_t)(H(hQuotient + 3) + 2);
    H(hMultiplier) = d;
    Multiply(E);
    H(hMultiplier) = b;
    Multiply(E);
    H(hDivisor) = c;
    Divide(E, 4);
    H(hDivisor) = 50;
    Divide(E, 4);

    /* type-boosting items */
    uint8_t ib, ic = c;
    GetUserItem(E, &ib, &ic);
    if(ib) {
        for(const uint8_t* p = pb_type_boost_items; *p != 0xFF; p += 2) {
            if(p[0] != ib) continue;
            if(GetBattleVar(E, BATTLE_VARS_MOVE_TYPE) == p[1]) {
                H(hMultiplier) = (uint8_t)(ic + 100);
                Multiply(E);
                H(hDivisor) = 100;
                Divide(E, 4);
            }
            break;
        }
    }
    /* .CriticalMultiplier */
    if(W(wCriticalHit)) {
        unsigned v = (unsigned)(H(hQuotient + 2) << 8 | H(hQuotient + 3)) << 1;
        H(hQuotient + 3) = (uint8_t)v;
        H(hQuotient + 2) = (uint8_t)(v >> 8);
        if(v > 0xFFFF) {
            H(hQuotient + 2) = 0xFF;
            H(hQuotient + 3) = 0xFF;
        }
    }

    /* cap at 997, then add 2 */
    const uint16_t cap = 997;
    bool do_cap = false;
    {
        unsigned q3 = (unsigned)H(hQuotient + 3) + W(wCurDamage);
        H(hQuotient + 3) = (uint8_t)q3;
        if(q3 > 0xFF) {
            H(hQuotient + 2) = (uint8_t)(H(hQuotient + 2) + 1);
            if(!H(hQuotient + 2)) do_cap = true;
        }
    }
    if(!do_cap && H(hQuotient + 1)) do_cap = true;
    if(!do_cap) {
        uint8_t q2 = H(hQuotient + 2);
        if(q2 >= ((cap + 1) >> 8)) {
            if(q2 >= ((cap + 1) >> 8) + 1) do_cap = true;
            else if(H(hQuotient + 3) >= ((cap + 1) & 0xFF)) do_cap = true;
        }
    }
    if(!do_cap) {
        unsigned lo = (unsigned)H(hQuotient + 3) + W(wCurDamage + 1);
        W(wCurDamage + 1) = (uint8_t)lo;
        unsigned hi = (unsigned)H(hQuotient + 2) + W(wCurDamage) + (lo >> 8);
        W(wCurDamage) = (uint8_t)hi;
        if(hi > 0xFF) do_cap = true;
        else if(W(wCurDamage) >= ((cap + 1) >> 8)) {
            if(W(wCurDamage) >= ((cap + 1) >> 8) + 1) do_cap = true;
            else if(W(wCurDamage + 1) >= ((cap + 1) & 0xFF)) do_cap = true;
        }
    }
    if(do_cap) {
        W(wCurDamage) = cap >> 8;
        W(wCurDamage + 1) = cap & 0xFF;
    }
    unsigned lo = (unsigned)W(wCurDamage + 1) + 2;
    W(wCurDamage + 1) = (uint8_t)lo;
    if(lo > 0xFF) W(wCurDamage)++;
}

/* ---- fixed damage, secondary effects, status, drain, stats, multi-hit ---- */

void BattleCommand_ResetTypeMatchup(PkbEngine* E) {
    BattleCheckTypeMatchup(E);
    if(W(wTypeMatchup)) {
        W(wTypeMatchup) = EFFECTIVE;
        return;
    }
    ResetDamage(E);
    W(wTypeModifier) = 0;
    W(wAttackMissed) = 1;
}

static void BattleCommand_EffectChance(PkbEngine* E) {
    W(wEffectFailed) = 0;
    if(!CheckSubstituteOpp(E)) {
        uint8_t chance = player_turn(E) ? W(wPlayerMoveStruct + MOVE_CHANCE) : W(wEnemyMoveStruct + MOVE_CHANCE);
        /* a 100% chance still fails on 255 (game bug) */
        if(BattleRandom(E, "BattleCommand_EffectChance") < chance) return;
    }
    W(wEffectFailed) = 1;
}

static void BattleCommand_StartLoop(PkbEngine* E) {
    W(player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount) = 0;
}

static void BattleCommand_ClearMissDamage(PkbEngine* E) {
    if(W(wAttackMissed)) ResetDamage(E);
}

static void BattleCommand_ConstantDamage(PkbEngine* E) {
    uint8_t level = player_turn(E) ? W(wBattleMonLevel) : W(wEnemyMonLevel);
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    uint8_t a = 0, b;
    if(effect == EFFECT_LEVEL_DAMAGE) {
        b = level;
    } else if(effect == EFFECT_PSYWAVE) {
        uint8_t max = (uint8_t)((level >> 1) + level);
        do {
            b = BattleRandom(E, "BattleCommand_ConstantDamage.psywave");
        } while(!b || b >= max);
    } else if(effect == EFFECT_SUPER_FANG) {
        uint16_t half = (uint16_t)(rd16(E, player_turn(E) ? wEnemyMonHP : wBattleMonHP) >> 1);
        a = (uint8_t)(half >> 8);
        b = half ? (uint8_t)half : 1;
    } else if(effect == EFFECT_REVERSAL) {
        /* .reversal: power from 48 * HP / max HP, then the normal formula */
        uint16_t hl = player_turn(E) ? wBattleMonHP : wEnemyMonHP;
        H(hDividend) = 0;
        H(hMultiplicand + 0) = 0;
        H(hMultiplicand + 1) = M(hl);
        H(hMultiplicand + 2) = M(hl + 1);
        H(hMultiplier) = 48;
        Multiply(E);
        uint8_t max_hi = M(hl + 2);
        H(hDivisor) = M(hl + 3);
        if(max_hi) {
            /* hDivisor is the low byte of hProduct + 4 */
            uint16_t div = (uint16_t)((max_hi << 8 | H(hProduct + 4)) >> 2);
            H(hDivisor) = (uint8_t)div;
            uint16_t prod = (uint16_t)((H(hProduct + 2) << 8 | H(hProduct + 3)) >> 2);
            H(hDividend + 3) = (uint8_t)prod;
            H(hDividend + 2) = (uint8_t)(prod >> 8);
        }
        Divide(E, 4);
        uint8_t px = H(hQuotient + 3);
        const uint8_t* p = pb_flail_reversal_power;
        while(p[0] < px) p += 2;
        uint16_t power = player_turn(E) ? wPlayerMoveStructPower : wEnemyMoveStructPower;
        M(power) = p[1];
        AttackDamage(E, player_turn(E));
        BattleCommand_DamageCalc(E);
        M(power) = 1;
        return;
    } else {
        b = GetBattleVar(E, BATTLE_VARS_MOVE_POWER);
    }
    W(wCurDamage) = a;
    W(wCurDamage + 1) = b;
}

/* Status helpers */

bool SafeCheckSafeguard(PkbEngine* E) { /* returns nz */
    return (W(player_turn(E) ? wEnemyScreens : wPlayerScreens) & (1 << SCREENS_SAFEGUARD)) != 0;
}

static bool CheckIfTargetIsPoisonType(PkbEngine* E) { /* returns z */
    uint16_t de = player_turn(E) ? wEnemyMonType1 : wBattleMonType1;
    return M(de) == POISON || M(de + 1) == POISON;
}

/* returns z if the move's type is one of the target's (Normal never matches) */
static bool CheckMoveTypeMatchesTarget(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyMonType1 : wBattleMonType1;
    uint8_t type = GetBattleVar(E, BATTLE_VARS_MOVE_TYPE);
    if(type == NORMAL) return false;
    return type == M(hl) || type == M(hl + 1);
}

bool CheckOpponentWentFirst(PkbEngine* E) { /* returns nz */
    return (W(wEnemyGoesFirst) ^ H(hBattleTurn)) != 0;
}

bool CheckHiddenOpponent(PkbEngine* E) { /* returns nz */
    return (GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_FLYING | 1 << SUBSTATUS_UNDERGROUND)) != 0;
}

/* GetItemName for the "protected by" texts: it also sets wNamedObjectIndex */
static void GetOpponentItemName(PkbEngine* E) {
    uint8_t b, c;
    W(wNamedObjectIndex) = M(GetOpponentItem(E, &b, &c));
}

static uint8_t opponent_held_effect(PkbEngine* E) {
    uint8_t b, c;
    GetOpponentItem(E, &b, &c);
    return b;
}

static void OpponentCantMove(PkbEngine* E) {
    BattleCommand_SwitchTurn(E);
    CantMove(E);
    BattleCommand_SwitchTurn(E);
}

static void CalcBattleStats(PkbEngine* E, uint16_t hl, uint16_t de, uint16_t bc) {
    for(uint8_t n = NUM_BATTLE_STATS; n; n--) {
        const uint8_t* mul = pb_stat_level_multipliers[(uint8_t)(M(hl++) - 1)];
        H(hMultiplicand + 0) = 0;
        H(hMultiplicand + 1) = M(de++);
        H(hMultiplicand + 2) = M(de++);
        H(hMultiplier) = mul[0];
        Multiply(E);
        H(hDivisor) = mul[1];
        Divide(E, 4);
        if(!(H(hQuotient + 2) | H(hQuotient + 3))) {
            H(hQuotient + 3) = 1;
        } else if((H(hQuotient + 2) << 8 | H(hQuotient + 3)) >= MAX_STAT_VALUE) {
            H(hQuotient + 3) = MAX_STAT_VALUE & 0xFF;
            H(hQuotient + 2) = MAX_STAT_VALUE >> 8;
        }
        M(bc++) = H(hQuotient + 2);
        M(bc++) = H(hQuotient + 3);
    }
}

/* Paralysis and burn are applied with the turn switched, so they hit the
 * right side only when it is that side's turn (as in the game). */
void CalcPlayerStats(PkbEngine* E) {
    CalcBattleStats(E, wPlayerAtkLevel, wPlayerStats, wBattleMonAttack);
    /* BadgeStatBoosts: not in link battles */
    BattleCommand_SwitchTurn(E);
    ApplyPrzEffectOnSpeed(E);
    ApplyBrnEffectOnAttack(E);
    BattleCommand_SwitchTurn(E);
}

void CalcEnemyStats(PkbEngine* E) {
    CalcBattleStats(E, wEnemyAtkLevel, wEnemyStats, wEnemyMonAttack);
    BattleCommand_SwitchTurn(E);
    ApplyPrzEffectOnSpeed(E);
    ApplyBrnEffectOnAttack(E);
    BattleCommand_SwitchTurn(E);
}

/* core.asm UseHeldStatusHealingItem: returns nz (true) if the opponent's
 * item cured the status it just got */
bool UseHeldStatusHealingItem(PkbEngine* E) {
    uint8_t b = opponent_held_effect(E), mask = 0;
    const uint8_t* p = pb_held_heal_status;
    for(;; p += 2) {
        if(p[0] == 0xFF) return false;
        if(p[0] == b) break;
    }
    mask = p[1];
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_STATUS_OPP);
    if(!(M(hl) & mask)) return false;
    M(hl) = 0;
    UpdateOpponentInParty(E);
    res_var(E, BATTLE_VARS_SUBSTATUS5_OPP, 1 << SUBSTATUS_TOXIC);
    res_var(E, BATTLE_VARS_SUBSTATUS1_OPP, 1 << SUBSTATUS_NIGHTMARE);
    if(mask == ALL_STATUS) res_var(E, BATTLE_VARS_SUBSTATUS3_OPP, 1 << SUBSTATUS_CONFUSED);
    SwitchTurnCore(E);
    if(player_turn(E)) CalcPlayerStats(E); /* the turns are switched here */
    else CalcEnemyStats(E);
    SwitchTurnCore(E);
    ItemRecoveryAnim(E);
    UseOpponentItem(E);
    return true;
}

void UseConfusionHealingItem(PkbEngine* E) {
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_CONFUSED))) return;
    uint8_t b, c;
    uint16_t item = GetOpponentItem(E, &b, &c);
    if(b != HELD_HEAL_CONFUSION && b != HELD_HEAL_STATUS) return;
    W(wNamedObjectIndex) = M(item);
    res_var(E, BATTLE_VARS_SUBSTATUS3_OPP, 1 << SUBSTATUS_CONFUSED);
    ItemRecoveryAnim(E);
    if(player_turn(E)) {
        W(wEnemyMonItem) = 0;
        if(W(wBattleMode) == WILD_BATTLE) return;
        M(GetPartyLocation(wOTPartyMon1Item, W(wCurOTMon))) = 0;
    } else {
        W(wBattleMonItem) = 0;
        M(GetPartyLocation(wPartyMon1Item, W(wCurBattleMon))) = 0;
    }
}

/* ---- status moves and secondary effects ---- */

static void BattleCommand_CheckSafeguard(PkbEngine* E) {
    if(!SafeCheckSafeguard(E)) return;
    W(wAttackMissed) = 1;
    EndMoveEffect(E);
}

static void BattleCommand_SleepTarget(PkbEngine* E) {
    if(opponent_held_effect(E) == HELD_PREVENT_SLEEP) {
        GetOpponentItemName(E);
        AnimateFailedMove(E);
        return;
    }
    uint16_t de = GetBattleVarAddr(E, BATTLE_VARS_STATUS_OPP);
    /* the AI's 25% failure roll (.CheckAIRandomFail) is not in link battles */
    if((M(de) & SLP_MASK) || W(wAttackMissed) || M(de) || CheckSubstituteOpp(E)) {
        AnimateFailedMove(E);
        return;
    }
    AnimateCurrentMove(E);
    uint8_t a;
    do {
        a = BattleRandom(E, "BattleCommand_SleepTarget") & SLP_MASK; /* %011 in the Battle Tower */
    } while(!a || a == SLP_MASK);
    M(de) = (uint8_t)(a + 1);
    UpdateOpponentInParty(E);
    if(!UseHeldStatusHealingItem(E)) OpponentCantMove(E);
}

static void PoisonOpponent(PkbEngine* E) {
    set_var(E, BATTLE_VARS_STATUS_OPP, 1 << PSN);
    UpdateOpponentInParty(E);
}

static void BattleCommand_PoisonTarget(PkbEngine* E) {
    if(CheckSubstituteOpp(E)) return;
    if(GetBattleVar(E, BATTLE_VARS_STATUS_OPP)) return;
    if(!(W(wTypeModifier) & EFFECTIVENESS_MASK)) return;
    if(CheckIfTargetIsPoisonType(E)) return;
    if(opponent_held_effect(E) == HELD_PREVENT_POISON) return;
    if(W(wEffectFailed)) return;
    if(SafeCheckSafeguard(E)) return;
    PoisonOpponent(E);
    UseHeldStatusHealingItem(E);
}

static void BattleCommand_Poison(PkbEngine* E) {
    if(!(W(wTypeModifier) & EFFECTIVENESS_MASK) || CheckIfTargetIsPoisonType(E) ||
       (GetBattleVar(E, BATTLE_VARS_STATUS_OPP) & (1 << PSN))) {
        AnimateFailedMove(E);
        return;
    }
    if(opponent_held_effect(E) == HELD_PREVENT_POISON) {
        GetOpponentItemName(E);
        AnimateFailedMove(E);
        return;
    }
    /* the AI's 25% failure roll is not in link battles */
    if(GetBattleVar(E, BATTLE_VARS_STATUS_OPP) || CheckSubstituteOpp(E) || W(wAttackMissed)) {
        AnimateFailedMove(E);
        return;
    }
    if(GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT) == EFFECT_TOXIC) {
        set_var(E, BATTLE_VARS_SUBSTATUS5_OPP, 1 << SUBSTATUS_TOXIC);
        W(player_turn(E) ? wEnemyToxicCount : wPlayerToxicCount) = 0;
    }
    AnimateCurrentMove(E);
    PoisonOpponent(E);
    UseHeldStatusHealingItem(E);
}

/* Heals the user by half the damage dealt (at least 1), up to its max HP.
 * It always updates the player's party copy, even on the enemy's turn. */
static void SapHealth(PkbEngine* E) {
    uint16_t half = (uint16_t)(rd16(E, wCurDamage) >> 1);
    H(hDividend) = (uint8_t)(half >> 8);
    H(hDividend + 1) = half ? (uint8_t)half : 1;
    uint16_t hl = player_turn(E) ? wBattleMonHP : wEnemyMonHP;
    uint16_t de = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
    W(wHPBuffer2 + 1) = M(hl);
    W(wHPBuffer2) = M(hl + 1);
    W(wHPBuffer1 + 1) = M(de);
    W(wHPBuffer1) = M(de + 1);
    uint32_t hp = (uint32_t)rd16(E, hl) + (uint16_t)(H(hDividend) << 8 | H(hDividend + 1));
    wr16(E, hl, (uint16_t)hp);
    if(hp > 0xFFFF || rd16(E, de) < (uint16_t)hp) wr16(E, hl, rd16(E, de));
    W(wHPBuffer3) = M(hl + 1);
    W(wHPBuffer3 + 1) = M(hl);
    W(wWhichHPBar) = player_turn(E) ? 1 : 0;
    UpdateBattleMonInParty(E);
}

/* Defrost (the burn path): hl is the opponent's status */
static void Defrost(PkbEngine* E, uint16_t hl) {
    if(!(M(hl) & (1 << FRZ))) return;
    M(hl) = 0;
    if(player_turn(E)) M(GetPartyLocation(wOTPartyMon1Status, W(wCurOTMon))) = 0;
    else M(GetPartyLocation(wPartyMon1Status, W(wCurBattleMon))) = 0;
    UpdateOpponentInParty(E);
}

static void BattleCommand_BurnTarget(PkbEngine* E) {
    W(wBattleAfterAnim) = 0;
    if(CheckSubstituteOpp(E)) return;
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_STATUS_OPP);
    if(M(hl)) {
        Defrost(E, hl);
        return;
    }
    if(!(W(wTypeModifier) & EFFECTIVENESS_MASK)) return;
    if(CheckMoveTypeMatchesTarget(E)) return;
    if(opponent_held_effect(E) == HELD_PREVENT_BURN) return;
    if(W(wEffectFailed)) return;
    if(SafeCheckSafeguard(E)) return;
    set_var(E, BATTLE_VARS_STATUS_OPP, 1 << BRN);
    UpdateOpponentInParty(E);
    ApplyBrnEffectOnAttack(E);
    UseHeldStatusHealingItem(E);
}

static void BattleCommand_FreezeTarget(PkbEngine* E) {
    W(wBattleAfterAnim) = 0;
    if(CheckSubstituteOpp(E)) return;
    if(GetBattleVar(E, BATTLE_VARS_STATUS_OPP)) return;
    if(!(W(wTypeModifier) & EFFECTIVENESS_MASK)) return;
    if(W(wBattleWeather) == WEATHER_SUN) return;
    if(CheckMoveTypeMatchesTarget(E)) return;
    if(opponent_held_effect(E) == HELD_PREVENT_FREEZE) return;
    if(W(wEffectFailed)) return;
    if(SafeCheckSafeguard(E)) return;
    set_var(E, BATTLE_VARS_STATUS_OPP, 1 << FRZ);
    UpdateOpponentInParty(E);
    if(UseHeldStatusHealingItem(E)) return;
    OpponentCantMove(E);
    EndRechargeOpp(E);
    W(player_turn(E) ? wEnemyJustGotFrozen : wPlayerJustGotFrozen) = 1;
}

static void BattleCommand_ParalyzeTarget(PkbEngine* E) {
    W(wBattleAfterAnim) = 0;
    if(CheckSubstituteOpp(E)) return;
    if(GetBattleVar(E, BATTLE_VARS_STATUS_OPP)) return;
    if(!(W(wTypeModifier) & EFFECTIVENESS_MASK)) return;
    if(opponent_held_effect(E) == HELD_PREVENT_PARALYZE) return;
    if(W(wEffectFailed)) return;
    if(SafeCheckSafeguard(E)) return;
    set_var(E, BATTLE_VARS_STATUS_OPP, 1 << PAR);
    UpdateOpponentInParty(E);
    ApplyPrzEffectOnSpeed(E);
    UseHeldStatusHealingItem(E);
}

static void BattleCommand_Paralyze(PkbEngine* E) {
    if((GetBattleVar(E, BATTLE_VARS_STATUS_OPP) & (1 << PAR)) || !(W(wTypeModifier) & EFFECTIVENESS_MASK)) {
        AnimateFailedMove(E);
        return;
    }
    if(opponent_held_effect(E) == HELD_PREVENT_PARALYZE) {
        GetOpponentItemName(E);
        AnimateFailedMove(E);
        return;
    }
    /* the AI's 25% failure roll is not in link battles */
    if(GetBattleVar(E, BATTLE_VARS_STATUS_OPP) || W(wAttackMissed) || CheckSubstituteOpp(E)) {
        AnimateFailedMove(E); /* PrintDidntAffect2 */
        return;
    }
    AnimateCurrentMove(E);
    set_var(E, BATTLE_VARS_STATUS_OPP, 1 << PAR);
    UpdateOpponentInParty(E);
    ApplyPrzEffectOnSpeed(E);
    UseHeldStatusHealingItem(E);
}

static void FinishConfusingTarget(PkbEngine* E) {
    uint16_t bc = player_turn(E) ? wEnemyConfuseCount : wPlayerConfuseCount;
    set_var(E, BATTLE_VARS_SUBSTATUS3_OPP, 1 << SUBSTATUS_CONFUSED);
    M(bc) = (uint8_t)((BattleRandom(E, "BattleCommand_FinishConfusingTarget") & 3) + 2); /* 2-5 turns */
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    if(effect != EFFECT_CONFUSE_HIT && effect != EFFECT_SNORE && effect != EFFECT_SWAGGER) AnimateCurrentMove(E);
    uint8_t b = opponent_held_effect(E);
    if(b == HELD_HEAL_STATUS || b == HELD_HEAL_CONFUSION) UseConfusionHealingItem(E);
}

static void BattleCommand_ConfuseTarget(PkbEngine* E) {
    if(opponent_held_effect(E) == HELD_PREVENT_CONFUSE) return;
    if(W(wEffectFailed)) return;
    if(SafeCheckSafeguard(E)) return;
    if(CheckSubstituteOpp(E)) return;
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_CONFUSED)) return;
    FinishConfusingTarget(E);
}

static void BattleCommand_Confuse(PkbEngine* E) {
    if(opponent_held_effect(E) == HELD_PREVENT_CONFUSE) {
        GetOpponentItemName(E);
        AnimateFailedMove(E);
        return;
    }
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_CONFUSED)) {
        AnimateFailedMove(E);
        return;
    }
    if(CheckSubstituteOpp(E) || W(wAttackMissed)) {
        /* BattleCommand_Confuse_CheckSnore_Swagger_ConfuseHit */
        uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
        if(effect != EFFECT_CONFUSE_HIT && effect != EFFECT_SNORE && effect != EFFECT_SWAGGER) AnimateFailedMove(E);
        return;
    }
    FinishConfusingTarget(E);
}

static void FlinchTarget(PkbEngine* E) {
    set_var(E, BATTLE_VARS_SUBSTATUS3_OPP, 1 << SUBSTATUS_FLINCHED);
    EndRechargeOpp(E);
}

static void BattleCommand_FlinchTarget(PkbEngine* E) {
    if(CheckSubstituteOpp(E)) return;
    if(GetBattleVar(E, BATTLE_VARS_STATUS_OPP) & (1 << FRZ | SLP_MASK)) return;
    if(CheckOpponentWentFirst(E)) return;
    if(W(wEffectFailed)) return;
    FlinchTarget(E);
}

static void BattleCommand_TriStatusChance(PkbEngine* E) {
    BattleCommand_EffectChance(E);
    uint8_t a;
    do {
        a = (uint8_t)((BattleRandom(E, "BattleCommand_TriStatusChance") >> 4) & 3); /* swap a; and %11 */
    } while(!a);
    if(a == 1) BattleCommand_ParalyzeTarget(E);
    else if(a == 2) BattleCommand_FreezeTarget(E);
    else BattleCommand_BurnTarget(E);
}

/* Thaws the user (Flame Wheel, Sacred Fire) */
static void BattleCommand_Defrost(PkbEngine* E) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_STATUS);
    if(!(M(hl) & (1 << FRZ))) return;
    M(hl) &= (uint8_t)~(1 << FRZ);
    if(!player_turn(E) && W(wBattleMode) == WILD_BATTLE) return;
    M(UserPartyAttr(E, MON_STATUS)) &= (uint8_t)~(1 << FRZ);
}

static void BattleCommand_Snore(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_STATUS) & SLP_MASK) return;
    ResetDamage(E);
    W(wAttackMissed) = 1;
    AnimateFailedMove(E); /* FailMove */
    EndMoveEffect(E);
}

/* ---- stat changes ---- */

static void MinimizeDropSub(PkbEngine* E) {
    uint8_t anim = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
    if(H(hBattleTurn) != E->move_user) {
        /* Swagger: the target's move struct, which the other Game Boy may
         * hold differently after a switch */
        bool theirs = player_turn(E) ? (E->player_sent_out ? E->player_prev_anim == MINIMIZE : anim == MINIMIZE) :
                                       (!E->enemy_sent_out && anim == MINIMIZE);
        if(theirs != (anim == MINIMIZE)) game_desync(E, PKB_DESYNC_SWAGGER_MINIMIZE);
    }
    if(anim != MINIMIZE) return;
    W(player_turn(E) ? wPlayerMinimized : wEnemyMinimized) = 1;
    /* with animations off, DropPlayerSub/DropEnemySub only redraw */
}

/* b: the stat ($10 set = two stages) */
void RaiseStat(PkbEngine* E, uint8_t b) {
    W(wLoweredStat) = b;
    uint16_t hl = player_turn(E) ? wPlayerStatLevels : wEnemyStatLevels;
    if(W(wAttackMissed) || W(wEffectFailed)) {
        W(wFailedMessage) = 1;
        return;
    }
    uint8_t c = W(wLoweredStat) & 0x0F;
    hl = (uint16_t)(hl + c);
    uint8_t level = (uint8_t)(M(hl) + 1);
    if(level > MAX_STAT_LEVEL) goto cant_raise;
    if(W(wLoweredStat) & 0xF0) {
        level++;
        if(level > MAX_STAT_LEVEL) level = MAX_STAT_LEVEL;
    }
    M(hl) = level;
    if(c < ACCURACY) {
        uint16_t stat = (uint16_t)((player_turn(E) ? wBattleMonStats : wEnemyMonStats) + 2 * c);
        if(rd16(E, stat) == MAX_STAT_VALUE) {
            M(hl)--;
            goto cant_raise;
        }
        if(player_turn(E)) CalcPlayerStats(E);
        else CalcEnemyStats(E);
    }
    W(wFailedMessage) = 0;
    return;
cant_raise:
    W(wFailedMessage) = 2;
    W(wAttackMissed) = 1;
}

void BattleCommand_StatUp(PkbEngine* E, uint8_t b) {
    RaiseStat(E, b);
    if(W(wFailedMessage)) return;
    MinimizeDropSub(E);
}

static bool CheckMist(PkbEngine* E) { /* returns nz */
    uint8_t a = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    bool lowers = (a >= EFFECT_ATTACK_DOWN && a <= EFFECT_EVASION_DOWN) ||
                  (a >= EFFECT_ATTACK_DOWN_2 && a <= EFFECT_EVASION_DOWN_2) ||
                  (a >= EFFECT_ATTACK_DOWN_HIT && a <= EFFECT_EVASION_DOWN_HIT);
    return lowers && (GetBattleVar(E, BATTLE_VARS_SUBSTATUS4_OPP) & (1 << SUBSTATUS_MIST));
}

/* Lowers stat c of the struct at hl unless it is already 1. Returns nz if
 * it recalculated the target's stats. */
static bool TryLowerStat(PkbEngine* E, uint16_t hl, uint8_t c) {
    if(rd16(E, (uint16_t)(hl + 2 * c)) == 1) return false;
    BattleCommand_SwitchTurn(E);
    if(player_turn(E)) CalcPlayerStats(E);
    else CalcEnemyStats(E);
    BattleCommand_SwitchTurn(E);
    return true;
}

static void BattleCommand_StatDown(PkbEngine* E, uint8_t a) {
    W(wLoweredStat) = a;
    if(CheckMist(E)) {
        W(wFailedMessage) = 2;
        W(wAttackMissed) = 1;
        return;
    }
    uint16_t hl = player_turn(E) ? wEnemyStatLevels : wPlayerStatLevels;
    uint8_t c = W(wLoweredStat) & 0x0F;
    hl = (uint16_t)(hl + c);
    uint8_t b = (uint8_t)(M(hl) - 1);
    if(!b) goto cant_lower;
    if((W(wLoweredStat) & 0xF0) && !--b) b++;
    /* .ComputerMiss: the AI's 25% failure roll is not in link battles */
    if(CheckSubstituteOpp(E) || W(wAttackMissed) || W(wEffectFailed) || CheckHiddenOpponent(E)) {
        W(wFailedMessage) = 1;
        W(wAttackMissed) = 1;
        return;
    }
    M(hl) = b;
    if(c < ACCURACY && !TryLowerStat(E, player_turn(E) ? wEnemyMonAttack : wBattleMonAttack, c)) {
        M(hl)++;
        goto cant_lower;
    }
    W(wFailedMessage) = 0;
    return;
cant_lower:
    W(wFailedMessage) = 3;
    W(wAttackMissed) = 1;
}

static void BattleCommand_AllStatsUp(PkbEngine* E) {
    for(uint8_t stat = ATTACK; stat <= SP_DEFENSE; stat++) {
        W(wAttackMissed) = 0; /* ResetMiss */
        BattleCommand_StatUp(E, stat);
    }
}

static void BattleCommand_ResetStats(PkbEngine* E) {
    for(uint8_t i = 0; i < NUM_LEVEL_STATS; i++) {
        W(wPlayerStatLevels + i) = BASE_STAT_LEVEL;
        W(wEnemyStatLevels + i) = BASE_STAT_LEVEL;
    }
    uint8_t turn = H(hBattleTurn);
    SetPlayerTurn(E);
    CalcPlayerStats(E);
    SetEnemyTurn(E);
    CalcEnemyStats(E);
    H(hBattleTurn) = turn;
    AnimateCurrentMove(E);
}

static void BattleCommand_Curl(PkbEngine* E) {
    set_var(E, BATTLE_VARS_SUBSTATUS2, 1 << SUBSTATUS_CURLED);
}

/* ---- multi-hit ---- */

static void BattleCommand_EndLoop(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount;
    /* the hit count for the "Hit N times!" text goes into the user's damage taken */
    uint16_t bc = player_turn(E) ? wPlayerDamageTaken : wEnemyDamageTaken;
    uint16_t ss3 = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3);
    uint8_t a;
    if(M(ss3) & (1 << SUBSTATUS_IN_LOOP)) {
        if(--M(de)) goto loop_back;
        goto done_loop;
    }
    M(ss3) |= 1 << SUBSTATUS_IN_LOOP;
    uint8_t effect = GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT);
    if(effect == EFFECT_POISON_MULTI_HIT || effect == EFFECT_DOUBLE_HIT) {
        a = 1;
    } else if(effect == EFFECT_BEAT_UP) {
        uint8_t count;
        if(player_turn(E)) count = W(wPartyCount);
        else count = W(wBattleMode) == WILD_BATTLE ? 1 : W(wOTPartyCount);
        if(count == 1) {
            /* .only_one_beatup */
            M(ss3) &= (uint8_t)~(1 << SUBSTATUS_IN_LOOP);
            EndMoveEffect(E);
            return;
        }
        a = (uint8_t)(count - 1);
    } else if(effect == EFFECT_TRIPLE_KICK) {
        do {
            a = BattleRandom(E, "BattleCommand_EndLoop.triple_kick") & 3;
        } while(!a);
        if(!--a) {
            M(bc) = 1;
            goto done_loop;
        }
    } else {
        a = BattleRandom(E, "BattleCommand_EndLoop") & 3;
        if(a >= 2) a = BattleRandom(E, "BattleCommand_EndLoop") & 3;
        a++;
    }
    /* .double_hit */
    M(de) = a;
    M(bc) = (uint8_t)(a + 1);
loop_back: {
    /* back to the critical command */
    uint16_t hl = (uint16_t)(W(wBattleScriptBufferAddress) | W(wBattleScriptBufferAddress + 1) << 8);
    while(M(hl--) != PB_CMD_CRITICAL) {
    }
    hl++;
    W(wBattleScriptBufferAddress) = (uint8_t)hl;
    W(wBattleScriptBufferAddress + 1) = (uint8_t)(hl >> 8);
    return;
}
done_loop:
    res_var(E, BATTLE_VARS_SUBSTATUS3, 1 << SUBSTATUS_IN_LOOP);
    M(bc) = 0;
}

static void BattleCommand_TripleKick(PkbEngine* E) {
    uint8_t b = (uint8_t)(W(wBattleAnimParam) + 1);
    uint16_t kick = rd16(E, wCurDamage);
    while(--b) {
        uint32_t v = (uint32_t)rd16(E, wCurDamage) + kick;
        if(v > 0xFFFF) {
            wr16(E, wCurDamage, 0xFFFF);
            return;
        }
        wr16(E, wCurDamage, (uint16_t)v);
    }
}

static void BattleCommand_KickCounter(PkbEngine* E) {
    W(wBattleAnimParam)++;
}

/* ---- damage modifiers ---- */

static void BattleCommand_OHKO(PkbEngine* E) {
    ResetDamage(E);
    if(W(wTypeModifier) & EFFECTIVENESS_MASK) {
        uint8_t target = player_turn(E) ? W(wEnemyMonLevel) : W(wBattleMonLevel);
        uint8_t user = player_turn(E) ? W(wBattleMonLevel) : W(wEnemyMonLevel);
        uint16_t bc = player_turn(E) ? wPlayerMoveStruct + MOVE_ACC : wEnemyMoveStruct + MOVE_ACC;
        if(user >= target) {
            /* +2 accuracy per level above the target */
            unsigned acc = M(bc) + (uint8_t)((user - target) << 1);
            M(bc) = acc > 0xFF ? 0xFF : (uint8_t)acc;
            BattleCommand_CheckHit(E);
            W(wCurDamage) = 0xFF;
            W(wCurDamage + 1) = 0xFF;
            W(wCriticalHit) = 2;
            return;
        }
    }
    W(wCriticalHit) = 0xFF;
    W(wAttackMissed) = 1;
}

static void DoubleDamage(PkbEngine* E) {
    uint32_t v = (uint32_t)rd16(E, wCurDamage) << 1;
    wr16(E, wCurDamage, v > 0xFFFF ? 0xFFFF : (uint16_t)v);
}

static void BattleCommand_DoubleFlyingDamage(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_FLYING)) DoubleDamage(E);
}

static void BattleCommand_DoubleUndergroundDamage(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3_OPP) & (1 << SUBSTATUS_UNDERGROUND)) DoubleDamage(E);
}

static void BattleCommand_DoubleMinimizeDamage(PkbEngine* E) {
    if(W(player_turn(E) ? wEnemyMinimized : wPlayerMinimized)) DoubleDamage(E);
}

static void BattleCommand_ThunderAccuracy(PkbEngine* E) {
    uint16_t acc = (uint16_t)(GetBattleVarAddr(E, BATTLE_VARS_MOVE_TYPE) + 1);
    if(W(wBattleWeather) == WEATHER_RAIN) M(acc) = PERCENT(100);
    else if(W(wBattleWeather) == WEATHER_SUN) M(acc) = PERCENT(50) + 1;
}

static void BattleCommand_GetMagnitude(PkbEngine* E) {
    uint8_t b = BattleRandom(E, "BattleCommand_GetMagnitude");
    const uint8_t* p = pb_magnitude_power;
    while(p[0] < b) p += 3;
    reg_d = p[1];
    W(wTextDecimalByte) = p[2];
}

/* Return and Frustration: power = happiness (or 255 - happiness) * 10 / 25 */
static void HappinessPower(PkbEngine* E, uint8_t happiness) {
    H(hMultiplicand + 0) = 0;
    H(hMultiplicand + 1) = 0;
    H(hMultiplicand + 2) = happiness;
    H(hMultiplier) = 10;
    Multiply(E);
    H(hDivisor) = 25;
    Divide(E, 4);
    reg_d = H(hQuotient + 3);
}

static void BattleCommand_HappinessPower(PkbEngine* E) {
    HappinessPower(E, player_turn(E) ? W(wBattleMonHappiness) : W(wEnemyMonHappiness));
}

static void BattleCommand_FrustrationPower(PkbEngine* E) {
    HappinessPower(E, (uint8_t)(0xFF - (player_turn(E) ? W(wBattleMonHappiness) : W(wEnemyMonHappiness))));
}

/* engine/battle/hidden_power.asm: type and power from the user's DVs */
static void HiddenPowerDamage(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonDVs : wEnemyMonDVs;
    uint8_t atkdef = M(hl), spdspc = M(hl + 1);
    uint8_t a = (uint8_t)(((atkdef >> 4) & 8) | ((atkdef & 8) >> 1) | ((spdspc >> 6) & 2) | ((spdspc & 8) >> 3));
    a = (uint8_t)(a * 5 + (spdspc & 3));
    uint8_t power = (uint8_t)((a >> 1) + 30 + 1);
    uint8_t type = (uint8_t)(((atkdef >> 4 & 3) << 2 | (atkdef & 3)) + 1); /* skip Normal */
    if(type >= BIRD) {
        type++;
        if(type >= UNUSED_TYPES) type += UNUSED_TYPES_END - UNUSED_TYPES;
    }
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE_TYPE)) = type;
    BattleCommand_DamageStats(E);
    reg_d = power;
}

static void BattleCommand_HiddenPower(PkbEngine* E) {
    if(W(wAttackMissed)) return;
    HiddenPowerDamage(E);
}

/* ---- other moves ---- */

static void BattleCommand_Heal(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wBattleMonHP : wEnemyMonHP;
    uint16_t hl = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
    if(rd16(E, de) == rd16(E, hl)) {
        AnimateFailedMove(E); /* .hp_full */
        return;
    }
    uint16_t bc;
    if(GetBattleVar(E, BATTLE_VARS_MOVE_ANIM) == REST) {
        res_var(E, BATTLE_VARS_SUBSTATUS5, 1 << SUBSTATUS_TOXIC);
        M(GetBattleVarAddr(E, BATTLE_VARS_STATUS)) = REST_SLEEP_TURNS + 1;
        if(player_turn(E)) CalcPlayerStats(E);
        else CalcEnemyStats(E);
        bc = GetMaxHP(E);
    } else {
        bc = GetHalfMaxHP(E);
    }
    AnimateCurrentMove(E);
    BattleCommand_SwitchTurn(E);
    RestoreHP(E, bc);
    BattleCommand_SwitchTurn(E);
    UpdateUserInParty(E);
}

static void BattleCommand_Selfdestruct(PkbEngine* E) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_STATUS);
    M(hl) = 0;     /* status */
    M(hl + 2) = 0; /* HP */
    M(hl + 3) = 0;
    W(wBattleAnimParam) = 1;
    BattleCommand_LowerSub(E);
    /* LoadMoveAnim */
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
    res_var(E, BATTLE_VARS_SUBSTATUS4, 1 << SUBSTATUS_LEECH_SEED);
    res_var(E, BATTLE_VARS_SUBSTATUS5_OPP, 1 << SUBSTATUS_DESTINY_BOND);
}

static void BattleCommand_PayDay(PkbEngine* E) {
    uint8_t level = player_turn(E) ? W(wBattleMonLevel) : W(wEnemyMonLevel);
    unsigned v = (unsigned)(uint8_t)(level << 1) + W(wPayDayMoney + 2);
    W(wPayDayMoney + 2) = (uint8_t)v;
    if(v > 0xFF && !++W(wPayDayMoney + 1)) W(wPayDayMoney)++;
}

/* Mist and Focus Energy: set the user's flag, or fail if it is already set */
static void SetUserFlag(PkbEngine* E, uint8_t bit) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS4);
    if(M(hl) & (1 << bit)) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << bit;
    AnimateCurrentMove(E);
}

/* ---- script control, two-turn moves, rampage, screens, forced switches ---- */

void SkipToBattleCommand(PkbEngine* E, uint8_t b) {
    uint16_t hl = (uint16_t)(W(wBattleScriptBufferAddress) | W(wBattleScriptBufferAddress + 1) << 8);
    while(M(hl++) != b) {
    }
    W(wBattleScriptBufferAddress) = (uint8_t)hl;
    W(wBattleScriptBufferAddress + 1) = (uint8_t)(hl >> 8);
}

/* Metronome, Mirror Move, Sleep Talk: run the new move's script as charged
 * (so usedmovetext and doturn skip), then end the calling one */
void ResetTurn(PkbEngine* E) {
    W(player_turn(E) ? wPlayerCharging : wEnemyCharging) = 1;
    W(wAlreadyDisobeyed) = 0;
    DoMove(E);
    EndMoveEffect(E);
}

void ClearLastMove(PkbEngine* E) {
    M(GetBattleVarAddr(E, BATTLE_VARS_LAST_COUNTER_MOVE)) = 0;
    M(GetBattleVarAddr(E, BATTLE_VARS_LAST_MOVE)) = 0;
}

void ResetActorDisable(PkbEngine* E) {
    if(!player_turn(E)) {
        W(wEnemyDisableCount) = 0;
        W(wEnemyDisabledMove) = 0;
    } else {
        W(wPlayerDisableCount) = 0;
        W(wDisabledMove) = 0;
    }
}

bool CheckUserMove(PkbEngine* E, uint8_t move) {
    uint16_t de = player_turn(E) ? wBattleMonMoves : wEnemyMonMoves;
    for(int i = 0; i < NUM_MOVES; i++) {
        if(M(de + i) == move) return true;
    }
    return false;
}

/* Lowers the user's own stat a (Curse's Speed drop); returns wFailedMessage */
uint8_t LowerStat(PkbEngine* E, uint8_t a) {
    W(wLoweredStat) = a;
    uint16_t hl = player_turn(E) ? wPlayerStatLevels : wEnemyStatLevels;
    uint8_t c = a & 0x0F;
    hl = (uint16_t)(hl + c);
    uint8_t b = (uint8_t)(M(hl) - 1);
    if(!b) goto cant_lower;
    if((a & 0xF0) && !--b) b++;
    M(hl) = b;
    /* TryLowerStat on the user's own stats still recalculates the other side's */
    if(c < ACCURACY && !TryLowerStat(E, player_turn(E) ? wBattleMonAttack : wEnemyMonAttack, c)) {
        M(hl)++;
        goto cant_lower;
    }
    if(player_turn(E)) CalcPlayerStats(E);
    else CalcEnemyStats(E);
    W(wFailedMessage) = 0;
    return 0;
cant_lower:
    W(wFailedMessage) = 2;
    return 2;
}

static void BattleCommand_CheckRampage(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount;
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3);
    if(!(M(hl) & (1 << SUBSTATUS_RAMPAGE))) return;
    if(--M(de) == 0) {
        M(hl) &= (uint8_t)~(1 << SUBSTATUS_RAMPAGE);
        BattleCommand_SwitchTurn(E);
        bool safeguard = SafeCheckSafeguard(E);
        BattleCommand_SwitchTurn(E);
        if(!safeguard) {
            M(hl) |= 1 << SUBSTATUS_CONFUSED;
            M(de + 1) = (uint8_t)((BattleRandom(E, "BattleCommand_CheckRampage") & 1) + 2); /* ConfuseCount */
        }
    }
    SkipToBattleCommand(E, PB_CMD_RAMPAGE);
}

static void BattleCommand_Rampage(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_STATUS) & SLP_MASK) return; /* not during Sleep Talk */
    uint16_t de = player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount;
    set_var(E, BATTLE_VARS_SUBSTATUS3, 1 << SUBSTATUS_RAMPAGE);
    M(de) = (uint8_t)((BattleRandom(E, "BattleCommand_Rampage") & 1) + 1);
    W(wSomeoneIsRampaging) = 1;
}

static void BattleCommand_TrapTarget(PkbEngine* E) {
    if(W(wAttackMissed)) return;
    uint16_t hl = player_turn(E) ? wEnemyWrapCount : wPlayerWrapCount;
    uint16_t de = player_turn(E) ? wEnemyTrappingMove : wPlayerTrappingMove;
    if(M(hl)) return;
    if(CheckSubstituteOpp(E)) return;
    M(hl) = (uint8_t)((BattleRandom(E, "BattleCommand_TrapTarget") & 3) + 3);
    M(de) = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
}

static void BattleCommand_RageDamage(PkbEngine* E) {
    uint16_t bc = rd16(E, wCurDamage);
    uint32_t hl = bc;
    for(uint8_t a = player_turn(E) ? W(wPlayerRageCounter) : W(wEnemyRageCounter); a; a--) {
        hl += bc;
        if(hl > 0xFFFF) {
            hl = 0xFFFF;
            break;
        }
    }
    wr16(E, wCurDamage, (uint16_t)hl);
}

static void BattleCommand_Charge(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_STATUS) & SLP_MASK) {
        BattleCommand_RaiseSub(E);
        EndMoveEffect(E);
        return;
    }
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3);
    M(hl) |= 1 << SUBSTATUS_CHARGED;
    BattleCommand_LowerSub(E);
    W(wBattleAfterAnim) = 0;
    W(wBattleAnimParam) = 1;
    /* LoadMoveAnim */
    W(wFXAnimID + 1) = 0;
    uint8_t b = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
    if(b != FLY && b != DIG) BattleCommand_RaiseSub(E); /* else DisappearUser (graphics) */
    if(b == FLY) M(hl) |= 1 << SUBSTATUS_FLYING;
    else if(b == DIG) M(hl) |= 1 << SUBSTATUS_UNDERGROUND;
    if(!CheckUserIsCharging(E)) {
        M(GetBattleVarAddr(E, BATTLE_VARS_LAST_COUNTER_MOVE)) = b;
        M(GetBattleVarAddr(E, BATTLE_VARS_LAST_MOVE)) = b;
    }
    ResetDamage(E);
    if(GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT) == EFFECT_SKULL_BASH) SkipToBattleCommand(E, PB_CMD_ENDTURN);
    else EndMoveEffect(E);
}

static void BattleCommand_CheckCharge(PkbEngine* E) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS3);
    if(!(M(hl) & (1 << SUBSTATUS_CHARGED))) return;
    M(hl) &= (uint8_t)~(1 << SUBSTATUS_CHARGED | 1 << SUBSTATUS_UNDERGROUND | 1 << SUBSTATUS_FLYING);
    SkipToBattleCommand(E, PB_CMD_CHARGE);
}

static void BattleCommand_SkipSunCharge(PkbEngine* E) {
    if(W(wBattleWeather) == WEATHER_SUN) SkipToBattleCommand(E, PB_CMD_CHARGE);
}

/* Mean Look, Spider Web: sets the user's flag, which keeps the target in */
static void BattleCommand_ArenaTrap(PkbEngine* E) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS5);
    if(CheckHiddenOpponent(E) || (M(hl) & (1 << SUBSTATUS_CANT_RUN))) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << SUBSTATUS_CANT_RUN;
    AnimateCurrentMove(E);
}

static void BattleCommand_FakeOut(PkbEngine* E) {
    if(W(wAttackMissed)) return;
    if(!CheckSubstituteOpp(E) && !(GetBattleVar(E, BATTLE_VARS_STATUS_OPP) & (1 << FRZ | SLP_MASK)) &&
       !CheckOpponentWentFirst(E)) {
        FlinchTarget(E);
        return;
    }
    W(wAttackMissed) = 1;
}

static void BattleCommand_Screen(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerScreens : wEnemyScreens;
    uint16_t bc = player_turn(E) ? wPlayerLightScreenCount : wEnemyLightScreenCount;
    uint8_t bit = SCREENS_LIGHT_SCREEN;
    if(GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT) != EFFECT_LIGHT_SCREEN) {
        bit = SCREENS_REFLECT;
        bc++; /* LightScreenCount -> ReflectCount */
    }
    if(M(hl) & (1 << bit)) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << bit;
    M(bc) = 5;
    AnimateCurrentMove(E);
}

/* Morning Sun, Synthesis, Moonlight: link battles ignore the time of day, so
 * only the weather counts (half the max HP, all of it in sun, a quarter in
 * rain or sandstorm) */
static void BattleCommand_TimeBasedHealContinue(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
    uint16_t de = player_turn(E) ? wBattleMonHP : wEnemyMonHP;
    if(rd16(E, hl) == rd16(E, de)) {
        AnimateFailedMove(E); /* .Full */
        return;
    }
    uint8_t c = 2;
    if(W(wBattleWeather)) c = W(wBattleWeather) == WEATHER_SUN ? 3 : 1;
    uint16_t bc = c == 3 ? GetMaxHP(E) : c == 2 ? GetHalfMaxHP(E) : GetQuarterMaxHP(E);
    AnimateCurrentMove(E);
    BattleCommand_SwitchTurn(E);
    RestoreHP(E, bc);
    BattleCommand_SwitchTurn(E);
    UpdateUserInParty(E);
}

static bool CheckPlayerHasMonToSwitchTo(PkbEngine* E) { /* returns carry: none */
    for(uint8_t e = 0; e < W(wPartyCount); e++) {
        if(e == W(wCurBattleMon)) continue;
        uint16_t hl = GetPartyLocation(wPartyMon1HP, e);
        if(M(hl) | M(hl + 1)) return false;
    }
    return true;
}

/* Roar, Whirlwind. Link battles are trainer battles: the target is replaced
 * by a random fit party mon (no exchange; both Game Boys roll the same). */
static void BattleCommand_ForceSwitch(PkbEngine* E) {
    uint8_t type = W(wBattleType);
    if(type == BATTLETYPE_FORCESHINY || type == BATTLETYPE_TRAP || type == BATTLETYPE_CELEBI ||
       type == BATTLETYPE_SUICUNE || W(wAttackMissed)) {
        goto fail;
    }
    if(player_turn(E)) {
        if(FindAliveEnemyMons(E) || !W(wEnemyGoesFirst)) goto fail;
        UpdateEnemyMonInParty(E);
        W(wBattleAnimParam) = 1;
        AnimateCurrentMove(E);
        uint8_t b = W(wOTPartyCount), c = W(wCurOTMon), a;
        for(;;) {
            a = BattleRandom(E, "BattleCommand_ForceSwitch") & 7;
            if(a >= b || a == c) continue;
            uint16_t hp = GetPartyLocation(wOTPartyMon1HP, a);
            if(M(hp) | M(hp + 1)) break;
        }
        W(wEnemySwitchMonIndex) = (uint8_t)(a + 1);
        ForceEnemySwitch(E);
    } else {
        if(CheckPlayerHasMonToSwitchTo(E) || W(wEnemyGoesFirst) == 1) goto fail;
        UpdateBattleMonInParty(E);
        W(wBattleAnimParam) = 1;
        AnimateCurrentMove(E);
        uint8_t b = W(wPartyCount), c = W(wCurBattleMon), a;
        for(;;) {
            a = BattleRandom(E, "BattleCommand_ForceSwitch.player") & 7;
            if(a >= b || a == c) continue;
            uint16_t hp = GetPartyLocation(wPartyMon1HP, a);
            if(M(hp) | M(hp + 1)) break;
        }
        W(wCurPartyMon) = a;
        SwitchPlayerMon(E);
    }
    SpikesDamage(E);
    return;
fail:
    AnimateFailedMove(E);
}

/* ---- dispatch ---- */

/* run_command maps the stat commands to stats by offset */
_Static_assert(PB_CMD_EVASIONUP - PB_CMD_ATTACKUP == EVASION - ATTACK &&
                   PB_CMD_EVASIONUP2 - PB_CMD_ATTACKUP2 == EVASION - ATTACK &&
                   PB_CMD_EVASIONDOWN - PB_CMD_ATTACKDOWN == EVASION - ATTACK &&
                   PB_CMD_EVASIONDOWN2 - PB_CMD_ATTACKDOWN2 == EVASION - ATTACK,
               "stat command order");

static bool pkb_engine_command_supported(uint8_t cmd) {
    switch(cmd) {
    case PB_CMD_CHECKOBEDIENCE:
    case PB_CMD_USEDMOVETEXT:
    case PB_CMD_DOTURN:
    case PB_CMD_CRITICAL:
    case PB_CMD_DAMAGESTATS:
    case PB_CMD_DAMAGECALC:
    case PB_CMD_STAB:
    case PB_CMD_DAMAGEVARIATION:
    case PB_CMD_CHECKHIT:
    case PB_CMD_MOVEANIM:
    case PB_CMD_FAILURETEXT:
    case PB_CMD_APPLYDAMAGE:
    case PB_CMD_CRITICALTEXT:
    case PB_CMD_SUPEREFFECTIVETEXT:
    case PB_CMD_CHECKFAINT:
    case PB_CMD_BUILDOPPONENTRAGE:
    case PB_CMD_KINGSROCK:
    case PB_CMD_RECOIL:
    case PB_CMD_ENDMOVE:
    case PB_CMD_ENDTURN:
    /* status, stats, fixed damage */
    case PB_CMD_LOWERSUB:
    case PB_CMD_RAISESUB:
    case PB_CMD_LOWERSUBNOANIM:
    case PB_CMD_RAISESUBNOANIM:
    case PB_CMD_MOVEANIMNOSUB:
    case PB_CMD_MOVEDELAY:
    case PB_CMD_CLEARTEXT:
    case PB_CMD_SWITCHTURN:
    case PB_CMD_EFFECTCHANCE:
    case PB_CMD_CHECKSAFEGUARD:
    case PB_CMD_SLEEPTARGET:
    case PB_CMD_POISONTARGET:
    case PB_CMD_POISON:
    case PB_CMD_DRAINTARGET:
    case PB_CMD_EATDREAM:
    case PB_CMD_BURNTARGET:
    case PB_CMD_FREEZETARGET:
    case PB_CMD_PARALYZETARGET:
    case PB_CMD_PARALYZE:
    case PB_CMD_CONFUSE:
    case PB_CMD_CONFUSETARGET:
    case PB_CMD_FLINCHTARGET:
    case PB_CMD_TRISTATUSCHANCE:
    case PB_CMD_DEFROST:
    case PB_CMD_SNORE:
    case PB_CMD_ATTACKUP:
    case PB_CMD_DEFENSEUP:
    case PB_CMD_SPEEDUP:
    case PB_CMD_SPECIALATTACKUP:
    case PB_CMD_SPECIALDEFENSEUP:
    case PB_CMD_ACCURACYUP:
    case PB_CMD_EVASIONUP:
    case PB_CMD_ATTACKUP2:
    case PB_CMD_DEFENSEUP2:
    case PB_CMD_SPEEDUP2:
    case PB_CMD_SPECIALATTACKUP2:
    case PB_CMD_SPECIALDEFENSEUP2:
    case PB_CMD_ACCURACYUP2:
    case PB_CMD_EVASIONUP2:
    case PB_CMD_ATTACKDOWN:
    case PB_CMD_DEFENSEDOWN:
    case PB_CMD_SPEEDDOWN:
    case PB_CMD_SPECIALATTACKDOWN:
    case PB_CMD_SPECIALDEFENSEDOWN:
    case PB_CMD_ACCURACYDOWN:
    case PB_CMD_EVASIONDOWN:
    case PB_CMD_ATTACKDOWN2:
    case PB_CMD_DEFENSEDOWN2:
    case PB_CMD_SPEEDDOWN2:
    case PB_CMD_SPECIALATTACKDOWN2:
    case PB_CMD_SPECIALDEFENSEDOWN2:
    case PB_CMD_ACCURACYDOWN2:
    case PB_CMD_EVASIONDOWN2:
    case PB_CMD_STATUPANIM:
    case PB_CMD_STATDOWNANIM:
    case PB_CMD_STATUPMESSAGE:
    case PB_CMD_STATDOWNMESSAGE:
    case PB_CMD_STATUPFAILTEXT:
    case PB_CMD_STATDOWNFAILTEXT:
    case PB_CMD_ALLSTATSUP:
    case PB_CMD_RESETSTATS:
    case PB_CMD_CURL:
    case PB_CMD_STARTLOOP:
    case PB_CMD_ENDLOOP:
    case PB_CMD_CLEARMISSDAMAGE:
    case PB_CMD_SUPEREFFECTIVELOOPTEXT:
    case PB_CMD_TRIPLEKICK:
    case PB_CMD_KICKCOUNTER:
    case PB_CMD_OHKO:
    case PB_CMD_CONSTANTDAMAGE:
    case PB_CMD_RESETTYPEMATCHUP:
    case PB_CMD_DOUBLEFLYINGDAMAGE:
    case PB_CMD_DOUBLEUNDERGROUNDDAMAGE:
    case PB_CMD_DOUBLEMINIMIZEDAMAGE:
    case PB_CMD_THUNDERACCURACY:
    case PB_CMD_GETMAGNITUDE:
    case PB_CMD_HAPPINESSPOWER:
    case PB_CMD_FRUSTRATIONPOWER:
    case PB_CMD_HIDDENPOWER:
    case PB_CMD_FALSESWIPE:
    case PB_CMD_HEAL:
    case PB_CMD_SELFDESTRUCT:
    case PB_CMD_SPLASH:
    case PB_CMD_PAYDAY:
    case PB_CMD_MIST:
    case PB_CMD_FOCUSENERGY:
    /* script control, two-turn moves, screens */
    case PB_CMD_CHECKRAMPAGE:
    case PB_CMD_RAMPAGE:
    case PB_CMD_RECHARGENEXTTURN:
    case PB_CMD_TRAPTARGET:
    case PB_CMD_RAGEDAMAGE:
    case PB_CMD_CHARGE:
    case PB_CMD_CHECKCHARGE:
    case PB_CMD_SKIPSUNCHARGE:
    case PB_CMD_ARENATRAP:
    case PB_CMD_FAKEOUT:
    case PB_CMD_SCREEN:
    case PB_CMD_HEALMORN:
    case PB_CMD_HEALDAY:
    case PB_CMD_HEALNITE:
    case PB_CMD_BIDEFAILTEXT:
    case PB_CMD_FORCESWITCH:
        return true;
    default:
        return MoveEffectCommandSupported(cmd);
    }
}

static void run_command(PkbEngine* E, uint8_t cmd) {
    switch(cmd) {
    case PB_CMD_CHECKOBEDIENCE:
        BattleCommand_CheckObedience(E);
        break;
    case PB_CMD_USEDMOVETEXT:
        BattleCommand_UsedMoveText(E);
        break;
    case PB_CMD_DOTURN:
        BattleCommand_DoTurn(E);
        break;
    case PB_CMD_CRITICAL:
        BattleCommand_Critical(E);
        break;
    case PB_CMD_DAMAGESTATS:
        BattleCommand_DamageStats(E);
        break;
    case PB_CMD_DAMAGECALC:
        BattleCommand_DamageCalc(E);
        break;
    case PB_CMD_STAB:
        BattleCommand_Stab(E);
        break;
    case PB_CMD_DAMAGEVARIATION:
        BattleCommand_DamageVariation(E);
        break;
    case PB_CMD_CHECKHIT:
        BattleCommand_CheckHit(E);
        break;
    case PB_CMD_MOVEANIM:
        BattleCommand_MoveAnim(E);
        break;
    case PB_CMD_FAILURETEXT:
        BattleCommand_FailureText(E);
        break;
    case PB_CMD_APPLYDAMAGE:
        BattleCommand_ApplyDamage(E);
        break;
    case PB_CMD_CRITICALTEXT:
        BattleCommand_CriticalText(E);
        break;
    case PB_CMD_SUPEREFFECTIVETEXT:
        break;
    case PB_CMD_CHECKFAINT:
        BattleCommand_CheckFaint(E);
        break;
    case PB_CMD_BUILDOPPONENTRAGE:
        BattleCommand_BuildOpponentRage(E);
        break;
    case PB_CMD_KINGSROCK:
        BattleCommand_HeldFlinch(E);
        break;
    case PB_CMD_RECOIL:
        BattleCommand_Recoil(E);
        break;
    /* status, stats, fixed damage */
    case PB_CMD_LOWERSUB:
        BattleCommand_LowerSub(E);
        break;
    case PB_CMD_RAISESUB:
        BattleCommand_RaiseSub(E);
        break;
    case PB_CMD_LOWERSUBNOANIM: /* redraw only */
    case PB_CMD_RAISESUBNOANIM:
    case PB_CMD_MOVEDELAY:
    case PB_CMD_CLEARTEXT:
    case PB_CMD_SUPEREFFECTIVELOOPTEXT:
    case PB_CMD_STATUPMESSAGE:
    case PB_CMD_STATDOWNMESSAGE:
    case PB_CMD_STATUPFAILTEXT:
    case PB_CMD_STATDOWNFAILTEXT:
        break;
    case PB_CMD_MOVEANIMNOSUB:
        BattleCommand_MoveAnimNoSub(E);
        break;
    case PB_CMD_SWITCHTURN:
        BattleCommand_SwitchTurn(E);
        break;
    case PB_CMD_EFFECTCHANCE:
        BattleCommand_EffectChance(E);
        break;
    case PB_CMD_CHECKSAFEGUARD:
        BattleCommand_CheckSafeguard(E);
        break;
    case PB_CMD_SLEEPTARGET:
        BattleCommand_SleepTarget(E);
        break;
    case PB_CMD_POISONTARGET:
        BattleCommand_PoisonTarget(E);
        break;
    case PB_CMD_POISON:
        BattleCommand_Poison(E);
        break;
    case PB_CMD_DRAINTARGET:
    case PB_CMD_EATDREAM:
        SapHealth(E);
        break;
    case PB_CMD_BURNTARGET:
        BattleCommand_BurnTarget(E);
        break;
    case PB_CMD_FREEZETARGET:
        BattleCommand_FreezeTarget(E);
        break;
    case PB_CMD_PARALYZETARGET:
        BattleCommand_ParalyzeTarget(E);
        break;
    case PB_CMD_PARALYZE:
        BattleCommand_Paralyze(E);
        break;
    case PB_CMD_CONFUSE:
        BattleCommand_Confuse(E);
        break;
    case PB_CMD_CONFUSETARGET:
        BattleCommand_ConfuseTarget(E);
        break;
    case PB_CMD_FLINCHTARGET:
        BattleCommand_FlinchTarget(E);
        break;
    case PB_CMD_TRISTATUSCHANCE:
        BattleCommand_TriStatusChance(E);
        break;
    case PB_CMD_DEFROST:
        BattleCommand_Defrost(E);
        break;
    case PB_CMD_SNORE:
        BattleCommand_Snore(E);
        break;
    case PB_CMD_ATTACKUP:
    case PB_CMD_DEFENSEUP:
    case PB_CMD_SPEEDUP:
    case PB_CMD_SPECIALATTACKUP:
    case PB_CMD_SPECIALDEFENSEUP:
    case PB_CMD_ACCURACYUP:
    case PB_CMD_EVASIONUP:
        BattleCommand_StatUp(E, (uint8_t)(ATTACK + (cmd - PB_CMD_ATTACKUP)));
        break;
    case PB_CMD_ATTACKUP2:
    case PB_CMD_DEFENSEUP2:
    case PB_CMD_SPEEDUP2:
    case PB_CMD_SPECIALATTACKUP2:
    case PB_CMD_SPECIALDEFENSEUP2:
    case PB_CMD_ACCURACYUP2:
    case PB_CMD_EVASIONUP2:
        BattleCommand_StatUp(E, (uint8_t)(0x10 | (ATTACK + (cmd - PB_CMD_ATTACKUP2))));
        break;
    case PB_CMD_ATTACKDOWN:
    case PB_CMD_DEFENSEDOWN:
    case PB_CMD_SPEEDDOWN:
    case PB_CMD_SPECIALATTACKDOWN:
    case PB_CMD_SPECIALDEFENSEDOWN:
    case PB_CMD_ACCURACYDOWN:
    case PB_CMD_EVASIONDOWN:
        BattleCommand_StatDown(E, (uint8_t)(ATTACK + (cmd - PB_CMD_ATTACKDOWN)));
        break;
    case PB_CMD_ATTACKDOWN2:
    case PB_CMD_DEFENSEDOWN2:
    case PB_CMD_SPEEDDOWN2:
    case PB_CMD_SPECIALATTACKDOWN2:
    case PB_CMD_SPECIALDEFENSEDOWN2:
    case PB_CMD_ACCURACYDOWN2:
    case PB_CMD_EVASIONDOWN2:
        BattleCommand_StatDown(E, (uint8_t)(0x10 | (ATTACK + (cmd - PB_CMD_ATTACKDOWN2))));
        break;
    case PB_CMD_STATUPANIM: /* BattleCommand_StatUpDownAnim */
    case PB_CMD_STATDOWNANIM:
        if(!W(wAttackMissed)) W(wBattleAnimParam) = 0;
        break;
    case PB_CMD_ALLSTATSUP:
        BattleCommand_AllStatsUp(E);
        break;
    case PB_CMD_RESETSTATS:
        BattleCommand_ResetStats(E);
        break;
    case PB_CMD_CURL:
        BattleCommand_Curl(E);
        break;
    case PB_CMD_STARTLOOP:
        BattleCommand_StartLoop(E);
        break;
    case PB_CMD_ENDLOOP:
        BattleCommand_EndLoop(E);
        break;
    case PB_CMD_CLEARMISSDAMAGE:
        BattleCommand_ClearMissDamage(E);
        break;
    case PB_CMD_TRIPLEKICK:
        BattleCommand_TripleKick(E);
        break;
    case PB_CMD_KICKCOUNTER:
        BattleCommand_KickCounter(E);
        break;
    case PB_CMD_OHKO:
        BattleCommand_OHKO(E);
        break;
    case PB_CMD_CONSTANTDAMAGE:
        BattleCommand_ConstantDamage(E);
        break;
    case PB_CMD_RESETTYPEMATCHUP:
        BattleCommand_ResetTypeMatchup(E);
        break;
    case PB_CMD_DOUBLEFLYINGDAMAGE:
        BattleCommand_DoubleFlyingDamage(E);
        break;
    case PB_CMD_DOUBLEUNDERGROUNDDAMAGE:
        BattleCommand_DoubleUndergroundDamage(E);
        break;
    case PB_CMD_DOUBLEMINIMIZEDAMAGE:
        BattleCommand_DoubleMinimizeDamage(E);
        break;
    case PB_CMD_THUNDERACCURACY:
        BattleCommand_ThunderAccuracy(E);
        break;
    case PB_CMD_GETMAGNITUDE:
        BattleCommand_GetMagnitude(E);
        break;
    case PB_CMD_HAPPINESSPOWER:
        BattleCommand_HappinessPower(E);
        break;
    case PB_CMD_FRUSTRATIONPOWER:
        BattleCommand_FrustrationPower(E);
        break;
    case PB_CMD_HIDDENPOWER:
        BattleCommand_HiddenPower(E);
        break;
    case PB_CMD_FALSESWIPE:
        BattleCommand_FalseSwipe(E);
        break;
    case PB_CMD_HEAL:
        BattleCommand_Heal(E);
        break;
    case PB_CMD_SELFDESTRUCT:
        BattleCommand_Selfdestruct(E);
        break;
    case PB_CMD_SPLASH:
        AnimateCurrentMove(E);
        break;
    case PB_CMD_PAYDAY:
        BattleCommand_PayDay(E);
        break;
    case PB_CMD_MIST:
        SetUserFlag(E, SUBSTATUS_MIST);
        break;
    case PB_CMD_FOCUSENERGY:
        SetUserFlag(E, SUBSTATUS_FOCUS_ENERGY);
        break;
    /* script control, two-turn moves, screens */
    case PB_CMD_CHECKRAMPAGE:
        BattleCommand_CheckRampage(E);
        break;
    case PB_CMD_RAMPAGE:
        BattleCommand_Rampage(E);
        break;
    case PB_CMD_RECHARGENEXTTURN:
        set_var(E, BATTLE_VARS_SUBSTATUS4, 1 << SUBSTATUS_RECHARGE);
        break;
    case PB_CMD_TRAPTARGET:
        BattleCommand_TrapTarget(E);
        break;
    case PB_CMD_RAGEDAMAGE:
        BattleCommand_RageDamage(E);
        break;
    case PB_CMD_CHARGE:
        BattleCommand_Charge(E);
        break;
    case PB_CMD_CHECKCHARGE:
        BattleCommand_CheckCharge(E);
        break;
    case PB_CMD_SKIPSUNCHARGE:
        BattleCommand_SkipSunCharge(E);
        break;
    case PB_CMD_ARENATRAP:
        BattleCommand_ArenaTrap(E);
        break;
    case PB_CMD_FAKEOUT:
        BattleCommand_FakeOut(E);
        break;
    case PB_CMD_SCREEN:
        BattleCommand_Screen(E);
        break;
    case PB_CMD_HEALMORN:
    case PB_CMD_HEALDAY:
    case PB_CMD_HEALNITE:
        BattleCommand_TimeBasedHealContinue(E);
        break;
    case PB_CMD_BIDEFAILTEXT: /* text only */
        break;
    case PB_CMD_FORCESWITCH:
        BattleCommand_ForceSwitch(E);
        break;
    default:
        if(!RunMoveEffectCommand(E, cmd)) unsupported(E, GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT), cmd);
        break;
    }
}

/* Effects whose whole script is ported (all of them). */
bool pkb_engine_effect_supported(uint8_t effect) {
    if(effect >= NUM_MOVE_EFFECTS) return false;
    for(uint16_t i = pb_move_effect_script_off[effect]; i < pb_move_effect_script_off[effect + 1]; i++) {
        if(!pkb_engine_command_supported(pb_move_effect_scripts[i])) return false;
    }
    return true;
}

bool pkb_engine_move_filter(void* ctx, uint8_t move) {
    (void)ctx;
    return move && move <= NUM_ATTACKS && pkb_engine_effect_supported(pb_moves[move - 1].effect);
}

#ifdef PKB_HOST
/* Host tests (test_ai): the damage of `move` between the two battle mons as
 * DamageStats, DamageCalc and Stab compute it, without a critical hit or
 * the random roll. Changes E (move struct, damage, HRAM): use a copy. */
uint16_t pkb_engine_test_damage(PkbEngine* E, bool player, uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    uint16_t ms = player ? wPlayerMoveStruct : wEnemyMoveStruct;
    H(hBattleTurn) = player ? 0 : 1;
    W(ms + MOVE_ANIM) = move;
    W(ms + MOVE_EFFECT) = m->effect;
    W(ms + MOVE_POWER) = m->power;
    W(ms + MOVE_TYPE) = m->type;
    W(ms + MOVE_ACC) = m->accuracy;
    W(ms + MOVE_PP) = m->pp;
    W(ms + MOVE_CHANCE) = m->effect_chance;
    W(wCriticalHit) = 0;
    W(wAttackMissed) = 0;
    W(wTypeModifier) = 0;
    BattleCommand_DamageStats(E);
    BattleCommand_DamageCalc(E);
    BattleCommand_Stab(E);
    return rd16(E, wCurDamage);
}
#endif
