/*
 * engine/battle/move_effects: the battle commands that have an .asm file of
 * their own (the rest, such as Snore and Hidden Power, are in effects.c).
 *
 * Text is dropped as everywhere else. FailMove and PrintDidntAffect2 are
 * AnimateFailedMove plus text; FailMimic is text only.
 */
#include "engine_i.h"

#define reg_b (E->reg_b)
#define reg_c (E->reg_c)
#define reg_d (E->reg_d)
#define reg_e (E->reg_e)

static uint16_t OpponentPartyAttr(PkbEngine* E, uint8_t attr) {
    return player_turn(E) ? OTPartyAttr(E, attr) : BattlePartyAttr(E, attr);
}

/* Index of move b among the moves at hl, searched like the asm loops do
 * (no bound: they run on past the four moves). The search gives up after
 * 256 bytes, where the Game Boy would hang. */
static bool find_move(PkbEngine* E, uint16_t hl, uint8_t b, uint8_t* c) {
    for(unsigned i = 0; i < 0x100; i++) {
        if(M(hl + i) == b) {
            *c = (uint8_t)i;
            return true;
        }
    }
    unsupported(E, GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT), 0xFF);
    return false;
}

/* wCurDamage doubled b - 1 times, $FFFF on overflow (Fury Cutter, Rollout) */
static void double_damage_times(PkbEngine* E, uint8_t b) {
    while(--b) {
        uint32_t v = (uint32_t)rd16(E, wCurDamage) << 1;
        if(v > 0xFFFF) {
            wr16(E, wCurDamage, 0xFFFF);
            return;
        }
        wr16(E, wCurDamage, (uint16_t)v);
    }
}

/* ---- attract.asm ---- */

/* engine/pokemon/mon_stats.asm GetGender for the mon wMonType names:
 * 1 male, 0 female, -1 genderless */
static int GetGender(PkbEngine* E) {
    uint16_t hl;
    switch(W(wMonType)) {
    case PARTYMON:
        hl = GetPartyLocation(wPartyMon1DVs, W(wCurPartyMon));
        break;
    case OTPARTYMON:
        hl = GetPartyLocation(wOTPartyMon1DVs, W(wCurPartyMon));
        break;
    case TEMPMON:
        hl = wTempMonDVs;
        break;
    default: /* wild (box mons are not used in battle) */
        hl = wEnemyMonDVs;
        break;
    }
    uint8_t b = (uint8_t)((M(hl) & 0xF0) | (M(hl + 1) >> 4)); /* attack and speed DVs */
    uint8_t ratio = pb_base_stats[(uint8_t)(W(wCurPartySpecies) - 1)].gender;
    if(ratio == 0xFF) return -1; /* GENDER_UNKNOWN */
    if(ratio == 0) return 1;
    if(ratio == GENDER_F100) return 0;
    return ratio < b ? 1 : 0;
}

/* returns carry: genderless, or both the same gender */
static bool CheckOppositeGender(PkbEngine* E) {
    W(wCurPartySpecies) = M(BattlePartyAttr(E, MON_SPECIES));
    W(wCurPartyMon) = W(wCurBattleMon);
    W(wMonType) = PARTYMON;
    int ours = GetGender(E);
    if(ours < 0) return true;
    W(wCurPartySpecies) = W(wTempEnemyMonSpecies);
    uint16_t hl = (W(wEnemySubStatus5) & (1 << SUBSTATUS_TRANSFORMED)) ? wEnemyBackupDVs : wEnemyMonDVs;
    W(wTempMonDVs) = M(hl);
    W(wTempMonDVs + 1) = M(hl + 1);
    W(wMonType) = TEMPMON;
    int theirs = GetGender(E);
    return theirs < 0 || theirs == ours;
}

static void BattleCommand_Attract(PkbEngine* E) {
    if(!W(wAttackMissed) && !CheckOppositeGender(E) && !CheckHiddenOpponent(E)) {
        uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1_OPP);
        if(!(M(hl) & (1 << SUBSTATUS_IN_LOVE))) {
            M(hl) |= 1 << SUBSTATUS_IN_LOVE;
            AnimateCurrentMove(E);
            return;
        }
    }
    AnimateFailedMove(E);
}

/* ---- baton_pass.asm ---- */

/* nz if a mon other than e of the d at hl has HP */
static bool CheckAnyOtherAliveMons(PkbEngine* E, uint16_t hl, uint8_t d, uint8_t e) {
    for(uint8_t c = 0; c < d; c++) {
        if(c == e) continue;
        uint16_t hp = GetPartyLocation(hl, c);
        if(M(hp) | M(hp + 1)) return true;
    }
    return false;
}

/* what Baton Pass does not pass on */
static void ResetBatonPassStatus(PkbEngine* E) {
    if(!(GetBattleVar(E, BATTLE_VARS_STATUS) & SLP_MASK)) res_var(E, BATTLE_VARS_SUBSTATUS1, 1 << SUBSTATUS_NIGHTMARE);
    ResetActorDisable(E);
    W(wPlayerSubStatus1) &= (uint8_t)~(1 << SUBSTATUS_IN_LOVE);
    W(wEnemySubStatus1) &= (uint8_t)~(1 << SUBSTATUS_IN_LOVE);
    res_var(E, BATTLE_VARS_SUBSTATUS5, 1 << SUBSTATUS_TRANSFORMED | 1 << SUBSTATUS_ENCORED);
    M(GetBattleVarAddr(E, BATTLE_VARS_LAST_MOVE)) = 0;
    W(wPlayerWrapCount) = 0;
    W(wEnemyWrapCount) = 0;
}

/* In a link battle the passing side picks its mon and both Game Boys
 * exchange actions again, so the other side learns the pick. */
static void BattleCommand_BatonPass(PkbEngine* E) {
    if(player_turn(E)) {
        if(!CheckAnyOtherAliveMons(E, wPartyMon1HP, W(wPartyCount), W(wCurBattleMon))) {
            AnimateFailedMove(E);
            return;
        }
        UpdateBattleMonInParty(E);
        AnimateCurrentMove(E);
        ForcePickSwitchMonInBattle(E);
        /* BatonPass_LinkPlayerSwitch */
        W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEITEM;
        LinkBattleSendReceiveAction(E);
        W(wBattlePlayerAction) = BATTLEPLAYERACTION_USEMOVE;
        PassedBattleMonEntrance(E);
        ResetBatonPassStatus(E);
        return;
    }
    if(W(wBattleMode) == WILD_BATTLE ||
       !CheckAnyOtherAliveMons(E, wOTPartyMon1HP, W(wOTPartyCount), W(wCurOTMon))) {
        AnimateFailedMove(E);
        return;
    }
    UpdateEnemyMonInParty(E);
    AnimateCurrentMove(E);
    /* BatonPass_LinkEnemySwitch: anything but a valid switch keeps the mon */
    LinkBattleSendReceiveAction(E);
    uint8_t a = W(wBattleAction);
    if(a < BATTLEACTION_SWITCH1 || a >= BATTLEACTION_SWITCH1 + W(wOTPartyCount)) {
        W(wBattleAction) = (uint8_t)(W(wCurOTMon) + BATTLEACTION_SWITCH1);
    }
    W(wEnemySwitchMonIndex) = 0;
    EnemySwitch_SetMode(E);
    ResetBattleParticipants(E);
    W(wApplyStatLevelMultipliersToEnemy) = 1;
    ApplyStatLevelMultiplierOnAllStats(E);
    SpikesDamage(E);
    ResetBatonPassStatus(E);
}

