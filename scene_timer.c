#include "cuberzero.h"
#include <furi_hal_light.h>

struct ViewDispatcher {
	bool eventLoopOwned;
	FuriEventLoop* eventLoop;
	FuriMessageQueue* queueInput;
	FuriMessageQueue* queueEvent;
	Gui* interface;
	ViewPort* viewport;
};

enum TimerState {
	TIMER_STATE_DEFAULT,
	TIMER_STATE_WAIT_FOR_READY,
	TIMER_STATE_READY,
	TIMER_STATE_TIMING
};

void SceneTimerTick(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	furi_hal_light_set(LightBlue, 0);

	switch(instance->scene.timer.state) {
	case TIMER_STATE_WAIT_FOR_READY:
		furi_hal_light_set(LightRed, 255);
		furi_hal_light_set(LightGreen, 0);
		break;
	case TIMER_STATE_READY:
		furi_hal_light_set(LightRed, 0);
		furi_hal_light_set(LightGreen, 255);
		break;
	default:
		furi_hal_light_set(LightRed, 0);
		furi_hal_light_set(LightGreen, 0);
		break;
	}

	if(instance->scene.timer.state == TIMER_STATE_WAIT_FOR_READY) {
		instance->scene.timer.state = TIMER_STATE_READY;
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
	instance->scene.timer.state = TIMER_STATE_DEFAULT;
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

	canvas_clear(canvas);
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "F D L2 U2 D2 R' F' D' R U2 B R2 F U2 B U2 L2 D2");
}

void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance) {
	if(!event || !instance) {
		return;
	}

	if(event->key == InputKeyOk && event->type == InputTypePress) {
		if(instance->scene.timer.state == TIMER_STATE_DEFAULT) {
			instance->scene.timer.state = TIMER_STATE_WAIT_FOR_READY;
			return;
		}
	}

	if(event->key == InputKeyOk && event->type == InputTypeRelease) {
		if(instance->scene.timer.state == TIMER_STATE_READY) {
			instance->scene.timer.state = TIMER_STATE_TIMING;
			return;
		}
	}

	if(event->type == InputTypePress) {
		if(instance->scene.timer.state == TIMER_STATE_TIMING) {
			instance->scene.timer.state = TIMER_STATE_DEFAULT;
			return;
		}
	}

	if(event->key == InputKeyBack && event->type == InputTypeShort && instance->scene.timer.state == TIMER_STATE_DEFAULT) {
		furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
	}
}
