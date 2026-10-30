#pragma once
// Read-only views of the private state of some scenes, for the PC version's native painters (platform/pc/src/pc_hd*.c, the design notes 4.5).
// Everything here exists only when NUCLEO_PC is defined (the PC build); the Flipper never defines it, so none of it reaches the device.
#ifdef NUCLEO_PC
#include <stdint.h>
#include <stdbool.h>
#include "font.h"

// The DIALOG scene (scene_dialog.c): the page being shown and how much of it the typewriter has revealed.
typedef struct {
    const tchar* buf; // the whole loaded text (pages, lines)
    int l0, l1; // the line starts of the current page (l1 = -1: one line)
    int n0, total; // glyphs of line 0 / of the page
    int shown; // glyphs revealed so far
    bool more; // another page follows
    bool arrow; // blink phase of the "more" arrow
    bool asking; // a YES / NO question closes this dialog (scripts' ASK)
} PcDialogView;
bool scene_dialog_pc_view(const void* state, PcDialogView* out);

// The CHOICE scene (scene_choice.c): the YES / NO box.
typedef struct {
    const tchar* yes;
    const tchar* no;
    uint8_t sel; // 0 = YES, 1 = NO
} PcChoiceView;
bool scene_choice_pc_view(const void* state, PcChoiceView* out);
#endif
