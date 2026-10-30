# Crystal Cable Club Colosseum link protocol (slave side)

What the Flipper must do as the second Game Boy, derived from pret/pokecrystal at the revision in
`tools/gen/pokecrystal.lock`. File/label references are to that tree. The implementation is
`lib/pkb_core/link/` and the master model used by the tests is `host/sim/master_sim.c`.

The Game Boy is always the **clock master** (`hSerialConnectionStatus = USING_INTERNAL_CLOCK`);
the Flipper is the external-clock slave and can only answer. Every transfer swaps one byte each
way.

## Shift-register lag

Both sides transmit what is in their shift register, which their `Serial` interrupt reloads from
`hSerialSend` *after* each transfer and then resets `hSerialSend` to `$FE`. So a Game Boy
transmits the value its code "sent" on the previous transfer. The Flipper (gblink callback) can
answer with a lag of one transfer, which is faster than a real slave GB; the preambles and padding
in the blocks make that harmless.

## Sequence

| Step | Master routine | Master transmits | Flipper answers |
| --- | --- | --- | --- |
| Handshake | `WaitForLinkedFriend` | `$01` once per frame (internal clock) | `$02` → master becomes `USING_INTERNAL_CLOCK`; then 2 × `$00` (`LinkDataReceived`) |
| Save prompt | `TryQuickSave` | nothing | — |
| Receptionist | `CheckLinkTimeout_Receptionist` → `WaitLinkTransfer` | `$61` (`SERIAL_TIMECAPSULE | 1`, because `wLinkMode = LINK_NULL`) | `$61`; `wOtherPlayerLinkMode` must be non-zero |
| Room sync | `CheckBothSelectedSameRoom` → `Link_EnsureSync` | stale byte, `$D2`, `$D2` (`Serial_ExchangeSyncBytes`) | `$D2` (pre-loaded as the idle byte; one of the two kept bytes must be `$Dx` with x = 2) |
| Walk in | — | nothing | — |
| Console 1 | `CheckLinkTimeout_Gen2` | `$85` | `$85` (`wOtherPlayerLinkMode` must be 5) |
| Console 2 | same, after a ~0.1 s busy loop | `$86` | `$86` (must be 6) |
| Strays | `Gen2ToGen2LinkComms` | 2 × `$00` | ignored |
| RN block | `Serial_ExchangeBytes`, 17 bytes | 7 × `$FD` + 10 RNs (< `$FD`) | same shape; our RNs are ignored |
| Party block | `Serial_ExchangeBytes`, 450 bytes | `wLinkSendParty` | ours |
| Patch block | `Serial_ExchangeBytes`, 200 bytes | `wPlayerPatchLists` | ours |
| Battle | `_LinkBattleSendReceiveAction` per exchange | `$80 | action` | `$80 | action` |
| Exit | `ExitLinkCommunications` | one `$00` | — |

Back in the Colosseum the player can use the console again (console 1 onwards, fresh RNs, and the
Flipper sends a fresh team) or leave (`WaitForOtherPlayerToExit`; the next visit starts again at
the handshake).

## Nibble exchanges (`WaitLinkTransfer`, `_LinkBattleSendReceiveAction`)

Master, once per frame: `LinkTransfer` reads `hSerialReceive` (accepting it if its high nibble
matches the mode: `$60` before entering a room, `$80` in the Colosseum), then sends its own value.
After it has accepted ours it does **10 more** `LinkTransfer`s (still re-reading and overwriting
our value) and then **10** `LinkDataReceived` (`$00`). It never clears a non-matching
`hSerialReceive`, so whatever we send last is the first thing its next exchange reads.
`_LinkBattleSendReceiveAction` has no timeout; the pre-battle checks time out after 768 or 306
frames.

Slave rules (implemented in `session.c` / `session_battle.c`):

1. Send the idle byte (`$00`, or `$D2` in the room) until our value is ready.
2. After seeing the master's value, send ours `PBL_REPEAT_AFTER_SEEN` (5) more times, then go back
   to idle, so no stale copy is left for the master's next exchange.
3. Accept a new master value only after `PBL_ARM_QUIET` (3) consecutive non-matching bytes. The 10
   acks always provide that; a single odd byte inside its `$8x` stream must not look like a new
   exchange.
