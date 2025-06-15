#include "cuberzero.h"
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>

typedef struct {
	ViewDispatcher* dispatcher;
	SceneManager* manager;
	struct {
		Submenu* submenu;
	} view;
} CUBERZERO, *PCUBERZERO;

typedef enum {
	CUBERZERO_VIEW_SUBMENU,
	CUBERZERO_VIEW_COUNT
} CUBERZEROVIEW;

typedef enum {
	CUBERZERO_SCENE_HOME,
	CUBERZERO_SCENE_COUNT
} CUBERZEROSCENE;

static void sceneHomeEnter(const PCUBERZERO instance) {
	submenu_reset(instance->view.submenu);
	submenu_add_item(instance->view.submenu, "Test 1", 0, NULL, instance);
	submenu_add_item(instance->view.submenu, "Pyraminx", 1, NULL, instance);
	submenu_add_item(instance->view.submenu, "Megaminx", 2, NULL, instance);
	submenu_add_item(instance->view.submenu, "3x3 Multi-Blindfolded", 3, NULL, instance);
	submenu_add_item(instance->view.submenu, "3x3 Blindfolded", 4, NULL, instance);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
}

static bool sceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

static void sceneHomeExit(const PCUBERZERO instance) {
	UNUSED(instance);
}

static const AppSceneOnEnterCallback onEnters[] = {
	(AppSceneOnEnterCallback) sceneHomeEnter};

static const AppSceneOnEventCallback onEvents[] = {
	(AppSceneOnEventCallback) sceneHomeEvent};

static const AppSceneOnExitCallback onExits[] = {
	(AppSceneOnExitCallback) sceneHomeExit};

static const SceneManagerHandlers sceneHandlers = {
	.on_enter_handlers = onEnters,
	.on_event_handlers = onEvents,
	.on_exit_handlers = onExits,
	.scene_num = CUBERZERO_SCENE_COUNT};

static bool callbackCustomEvent(const PCUBERZERO instance, const uint32_t event) {
	return scene_manager_handle_custom_event(instance->manager, event);
}

static bool callbackNavigationEvent(const PCUBERZERO instance) {
	return scene_manager_handle_back_event(instance->manager);
}

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	PCUBERZERO instance = malloc(sizeof(CUBERZERO));
	instance->manager = scene_manager_alloc(&sceneHandlers, instance);
	instance->dispatcher = view_dispatcher_alloc();
	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	view_dispatcher_set_custom_event_callback(instance->dispatcher, (ViewDispatcherCustomEventCallback) callbackCustomEvent);
	view_dispatcher_set_navigation_event_callback(instance->dispatcher, (ViewDispatcherNavigationEventCallback) callbackNavigationEvent);
	instance->view.submenu = submenu_alloc();
	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU, submenu_get_view(instance->view.submenu));
	Gui* interface = furi_record_open(RECORD_GUI);
	view_dispatcher_attach_to_gui(instance->dispatcher, interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, CUBERZERO_SCENE_HOME);
	view_dispatcher_run(instance->dispatcher);

	view_dispatcher_remove_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
	scene_manager_free(instance->manager);
	view_dispatcher_free(instance->dispatcher);
	submenu_free(instance->view.submenu);
	furi_record_close(RECORD_GUI);
	free(instance);
	return 0;
}
