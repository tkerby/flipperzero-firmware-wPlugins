#pragma once

#include <gui/view.h>

typedef struct ChameleonAnimationView ChameleonAnimationView;

typedef void (*ChameleonAnimationViewCallback)(void* context);

ChameleonAnimationView* chameleon_animation_view_alloc();
void chameleon_animation_view_free(ChameleonAnimationView* animation_view);
View* chameleon_animation_view_get_view(ChameleonAnimationView* animation_view);

void chameleon_animation_view_set_callback(
    ChameleonAnimationView* animation_view,
    ChameleonAnimationViewCallback callback,
    void* context);

void chameleon_animation_view_start(ChameleonAnimationView* animation_view);
void chameleon_animation_view_stop(ChameleonAnimationView* animation_view);
