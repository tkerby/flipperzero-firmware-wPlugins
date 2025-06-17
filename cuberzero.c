#include "cuberzero.h"

static bool callbackCustomEvent(const PCUBERZERO instance, const uint32_t event) {
	if(!instance) {
		return false;
	}

	return scene_manager_handle_custom_event(instance->manager, event);
}

static bool callbackNavigationEvent(const PCUBERZERO instance) {
	if(!instance) {
		return false;
	}

	return scene_manager_handle_back_event(instance->manager);
}

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	FURI_LOG_I(CUBERZERO_TAG, "Initializing");
	const char* messageError = NULL;
	const PCUBERZERO instance = malloc(sizeof(CUBERZERO));

	if(!instance) {
		messageError = "malloc() failed";
		goto functionExit;
	}

	memset(instance, 0, sizeof(CUBERZERO));
	CuberZeroSettingsLoad(&instance->settings);
	instance->scene.home.selectedItem = CUBERZERO_SCENE_TIMER;

	if(!(instance->interface = furi_record_open(RECORD_GUI))) {
		messageError = "furi_record_open(RECORD_GUI) failed";
		goto freeInstance;
	}

	if(!(instance->view.submenu = submenu_alloc())) {
		messageError = "submenu_alloc() failed";
		goto closeInterface;
	}

	if(!(instance->view.variableList = variable_item_list_alloc())) {
		messageError = "variable_item_list_alloc() failed";
		goto freeSubmenu;
	}

	if(!(instance->viewport = view_port_alloc())) {
		messageError = "view_port_alloc() failed";
		goto freeVariableList;
	}

	if(!(instance->dispatcher = view_dispatcher_alloc())) {
		messageError = "view_dispatcher_alloc() failed";
		goto freeViewport;
	}

	const AppSceneOnEnterCallback onEnter[] = {(AppSceneOnEnterCallback) SceneAboutEnter, (AppSceneOnEnterCallback) SceneCubeSelectEnter, (AppSceneOnEnterCallback) SceneHomeEnter, (AppSceneOnEnterCallback) SceneSettingsEnter, (AppSceneOnEnterCallback) SceneTimerEnter};
	const AppSceneOnEventCallback onEvent[] = {(AppSceneOnEventCallback) SceneAboutEvent, (AppSceneOnEventCallback) SceneCubeSelectEvent, (AppSceneOnEventCallback) SceneHomeEvent, (AppSceneOnEventCallback) SceneSettingsEvent, (AppSceneOnEventCallback) SceneTimerEvent};
	const AppSceneOnExitCallback onExit[] = {(AppSceneOnExitCallback) SceneAboutExit, (AppSceneOnExitCallback) SceneCubeSelectExit, (AppSceneOnExitCallback) SceneHomeExit, (AppSceneOnExitCallback) SceneSettingsExit, (AppSceneOnExitCallback) SceneTimerExit};
	const SceneManagerHandlers handlers = {onEnter, onEvent, onExit, CUBERZERO_SCENE_COUNT};

	if(!(instance->manager = scene_manager_alloc(&handlers, instance))) {
		messageError = "scene_manager_alloc() failed";
		goto freeDispatcher;
	}

	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	view_dispatcher_set_custom_event_callback(instance->dispatcher, (ViewDispatcherCustomEventCallback) callbackCustomEvent);
	view_dispatcher_set_navigation_event_callback(instance->dispatcher, (ViewDispatcherNavigationEventCallback) callbackNavigationEvent);
	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU, submenu_get_view(instance->view.submenu));
	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST, variable_item_list_get_view(instance->view.variableList));
	view_dispatcher_attach_to_gui(instance->dispatcher, instance->interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, CUBERZERO_SCENE_HOME);
	view_dispatcher_run(instance->dispatcher);
	view_dispatcher_remove_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
	view_dispatcher_remove_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST);
	scene_manager_free(instance->manager);
freeDispatcher:
	view_dispatcher_free(instance->dispatcher);
freeViewport:
	view_port_free(instance->viewport);
freeVariableList:
	variable_item_list_free(instance->view.variableList);
freeSubmenu:
	submenu_free(instance->view.submenu);
closeInterface:
	furi_record_close(RECORD_GUI);
freeInstance:
	CuberZeroSettingsSave(&instance->settings);
	free(instance);
functionExit:
	FURI_LOG_I(CUBERZERO_TAG, "Exiting");

	if(!messageError) {
		return 0;
	}

	FURI_LOG_E(CUBERZERO_TAG, "Error: %s", messageError);
	__furi_crash(messageError);
	return 1;
}
