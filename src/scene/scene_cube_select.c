#include "src/cuberzero.h"

static const char* const Cubes[] = {"3x3x3 Cube", "2x2x2 Cube", "4x4x4 Cube", "5x5x5 Cube", "6x6x6 Cube", "7x7x7 Cube", "3x3x3 Blindfolded", "3x3x3 Fewest Moves", "3x3x3 One-Handed", "Clock", "Megaminx", "Pyraminx", "Skewb", "Square-1", "4x4x4 Blindfolded", "5x5x5 Blindfolded", "3x3x3 Multi-Blind"};

static void callbackItem(const PCUBERZERO instance, const uint32_t index) {
	if(instance) {
		scene_manager_handle_custom_event(instance->manager, index);
		scene_manager_handle_back_event(instance->manager);
	}
}

void SceneCubeSelectEnter(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	submenu_reset(instance->view.submenu);

	for(uint8_t i = 0; i < COUNT_CUBERZEROCUBE; i++) {
		submenu_add_item(instance->view.submenu, Cubes[i], i, (SubmenuItemCallback) callbackItem, instance);
	}

	submenu_set_selected_item(instance->view.submenu, instance->settings.cube);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
}

bool SceneCubeSelectEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	if(!instance || event.type != SceneManagerEventTypeCustom || event.event >= COUNT_CUBERZEROCUBE) {
		return false;
	}

	instance->settings.cube = event.event;
	CuberZeroSettingsSave(&instance->settings);
	return true;
}