/* ---- belly_drum.asm ---- */

static void BattleCommand_BellyDrum(PkbEngine* E) {
    /* the first +2 comes before the HP check (game bug) */
    BattleCommand_StatUp(E, 0x10 | ATTACK);
    if(!W(wAttackMissed)) {
        uint16_t bc = GetHalfMaxHP(E);
        uint16_t hp = rd16(E, player_turn(E) ? wBattleMonHP : wEnemyMonHP);
        if(bc < hp) { /* CheckUserHasEnoughHP */
            AnimateCurrentMove(E);
            SubtractHPFromUser(E, bc);
            UpdateUserInParty(E);
            for(uint8_t a = MAX_STAT_LEVEL - BASE_STAT_LEVEL - 1; a; a--) BattleCommand_StatUp(E, 0x10 | ATTACK);
            return;
        }
    }
    AnimateFailedMove(E);
}

/* ---- bide.asm ---- */

static void BattleCommand_StoreEnergy(PkbEngine* E) {
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3) & (1 << SUBSTATUS_BIDE))) return;
    uint16_t hl = player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount;
    if(--M(hl)) {
        EndMoveEffect(E); /* still storing */
        return;
    }
    res_var(E, BATTLE_VARS_SUBSTATUS3, 1 << SUBSTATUS_BIDE);
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE_POWER)) = 1;
    uint16_t taken = player_turn(E) ? wPlayerDamageTaken : wEnemyDamageTaken;
    uint16_t charging = player_turn(E) ? wPlayerCharging : wEnemyCharging;
    uint8_t lo = M(taken + 1), hi = M(taken);
    uint8_t b = (uint8_t)(lo << 1);
    W(wCurDamage + 1) = b;
    W(wCurDamage) = (uint8_t)(hi << 1 | lo >> 7);
    if(hi & 0x80) {
        W(wCurDamage) = 0xFF;
        W(wCurDamage + 1) = 0xFF;
    }
    if(!(W(wCurDamage) | b)) W(wAttackMissed) = 1;
    M(taken) = 0;
    M(taken + 1) = 0;
    M(charging) = 0;
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE_ANIM)) = BIDE;
    SkipToBattleCommand(E, PB_CMD_UNLEASHENERGY);
}

static void BattleCommand_UnleashEnergy(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wPlayerDamageTaken : wEnemyDamageTaken;
    uint16_t bc = player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount;
    set_var(E, BATTLE_VARS_SUBSTATUS3, 1 << SUBSTATUS_BIDE);
    M(de) = 0;
    M(de + 1) = 0;
    W(wPlayerMoveStruct + MOVE_EFFECT) = 0;
    W(wEnemyMoveStruct + MOVE_EFFECT) = 0;
    M(bc) = (uint8_t)((BattleRandom(E, "BattleCommand_UnleashEnergy") & 1) + 2);
    W(wBattleAnimParam) = 1;
    AnimateCurrentMove(E);
    EndMoveEffect(E);
}

/* ---- conversion.asm, conversion2.asm ---- */

static void BattleCommand_Conversion(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonMoves : wEnemyMonMoves;
    uint16_t types = player_turn(E) ? wBattleMonType1 : wEnemyMonType1;
    /* the move types into wStringBuffer1, $FF-terminated */
    uint16_t de = wStringBuffer1;
    for(uint8_t c = 0; c < NUM_MOVES; c++) {
        uint8_t move = M(hl + c);
        if(!move) break;
        M(de++) = pb_moves[move - 1].type;
    }
    M(de) = 0xFF;
    M(de + 1) = 0xFF;
    M(de + 2) = 0xFF;
    bool other = false;
    for(uint16_t p = wStringBuffer1; M(p) != 0xFF; p++) {
        uint8_t a = M(p);
        if(a != CURSE_TYPE && a != M(types) && a != M(types + 1)) {
            other = true;
            break;
        }
    }
    if(!other) {
        AnimateFailedMove(E);
        return;
    }
    uint8_t a;
    for(;;) {
        a = M(wStringBuffer1 + (BattleRandom(E, "BattleCommand_Conversion") & 3));
        if(a == 0xFF || a == CURSE_TYPE || a == M(types) || a == M(types + 1)) continue;
        break;
    }
    M(types) = a;
    M(types + 1) = a;
    W(wNamedObjectIndex) = a;
    AnimateCurrentMove(E);
}

static void BattleCommand_Conversion2(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonType1 : wEnemyMonType1;
    uint8_t move = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    uint8_t d = move ? pb_moves[move - 1].type : 0;
    if(W(wAttackMissed) || !move || d == CURSE_TYPE) {
        AnimateFailedMove(E);
        return;
    }
    AnimateCurrentMove(E);
    BattleCommand_SwitchTurn(E);
    for(;;) {
        uint8_t a = BattleRandom(E, "BattleCommand_Conversion2") & 0x1F; /* maskbits TYPES_END */
        if(a >= UNUSED_TYPES && a < UNUSED_TYPES_END) continue;
        if(a >= TYPES_END) continue;
        M(hl) = a;
        M(hl + 1) = a;
        /* resists the opponent's last move? */
        uint16_t type = GetBattleVarAddr(E, BATTLE_VARS_MOVE_TYPE);
        uint8_t saved = M(type);
        M(type) = d;
        BattleCheckTypeMatchup(E);
        M(type) = saved;
        if(W(wTypeMatchup) < EFFECTIVE) break;
    }
    BattleCommand_SwitchTurn(E);
    W(wNamedObjectIndex) = M(hl);
}

/* ---- counter.asm, mirror_coat.asm ---- */

static void CounterCommon(PkbEngine* E, uint8_t own_effect, bool special) {
    W(wAttackMissed) = 1;
    uint8_t move = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    if(!move || GetMoveEffect(move) == own_effect) return;
    BattleCommand_ResetTypeMatchup(E);
    if(!W(wTypeMatchup)) return;
    if(!CheckOpponentWentFirst(E)) return;
    GetMoveData(E, (uint8_t)(GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP) - 1), wStringBuffer1);
    if(!W(wStringBuffer1 + MOVE_POWER)) return;
    if((W(wStringBuffer1 + MOVE_TYPE) >= SPECIAL) != special) return;
    uint16_t dmg = rd16(E, wCurDamage);
    if(!dmg) return;
    wr16(E, wCurDamage, dmg >= 0x8000 ? 0xFFFF : (uint16_t)(dmg << 1));
    W(wAttackMissed) = 0;
}

/* ---- curse.asm ---- */

