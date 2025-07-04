#include "cuberzero.h"

static void callbackItem(const PCUBERZERO instance, const uint32_t index) {
	if(instance) {
		scene_manager_handle_custom_event(instance->manager, index);
	}
}

void SceneHomeEnter(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	submenu_reset(instance->view.submenu);
	submenu_set_header(instance->view.submenu, "Cuber Zero");
	submenu_add_item(instance->view.submenu, "Timer", CUBERZERO_SCENE_TIMER, (SubmenuItemCallback) callbackItem, instance);
	submenu_add_item(instance->view.submenu, "Settings", CUBERZERO_SCENE_SETTINGS, (SubmenuItemCallback) callbackItem, instance);
	submenu_add_item(instance->view.submenu, "About", CUBERZERO_SCENE_ABOUT, (SubmenuItemCallback) callbackItem, instance);
	submenu_set_selected_item(instance->view.submenu, instance->scene.home.selectedItem);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
}

bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	if(!instance || event.type != SceneManagerEventTypeCustom || event.event >= COUNT_CUBERZEROSCENE) {
		return false;
	}

	instance->scene.home.selectedItem = submenu_get_selected_item(instance->view.submenu);
	scene_manager_next_scene(instance->manager, event.event);
	return true;
}
