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

	if(instance->scene.timer.waitForReady && furi_get_tick() - instance->scene.timer.pressedTime >= 500) {
		instance->scene.timer.ready = 1;
		furi_hal_light_set(LightRed, 0);
		furi_hal_light_set(LightGreen, 255);
		furi_hal_light_set(LightBlue, 0);
	}

	/*furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);
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

	if(instance->scene.timer.state == TIMER_STATE_WAIT_FOR_READY && furi_get_tick() - instance->scene.timer.pressedTime >= 500) {
		instance->scene.timer.state = TIMER_STATE_READY;
	}

	furi_mutex_release(instance->scene.timer.mutex);*/
	//view_port_update(instance->scene.timer.viewport);
}

void SceneTimerEnter(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);
	furi_mutex_release(instance->scene.timer.mutex);
	view_dispatcher_stop(instance->dispatcher);
	gui_remove_view_port(instance->interface, instance->dispatcher->viewport);
	furi_message_queue_reset(instance->scene.timer.queue);
	//instance->scene.timer.state = TIMER_STATE_DEFAULT;
	gui_add_view_port(instance->interface, instance->scene.timer.viewport, GuiLayerFullscreen);
	furi_timer_start(instance->scene.timer.timer, 1);
	const InputEvent* event;

	while(furi_message_queue_get(instance->scene.timer.queue, &event, FuriWaitForever) == FuriStatusOk) {
		break;
	}

	furi_timer_stop(instance->scene.timer.timer);
	furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);
	furi_mutex_release(instance->scene.timer.mutex);
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
	canvas_set_font(canvas, FontBigNumbers);
	uint32_t tick = furi_get_tick();
	uint32_t seconds = tick / 1000;
	furi_string_printf(instance->scene.timer.string, "%lu:%02lu.%03lu", seconds / 60, seconds % 60, tick % 1000);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(instance->scene.timer.string));
}

void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance) {
	const uint32_t tick = furi_get_tick();

	if(!event || !instance) {
		return;
	}

	/*if(event->key == InputKeyBack && event->type == InputTypeShort && instance->scene.timer.state == TIMER_STATE_DEFAULT) {
		furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
	}*/

	/*furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);

	if(instance->scene.timer.state == TIMER_STATE_TIMING && event->type == InputTypePress) {
		instance->scene.timer.state = TIMER_STATE_DEFAULT;
		instance->scene.timer.stopTime = tick;
		goto functionExit;
	}

	if(event->key != InputKeyOk) {
		goto functionExit;
	}

	switch(event->type) {
	case InputTypePress:
		if(instance->scene.timer.state == TIMER_STATE_DEFAULT) {
			instance->scene.timer.state = TIMER_STATE_WAIT_FOR_READY;
			instance->scene.timer.pressedTime = tick;
		}

		break;
	case InputTypeRelease:
		if(instance->scene.timer.state == TIMER_STATE_READY) {
			instance->scene.timer.state = TIMER_STATE_TIMING;
		}

		break;
	default:
		break;
	}
functionExit:
	furi_mutex_release(instance->scene.timer.mutex);*/

	if(event->key == InputKeyOk) {
		if(!instance->scene.timer.ready) {
			if(event->type == InputTypePress) {
				instance->scene.timer.pressedTime = tick;
				instance->scene.timer.waitForReady = 1;
				furi_hal_light_set(LightRed, 255);
				furi_hal_light_set(LightGreen, 0);
				furi_hal_light_set(LightBlue, 0);
			} else if(event->type == InputTypeRelease) {
				instance->scene.timer.waitForReady = 0;
				furi_hal_light_set(LightRed, 0);
				furi_hal_light_set(LightGreen, 0);
				furi_hal_light_set(LightBlue, 0);
			}
		} else {
			if(event->type == InputTypeRelease) {
				instance->scene.timer.waitForReady = 0;
				instance->scene.timer.ready = 0;
				furi_hal_light_set(LightRed, 0);
				furi_hal_light_set(LightGreen, 0);
				furi_hal_light_set(LightBlue, 0);
			}
		}
	}

	if(event->key == InputKeyBack) {
		furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
	}
}
