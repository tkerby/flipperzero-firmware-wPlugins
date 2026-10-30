# Battle engine port

`lib/pkb_core/engine/` is a bit-exact port of Pokémon Crystal's link-battle engine, from
pret/pokecrystal at the revision in `tools/gen/pokecrystal.lock`. The Flipper runs one side of a
Colosseum battle with it, in lockstep with the Game Boy on the other end of the cable.

## How it is built

- **Memory.** `PkbEngine` holds an 8 KiB copy of WRAM ($C000–$DFFF with WRAMX bank 1 at $D000)
  and HRAM, laid out exactly like the game. Addresses come from `pokecrystal.sym`
  (`lib/pkb_core/gen/pb_wram.h`), so code that walks memory, unions and overlapping temporaries
  (`wCurType`, `wTypeMatchup` and `wNamedObjectIndex` are one byte) behave as on the Game Boy, and
  tests can compare the engine with an emulated Game Boy byte for byte.
- **Routines.** One C function per asm label, same name, in `core.c` (engine/battle/core.asm),
  `effects.c` (engine/battle/effect_commands.asm) and `engine.c` (home bank: battle variables,
  `_BattleRandom`, `_Multiply`/`_Divide` ported literally on the HRAM bytes,
  `_LinkBattleSendReceiveAction`). Player and enemy twins stay separate because the asm is not
  symmetric (for example, only the player's confusion hit clears `wCriticalHit`).
- **Move effects.** `DoMove` copies the effect's command bytes (generated from
  `data/moves/effects.asm` into `battle_tables.c`) into `wBattleScriptBuffer` and runs them from
  there, as the asm does, so `EndMoveEffect` and skipped commands work the same. Commands pass
  values through the Z80 registers b, c, d and e (damagestats → damagecalc); `PkbEngine.reg_*`
  keep them.
- **Perspective.** The engine runs as the slave (`USING_EXTERNAL_CLOCK`). It can also run as the
  master for self-play.
- **Controller.** `PkbEngineIo` supplies the player's choices (battle menu, replacement after a
  faint) and the blocking action exchange. The app runs the engine on its own thread
  (`app/engine_glue.c`); the exchange posts our action to the link session and waits for the Game
  Boy's. On device the choices come from the AI (`lib/pkb_core/ai/ai.c`), which reads the engine
  only through `pkb_engine_peek`. Its damage estimate repeats `AttackDamage`, `DamageCalc` and
  `Stab` without side effects (Colosseum truncation in one step; Struggle skips all of Stab).
  `test_ai` checks it against the engine (`pkb_engine_test_damage`, host only), and self-play with
  `PKB_SP_AI=1` checks it never changes the state. It never uses Swagger (the Swagger/Minimize game
  desync), and Snore or Sleep Talk only while asleep.

Text, sound, animations and HUD drawing are dropped. What they do to game state is kept: these
"display" routines write state the battle reads later.

| Routine | Hidden effect |
| --- | --- |
| `UsedMoveText` (text_asm) | sets the user's last move and last counter move, and `wPlayerUsedMoves` |
| `MoveInfoBox` (move menu panel) | sets `wCurPartyMon = wCurBattleMon` and the move struct; `wCurPartyMon` is what a later forced-switch exchange sends |
| `SendOutMonText` | records `wEnemyHPAtTimeOfPlayerSwitch` |
| `ChangeHappiness` (after fainting) | also writes `wBattleMonHappiness` when `wCurPartyMon` equals the 1-based `wPartyMenuCursor` (a game bug) |

## What is ported

Battle flow (`DoBattle`, `BattleTurn`, move order with priorities, Quick Claw and speed ties,
`Battle_PlayerFirst`/`EnemyFirst`), residual damage and the between-turn handlers, every faint and
switch path including both sides switching and double faints, `ParsePlayerAction`/`ParseEnemyAction`,
`StageForSend` (dummy bytes included), stat setup and status stat effects, `ChangeHappiness`, and
these battle commands:

- **Core:** checkobedience, usedmovetext, doturn, critical, damagestats, damagecalc, stab,
  damagevariation, checkhit, moveanim, failuretext, applydamage, criticaltext, supereffectivetext,
  checkfaint, buildopponentrage, kingsrock, recoil.
