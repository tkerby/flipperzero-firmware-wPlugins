/*#include "cuberzero.h"
#include <gui/modules/submenu.h>

void SceneHomeInitialize(const PCUBERZERO instance, char** const message) {
	instance->scene.home = (View*) submenu_alloc();

	if(!instance->scene.home) {
		*message = "submenu_alloc() failed";
	}
}

void SceneHomeOnEnter(const PCUBERZERO instance) {
	submenu_reset((Submenu*) instance->scene.home);
	submenu_set_header((Submenu*) instance->scene.home, "Test Header");
	//submenu_add_item((Submenu*) instance->scene.home, "Test Item 1", 0, NULL, instance);
	//submenu_add_item((Submenu*) instance->scene.home, "Test Item 2", 1, NULL, instance);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_SCENE_HOME);
}

bool SceneHomeOnEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return true;
}

void SceneHomeOnExit(const PCUBERZERO instance) {
	UNUSED(instance);
	//submenu_reset((Submenu*) instance->scene.home);
}

void SceneHomeFree(const PCUBERZERO instance) {
	submenu_free((Submenu*) instance->scene.home);
}*/
