#include "cuberzero.h"
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
	char* message = 0;
	PCUBERZERO instance = malloc(sizeof(CUBERZERO));

	if(!instance) {
		message = "malloc() failed";
		goto functionExit;
	}

	/*const AppSceneOnEnterCallback handlerEnter[] = {SceneHomeEnter};
	const AppSceneOnEventCallback handlerEvent[] = {callbackEmptyEvent};
	const AppSceneOnExitCallback handlerExit[] = {callbackEmptyExit};
	const SceneManagerHandlers handlers = {handlerEnter, handlerEvent, handlerExit, COUNT_SCENE};
	scene_manager_alloc(&handlers, 0);*/
	free(instance);
functionExit:
	CUBERZERO_INFO("Exiting");

	if(!message) {
		return 0;
	}

	CUBERZERO_ERROR("Error: %s", message);
	__furi_crash(message);
	return 1;
}
