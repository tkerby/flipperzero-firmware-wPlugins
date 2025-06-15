#include "cuberzero.h"

static void callbackItem(const PCUBERZERO instance, const uint32_t index) {
	scene_manager_handle_custom_event(instance->manager, index);
}

void SceneHomeEnter(const PCUBERZERO instance) {
	submenu_reset(instance->view.submenu);
	submenu_add_item(instance->view.submenu, "Timer", CUBERZERO_SCENE_TIMER, (SubmenuItemCallback) callbackItem, instance);
	submenu_add_item(instance->view.submenu, "Settings", CUBERZERO_SCENE_SETTINGS, (SubmenuItemCallback) callbackItem, instance);
	submenu_add_item(instance->view.submenu, "About", CUBERZERO_SCENE_ABOUT, (SubmenuItemCallback) callbackItem, instance);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
}

bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	if(event.type != SceneManagerEventTypeCustom || event.event >= CUBERZERO_SCENE_COUNT) {
		return false;
	}

	scene_manager_next_scene(instance->manager, event.event);
	return true;
}

void SceneHomeExit(const PCUBERZERO instance) {
	submenu_reset(instance->view.submenu);
}
