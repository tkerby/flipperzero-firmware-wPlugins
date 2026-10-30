#pragma once

/*
 * Mirror comparison of two sides of one link battle. Each Game Boy keeps
 * its own mon as "player" and the other one as "enemy", so at the start of
 * a turn our wBattleMon must equal their wEnemyMon, our wPlayerSubStatus*
 * their wEnemySubStatus*, our party their OT party, and so on, while the
 * shared random numbers and the weather are equal as they are.
 */

#include <stddef.h>
#include <stdint.h>

/* wram: 8 KiB images of $C000-$DFFF (WRAMX bank 1 at $D000). Returns the
 * number of differing bytes; writes up to `max_lines` of "name: ours/theirs"
 * to report (may be NULL). */
unsigned mirror_compare(
    const uint8_t* ours,
    const uint8_t* theirs,
    char* report,
    size_t report_size,
    unsigned max_lines);
