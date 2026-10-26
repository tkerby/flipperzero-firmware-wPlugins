#pragma once

#include <gui/view.h>

View* fidget_view_alloc(void);
void fidget_view_free(View* view);
void fidget_view_tick(View* view, uint32_t frame);
void fidget_view_reset(View* view);