- **Status, stats, fixed damage:** effectchance, checksafeguard; sleeptarget, poisontarget, poison
  (Toxic too), burntarget, freezetarget, paralyzetarget, paralyze, confuse, confusetarget,
  flinchtarget, tristatuschance, defrost; draintarget and eatdream (`SapHealth`); snore; all 28
  stat up/down commands with `CheckMist`, `MinimizeDropSub` and the stat recalculation;
  statupanim/statdownanim, allstatsup, resetstats, curl, switchturn; startloop/endloop (Beat Up
  counts, Triple Kick), triplekick, kickcounter, clearmissdamage; ohko, constantdamage (level,
  fixed, Psywave, Super Fang, Flail/Reversal), resettypematchup, the double damage against
  flying/underground/minimized targets, thunderaccuracy, getmagnitude, happiness/frustration power,
  hiddenpower, falseswipe, heal (Recover family and Rest), selfdestruct, splash, payday, mist,
  focusenergy. Text and delay commands are no-ops. The AI's 25% failure rolls are skipped (link
  battles never make them).
- **Two-turn, screens, and move scripts:** in `effects.c` (effect_commands.asm) checkcharge, charge,
  skipsuncharge (two-turn moves), checkrampage, rampage, rechargenextturn, traptarget, rage,
  ragedamage, arenatrap, fakeout, screen, healmorn/healday/healnite, forceswitch (Roar, Whirlwind)
  and teleport; in `move_effects.c` (one asm file each in engine/battle/move_effects) attract (with
  `GetGender`), batonpass, bellydrum, storeenergy/unleashenergy (Bide), conversion, conversion2,
  counter, mirrorcoat, curse, destinybond, disable, encore, spite, protect, endure, foresight,
  lockon, leechseed, nightmare, spikes, safeguard, perishsong, furycutter, checkrollout,
  rolloutpower, pursuit, checkfuturesight, futuresight, healbell, painsplit, psychup, metronome,
  mirrormove, sleeptalk, mimic, sketch, present, clearhazards (Rapid Spin), the three weather
  moves, substitute, thief, transform and Beat Up (link-battle paths only). In `core.c`:
  `PursuitSwitch` (Pursuit on a switching mon), Future Sight's hit in `HandleFutureSight`,
  `ForceEnemySwitch`, `SwitchPlayerMon` and `PassedBattleMonEntrance`.

`pkb_engine_effect_supported()` derives the complete effects from the generated scripts, and
`pkb_engine_move_filter()` keeps Beat Up out of the Flipper's own teams (`legality.py` bans it, so
`movepools.bin` never has it). It can still come from the Game Boy, or from Metronome and Mirror
Move. Its two link bugs are flagged as game desyncs (5 and 6 below). The fuzz parties leave it out
unless asked (`--beat-up`, `PKB_SP_BEAT_UP=1`), so the regression seeds keep their battles.

Details that matter:

- **Link exchanges inside a turn.** Baton Pass makes the passing side pick a mon and both Game
  Boys exchange actions again (`BatonPass_LinkPlayerSwitch`/`LinkEnemySwitch`). The engine asks
  `choose_replacement` for our pick, so the controllers' replacement must not be the mon that is
  out (`pkb_engine_random_replacement` excludes it). Roar and Whirlwind need no exchange: both
  Game Boys roll the replacement with the shared random numbers.
- **No time of day.** Morning Sun, Synthesis and Moonlight ignore the clock in link battles; only
  the weather changes the heal.
- **Present's damage** uses whatever registers Stab leaves behind, because the Colosseum skips the
  push/pop around it (`BattleCommand_Present` documents the values). A Present that missed or
  does not affect still runs damagecalc, with c = 0 from `AnimateFailedMove`'s `MoveDelay` (40
  frames counted down in c), so its damage is 2 and damagevariation draws random numbers.
- **Searches without a bound.** Disable, Encore, Spite and Mimic look for a move in a loop that
  can run past the four moves; the port searches the same bytes and reports an unported path if
  it would have to go further than the Game Boy could without hanging.
- **Past the Moves table.** `GetMoveEffect` of move 0 or `CANNOT_MOVE` (Pursuit checks the move
  of a switching side) reads the bytes after the table in the ROM; the port returns them.
- **Shiny send-outs** leave `wBattleAnimParam` at 1 (the shiny animation); this was missing
  before the animation ports.
