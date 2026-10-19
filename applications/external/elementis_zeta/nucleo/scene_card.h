#pragma once
// Trainer card (contract section 25.4): the pause menu entry that carries the player's name (START -> <NAME>). One framed screen: name, money, play
// time, and eight badge slots in two rows (code-drawn icons; a slot is filled when flag FLAG_BADGE_FIRST + k is set, empty slots are a dotted outline).
// Lives in the PC plugin on the device (ez_pc.fal); the host harness links it in. Any of OK / Back closes it.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"

extern const SceneOps scene_card_ops;

// Pushes the card on top of the stack (the pause menu stays below). False (nothing pushed) when the stack is full or a card is already open.
bool scene_card_open(SceneCtx* c);
// True while the screen's state exists.
bool scene_card_active(void);
// C4-A (contract 35.5): the elevator's floor list lives in this file too (scene_list_open, scene.h); true while it is on the stack.
bool scene_list_active(void);

#ifndef NUCLEO_DEVICE
// Test hook (host harness only): the badge mask the open card shows (bit k = slot k filled), -1 when it is not open.
int scene_card_badges(void);
#endif