static void BattleCommand_Curse(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wBattleMonType1 : wEnemyMonType1;
    uint16_t bc = player_turn(E) ? wPlayerStatLevels : wEnemyStatLevels;
    if(M(de) == GHOST || M(de + 1) == GHOST) {
        uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1_OPP);
        if(CheckHiddenOpponent(E) || CheckSubstituteOpp(E) || (M(hl) & (1 << SUBSTATUS_CURSE))) {
            AnimateFailedMove(E);
            return;
        }
        M(hl) |= 1 << SUBSTATUS_CURSE;
        AnimateCurrentMove(E);
        SubtractHPFromUser(E, GetHalfMaxHP(E));
        UpdateUserInParty(E);
        return;
    }
    if(M(bc) >= MAX_STAT_LEVEL && M(bc + 1) >= MAX_STAT_LEVEL) {
        AnimateFailedMove(E); /* .cantraise (GetStatName: text) */
        return;
    }
    W(wBattleAnimParam) = 1;
    AnimateCurrentMove(E);
    LowerStat(E, SPEED);
    W(wAttackMissed) = 0; /* ResetMiss, between the turn switches of StatDownMessage */
    BattleCommand_StatUp(E, ATTACK);
    W(wAttackMissed) = 0;
    BattleCommand_StatUp(E, DEFENSE);
}

/* ---- disable.asm, encore.asm, spite.asm ---- */

static void BattleCommand_Disable(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wEnemyDisableCount : wPlayerDisableCount;
    uint16_t hl = player_turn(E) ? wEnemyMonMoves : wBattleMonMoves;
    uint8_t b = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP), c;
    if(W(wAttackMissed) || M(de) || !b || b == STRUGGLE || !find_move(E, hl, b, &c)) goto failed;
    if(!M((player_turn(E) ? wEnemyMonPP : wBattleMonPP) + c)) goto failed;
    uint8_t a;
    do {
        a = BattleRandom(E, "BattleCommand_Disable") & 7;
    } while(!a);
    uint8_t slot = (uint8_t)(c + 1);
    M(de) = (uint8_t)(a + 1 + (uint8_t)(slot << 4 | slot >> 4)); /* swap c */
    AnimateCurrentMove(E);
    b = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    W(player_turn(E) ? wEnemyDisabledMove : wDisabledMove) = b;
    W(wNamedObjectIndex) = b;
    W(wCurSpecies) = b; /* GetMoveName */
    return;
failed:
    AnimateFailedMove(E);
}

/* the encored side keeps the move it is about to use this turn */
static bool EncoreForceLastMove(PkbEngine* E, bool player) {
    uint8_t b = W(player ? wLastPlayerMove : wLastEnemyMove);
    uint16_t moves = player ? wBattleMonMoves : wEnemyMonMoves;
    for(uint8_t c = 0; c < NUM_MOVES; c++) {
        if(M(moves + c) != b) continue;
        W(player ? wCurMoveNum : wCurEnemyMoveNum) = c;
        W(player ? wCurPlayerMove : wCurEnemyMove) = b;
        GetMoveData(E, (uint8_t)(b - 1), player ? wPlayerMoveStruct : wEnemyMoveStruct);
        return true;
    }
    return false;
}

static void BattleCommand_Encore(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyMonMoves : wBattleMonMoves;
    uint16_t de = player_turn(E) ? wEnemyEncoreCount : wPlayerEncoreCount;
    uint8_t b = GetBattleVar(E, BATTLE_VARS_LAST_MOVE_OPP), c;
    if(!b || b == STRUGGLE || b == ENCORE || b == MIRROR_MOVE || !find_move(E, hl, b, &c)) goto failed;
    if(!(M(hl + c + (wBattleMonPP - wBattleMonMoves)) & PP_MASK)) goto failed;
    if(W(wAttackMissed)) goto failed;
    uint16_t ss5 = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS5_OPP);
    if(M(ss5) & (1 << SUBSTATUS_ENCORED)) goto failed;
    M(ss5) |= 1 << SUBSTATUS_ENCORED;
    M(de) = (uint8_t)((BattleRandom(E, "BattleCommand_Encore") & 3) + 3);
    if(!CheckOpponentWentFirst(E) && !EncoreForceLastMove(E, !player_turn(E))) {
        M(ss5) &= (uint8_t)~(1 << SUBSTATUS_ENCORED);
        M(de) = 0;
        goto failed;
    }
    AnimateCurrentMove(E);
    return;
failed:
    AnimateFailedMove(E);
}

static void BattleCommand_Spite(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyMonMoves : wBattleMonMoves;
    uint8_t b = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP), c;
    if(W(wAttackMissed) || !b || b == STRUGGLE || !find_move(E, hl, b, &c)) goto failed;
    W(wNamedObjectIndex) = b;
    uint16_t pp = (uint16_t)(hl + c + (wBattleMonPP - wBattleMonMoves));
    if(!(M(pp) & PP_MASK)) goto failed;
    W(wCurSpecies) = b; /* GetMoveName */
    uint8_t d = (uint8_t)((BattleRandom(E, "BattleCommand_Spite") & 3) + 2);
    if((M(pp) & PP_MASK) < d) d = M(pp) & PP_MASK;
    M(pp) = (uint8_t)(M(pp) - d);
    uint8_t e = M(pp);
    uint16_t party = (uint16_t)(OpponentPartyAttr(E, MON_PP) + c);
    /* not for a transformed target (wild mons use wWildMonPP: not in link battles) */
    if(!(GetBattleVar(E, BATTLE_VARS_SUBSTATUS5_OPP) & (1 << SUBSTATUS_TRANSFORMED))) M(party) = e;
    AnimateCurrentMove(E);
    W(wTextDecimalByte) = d;
    return;
failed:
    AnimateFailedMove(E);
}

/* ---- protect.asm, endure.asm ---- */

/* returns carry: failed (the consecutive-use chance halves every time) */
static bool ProtectChance(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wPlayerProtectCount : wEnemyProtectCount;
    if(!CheckOpponentWentFirst(E) && !(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_SUBSTITUTE))) {
        uint8_t b = 0xFF, c = M(de);
        while(c && b) {
            c--;
            b >>= 1;
        }
        if(b) {
            uint8_t a;
            do {
                a = BattleRandom(E, "ProtectChance");
            } while(!a);
            if((uint8_t)(a - 1) < b) {
                M(de)++;
                return false;
            }
        }
    }
    M(de) = 0;
    AnimateFailedMove(E);
    return true;
}

static void ProtectOrEndure(PkbEngine* E, uint8_t bit) {
    if(ProtectChance(E)) return;
    set_var(E, BATTLE_VARS_SUBSTATUS1, 1 << bit);
    AnimateCurrentMove(E);
}

/* ---- flags set on one side, failing if already set ---- */

/* Foresight, Lock-On, Leech Seed, Nightmare, Spikes, Safeguard */
static void BattleCommand_Foresight(PkbEngine* E) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1_OPP);
    if(W(wAttackMissed) || CheckHiddenOpponent(E) || (M(hl) & (1 << SUBSTATUS_IDENTIFIED))) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << SUBSTATUS_IDENTIFIED;
    AnimateCurrentMove(E);
}

static void BattleCommand_LockOn(PkbEngine* E) {
    if(CheckSubstituteOpp(E) || W(wAttackMissed)) {
        AnimateFailedMove(E);
        return;
    }
    set_var(E, BATTLE_VARS_SUBSTATUS5_OPP, 1 << SUBSTATUS_LOCK_ON);
    AnimateCurrentMove(E);
}

