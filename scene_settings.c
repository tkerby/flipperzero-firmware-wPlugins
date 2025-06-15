#include "cuberzero.h"

static const char* const OptionsCube[] = {"3x3", "2x2", "4x4", "5x5", "6x6", "7x7", "3x3 BLD", "3x3 FM", "3x3 OH", "Clock", "Mega", "Pyra", "Skewb", "SQ-1", "4x4 BLD", "5x5 BLD", "3x3 MB"};

static void callbackChangeCube(VariableItem* const item) {
	const PCUBERZERO instance = variable_item_get_context(item);

	if(instance) {
		variable_item_set_current_value_text(item, OptionsCube[instance->settings.cube = variable_item_get_current_value_index(item)]);
		CuberZeroSettingsSave(&instance->settings);
	}
}

static void enterCallback(void* context, uint32_t index) {
	UNUSED(context);
	FURI_LOG_I("CuberZero", "Enter Callback: %lu", index);
}

void SceneSettingsEnter(const PCUBERZERO instance) {
	variable_item_list_reset(instance->view.variableList);
	VariableItem* item = variable_item_list_add(instance->view.variableList, "Cube", CUBERZERO_CUBE_COUNT, callbackChangeCube, instance);
	variable_item_set_current_value_index(item, instance->settings.cube);
	variable_item_set_current_value_text(item, OptionsCube[instance->settings.cube]);
	variable_item_list_set_enter_callback(instance->view.variableList, enterCallback, instance);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST);
}

bool SceneSettingsEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

void SceneSettingsExit(const PCUBERZERO instance) {
	variable_item_list_reset(instance->view.variableList);
	CuberZeroSettingsSave(&instance->settings);
}
