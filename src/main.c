#include "cuberzero.h"
#include <applications/services/gui/scene_manager.h>
#include "scene/scene.h"

static bool callbackEmptyEvent(void* const context, const SceneManagerEvent event) {
	UNUSED(context);
	UNUSED(event);
	return false;
}

static void callbackEmptyExit(void* const context) {
	UNUSED(context);
}

int32_t cuberzeroMain(const void* const unused) {
	UNUSED(unused);
	CUBERZERO_INFO("Initializing");
	const AppSceneOnEnterCallback handlerEnter[] = {SceneHomeEnter};
	const AppSceneOnEventCallback handlerEvent[] = {callbackEmptyEvent};
	const AppSceneOnExitCallback handlerExit[] = {callbackEmptyExit};
	const SceneManagerHandlers handlers = {handlerEnter, handlerEvent, handlerExit, COUNT_SCENE};
	scene_manager_alloc(&handlers, 0);
	CUBERZERO_INFO("Exiting");
	return 0;
}