static void BattleCommand_LeechSeed(PkbEngine* E) {
    uint16_t de = player_turn(E) ? wEnemyMonType1 : wBattleMonType1;
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS4_OPP);
    if(!W(wAttackMissed) && !CheckSubstituteOpp(E)) {
        if(M(de) == GRASS || M(de + 1) == GRASS) {
            AnimateFailedMove(E);
            return;
        }
        if(!(M(hl) & (1 << SUBSTATUS_LEECH_SEED))) {
            M(hl) |= 1 << SUBSTATUS_LEECH_SEED;
            AnimateCurrentMove(E);
            return;
        }
    }
    AnimateFailedMove(E); /* .evaded */
}

static void BattleCommand_Nightmare(PkbEngine* E) {
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_SUBSTATUS1_OPP);
    if(CheckHiddenOpponent(E) || CheckSubstituteOpp(E) || !(GetBattleVar(E, BATTLE_VARS_STATUS_OPP) & SLP_MASK) ||
       (M(hl) & (1 << SUBSTATUS_NIGHTMARE))) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << SUBSTATUS_NIGHTMARE;
    AnimateCurrentMove(E);
}

static void BattleCommand_Spikes(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyScreens : wPlayerScreens;
    if(M(hl) & (1 << SCREENS_SPIKES)) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << SCREENS_SPIKES;
    AnimateCurrentMove(E);
}

static void BattleCommand_Safeguard(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerScreens : wEnemyScreens;
    uint16_t de = player_turn(E) ? wPlayerSafeguardCount : wEnemySafeguardCount;
    if(M(hl) & (1 << SCREENS_SAFEGUARD)) {
        AnimateFailedMove(E);
        return;
    }
    M(hl) |= 1 << SCREENS_SAFEGUARD;
    M(de) = 5;
    AnimateCurrentMove(E);
}

static void BattleCommand_PerishSong(PkbEngine* E) {
    uint8_t perish = 1 << SUBSTATUS_PERISH;
    if((W(wPlayerSubStatus1) & perish) && (W(wEnemySubStatus1) & perish)) {
        AnimateFailedMove(E);
        return;
    }
    if(!(W(wPlayerSubStatus1) & perish)) {
        W(wPlayerSubStatus1) |= perish;
        W(wPlayerPerishCount) = 4;
    }
    if(!(W(wEnemySubStatus1) & perish)) {
        W(wEnemySubStatus1) |= perish;
        W(wEnemyPerishCount) = 4;
    }
    AnimateCurrentMove(E);
}

/* ---- fury_cutter.asm, rollout.asm, rage.asm, pursuit.asm ---- */

static void BattleCommand_FuryCutter(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerFuryCutterCount : wEnemyFuryCutterCount;
    if(W(wAttackMissed)) {
        ResetFuryCutterCount(E);
        return;
    }
    uint8_t b = ++M(hl);
    if(b >= 6) b = 5;
    double_damage_times(E, b);
}

static void BattleCommand_CheckRollout(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS1) & (1 << SUBSTATUS_ROLLOUT)) {
        SkipToBattleCommand(E, PB_CMD_DOTURN);
        return;
    }
    W(player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount) = 0;
}

#define MAX_ROLLOUT_COUNT 5

static void BattleCommand_RolloutPower(PkbEngine* E) {
    if(GetBattleVar(E, BATTLE_VARS_STATUS) & SLP_MASK) return;
    uint16_t hl = player_turn(E) ? wPlayerRolloutCount : wEnemyRolloutCount;
    if(!M(hl)) W(wSomeoneIsRampaging) = 1;
    if(W(wAttackMissed)) {
        res_var(E, BATTLE_VARS_SUBSTATUS1, 1 << SUBSTATUS_ROLLOUT);
        return;
    }
    uint8_t b = ++M(hl);
    if(b >= MAX_ROLLOUT_COUNT) res_var(E, BATTLE_VARS_SUBSTATUS1, 1 << SUBSTATUS_ROLLOUT);
    else set_var(E, BATTLE_VARS_SUBSTATUS1, 1 << SUBSTATUS_ROLLOUT);
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS2) & (1 << SUBSTATUS_CURLED)) b++;
    double_damage_times(E, b);
}

static void BattleCommand_Pursuit(PkbEngine* E) {
    bool theirs = player_turn(E) ? E->peer_enemy_switching : E->peer_player_switching;
    bool ours = W(player_turn(E) ? wEnemyIsSwitching : wPlayerIsSwitching) != 0;
    if(ours != theirs && rd16(E, wCurDamage)) game_desync(E, PKB_DESYNC_PURSUIT_BATON_PASS);
    if(!ours) return;
    double_damage_times(E, 2);
}

/* ---- future_sight.asm ---- */

static void BattleCommand_CheckFutureSight(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wPlayerFutureSightCount : wEnemyFutureSightCount;
    uint16_t de = player_turn(E) ? wPlayerFutureSightDamage : wEnemyFutureSightDamage;
    if(M(hl) != 1) return;
    M(hl) = 0;
    W(wCurDamage) = M(de);
    W(wCurDamage + 1) = M(de + 1);
    SkipToBattleCommand(E, PB_CMD_FUTURESIGHT);
}

static void BattleCommand_FutureSight(PkbEngine* E) {
    if(!CheckUserIsCharging(E)) {
        uint8_t b = GetBattleVar(E, BATTLE_VARS_MOVE_ANIM);
        M(GetBattleVarAddr(E, BATTLE_VARS_LAST_COUNTER_MOVE)) = b;
        M(GetBattleVarAddr(E, BATTLE_VARS_LAST_MOVE)) = b;
    }
    uint16_t hl = player_turn(E) ? wPlayerFutureSightCount : wEnemyFutureSightCount;
    if(M(hl)) {
        /* .failed pops the command's return, so DoMove returns without
         * reading the endmove bytes EndMoveEffect writes */
        ResetDamage(E);
        AnimateFailedMove(E);
        EndMoveEffect(E);
        E->leave_do_move = true;
        return;
    }
    M(hl) = 4;
    BattleCommand_LowerSub(E);
    BattleCommand_RaiseSub(E);
    uint16_t de = player_turn(E) ? wPlayerFutureSightDamage : wEnemyFutureSightDamage;
    M(de) = W(wCurDamage);
    W(wCurDamage) = 0;
    M(de + 1) = W(wCurDamage + 1);
    W(wCurDamage + 1) = 0;
    EndMoveEffect(E);
}

/* ---- heal_bell.asm, pain_split.asm, psych_up.asm ---- */

static void BattleCommand_HealBell(PkbEngine* E) {
    res_var(E, BATTLE_VARS_SUBSTATUS1, 1 << SUBSTATUS_NIGHTMARE);
    uint16_t de = player_turn(E) ? wPartyMon1Status : wOTPartyMon1Status;
    M(GetBattleVarAddr(E, BATTLE_VARS_STATUS)) = 0;
    for(uint8_t i = 0; i < PARTY_LENGTH; i++) M(GetPartyLocation(de, i)) = 0; /* all six slots */
    AnimateCurrentMove(E);
    if(player_turn(E)) CalcPlayerStats(E);
    else CalcEnemyStats(E);
}

