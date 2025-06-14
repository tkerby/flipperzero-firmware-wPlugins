#include "cuberzero.h"

static bool callbackCustomEvent(const PCUBERZERO instance, const uint32_t event) {
	return scene_manager_handle_custom_event(instance->manager, event);
}

static bool callbackNavigationEvent(const PCUBERZERO instance) {
	return scene_manager_handle_back_event(instance->manager);
}

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	CUBERZERO_LOG("Initializing");
	char* messageError;
	const PCUBERZERO instance = malloc(sizeof(CUBERZERO));

	if(!instance) {
		messageError = "malloc() failed";
		goto functionExit;
	}

	if(!(instance->dispatcher = view_dispatcher_alloc())) {
		messageError = "view_dispatcher_alloc() failed";
		goto freeInstance;
	}

	const AppSceneOnEnterCallback callbackEnter[] = {(const AppSceneOnEnterCallback) SceneHomeOnEnter};
	const AppSceneOnEventCallback callbackEvent[] = {(const AppSceneOnEventCallback) SceneHomeOnEvent};
	const AppSceneOnExitCallback callbackExit[] = {(const AppSceneOnExitCallback) SceneHomeOnExit};
	const SceneManagerHandlers handlers = {callbackEnter, callbackEvent, callbackExit, CUBERZERO_SCENE_COUNT};

	if(!(instance->manager = scene_manager_alloc(&handlers, instance))) {
		messageError = "scene_manager_alloc() failed";
		goto freeDispatcher;
	}

	Gui* const interface = furi_record_open(RECORD_GUI);

	if(!interface) {
		messageError = "furi_record_open(RECORD_GUI) failed";
		goto freeManager;
	}

	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	view_dispatcher_set_custom_event_callback(instance->dispatcher, (ViewDispatcherCustomEventCallback) callbackCustomEvent);
	view_dispatcher_set_navigation_event_callback(instance->dispatcher, (ViewDispatcherNavigationEventCallback) callbackNavigationEvent);
	messageError = NULL;
	SceneHomeInitialize(instance, &messageError);

	if(messageError) {
		goto closeInterface;
	}

	if(!instance->scene.home) {
		messageError = "SceneHomeInitialize() failed";
		goto closeInterface;
	}

	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_SCENE_HOME, instance->scene.home);
	view_dispatcher_attach_to_gui(instance->dispatcher, interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, CUBERZERO_SCENE_HOME);
	view_dispatcher_run(instance->dispatcher);
	SceneHomeFree(instance);
closeInterface:
	furi_record_close(RECORD_GUI);
freeManager:
	scene_manager_free(instance->manager);
freeDispatcher:
	view_dispatcher_free(instance->dispatcher);
freeInstance:
	free(instance);
functionExit:
	if(messageError) {
		CUBERZERO_LOG("Exiting with errors");
		__furi_crash(messageError);
		return 1;
	}

	CUBERZERO_LOG("Exiting");
	return 0;
}
