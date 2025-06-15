#include "cuberzero.h"

void SceneSettingsEnter(const PCUBERZERO instance) {
	variable_item_list_reset(instance->view.variableList);
	VariableItem* item = variable_item_list_add(instance->view.variableList, "Cube", 4, NULL, instance);
	variable_item_set_current_value_index(item, 0);
	variable_item_set_current_value_text(item, "3x3");
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST);
}

bool SceneSettingsEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

void SceneSettingsExit(const PCUBERZERO instance) {
	variable_item_list_reset(instance->view.variableList);
}