/* .EnemyShareHP: HP at hl - 2 becomes min(wCurDamage, max HP at hl) */
static void ShareHP(PkbEngine* E, uint16_t max) {
    uint16_t avg = rd16(E, wCurDamage), bc = avg < rd16(E, max) ? avg : rd16(E, max);
    wr16(E, (uint16_t)(max - 2), bc);
    W(wHPBuffer3) = (uint8_t)bc;
    W(wHPBuffer3 + 1) = (uint8_t)(bc >> 8);
}

static void BattleCommand_PainSplit(PkbEngine* E) {
    if(W(wAttackMissed) || CheckSubstituteOpp(E)) {
        AnimateFailedMove(E);
        return;
    }
    AnimateCurrentMove(E);
    /* .PlayerShareHP: the average (17th bit lost) goes into wCurDamage */
    W(wHPBuffer1) = W(wBattleMonMaxHP + 1);
    W(wHPBuffer1 + 1) = W(wBattleMonMaxHP);
    W(wHPBuffer2) = W(wBattleMonHP + 1);
    W(wHPBuffer2 + 1) = W(wBattleMonHP);
    wr16(E, wCurDamage, (uint16_t)((uint16_t)(rd16(E, wBattleMonHP) + rd16(E, wEnemyMonHP)) >> 1));
    ShareHP(E, wBattleMonMaxHP);
    W(wWhichHPBar) = 1;
    W(wHPBuffer2 + 1) = W(wEnemyMonHP);
    W(wHPBuffer2) = W(wEnemyMonHP + 1);
    W(wHPBuffer1 + 1) = W(wEnemyMonMaxHP);
    W(wHPBuffer1) = W(wEnemyMonMaxHP + 1);
    ShareHP(E, wEnemyMonMaxHP);
    W(wWhichHPBar) = 0;
    ResetDamage(E);
}

static void BattleCommand_PsychUp(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wEnemyStatLevels : wPlayerStatLevels;
    uint16_t de = player_turn(E) ? wPlayerStatLevels : wEnemyStatLevels;
    bool changed = false;
    for(uint8_t i = 0; i < NUM_LEVEL_STATS; i++) changed |= M(hl + i) != BASE_STAT_LEVEL;
    if(!changed) {
        AnimateFailedMove(E);
        return;
    }
    for(uint8_t i = 0; i < NUM_LEVEL_STATS; i++) M(de + i) = M(hl + i);
    if(player_turn(E)) CalcPlayerStats(E);
    else CalcEnemyStats(E);
    AnimateCurrentMove(E);
}

/* ---- metronome.asm, mirror_move.asm, sleep_talk.asm, mimic.asm, sketch.asm ---- */

/* keeps wBattleAnimParam across the lowered substitute, unless charging */
static void LowerSubForCalledMove(PkbEngine* E) {
    if(CheckUserIsCharging(E)) return;
    uint8_t param = W(wBattleAnimParam);
    BattleCommand_LowerSub(E);
    W(wBattleAnimParam) = param;
}

static void LoadMoveAnim(PkbEngine* E) {
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
}

static void BattleCommand_Metronome(PkbEngine* E) {
    ClearLastMove(E);
    LowerSubForCalledMove(E);
    LoadMoveAnim(E);
    uint8_t b;
    do {
        do {
            b = BattleRandom(E, "BattleCommand_Metronome");
        } while(b >= NUM_ATTACKS + 1);
    } while(in_list(pb_metronome_excepts, b) || CheckUserMove(E, b));
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE)) = b;
    UpdateMoveData(E);
    ResetTurn(E);
}

static void BattleCommand_MirrorMove(PkbEngine* E) {
    ClearLastMove(E);
    uint16_t hl = GetBattleVarAddr(E, BATTLE_VARS_MOVE);
    uint8_t b = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    if(!b || CheckUserMove(E, b)) {
        AnimateFailedMove(E);
        EndMoveEffect(E);
        return;
    }
    M(hl) = b;
    W(wNamedObjectIndex) = b;
    GetMoveData(E, (uint8_t)(b - 1), GetBattleVarAddr(E, BATTLE_VARS_MOVE_ANIM));
    W(wCurSpecies) = b; /* GetMoveName */
    LowerSubForCalledMove(E);
    ResetTurn(E);
}

/* returns z: a two-turn move or Bide, which Sleep Talk can't call */
static bool SleepTalkTwoTurnMove(uint8_t move) {
    uint8_t e = GetMoveEffect(move);
    return e == EFFECT_SKULL_BASH || e == EFFECT_RAZOR_WIND || e == EFFECT_SKY_ATTACK || e == EFFECT_SOLARBEAM ||
           e == EFFECT_FLY || e == EFFECT_BIDE;
}

static void BattleCommand_SleepTalk(PkbEngine* E) {
    ClearLastMove(E);
    uint16_t hl = player_turn(E) ? wBattleMonMoves : wEnemyMonMoves;
    uint8_t d = player_turn(E) ? W(wDisabledMove) : W(wEnemyDisabledMove);
    if(W(wAttackMissed) || !(GetBattleVar(E, BATTLE_VARS_STATUS) & SLP_MASK) || !M(hl + 1)) goto fail;
    { /* .check_has_usable_move */
        uint8_t self = GetBattleVar(E, BATTLE_VARS_MOVE);
        bool usable = false;
        for(uint8_t i = 0; i < NUM_MOVES && M(hl + i); i++) {
            uint8_t a = M(hl + i);
            if(a != self && a != d && !SleepTalkTwoTurnMove(a)) {
                usable = true;
                break;
            }
        }
        if(!usable) goto fail;
    }
    uint8_t e;
    for(;;) {
        e = M(hl + (BattleRandom(E, "BattleCommand_SleepTalk") & 3));
        if(!e || e == GetBattleVar(E, BATTLE_VARS_MOVE_ANIM) || e == d || SleepTalkTwoTurnMove(e)) continue;
        break;
    }
    M(GetBattleVarAddr(E, BATTLE_VARS_MOVE)) = e;
    LowerSubForCalledMove(E);
    LoadMoveAnim(E);
    UpdateMoveData(E);
    ResetTurn(E);
    return;
fail:
    AnimateFailedMove(E);
}

