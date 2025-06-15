#include "cuberzero.h"

void SceneHomeEnter(const PCUBERZERO instance) {
	submenu_reset(instance->view.submenu);
	submenu_add_item(instance->view.submenu, "Timer", 0, NULL, instance);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
}

bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

void SceneHomeExit(const PCUBERZERO instance) {
	UNUSED(instance);
}
