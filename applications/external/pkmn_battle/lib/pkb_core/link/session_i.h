/* Internal interface between session.c, xb.c and session_battle.c. */
#pragma once

#include <pkb/session.h>

/* WaitForLinkedFriend: the master probes with $01 and wants $02 back. */
#define ESTABLISH_INTERNAL_PROBE 0x01 /* USING_EXTERNAL_CLOCK, sent by the master */
#define ESTABLISH_EXTERNAL 0x02 /* USING_INTERNAL_CLOCK, our reply */
/* SERIAL_TIMECAPSULE | 1: CheckLinkTimeout_Receptionist (wLinkMode = LINK_NULL) */
#define RECEPT_CHECK 0x61
/* $D0 | CABLECLUBROOM_COLOSSEUM: Link_EnsureSync from CheckBothSelectedSameRoom */
#define SYNC_COLOSSEUM 0xD2
/* SERIAL_BATTLE | 5 / | 6: CheckLinkTimeout_Gen2 (wLinkMode = LINK_COLOSSEUM) */
#define CONSOLE_CHECK_1 0x85
#define CONSOLE_CHECK_2 0x86
#define SERIAL_BATTLE_NIBBLE 0x80

void pbl_notify(PblSession* s, uint32_t events);
void pbl_set_phase(PblSession* s, PblPhase phase);
uint8_t pbl_nibble_on_byte(PblSession* s, uint8_t rx);
uint8_t pbl_enter_room_with(PblSession* s, uint8_t rx);

uint8_t pbl_xb_on_byte(PblSession* s, uint8_t rx);

void pbl_battle_reset(PblSession* s);
void pbl_battle_begin(PblSession* s);
uint8_t pbl_battle_on_byte(PblSession* s, uint8_t rx, uint32_t gap_ms);