static void BattleCommand_Mimic(PkbEngine* E) {
    ClearLastMove(E);
    uint16_t hl = player_turn(E) ? wBattleMonMoves : wEnemyMonMoves;
    uint8_t b = GetBattleVar(E, BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    if(W(wAttackMissed) || CheckHiddenOpponent(E) || !b || b == STRUGGLE || CheckUserMove(E, b)) return; /* FailMimic */
    /* the last Mimic among the moves; the search runs on backwards before
     * them (a Mimic called by another move) */
    uint16_t p = (uint16_t)(hl + NUM_MOVES - 1);
    while(M(p) != MIMIC) {
        if(p == PB_WRAM_START) {
            unsupported(E, GetBattleVar(E, BATTLE_VARS_MOVE_EFFECT), 0xFF);
            return;
        }
        p--;
    }
    M(p) = b;
    W(wNamedObjectIndex) = b;
    M(p + (wBattleMonPP - wBattleMonMoves)) = 5;
    W(wCurSpecies) = b; /* GetMoveName */
    AnimateCurrentMove(E);
}

/* Sketch does nothing in link battles */
static void BattleCommand_Sketch(PkbEngine* E) {
    ClearLastMove(E);
    AnimateFailedMove(E);
}

/* ---- present.asm ---- */

/* (effect_commands.asm) PlayDamageAnim plays the move's own animation, and
 * BattleAnim_Present's anim_jumpuntil loop counts wBattleAnimParam down to 0
 * (the heal branch, param 3, skips it). With animations off it does not run. */
static void AnimateCurrentMoveEitherSide(PkbEngine* E) {
    uint8_t param = W(wBattleAnimParam);
    BattleCommand_LowerSub(E);
    W(wBattleAnimParam) = param;
    W(wFXAnimID + 1) = 0;
    if(GetBattleVar(E, BATTLE_VARS_MOVE_ANIM) == PRESENT && !CheckBattleScene(E) && param != 3) {
        W(wBattleAnimParam) = 0;
    }
    BattleCommand_RaiseSub(E);
}

static void BattleCommand_Present(PkbEngine* E) {
    /* In the Colosseum the registers are not saved around Stab (game bug), so
     * damagecalc gets b = the type matchup, c = the user's second type (or
     * the damage so far halved, 0, if Stab applied), d = the power and e =
     * the target's second type. */
    uint16_t user = player_turn(E) ? wBattleMonType1 : wEnemyMonType1;
    uint16_t target = player_turn(E) ? wEnemyMonType1 : wBattleMonType1;
    uint8_t type = GetBattleVar(E, BATTLE_VARS_MOVE_TYPE);
    uint8_t c = (type == M(user) || type == M(user + 1)) ? (uint8_t)(rd16(E, wCurDamage) >> 1) : M(user + 1);
    BattleCommand_Stab(E);
    reg_b = W(wTypeMatchup);
    reg_c = c;
    reg_d = M(target);
    reg_e = M(target + 1);
    if(!W(wTypeMatchup) || W(wAttackMissed)) {
        AnimateFailedMove(E);
        return;
    }
    uint8_t r = BattleRandom(E, "BattleCommand_Present");
    uint8_t idx = 0;
    for(const uint8_t* p = pb_present_power; p[0] != 0xFF; p += 2, idx++) {
        if(p[0] >= r) {
            W(wBattleAnimParam) = idx;
            AnimateCurrentMoveEitherSide(E);
            reg_d = p[1];
            return;
        }
    }
    /* .heal_effect: a quarter of the target's max HP */
    W(wBattleAnimParam) = 3;
    AnimateCurrentMoveEitherSide(E);
    BattleCommand_SwitchTurn(E);
    uint16_t hp = player_turn(E) ? wBattleMonHP : wEnemyMonHP; /* AICheckPlayerMaxHP/AICheckEnemyMaxHP */
    if(rd16(E, hp) != rd16(E, (uint16_t)(hp + 2))) {
        uint16_t bc = GetQuarterMaxHP(E);
        BattleCommand_SwitchTurn(E);
        RestoreHP(E, bc);
        BattleCommand_SwitchTurn(E);
        BattleCommand_SwitchTurn(E);
        UpdateOpponentInParty(E);
    } else {
        BattleCommand_SwitchTurn(E);
        if(CheckBattleScene(E)) AnimateFailedMove(E); /* only with animations off */
    }
    EndMoveEffect(E);
}

/* ---- rapid_spin.asm, weather ---- */

static void BattleCommand_ClearHazards(PkbEngine* E) {
    res_var(E, BATTLE_VARS_SUBSTATUS4, 1 << SUBSTATUS_LEECH_SEED);
    uint16_t hl = player_turn(E) ? wPlayerScreens : wEnemyScreens;
    uint16_t de = player_turn(E) ? wPlayerWrapCount : wEnemyWrapCount;
    M(hl) &= (uint8_t)~(1 << SCREENS_SPIKES);
    M(de) = 0;
}

static void StartWeather(PkbEngine* E, uint8_t weather) {
    W(wBattleWeather) = weather;
    W(wWeatherCount) = 5;
    AnimateCurrentMove(E);
}

static void BattleCommand_StartSandstorm(PkbEngine* E) {
    if(W(wBattleWeather) == WEATHER_SANDSTORM) {
        AnimateFailedMove(E);
        return;
    }
    StartWeather(E, WEATHER_SANDSTORM);
}

/* ---- substitute.asm ---- */

static void BattleCommand_Substitute(PkbEngine* E) {
    uint16_t hl = player_turn(E) ? wBattleMonMaxHP : wEnemyMonMaxHP;
    uint16_t de = player_turn(E) ? wPlayerSubstituteHP : wEnemySubstituteHP;
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS4) & (1 << SUBSTATUS_SUBSTITUTE)) goto fail; /* .already_has_sub */
    {
        uint8_t b = (uint8_t)(rd16(E, hl) >> 2); /* the substitute keeps the low byte only */
        M(de) = b;
        uint16_t hp = rd16(E, (uint16_t)(hl - 2));
        if(hp <= b) goto fail; /* .too_weak_to_sub */
        wr16(E, (uint16_t)(hl - 2), (uint16_t)(hp - b));
    }
    set_var(E, BATTLE_VARS_SUBSTATUS4, 1 << SUBSTATUS_SUBSTITUTE);
    W(player_turn(E) ? wPlayerWrapCount : wEnemyWrapCount) = 0;
    W(player_turn(E) ? wPlayerTrappingMove : wEnemyTrappingMove) = 0;
    if(!CheckBattleScene(E)) {
        W(wBattleAfterAnim) = 0;
        W(wFXAnimID + 1) = 0;
        W(wBattleAnimParam) = 0;
        W(wFXAnimID) = SUBSTITUTE; /* LoadAnim */
    }
    return;
fail:
    if(CheckUserIsCharging(E)) BattleCommand_RaiseSub(E);
}

/* ---- thief.asm ---- */

static bool ItemIsMail(uint8_t item) {
    return in_list(pb_mail_items, item);
}

static void BattleCommand_Thief(PkbEngine* E) {
    bool player = player_turn(E);
    uint16_t mine = player ? wBattleMonItem : wEnemyMonItem;
    uint16_t theirs = player ? wEnemyMonItem : wBattleMonItem;
    uint16_t mine_party = player ? BattlePartyAttr(E, MON_ITEM) : OTPartyAttr(E, MON_ITEM);
    uint16_t theirs_party = player ? OTPartyAttr(E, MON_ITEM) : BattlePartyAttr(E, MON_ITEM);
    if(M(mine) || !M(theirs)) return;
    W(wNamedObjectIndex) = M(theirs);
    if(ItemIsMail(M(theirs)) || W(wEffectFailed)) return;
    if(player && W(wLinkMode) && W(wBattleMode) == WILD_BATTLE) return;
    M(theirs) = 0;
    M(theirs_party) = 0;
    M(mine) = W(wNamedObjectIndex);
    M(mine_party) = W(wNamedObjectIndex);
}

