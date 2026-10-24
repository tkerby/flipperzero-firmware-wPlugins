#pragma once

#include <gui/view.h>

/** Startup splash: "GHOST TRACKER / by Zinetick" + the ghost icon.
 * Auto-dismisses after a short delay, or immediately on any key press. */

typedef struct SplashView SplashView;

typedef void (*SplashDoneCallback)(void* context);

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* instance);
View* splash_view_get_view(SplashView* instance);

/** Called (from the view's own enter/exit lifecycle, i.e. safe to act on
 * immediately) once the splash is done being shown, either via timeout or
 * an early key press. May be called more than once if both race; the
 * caller should treat it idempotently. */
void splash_view_set_done_callback(SplashView* instance, SplashDoneCallback callback, void* context);
