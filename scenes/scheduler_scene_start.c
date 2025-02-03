#include "src/scheduler_app_i.h"
#include <furi_hal_power.h>
#include <furi_hal_usb.h>
#include <furi_hal.h>
#include <dolphin/dolphin.h>
#include "scheduler_scene_loadfile.h"

#include <string.h>

#define TAG "Sub-GHzSchedulerSceneStart"

static void scheduler_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    SchedulerApp* app = context;

    if(index == SchedulerStartRunEvent) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SchedulerStartRunEvent);
    } else if(index == SchedulerStartEventSelectFile) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SchedulerStartEventSelectFile);
    }
}

static void scheduler_scene_start_set_interval(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, interval_text[index]);
    scheduler_set_interval(app->scheduler, index);
}

static void scheduler_scene_start_set_repeats(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, tx_repeats_text[index]);
    scheduler_set_tx_repeats(app->scheduler, index);
}

static void scheduler_scene_start_set_immediate(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    bool index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, immediate_mode_text[index]);
    scheduler_set_immediate_mode(app->scheduler, index);
}

void scheduler_scene_start_on_enter(void* context) {
    SchedulerApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;

    scheduler_reset(app->scheduler);

    variable_item_list_set_enter_callback(
        var_item_list, scheduler_scene_start_var_list_enter_callback, app);

    item = variable_item_list_add(
        var_item_list, "Interval:", INTERVAL_COUNT, scheduler_scene_start_set_interval, app);
    value_index = scheduler_get_interval(app->scheduler);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, interval_text[value_index]);
    value_index = variable_item_get_current_value_index(item);

    item = variable_item_list_add(
        var_item_list, "Repeats:", REPEATS_COUNT, scheduler_scene_start_set_repeats, app);
    value_index = scheduler_get_tx_repeats(app->scheduler);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, tx_repeats_text[value_index]);
    value_index = variable_item_get_current_value_index(item);

    item = variable_item_list_add(
        var_item_list,
        "Immediate:",
        SchedulerImmediateModeSettingsNum,
        scheduler_scene_start_set_immediate,
        app);

    value_index = scheduler_get_immediate_mode(app->scheduler);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, immediate_mode_text[value_index]);
    scheduler_set_immediate_mode(app->scheduler, value_index);

    item = variable_item_list_add(var_item_list, "Select File", 0, NULL, app);
    if(check_file_extension(furi_string_get_cstr(app->file_path))) {
        scene_manager_set_scene_state(
            app->scene_manager, SchedulerSceneStart, SchedulerStartRunEvent);
        if(scheduler_get_file_type(app->scheduler) == SchedulerFileTypeSingle) {
            variable_item_set_current_value_text(item, "[Single]");
        } else {
            variable_item_set_current_value_text(item, "[Playlist]");
        }
    }

    variable_item_list_add(var_item_list, "Start", 0, NULL, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, SchedulerSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewVarItemList);
}

bool scheduler_scene_start_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SchedulerStartRunEvent) {
            if(!check_file_extension(furi_string_get_cstr(app->file_path))) {
                dialog_message_show_storage_error(
                    app->dialogs, "Please select\nplaylist (*.txt) or\n *.sub file!");
            } else {
                scene_manager_next_scene(app->scene_manager, SchedulerSceneRunSchedule);
            }
        } else if(event.event == SchedulerStartEventSelectFile) {
            scene_manager_next_scene(app->scene_manager, SchedulerSceneLoadFile);
        }
        consumed = true;
    }
    return consumed;
}

void scheduler_scene_start_on_exit(void* context) {
    SchedulerApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
