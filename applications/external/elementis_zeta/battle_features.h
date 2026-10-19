// Contract 73 + contract 77: which battle features are compiled into the battle plugin. The DEV battle peak is the binding memory row of the whole game (floor 29,000 B, the maintainer's bar 29,300 B):
// every feature that is 1 here is built, tested and has its data (moves, items, species, texts) in the tree. A switch set to 0 compiles the feature out (its fields of `Side` / `Battle` do not exist, its code is not built) and costs nothing:
// contract section 73.2 / 77.2 have the measured cost of each one in plugin bytes. Since contract 77 ALL ELEVEN are on (the data of the last four was the pending kit the design notes, applied and removed by F4); keep the switches:
// they are what lets a later feature be measured against the bar by turning one off.
#pragma once
#pragma GCC diagnostic ignored \
    "-Wunused-parameter" // (a switched-off feature leaves parameters and locals unused)
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#define BF_RECH  1 // Mega Beam: the turn after it is lost (unless it KOs, breaks a decoy or misses)
#define BF_CRASH 1 // Jump Kick / High Kick: a miss or an immune target costs the user 1 HP
#define BF_TOXIC 1 // Toxic: poison that grows N / 16 of the max HP every turn
#define BF_RAGE  1 // Rage: locked into it, Attack +1 each time the user is hit
#define BF_ITEMS \
    1 // boost items, Mist Veil, Crit Focus (in battle); Cure-All / Defrost are plain cure items
#define BF_BADGE 1 // the badges' x 9/8 on Attack / Defense / Speed / Special
#define BF_DISABLE \
    1 // Disable (the move Lockout): a random move of the target cannot be used for 1 .. 8 of its turns
#define BF_CONVERT 1 // Typecast (the move Typecast): the user takes the target's types
#define BF_MOVE_COPY \
    1 // Echoing: the player picks one of the foe's moves, it replaces Echoing in its slot until the battle ends
#define BF_OBEY 1 // a creature from another trainer above the badges' level may not obey
#define BF_TRANSFORM \
    1 // Transform (the move Refold, Foldling): the picture, types, moves, PP and stats of the target until the switch
// Contract 82 (agent trainer AI and battle). Every switch is built, tested and has its data in the tree; the Flipper's battle plugin has them on as far as the DEV battle peak allows (contract 82.1 has the bytes), the PC version turns all on.
#define BF_AI_ITEMS \
    1 // (ON since contract 83) trainers use items (salves, Cure-All / Restore-likes, boost items, Mist Veil) on their creature as their ai.toml class says; the item takes the turn
#define BF_AI_MOD3 \
    1 // (ON since contract 83) AI modifier 3: prefer a super effective move, avoid a not very effective one when a better move exists
#define BF_AI_MOD2 0 // AI modifier 2: from its second turn on, prefer stat / set-up moves
#define BF_AI_SWITCH \
    0 // some trainer classes (Switcher, Veteran F, Velo) withdraw their creature for the first other one that can fight
#define BF_FLUTE \
    1 // the Flauta Clara (flute) in the battle bag: every sleeping creature of both parties wakes up (the enemy's only in a trainer battle)
#define BF_COIN_HIT \
    1 // Pilfer scatters coins (2 x the user's level at every hit, paid at the end of a won / caught battle)
#define BF_MIND_WAVE 1 // Mind Wave's random damage, 1 .. 1.5 x the user's level (`fixed` arg0 254)
// Contract 85 (the PC port): `make -C tools/host_battle allbf` builds the unit tests and the UI scenarios with -DBF_ALL_ON, which turns EVERY switch above on whatever its value is (the firmware leaves
// BF_AI_MOD2 / BF_AI_SWITCH off for the plugin's RAM). A new switch goes here too: tools/tai_data_test.py compares the two lists (indented `# define` below: the host Makefile greps the unindented `#define BF_x 1` lines of the top).
#ifdef BF_ALL_ON
#undef BF_RECH
#define BF_RECH 1
#undef BF_CRASH
#define BF_CRASH 1
#undef BF_TOXIC
#define BF_TOXIC 1
#undef BF_RAGE
#define BF_RAGE 1
#undef BF_ITEMS
#define BF_ITEMS 1
#undef BF_BADGE
#define BF_BADGE 1
#undef BF_DISABLE
#define BF_DISABLE 1
#undef BF_CONVERT
#define BF_CONVERT 1
#undef BF_MOVE_COPY
#define BF_MOVE_COPY 1
#undef BF_OBEY
#define BF_OBEY 1
#undef BF_TRANSFORM
#define BF_TRANSFORM 1
#undef BF_AI_ITEMS
#define BF_AI_ITEMS 1
#undef BF_AI_MOD3
#define BF_AI_MOD3 1
#undef BF_AI_MOD2
#define BF_AI_MOD2 1
#undef BF_AI_SWITCH
#define BF_AI_SWITCH 1
#undef BF_FLUTE
#define BF_FLUTE 1
#undef BF_COIN_HIT
#define BF_COIN_HIT 1
#undef BF_MIND_WAVE
#define BF_MIND_WAVE 1
#endif
