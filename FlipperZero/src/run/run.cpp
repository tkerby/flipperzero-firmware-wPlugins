#include "run/run.hpp"
#include "app.hpp"

FlipWorldRun::FlipWorldRun()
{
    // nothing to do
}

FlipWorldRun::~FlipWorldRun()
{
    // nothing to do
}

void FlipWorldRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Hello World Run!");
}

void FlipWorldRun::updateInput(InputEvent *event)
{
    if (event->key == InputKeyBack)
    {
        // return to menu
        shouldReturnToMenu = true;
    }
}
