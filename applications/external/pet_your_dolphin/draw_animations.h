#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_i.h>

void draw_base(Canvas* canvas, uint32_t ticks);
void draw_petting(Canvas* canvas, uint32_t ticks, uint32_t petting_duration);
void draw_happy_idle(Canvas* canvas, uint32_t ticks);
void draw_neutral_idle(Canvas* canvas, uint32_t ticks);
void draw_sad_idle(Canvas* canvas, uint32_t ticks);
void draw_pet_prompt(Canvas* canvas, uint32_t ticks, bool pet_limit_reached);
void draw_speech_bubble(Canvas* canvas);