/* ---- transform.asm ---- */

static void BattleCommand_Transform(PkbEngine* E) {
    ClearLastMove(E);
    if((GetBattleVar(E, BATTLE_VARS_SUBSTATUS5_OPP) & (1 << SUBSTATUS_TRANSFORMED)) || CheckHiddenOpponent(E)) {
        AnimateFailedMove(E); /* BattleEffect_ButItFailed */
        return;
    }
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
    W(wBattleAnimParam) = 1;
    set_var(E, BATTLE_VARS_SUBSTATUS5, 1 << SUBSTATUS_TRANSFORMED);
    ResetActorDisable(E);
    uint16_t src, dst;
    if(player_turn(E)) {
        src = wEnemyMonSpecies;
        dst = wBattleMonSpecies;
        W(wCurMoveNum) = 0;
    } else {
        src = wBattleMonSpecies;
        dst = wEnemyMonSpecies;
        W(wEnemyBackupDVs) = W(wEnemyMonDVs);
        W(wEnemyBackupDVs + 1) = W(wEnemyMonDVs + 1);
    }
    /* species, moves, DVs, then stats and types; the PP become 5 (1 for
     * Sketch, 0 for no move) */
    M(dst) = M(src);
    for(uint16_t i = wBattleMonMoves - wBattleMon; i < wBattleMonPP - wBattleMon; i++) M(dst + i) = M(src + i);
    for(uint16_t i = wBattleMonStats - wBattleMon; i < wBattleMonStructEnd - wBattleMon; i++) M(dst + i) = M(src + i);
    for(uint8_t i = 0; i < NUM_MOVES; i++) {
        uint8_t move = M(src + (wBattleMonMoves - wBattleMon) + i);
        M(dst + (wBattleMonPP - wBattleMon) + i) = !move ? 0 : move == SKETCH ? 1 : 5;
    }
    W(wNamedObjectIndex) = M(src);
    /* BattleSideCopy: the user's unboosted stats and stat levels */
    uint16_t from = player_turn(E) ? wEnemyStats : wPlayerStats;
    uint16_t to = player_turn(E) ? wPlayerStats : wEnemyStats;
    for(uint8_t i = 0; i < 2 * 5; i++) M(to + i) = M(from + i);
    from = player_turn(E) ? wEnemyStatLevels : wPlayerStatLevels;
    to = player_turn(E) ? wPlayerStatLevels : wEnemyStatLevels;
    for(uint8_t i = 0; i < 8; i++) M(to + i) = M(from + i);
    W(wBattleAfterAnim) = 0;
    W(wFXAnimID + 1) = 0;
    W(wBattleAnimParam) = 2;
}

/* ---- beat_up.asm ---- */

/* GetBeatupMonLocation: field `attr` of the user's party mon wCurBeatUpPartyMon */
static uint16_t GetBeatupMonLocation(PkbEngine* E, uint8_t attr) {
    uint16_t base = player_turn(E) ? wPartyMon1Species : wOTPartyMon1Species;
    return GetPartyLocation((uint16_t)(base + attr), W(wCurBeatUpPartyMon));
}

/* Link-battle paths only (no wild or single-player branches). Damage stats go
 * to b, c, d, e for damagecalc, as damagestats does. */
static void BattleCommand_BeatUp(PkbEngine* E) {
    ResetDamage(E);
    bool player = player_turn(E);
    uint16_t count = player ? wPlayerRolloutCount : wEnemyRolloutCount;
    if(GetBattleVar(E, BATTLE_VARS_SUBSTATUS3) & (1 << SUBSTATUS_IN_LOOP)) {
        /* .next_mon */
        W(wCurBeatUpPartyMon) = (uint8_t)(W(player ? wPartyCount : wOTPartyCount) - M(count));
        /* A Beat Up that missed after a failed first mon ends in failuretext
         * with IN_LOOP still set; the next one starts here with the count at
         * 0 and runs 255 times past the end of the party. The Game Boys then
         * read different memory (their own party vs the OT copy and what
         * follows each). */
        if(W(wCurBeatUpPartyMon) >= W(player ? wPartyCount : wOTPartyCount)) {
            game_desync(E, PKB_DESYNC_BEAT_UP_OVERRUN);
        }
    } else {
        M(count) = 0;
        W(wCurBeatUpPartyMon) = 0;
        W(wBeatUpHitAtLeastOnce) = 0;
    }
    /* GetNickname / the OT nickname copy (for the text) */
    uint16_t names = player ? wPartyMonNicknames : wOTPartyMonNicknames;
    for(uint8_t i = 0; i < MON_NAME_LENGTH; i++) {
        W(wStringBuffer1 + i) = M(names + MON_NAME_LENGTH * W(wCurBeatUpPartyMon) + i);
    }
    uint16_t hp = GetBeatupMonLocation(E, MON_HP);
    if(!(M(hp) | M(hp + 1))) goto fail;

    /* The active mon's status is the battle mon's, the others' their party
     * struct's. BUG: the player branch compares wCurBattleMon with the low HP
     * byte (still in hl) instead of the party index; the enemy branch gets it
     * right. The other Game Boy runs the other branch for this same hit, so
     * when the two disagree on whether the mon is statused the battles part. */
    uint8_t active = player ? W(wCurBattleMon) : W(wCurOTMon);
    uint16_t battle_status = player ? wBattleMonStatus : wEnemyMonStatus;
    uint16_t party_status = GetBeatupMonLocation(E, MON_STATUS);
    uint8_t buggy = M(active == M(hp + 1) ? battle_status : party_status);
    uint8_t right = M(active == W(wCurBeatUpPartyMon) ? battle_status : party_status);
    if(!buggy != !right) game_desync(E, PKB_DESYNC_BEAT_UP);
    if(player ? buggy : right) goto fail;

    W(wBeatUpHitAtLeastOnce) = 1;
    uint8_t target = player ? W(wEnemyMonSpecies) : W(wBattleMonSpecies);
    W(wCurSpecies) = target;
    GetBaseData(E, target);
    reg_c = W(wBaseDefense);
    uint8_t species = M(GetBeatupMonLocation(E, MON_SPECIES));
    W(wCurSpecies) = species;
    GetBaseData(E, species);
    reg_b = W(wBaseAttack);
    reg_e = M(GetBeatupMonLocation(E, MON_LEVEL));
    reg_d = W(player ? wPlayerMoveStructPower : wEnemyMoveStructPower);
    return;
fail:
    /* .beatup_fail */
    SkipToBattleCommand(E, PB_CMD_BUILDOPPONENTRAGE);
}

/* ---- dispatch ---- */

