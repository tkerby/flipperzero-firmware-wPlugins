#include "../twentyq_app_i.h"

static void twentyq_scene_learn_name_callback(void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

/** Rejects empty, non-ASCII and already-known names (SPEC 8.6). */
static bool twentyq_learn_name_validator(const char* text, FuriString* error, void* context) {
    TwentyQApp* app = context;

    if(!text || !text[0]) {
        furi_string_set(error, "Type a name first");
        return false;
    }
    for(const char* p = text; *p; p++) {
        if((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E) {
            furi_string_set(error, "Letters and\nnumbers only");
            return false;
        }
    }
    if(tq_dataset_find_entity(&app->dataset, text, NULL)) {
        furi_string_printf(error, "I already\nknow that one");
        return false;
    }
    if(tq_learned_find(&app->learned, text, NULL)) {
        furi_string_printf(error, "You taught me\nthat already");
        return false;
    }
    return true;
}

void twentyq_scene_learn_name_on_enter(void* context) {
    TwentyQApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_reset(text_input);
    text_input_set_header_text(text_input, "What was it?");
    text_input_set_result_callback(
        text_input, twentyq_scene_learn_name_callback, app, app->learn_name, TQ_MAX_NAME + 1, true);
    text_input_set_validator(text_input, twentyq_learn_name_validator, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewTextInput);
}

bool twentyq_scene_learn_name_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    /* Names are stored lower-case so the sorted binary search stays valid. */
    for(char* p = app->learn_name; *p; p++) {
        if(*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    }
    scene_manager_next_scene(app->scene_manager, TwentyQSceneLearnCategory);
    return true;
}

void twentyq_scene_learn_name_on_exit(void* context) {
    TwentyQApp* app = context;
    text_input_set_validator(app->text_input, NULL, NULL);
    text_input_reset(app->text_input);
}