- **Animations that change state.** `anim_jumpuntil` counts `wBattleAnimParam` down to 0. Only
  Present's animation reaches it with a nonzero param (its power index), so with animations on a
  Present hit leaves 0 (Fury Cutter and Magnitude run it from `MoveAnim`, which cleared it).
- **Leaving DoMove.** Future Sight's `.failed` pops the command's return address, so `DoMove`
  returns without reading the endmove bytes `EndMoveEffect` wrote and `wBattleScriptBufferAddress`
  stays one byte earlier (`PkbEngine.leave_do_move`). No other battle command does this.

**Substitute animations change state.** `LowerSub`/`RaiseSub` set `wBattleAnimParam` when battle
animations are on (`CheckBattleScene` reads the `BATTLE_SCENE` bit of `wOptions`), and
statupanim/statdownanim clear it. So `MoveAnim` is LowerSub + `MoveAnimNoSub` + RaiseSub, and the
RaiseSub calls in the confusion self-hits, FailureText and CheckFaint (multi-hit) are kept.

**Held items.** Every held item a battle reads is ported: Quick Claw, Focus Band, King's Rock,
BrightPowder, Scope Lens, Leftovers, the type boosters, Thick Club, Light Ball, Metal Powder, Lucky
Punch, Stick and Amulet Coin, the HP berries (`HandleHPHealingItem`), Mystery Berry
(`HandleMysteryberry`, which reads the party struct's PP and adds 5, or 1 for Sketch, to the battle
struct only if the move there is the same and the mon is not transformed), the status and confusion
berries (`UseHeldStatusHealingItem`, `UseConfusionHealingItem`), Berserk Gene (`HandleBerserkGene`:
+2 Attack and confusion with the confusion count left as it was, a game bug) and `ConsumeHeldItem`
with its table (`pb_held_consumables`). `ItemRecoveryAnim` only sets `wFXAnimID` (RECOVER). The
fuzzed parties (`host/sim/fuzz_party.c`) hold an item two times in three, mostly one with a held
effect; `--no-items` (harness) and `PKB_SP_NO_ITEMS=1` (self-play) give item-free parties for
stable regression seeds. Flipper teams carry held items (bosses get hand-picked ones; random teams
get a sensible pick).

Anything else sets `PKB_ENGINE_UNSUPPORTED` (the app shows "Desync risk (unported move)"), including
`HandleStatBoostingHeldItems`, whose held effects no item has.

## Game bugs that desync two Game Boys

Some Crystal bugs make the two Game Boys of a link battle compute a turn differently. The engine
reproduces them, because it must match the Game Boy it stands in for, and sets
`PKB_ENGINE_GAME_DESYNC` with `PkbEngine.game_desync` naming the first one. The app shows
"Desync risk (game bug)" and logs `game desync N`; `replay_log`, `test_selfplay` and the harness
count these battles apart from divergences.

| `PKB_DESYNC_*` | Bug | Flagged when |
| --- | --- | --- |
| `CONFUSION_CRIT` (1) | Snore used while awake runs `critical` before `snore` ends the move, so `wCriticalHit` stays set. The player's confusion self-hit (`HitConfusion`) clears it, the enemy's (`CheckEnemyTurn`) does not, so one Game Boy doubles the self-hit. | the HP left differs (a self-hit that faints the mon either way does not desync; `CheckConfusionCritDesync` works out both damages) |
| `SWAGGER_MINIMIZE` (2) | Swagger's attack raise runs with the turn switched, so `MinimizeDropSub` reads the target's move struct. `SendOutPlayerMon` zeroes the player struct's animation but nothing zeroes the enemy's, so after a switch the two Game Boys hold different animations there and disagree on the Minimize flag (Stomp then hits differently). | the other Game Boy's copy of that animation (tracked through the send-outs) differs in being Minimize |
| `PURSUIT_BATON_PASS` (3) | Pursuit doubles its damage if the target's `w*IsSwitching` is set. The passer's `PassedBattleMonEntrance` sets nothing, but the other Game Boy's `EnemySwitch_SetMode` sets `wEnemyIsSwitching`, so a slower Pursuit in the turn of a Baton Pass is doubled on one Game Boy only. | Pursuit runs with damage and the other Game Boy's flag (`peer_*_switching`, set by the menu switches and the pass) differs from ours |
| `STALE_MOVE_STRUCT` (4) | `ParsePlayerAction` skips `UpdateMoveData` for a locked-in move while `ParseEnemyAction` always reloads. A Future Sight hit at the end of the turn leaves `FUTURE_SIGHT` in the user's struct, so in the attack turn of its Fly or Dig the two Game Boys hold different animations there. If the mon then cannot move (paralysis, flinch...), `CantMove` lowers it from the air or the ground on one Game Boy only. | `CantMove` finds the Fly/Dig bits set and the other Game Boy's copy of the animation (`peer_*_anim`) differs in being Fly or Dig |
| `BEAT_UP` (5) | `BattleCommand_BeatUp`'s player branch compares `wCurBattleMon` with the low HP byte (still in `hl`) instead of the party index, so it picks the battle mon's or the party struct's status for the wrong mon. The enemy branch, which the other Game Boy runs for the same hit, gets it right. | the two branches disagree on whether that mon is statused (it is then skipped on one Game Boy only) |
| `BEAT_UP_OVERRUN` (6) | A Beat Up that misses after its first mon was skipped ends in `failuretext` with `SUBSTATUS_IN_LOOP` still set (only a faint or a switch clears it). The next Beat Up then starts with the count at 0, indexes past the end of the party and loops 255 times, and the two Game Boys read different memory (their own party and the OT copy, and whatever follows each). | Beat Up computes a party index at or past the party count |

