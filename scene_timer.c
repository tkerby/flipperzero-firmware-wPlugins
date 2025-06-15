#include "cuberzero.h"

static void callbackDraw(Canvas* const canvas, const void* const model) {
	UNUSED(model);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Hello, world!");
}

void SceneTimerEnter(const PCUBERZERO instance) {
	view_set_draw_callback(instance->view.view, (ViewDrawCallback) callbackDraw);
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VIEW);
}

bool SceneTimerEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

void SceneTimerExit(const PCUBERZERO instance) {
	UNUSED(instance);
}
