#include "src/cuberzero.h"

static void callbackItem(void* const context, const uint32_t index) {
	if(context) {
		scene_manager_handle_custom_event(((PCUBERZERO) context)->manager, index);
	}
}

void SceneHomeEnter(void* const context) {
	if(!context) {
		return;
	}

	submenu_reset(((PCUBERZERO) context)->view.submenu);
	submenu_set_header(((PCUBERZERO) context)->view.submenu, "Cuber Zero");
	submenu_add_item(((PCUBERZERO) context)->view.submenu, "Timer", SCENE_TIMER, callbackItem, context);
	// ----- Test Only -----
	submenu_add_item(((PCUBERZERO) context)->view.submenu, "Session Select", SCENE_SESSION_SELECT, callbackItem, context);
	// ---------------------
	submenu_set_selected_item(((PCUBERZERO) context)->view.submenu, ((PCUBERZERO) context)->scene.home.index);
	view_dispatcher_switch_to_view(((PCUBERZERO) context)->dispatcher, VIEW_SUBMENU);
}

bool SceneHomeEvent(void* const context, const SceneManagerEvent event) {
	if(!context || event.type != SceneManagerEventTypeCustom || event.event >= COUNT_SCENE) {
		return 0;
	}

	((PCUBERZERO) context)->scene.home.index = submenu_get_selected_item(((PCUBERZERO) context)->view.submenu);
	scene_manager_next_scene(((PCUBERZERO) context)->manager, event.event);
	return 1;
}
