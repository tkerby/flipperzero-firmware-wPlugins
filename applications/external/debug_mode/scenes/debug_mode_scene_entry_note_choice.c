#include "../debug_mode.h"

typedef enum {
    EntryNoteChoiceIndexAdd,
    EntryNoteChoiceIndexSkip,
} EntryNoteChoiceIndex;

static void debug_mode_scene_entry_note_choice_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_entry_note_choice_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Add a note?");
    submenu_add_item(
        app->submenu,
        "Add Note",
        EntryNoteChoiceIndexAdd,
        debug_mode_scene_entry_note_choice_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Skip",
        EntryNoteChoiceIndexSkip,
        debug_mode_scene_entry_note_choice_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_entry_note_choice_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == EntryNoteChoiceIndexAdd) {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryNoteText);
        } else if(event.event == EntryNoteChoiceIndexSkip) {
            app->pending_entry_note[0] = '\0';
            debug_mode_save_pending_entry_and_return(app);
        } else {
            consumed = false;
        }
    }

    return consumed;
}

void debug_mode_scene_entry_note_choice_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
