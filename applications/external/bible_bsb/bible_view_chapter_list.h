#pragma once

#include <gui/canvas.h>
#include <input/input.h>

/* ChapterList view — 5-column grid of chapter numbers for selected book. */

void bible_bsb_view_chapter_list_draw(Canvas* canvas, void* ctx);
bool bible_bsb_view_chapter_list_input(InputEvent* event, void* ctx);
