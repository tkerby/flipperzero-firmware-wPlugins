#include "../pkb_app.h"
#include "pkb_scene.h"

static const char about_text[] = "\e#Pokemon Battle\n"
                                 "Link battle opponent for\n"
                                 "Pokemon Crystal.\n\n"
                                 "Connect the link cable,\n"
                                 "choose Start Battle, then\n"
                                 "talk to the Cable Club\n"
                                 "receptionist and pick the\n"
                                 "Colosseum.\n\n"
                                 "github.com/cycalo/\n"
                                 "Flipper-Zero-Game-\n"
                                 "Boy-Pokemon-Battle\n\n"
                                 "Not affiliated with\n"
                                 "Nintendo, Game Freak or\n"
                                 "The Pokemon Company.";

void pkb_scene_about_on_enter(void* context) {
    PkbApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, about_text);

    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewWidget);
}

bool pkb_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pkb_scene_about_on_exit(void* context) {
    PkbApp* app = context;
    widget_reset(app->widget);
}