4. **Do not start sending the next battle action while the master is still sending its previous
   `$8x`**: it is then in its 10-frame `.receive` phase and would take ours as a replacement for the
   previous action. Starting during its `$00` acks is correct: the value is then read first by its
   next exchange, which is the one it belongs to. (Found by `host/tests/test_session.c`.)

Battle action nibbles (`BATTLEACTION_*`): 0–3 move slot, 4–9 switch to party slot 0–5, `$D` skip
turn, `$E` Struggle, `$F` forfeit. The value we send is always computed by the ported
`.StageForSend`, which also reproduces the "dummy" byte sent when only the master chooses (our mon
fainted, Baton Pass target, and so on).

End of battle: every exchange starts with a stale `$00` followed one frame later by `$8x`; the exit
`$00` is followed by nothing for seconds. The session treats a lone `$00` after a ≥300 ms gap that
is again followed by a ≥300 ms gap as the end (the engine also detects the end of battle directly).

## Byte blocks (`Serial_ExchangeBytes`)

Each side keeps sending the block's first byte (`$FD`) until it receives a `$FD` (not stored), then
sends byte *k* while storing received byte *k*. Because the master transmits one transfer late,
its last byte of each block reaches us in its next block's first transfer, and our last byte is
never stored by it. Consequences:

- The RN block's last byte is RN[9], which `_BattleRandom` never returns, and we still receive it.
- The party block's 3 padding bytes absorb the shift.
- Our patch block completes on the master's first battle transfer, so the battle phase begins one
  byte later on our side.
- While the master is still ignoring (looking for `$FD`), `$FE` from it triggers no retry because
  `rIE = IE_SERIAL` during the blocks.

**RN block.** The slave copies the master's numbers (`Link_CopyRandomNumbers` skips every `$FD`/`$FE`);
the master keeps its own. The shared seed is the Game Boy's.

**Party block** (`wLinkSendParty`, 450 bytes): 6 × `$FD`, player name (11), party count, 6 species,
`$FF`, trainer ID (2, big endian), 6 × 48-byte `party_struct`, 6 OT names (11), 6 nicknames (11),
3 zero padding bytes. The receiver skips leading `0`/`$FD`/`$FE`, then copies 441 bytes **dropping
every `$FE`** (`Link_CopyOTData`), then applies the patch list. `'8'` encodes to `$FE`, so names must
not contain it.

**Patch list** (`FixDataForLinkTransfer`): covers the 290 bytes from the trainer ID to the end of
the party structs, in two parts of `$FC` and 38 bytes. Every `$FE` there is replaced by `$FF` and its
1-based index within its part is recorded. Layout of the 200 sent bytes: 3 × `$FD`, 7 zero bytes,
then the part-1 indices, `$FF`, the part-2 indices, `$FF`, zeros (entries start at offset
`SERIAL_RNS_LENGTH` = 10). The receiver skips `0`/`$FD`/`$FE` and stops after the second `$FF`.

There is no mail block in the Colosseum.

## Checked against the real game (SameBoy harness)

`host/harness/` runs the real ROM (US/Europe 1.0) in SameBoy and links it to `PblSession` through a
software shift register (the gblink preload model) or to a second emulated GB.

- **Hardware logs replay exactly.** With the Flipper's blocks and the GB's RNs, party and choices
  taken from hardware logs, the emulated battles produce the same exchanges and end on the same
  turn (including a critical hit and a Thunder miss). Fresh sessions also count the same link bytes
  as the device did (898, 826, 802).
- **A real slave GB sends the same values as our session.** In `pkb_harness golden` the second GB
  holds the Flipper's party, trainer and RNs. With `$00`/`$FE` dropped and repeats collapsed, the
  master's byte stream and the slave's are identical in both runs, block payloads included. What
  differs is timing only:
  - the real slave's block bytes arrive one transfer later (the shift-register lag above);
  - it idles on `$FE` (its `Serial` handler resets `hSerialSend`);
  - it answers battle actions after its own menus, several frames late, and then sends its value
    for 10 transfers like the master does, where we answer on the next transfer and stop after
    5 repeats.
- **Handshake between two GBs relies on clock drift.** `WaitForLinkedFriend` arms the external
  clock only for a few instructions per frame before starting its own transfer, so two GBs
  running exactly in phase never link. Real GBs drift; the harness runs the second GB 0.1% slow.
  This doesn't affect the Flipper, which only ever uses the external clock.

