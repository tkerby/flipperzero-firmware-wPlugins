#pragma once

#include <gui/view.h>
#include <input/input.h>

/* BookList custom view — draw & input callbacks */
void bible_bsb_view_book_list_draw(Canvas* canvas, void* ctx);
bool bible_bsb_view_book_list_input(InputEvent* event, void* ctx);
