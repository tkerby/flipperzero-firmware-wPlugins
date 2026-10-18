// Day care (C3, contract section 33, daycare.h): the RESIDENT part. ONE creature lives in PartyState.dc (saved with the rest). While it is there only its `exp` moves:
// daycare_on_step() adds 1 per hero step (a compare and an increment: no SD, no heap; contract 65: and one more increment, the walking friendship of the lead creature). Its level, stats and moves are SETTLED LAZILY, when it is withdrawn, because the
// species record (growth rate, level-up moves) is on the card: that work (and the deposit, and the lady's level / price counts) is daycare_pc.c, in the PC plugin (the party <->
// box moves it reuses are there too), reached through menu_open_daycare(): the whole FAP is in RAM and ~0.7 KB of code that the player needs once per visit to the lady is not
// worth being resident.
#include "daycare.h"
#include "party.h"
#include "pc_host.h"

#define DC_EXP_MAX 1250000u // 100^3 * 5/4: the exp of level 100 of the slowest growth; the species' own cap is applied when settling

void daycare_on_step(void) {
    PartyState* ps = party_state();
    Mon* m = &ps->dc.mon;
    if(m->species && m->exp < DC_EXP_MAX) m->exp++;
    m = &ps->party.mon[0]; // contract 65: walking is the maintainer's friendship: 256 steps = +1 (Mon.fr counts 1/256 steps; the lead is party slot 0, a fainted one earns nothing, an empty slot has hp 0)
    if(m->hp && m->fr != BC_FR_STEP_MAX) m->fr++;
}
