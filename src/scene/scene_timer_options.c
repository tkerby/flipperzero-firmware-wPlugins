#include "src/cuberzero.h"

void SceneTimerOptionsEnter(const PCUBERZERO instance) {
	variable_item_list_reset(instance->view.variableList);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST);
}
