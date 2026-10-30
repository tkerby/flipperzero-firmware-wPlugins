#pragma once

#include <gui/view.h>
#include <input/input.h>

/* BookFilter custom view — draw & input callbacks */
void bible_bsb_view_book_filter_draw(Canvas* canvas, void* ctx);
bool bible_bsb_view_book_filter_input(InputEvent* event, void* ctx);
