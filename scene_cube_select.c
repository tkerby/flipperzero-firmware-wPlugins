#include "cuberzero.h"

static const char* const WCAEvents[] = {"3x3x3 Cube", "2x2x2 Cube", "4x4x4 Cube", "5x5x5 Cube", "6x6x6 Cube", "7x7x7 Cube", "3x3x3 Blindfolded", "3x3x3 Fewest Moves", "3x3x3 One-Handed", "Clock", "Megaminx", "Pyraminx", "Skewb", "Square-1", "4x4x4 Blindfolded", "5x5x5 Blindfolded", "3x3x3 Multi-Blind"};

void SceneCubeSelectEnter(const PCUBERZERO instance) {
	submenu_reset(instance->view.submenu);

	for(uint8_t i = 0; i < CUBERZERO_CUBE_COUNT; i++) {
		submenu_add_item(instance->view.submenu, WCAEvents[i], i, NULL, instance);
	}

	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
}

bool SceneCubeSelectEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

void SceneCubeSelectExit(const PCUBERZERO instance) {
	submenu_reset(instance->view.submenu);
}