bool MoveEffectCommandSupported(uint8_t cmd) {
    switch(cmd) {
    case PB_CMD_ATTRACT:
    case PB_CMD_BATONPASS:
    case PB_CMD_BELLYDRUM:
    case PB_CMD_STOREENERGY:
    case PB_CMD_UNLEASHENERGY:
    case PB_CMD_CONVERSION:
    case PB_CMD_CONVERSION2:
    case PB_CMD_COUNTER:
    case PB_CMD_MIRRORCOAT:
    case PB_CMD_CURSE:
    case PB_CMD_DESTINYBOND:
    case PB_CMD_DISABLE:
    case PB_CMD_ENCORE:
    case PB_CMD_SPITE:
    case PB_CMD_PROTECT:
    case PB_CMD_ENDURE:
    case PB_CMD_FORESIGHT:
    case PB_CMD_LOCKON:
    case PB_CMD_LEECHSEED:
    case PB_CMD_NIGHTMARE:
    case PB_CMD_SPIKES:
    case PB_CMD_SAFEGUARD:
    case PB_CMD_PERISHSONG:
    case PB_CMD_FURYCUTTER:
    case PB_CMD_CHECKROLLOUT:
    case PB_CMD_ROLLOUTPOWER:
    case PB_CMD_RAGE:
    case PB_CMD_PURSUIT:
    case PB_CMD_CHECKFUTURESIGHT:
    case PB_CMD_FUTURESIGHT:
    case PB_CMD_HEALBELL:
    case PB_CMD_PAINSPLIT:
    case PB_CMD_PSYCHUP:
    case PB_CMD_METRONOME:
    case PB_CMD_MIRRORMOVE:
    case PB_CMD_SLEEPTALK:
    case PB_CMD_MIMIC:
    case PB_CMD_SKETCH:
    case PB_CMD_PRESENT:
    case PB_CMD_CLEARHAZARDS:
    case PB_CMD_STARTRAIN:
    case PB_CMD_STARTSUN:
    case PB_CMD_STARTSANDSTORM:
    case PB_CMD_SUBSTITUTE:
    case PB_CMD_TELEPORT:
    case PB_CMD_THIEF:
    case PB_CMD_TRANSFORM:
    case PB_CMD_BEATUP:
    case PB_CMD_BEATUPFAILTEXT:
        return true;
    default:
        return false;
    }
}

PKB_TEXT2 bool RunMoveEffectCommand(PkbEngine* E, uint8_t cmd) {
    switch(cmd) {
    case PB_CMD_ATTRACT:
        BattleCommand_Attract(E);
        break;
    case PB_CMD_BATONPASS:
        BattleCommand_BatonPass(E);
        break;
    case PB_CMD_BELLYDRUM:
        BattleCommand_BellyDrum(E);
        break;
    case PB_CMD_STOREENERGY:
        BattleCommand_StoreEnergy(E);
        break;
    case PB_CMD_UNLEASHENERGY:
        BattleCommand_UnleashEnergy(E);
        break;
    case PB_CMD_CONVERSION:
        BattleCommand_Conversion(E);
        break;
    case PB_CMD_CONVERSION2:
        BattleCommand_Conversion2(E);
        break;
    case PB_CMD_COUNTER:
        CounterCommon(E, EFFECT_COUNTER, false);
        break;
    case PB_CMD_MIRRORCOAT:
        CounterCommon(E, EFFECT_MIRROR_COAT, true);
        break;
    case PB_CMD_CURSE:
        BattleCommand_Curse(E);
        break;
    case PB_CMD_DESTINYBOND:
        set_var(E, BATTLE_VARS_SUBSTATUS5, 1 << SUBSTATUS_DESTINY_BOND);
        AnimateCurrentMove(E);
        break;
    case PB_CMD_DISABLE:
        BattleCommand_Disable(E);
        break;
    case PB_CMD_ENCORE:
        BattleCommand_Encore(E);
        break;
    case PB_CMD_SPITE:
        BattleCommand_Spite(E);
        break;
    case PB_CMD_PROTECT:
        ProtectOrEndure(E, SUBSTATUS_PROTECT);
        break;
    case PB_CMD_ENDURE:
        ProtectOrEndure(E, SUBSTATUS_ENDURE);
        break;
    case PB_CMD_FORESIGHT:
        BattleCommand_Foresight(E);
        break;
    case PB_CMD_LOCKON:
        BattleCommand_LockOn(E);
        break;
    case PB_CMD_LEECHSEED:
        BattleCommand_LeechSeed(E);
        break;
    case PB_CMD_NIGHTMARE:
        BattleCommand_Nightmare(E);
        break;
    case PB_CMD_SPIKES:
        BattleCommand_Spikes(E);
        break;
    case PB_CMD_SAFEGUARD:
        BattleCommand_Safeguard(E);
        break;
    case PB_CMD_PERISHSONG:
        BattleCommand_PerishSong(E);
        break;
    case PB_CMD_FURYCUTTER:
        BattleCommand_FuryCutter(E);
        break;
    case PB_CMD_CHECKROLLOUT:
        BattleCommand_CheckRollout(E);
        break;
    case PB_CMD_ROLLOUTPOWER:
        BattleCommand_RolloutPower(E);
        break;
    case PB_CMD_RAGE:
        set_var(E, BATTLE_VARS_SUBSTATUS4, 1 << SUBSTATUS_RAGE);
        break;
    case PB_CMD_PURSUIT:
        BattleCommand_Pursuit(E);
        break;
    case PB_CMD_CHECKFUTURESIGHT:
        BattleCommand_CheckFutureSight(E);
        break;
    case PB_CMD_FUTURESIGHT:
        BattleCommand_FutureSight(E);
        break;
    case PB_CMD_HEALBELL:
        BattleCommand_HealBell(E);
        break;
    case PB_CMD_PAINSPLIT:
        BattleCommand_PainSplit(E);
        break;
    case PB_CMD_PSYCHUP:
        BattleCommand_PsychUp(E);
        break;
    case PB_CMD_METRONOME:
        BattleCommand_Metronome(E);
        break;
    case PB_CMD_MIRRORMOVE:
        BattleCommand_MirrorMove(E);
        break;
    case PB_CMD_SLEEPTALK:
        BattleCommand_SleepTalk(E);
        break;
    case PB_CMD_MIMIC:
        BattleCommand_Mimic(E);
        break;
    case PB_CMD_SKETCH:
        BattleCommand_Sketch(E);
        break;
    case PB_CMD_PRESENT:
        BattleCommand_Present(E);
        break;
    case PB_CMD_CLEARHAZARDS:
        BattleCommand_ClearHazards(E);
        break;
    case PB_CMD_STARTRAIN:
        StartWeather(E, WEATHER_RAIN);
        break;
    case PB_CMD_STARTSUN:
        StartWeather(E, WEATHER_SUN);
        break;
    case PB_CMD_STARTSANDSTORM:
        BattleCommand_StartSandstorm(E);
        break;
    case PB_CMD_SUBSTITUTE:
        BattleCommand_Substitute(E);
        break;
    case PB_CMD_TELEPORT: /* fails in trainer battles */
        AnimateFailedMove(E);
        break;
    case PB_CMD_THIEF:
        BattleCommand_Thief(E);
        break;
    case PB_CMD_TRANSFORM:
        BattleCommand_Transform(E);
        break;
    case PB_CMD_BEATUP:
        BattleCommand_BeatUp(E);
        break;
    case PB_CMD_BEATUPFAILTEXT: /* text only ("But it failed!") */
        break;
    default:
        return false;
    }
    return true;
}
