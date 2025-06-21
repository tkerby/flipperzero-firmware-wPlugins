#include "cuberzero.h"

struct ViewDispatcher {
	bool eventLoopOwned;
	FuriEventLoop* eventLoop;
	FuriMessageQueue* queueInput;
	FuriMessageQueue* queueEvent;
	Gui* interface;
	ViewPort* viewport;
};

void SceneTimerTick(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	view_port_update(instance->scene.timer.viewport);
}

void SceneTimerEnter(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	view_dispatcher_stop(instance->dispatcher);
	gui_remove_view_port(instance->interface, instance->dispatcher->viewport);
	furi_message_queue_reset(instance->scene.timer.queue);
	gui_add_view_port(instance->interface, instance->scene.timer.viewport, GuiLayerFullscreen);
	furi_timer_start(instance->scene.timer.timer, 1);
	const InputEvent* event;

	while(furi_message_queue_get(instance->scene.timer.queue, &event, FuriWaitForever) == FuriStatusOk) {
		break;
	}

	furi_timer_stop(instance->scene.timer.timer);
	gui_remove_view_port(instance->interface, instance->scene.timer.viewport);
	gui_add_view_port(instance->interface, instance->dispatcher->viewport, GuiLayerFullscreen);
	scene_manager_handle_back_event(instance->manager);
	view_dispatcher_run(instance->dispatcher);
}

void SceneTimerDraw(Canvas* const canvas, const PCUBERZERO instance) {
	if(!canvas || !instance) {
		return;
	}

	uint32_t tick = furi_get_tick();
	uint32_t seconds = tick / 1000;
	furi_string_printf(instance->scene.timer.string, "%lu:%02lu.%03lu", seconds / 60, seconds % 60, tick % 1000);
	canvas_clear(canvas);
	canvas_set_font(canvas, FontBigNumbers);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(instance->scene.timer.string));
}

void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance) {
	if(!event || !instance) {
		return;
	}

	furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
}
