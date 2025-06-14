#include "cuberzero.h"

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	CUBERZERO_LOG("Initializing");
	const PCUBERZERO instance = malloc(sizeof(CUBERZERO));
	furi_check(instance, "malloc() failed");
	const SceneManagerHandlers handlers = {NULL, NULL, NULL, 0};
	instance->dispatcher = view_dispatcher_alloc();
	furi_check(instance->dispatcher, "view_dispatcher_alloc() failed");
	instance->manager = scene_manager_alloc(&handlers, instance);
	furi_check(instance->manager, "scene_manager_alloc() failed");
	// ------------
	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	//view_dispatcher_set_custom_event_callback(instance->dispatcher, NULL);
	//view_dispatcher_set_navigation_event_callback(instance->dispatcher, NULL);
	view_dispatcher_add_view(instance->dispatcher, 0, NULL);
	Gui* interface = furi_record_open(RECORD_GUI);
	furi_check(interface, "furi_record_open(RECORD_GUI) failed");
	view_dispatcher_attach_to_gui(instance->dispatcher, interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, 0);
	view_dispatcher_run(instance->dispatcher);
	// ------------
	furi_record_close(RECORD_GUI);
	view_dispatcher_free(instance->dispatcher);
	scene_manager_free(instance->manager);
	free(instance);
	CUBERZERO_LOG("Exiting");
	return 0;
}
