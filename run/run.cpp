#include "run/run.hpp"
#include "app.hpp"

FlipMapRun::FlipMapRun(void *appContext) : appContext(appContext), shouldReturnToMenu(false)
{
}

FlipMapRun::~FlipMapRun()
{
    // nothing to do
}

void FlipMapRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Find Nearby Flipper Users!");
}

void FlipMapRun::updateInput(InputEvent *event)
{
    if (event->key == InputKeyBack)
    {
        // return to menu
        shouldReturnToMenu = true;
    }
}
