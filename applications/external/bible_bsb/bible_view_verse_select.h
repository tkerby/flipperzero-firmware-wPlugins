#pragma once

#include <gui/view.h>
#include <input/input.h>

/* VerseSelect custom view — draw & input callbacks */
void bible_bsb_view_verse_select_draw(Canvas* canvas, void* ctx);
bool bible_bsb_view_verse_select_input(InputEvent* event, void* ctx);