Self-play hits one of these in about one battle in 3,000 (32 in 100,000 with Beat Up left out of
fuzz parties; with Beat Up in the parties 97 in 100,000, 72 of them Beat Up's). Afterwards the sides
can fall out of step
(one asks for a replacement while the other starts a turn), so `test_selfplay` notices when both
sides are blocked on each other and ends the battle. The other side's actions then need not make sense
here (a replacement can name a party slot that does not exist), so after a game desync the engine
no longer reports unported paths and host builds no longer abort on accesses outside WRAM.

## How it is checked

| Check | What it proves | Where |
| --- | --- | --- |
| `pkb_harness shadow` | Two emulated Game Boys battle with random parties and choices; the engine runs alongside the slave, starting from a copy of the slave's WRAM (or, with `--init`, from `pkb_engine_init()` and the parties only), takes the slave's choices and the master's actions, and must equal the slave's WRAM byte for byte at every exchange and at the end. Differences are named by WRAM symbol. | `host/harness/shadow.c` |
| `pkb_harness engine` | The device setup: the Game Boy against our link session with the engine behind it, mirror-compared with the Game Boy at the start of every turn. Its logs must replay in the engine alone. | `host/harness/engine_session.c`, `host/tools/replay_log.c` |
| `test_selfplay` | 10,000 battles between a master and a slave engine; their states must mirror each other every turn unless a game bug was flagged. No ROM needed (runs in CI). | `host/tests/test_selfplay.c`, `host/sim/mirror.c` |

The shadow comparison excludes `wCurPartySpecies` (scratch for menus and cries) and the trainer
attributes block (`wEnemyTrainerItem1`..`wOTClassName`, `wOtherTrainerClass`, `wTrainerClass`,
`wOtherTrainerID`), which link battles never read. Mirror comparisons skip what the two sides
legitimately keep differently: happiness, the HP and status of the other side's party copy of the
active mon (a Game Boy refreshes only its own party every turn), and the last-move counters after
the battle ends.

The harness driver makes its choices by poking WRAM from hooks, and every poke must be one a
player could cause. For example, it sets `wCurMoveNum` (the starting cursor) at
`MoveSelectionScreen.got_menu_type`, not at the routine's entry. With no usable moves the game
returns to use Struggle before that point, and a poke at the entry wrote a `wCurMoveNum` that no
real player could produce. That showed up as a false shadow divergence.

The game has a serial interrupt race of its own. `VBlank_CutsceneCGB`, the VBlank mode used during
battle animations, reads `rIF` and later clears it, and other handlers have shorter windows of the
same kind. An external-clock byte that completes inside such a window loses its interrupt. The
`Serial` handler then never re-arms the transfer, and that Game Boy waits at "Waiting...!" forever.
A clock master restarts its transfer every frame and recovers. In the shadow runs, two Game Boys
drifting slowly through each other's frames hit this in about one battle in 70, so the twin bridge
requests the lost interrupt again at `GameTimer` (`host/harness/bridge.c`). On the device the
Game Boy is the clock master, which recovers on its own.
