#include "cuberzero.h"

static bool callbackEmptyEvent(void* const context, const SceneManagerEvent event) {
	UNUSED(context);
	UNUSED(event);
	return 0;
}

static void callbackEmptyExit(void* const context) {
	UNUSED(context);
}

static bool callbackCustomEvent(void* const context, const uint32_t event) {
	if(!context) {
		return 0;
	}

	return scene_manager_handle_custom_event(((PCUBERZERO) context)->manager, event);
}

static bool callbackNavigationEvent(void* const context) {
	if(!context) {
		return 0;
	}

	return scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
}

int32_t cuberzeroMain(const void* const unused) {
	UNUSED(unused);
	CUBERZERO_INFO("Initializing");
	const char* message = 0;
	const PCUBERZERO instance = malloc(sizeof(CUBERZERO));

	if(!instance) {
		message = "malloc() failed";
		goto functionExit;
	}

	memset(instance, 0, sizeof(CUBERZERO));

	if(!(instance->view.submenu = submenu_alloc())) {
		message = "submenu_alloc() failed";
		goto freeInstance;
	}

	if(!(instance->interface = furi_record_open(RECORD_GUI))) {
		message = "furi_record_open(RECORD_GUI) failed";
		goto freeSubmenu;
	}

	if(!(instance->dispatcher = view_dispatcher_alloc())) {
		message = "view_dispatcher_alloc() failed";
		goto freeInterface;
	}
    view_dispatcher_enable_queue(instance->dispatcher);

	const AppSceneOnEnterCallback handlerEnter[] = {SceneHomeEnter, SceneTimerEnter};
	const AppSceneOnEventCallback handlerEvent[] = {SceneHomeEvent, callbackEmptyEvent};
	const AppSceneOnExitCallback handlerExit[] = {callbackEmptyExit, callbackEmptyExit};
	const SceneManagerHandlers handlers = {handlerEnter, handlerEvent, handlerExit, COUNT_SCENE};

	if(!(instance->manager = scene_manager_alloc(&handlers, instance))) {
		message = "scene_manager_alloc() failed";
		goto freeDispatcher;
	}

	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	view_dispatcher_set_custom_event_callback(instance->dispatcher, callbackCustomEvent);
	view_dispatcher_set_navigation_event_callback(instance->dispatcher, callbackNavigationEvent);
	view_dispatcher_add_view(instance->dispatcher, VIEW_SUBMENU, submenu_get_view(instance->view.submenu));
	view_dispatcher_attach_to_gui(instance->dispatcher, instance->interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, SCENE_HOME);
	view_dispatcher_run(instance->dispatcher);
	view_dispatcher_remove_view(instance->dispatcher, VIEW_SUBMENU);
	scene_manager_free(instance->manager);
freeDispatcher:
	view_dispatcher_free(instance->dispatcher);
freeInterface:
	furi_record_close(RECORD_GUI);
freeSubmenu:
	submenu_free(instance->view.submenu);
freeInstance:
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
