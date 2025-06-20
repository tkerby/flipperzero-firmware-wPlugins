#include "cuberzero.h"

typedef enum {
    CUBERZERO_SCENE_SETTINGS_CUBE
} CUBERZEROSCENESETTINGS;

static const char* const OptionCubes[] = {
    "3x3",
    "2x2",
    "4x4",
    "5x5",
    "6x6",
    "7x7",
    "3x3 BLD",
    "3x3 FM",
    "3x3 OH",
    "Clock",
    "Mega",
    "Pyra",
    "Skewb",
    "SQ-1",
    "4x4 BLD",
    "5x5 BLD",
    "3x3 MB"};

static void callbackEnter(const PCUBERZERO instance, const uint32_t index) {
    if(index == CUBERZERO_SCENE_SETTINGS_CUBE) {
        scene_manager_next_scene(instance->manager, CUBERZERO_SCENE_CUBE_SELECT);
    }
}

static void callbackChange(VariableItem* const item) {
    const PCUBERZERO instance = variable_item_get_context(item);

    if(instance) {
        variable_item_set_current_value_text(
            item,
            OptionCubes[instance->settings.cube = variable_item_get_current_value_index(item)]);
        CuberZeroSettingsSave(&instance->settings);
    }
}

void SceneSettingsEnter(const PCUBERZERO instance) {
    variable_item_list_reset(instance->view.variableList);
    variable_item_list_set_enter_callback(
        instance->view.variableList, (VariableItemListEnterCallback)callbackEnter, instance);
    VariableItem* const item = variable_item_list_add(
        instance->view.variableList, "Cube", COUNT_CUBERZEROCUBE, callbackChange, instance);

    if(!item) {
        goto error;
    }

    variable_item_set_current_value_index(item, instance->settings.cube);
    variable_item_set_current_value_text(item, OptionCubes[instance->settings.cube]);
    view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST);
    return;
error:
    variable_item_list_reset(instance->view.variableList);
    scene_manager_handle_back_event(instance->manager);
}

void SceneSettingsExit(const PCUBERZERO instance) {
    CuberZeroSettingsSave(&instance->settings);
}
