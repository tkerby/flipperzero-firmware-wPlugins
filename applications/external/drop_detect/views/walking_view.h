#pragma once

#include <gui/view.h>

View* walking_view_alloc(void);
void walking_view_free(View* view);
void walking_view_tick(View* view, uint32_t frame);
void walking_view_reset(View* view);
