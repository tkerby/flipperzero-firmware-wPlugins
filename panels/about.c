#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../utils/draw.h"
#include "../iconedit.h"

void about_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    char app_name[32];
    snprintf(app_name, 32, "IconEdit v%s", VERSION);
    canvas_draw_str(canvas, 2, 22, app_name);
    ie_draw_str(canvas, 2, 32, AlignLeft, AlignTop, Font5x7, "github.com");
    ie_draw_str(canvas, 8, 40, AlignLeft, AlignTop, Font5x7, "/rdefeo");
    ie_draw_str(canvas, 14, 48, AlignLeft, AlignTop, Font5x7, "/iconedit");
}

bool about_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
        case InputKeyBack:
            app->panel = Panel_TabBar;
            break;
        default:
            break;
        }
    }
    return consumed;
}
