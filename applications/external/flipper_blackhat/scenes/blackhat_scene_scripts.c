#include "../blackhat_app_i.h"

static void blackhat_scene_script_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    BlackhatApp* app = context;

    app->selected_tx_string = "bh script run";
    app->selected_option_item_text = app->cmd[index + 1];
    app->text_input_req = false;
    app->selected_menu_index = index;

    FURI_LOG_I("tag/app name", "%s", app->selected_tx_string);

    scene_manager_set_scene_state(
        app->scene_manager, BlackhatSceneStart, app->selected_menu_index);

    scene_manager_next_scene(app->scene_manager, BlackhatAppViewConsoleOutput);
}

static void blackhat_scene_script_list_change_callback(VariableItem* item) {
    furi_assert(item); // REMOVE
}

void blackhat_scene_scripts_on_enter(void* context) {
    BlackhatApp* app = context;
    VariableItemList* var_item_list = app->script_item_list;
    size_t i = 0;
    int start = 0;
    memset(app->cmd, 0x00, sizeof(app->cmd));
    app->num_scripts = 0;

    while(app->script_text[i++]) {
        if(app->script_text[i] == '\n') {
            i++;
            app->cmd[app->num_scripts] = malloc(i - start);
            memcpy(app->cmd[app->num_scripts], &app->script_text[start], i - start);
            start = i;
            app->num_scripts++;
        }
    }

    variable_item_list_set_enter_callback(
        var_item_list, blackhat_scene_script_list_enter_callback, app);

    for(int i = 1; i < app->num_scripts; i++) {
        variable_item_list_add(
            var_item_list, app->cmd[i], 1, blackhat_scene_script_list_change_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BlackhatAppViewScriptItemList);
}

bool blackhat_scene_scripts_on_event(void* context, SceneManagerEvent event) {
    BlackhatApp* app = context;
    furi_assert(app); // REMOVE
    furi_assert(event); // REMOVE
    return false;
}

void blackhat_scene_scripts_on_exit(void* context) {
    BlackhatApp* app = context;

    // for(int i = 0 ; i < app->num_scripts ; i++) {
    //     free(app->cmd[i]);
    // }

    variable_item_list_reset(app->script_item_list);

    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BlackhatSceneStart);
}
