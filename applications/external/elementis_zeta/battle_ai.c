// The trainer AI and the Flute (contract 82; the per-class item use, the move-choice modifiers and the flute item).
// NOT a translation unit: battle_run.c #includes it (the plugin's unity build and the host harness both compile battle_run.c), so the static helpers of the engine are in scope. Everything is DETERMINISTIC: the only random draw is the
// battle's own (rnd8), taken at the same point of the turn on every machine, and nothing here reads anything the player could not affect through the battle's own state.
//
// Two layers:
//  * the MOVE CHOICE modifiers (trainers.bin byte 0, bits 0 .. 2; assets/data/ai.toml `mods`): 1 (in enemy_choose: no second status on a creature that has one), 2 and 3 below;
//  * the ROUTINE (bits 3 .. 7 of that byte = a row of files/ai.bin): before the trainer's creature moves, with a limited number of uses per creature, it may use an item on the creature or switch it. It REPLACES the creature's move.

#if BF_AI_MOD2 || BF_AI_MOD3
// The scores of the foe's moves (10 = plain, lowest wins, 255 = unusable; enemy_choose has done modifier 1): -1 for a move modifiers 2 / 3 encourage, +1 for one modifier 3 discourages.
// Modifier 3: how effective the move's type is on the player's creature in play = the FIRST chart row that names that type and one of its two types, 16 when none does
// (the answer starts at $10, not to EFFECTIVE (10): "neutral" is therefore above 10 and below 20, which is what the comparisons rely on). A not very effective move is discouraged when the foe has "a better move": a special-damage
// or Fly move (MF_SPEC), or a damaging move of ANOTHER type. Modifier 2 (only on the creature's second turn) encourages the moves with MF_AI2 (tools/regc.py: the stat and set-up moves, screens, Recover ...).
static __attribute__((noinline)) void ai_mods(Battle* b, uint8_t* score) {
    const BcData* d = b->data;
    const uint8_t t1 = ty_of(b, 0, 0), t2 = ty_of(b, 0, 1);
    for(int k = 0; k < 4; k++) {
        if(score[k] == 255) continue;
        const MoveDef* m = bc_move_def(b, bc_mv(b, 1, k));
        const uint8_t type = m->type, fl = m->flags;
#if BF_AI_MOD2
        if((b->ai_mod & 2) && (b->ai_st & 0x30) == 0x10 && (fl & MF_AI2)) score[k]--;
#endif
#if BF_AI_MOD3
        if(!(b->ai_mod & 4)) continue;
        uint8_t i = 0,
                e = chart_next(d, &i, type, t1, t2); // (the FIRST row; 0 = none: neutral 16)
        e = e ? e - 1 : 16;
        if(e > 16)
            score[k]--;
        else if(e < 16)
            for(int j = 0; j < 4; j++) { // a better move?
                const uint8_t id = bc_mv(b, 1, j);
                if(!id) break;
                const MoveDef* o = bc_move_def(b, id);
                if((o->flags & MF_SPEC) || (o->type != type && o->power)) {
                    score[k]++;
                    break;
                }
            }
#endif
    }
}
#endif

#if BF_AI_ITEMS || BF_AI_SWITCH
// The switch: with two or more creatures able to fight, the trainer withdraws the one in play for the first OTHER one that can (party order). The new creature does not attack this turn.
static bool ai_switch(Battle* b) {
    const int nx = any_alive_other(
        &b->enemy,
        b->side[1]
            .active); // (the creature in play can fight, so "two that can" = another one that can)
    if(nx < 0) return false;
    ev_msg(b, 1, MSG_AIOUT, 0);
    do_sendout(b, 1, (uint8_t)nx);
    b->ev[b->n_ev - 1].b =
        1; // (the send-out is the trainer's own choice: the scene does not offer the free swap of battle style Shift)
    return true;
}

// The trainer AI, called when the foe's creature is about to act. Returns true when the AI acted (an item or a switch: the creature does not move this turn).
static __attribute__((noinline)) bool ai_turn(Battle* b) {
    const uint8_t cls = b->ai_mod >> 3;
    uint8_t* const r =
        b->ai_row; // contract 85: the class's row is read from the card ONCE per battle (the first turn that gets here) and kept; the item of its first rule is never 0, so a zero there = not read yet
    if(!cls || b->no_reward ||
       (!r[3] && !b->data->io.read(b->data->io.ctx, "ai.bin", 12u + cls * 8u, r, 7)))
        return false; // (no routine, a wild or an exhibition battle: link battles had no AI either)
    if(!(b->ai_st & 15))
        b->ai_st =
            (uint8_t)((b->ai_st & 0xF0) |
                      (r[0] +
                       1)); // the uses for this creature are read from the class the first time (-> the table's count)
    if((b->ai_st & 15) < 2) return false; // none left: no random draw either
    const uint8_t a = rnd8(b);
    const Mon* m = act_mon(b, 1);
    for(int i = 0; i < 2; i++) {
        const uint8_t* q =
            r + 1 +
            3 * i; // {random byte below [0 = no draw condition], condition [255 status, N: HP < max / N], item}
        if(!q[2] || (q[2] != AI_SWITCH && q[2] >= b->data->n_items))
            break; // (no rule / a damaged row: the item id indexes the table)
        if((q[0] && a >= q[0]) || (q[1] == 255 ? !m->status : q[1] && m->hp >= m->max_hp / q[1]))
            continue;
        if(q[2] == AI_SWITCH) return BF_AI_SWITCH && ai_switch(b);
#if BF_AI_ITEMS
        b->ai_st--; // one use (a switch costs none: the next creature starts fresh)
        ev_msg(b, 1, MSG_AIUSE, q[2]);
        item_on(b, 1, q[2], b->side[1].active);
        return true;
#else
        return false;
#endif
    }
    return false;
}
#endif

#if BF_FLUTE
// The flute item in battle: every sleeping creature of the player's party and of the enemy's (a trainer's whole party; a wild creature is its whole party) wakes. "played the Flute", an EV_CURED for each creature in play that woke
// (the scene clears its tag and says it), then "everybody asleep woke up"; or, when nobody was asleep, "no effect".
static __attribute__((noinline)) void flute(Battle* b) {
    bool woke = false;
    ev_msg(b, 0, MSG_FLUTE, 0);
    for(int s = 0; s < 2; s++)
        for(int i = 0; i < b->party[s]->count; i++) {
            Mon* m = &b->party[s]->mon[i];
            if(!(m->status & BS_SLP_MASK)) continue;
            m->status = (uint8_t)(m->status & ~BS_SLP_MASK);
            woke = true;
            if(i == b->side[s].active) ev3(b, EV_CURED, (uint8_t)s, SK_SLEEP);
        }
    ev_msg(b, 0, woke ? MSG_WOKE : MSG_FLUTE0, 0);
}
#endif
