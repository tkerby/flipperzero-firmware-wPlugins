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

void SceneTimerDraw(const Canvas* const canvas, const PCUBERZERO instance) {
	if(!canvas || !instance) {
		return;
	}
}

void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance) {
	if(!event || !instance) {
		return;
	}

	furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
}
