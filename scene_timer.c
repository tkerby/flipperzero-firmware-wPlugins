#include <furi_hal_light.h>
#include "cuberzero.h"
#include "scramble/puzzle.h"

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
	TIMER_STATE_TIMING,
	TIMER_STATE_STOP,
	TIMER_STATE_HALT
};

static void updateLight(const PCUBERZERO instance) {
	if(instance->scene.timer.previousState == instance->scene.timer.state) {
		return;
	}

	switch(instance->scene.timer.previousState = instance->scene.timer.state) {
	case TIMER_STATE_DEFAULT:
	case TIMER_STATE_HALT:
		furi_hal_light_set(LightRed, 0);
		furi_hal_light_set(LightGreen, 0);
		furi_hal_light_set(LightBlue, 0);
		break;
	case TIMER_STATE_WAIT_FOR_READY:
	case TIMER_STATE_STOP:
		furi_hal_light_set(LightRed, 255);
		furi_hal_light_set(LightGreen, 0);
		furi_hal_light_set(LightBlue, 0);
		break;
	case TIMER_STATE_READY:
		furi_hal_light_set(LightRed, 0);
		furi_hal_light_set(LightGreen, 255);
		furi_hal_light_set(LightBlue, 0);
		break;
	case TIMER_STATE_TIMING:
		furi_hal_light_blink_start(LightRed | LightGreen, 255, 25, 50);
		return;
	}

	furi_hal_light_blink_stop();
}

void SceneTimerTick(const PCUBERZERO instance) {
	const uint32_t tick = furi_get_tick();

	if(!instance) {
		return;
	}

	struct {
		uint8_t light	 : 1;
		uint8_t viewport : 1;
	} update;

	*((uint8_t*) &update) = 0;

	switch(instance->scene.timer.state) {
	case TIMER_STATE_WAIT_FOR_READY:
		furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);

		if(instance->scene.timer.state == TIMER_STATE_WAIT_FOR_READY && tick - instance->scene.timer.pressedTime >= 500) {
			instance->scene.timer.state = TIMER_STATE_READY;
			update.light = 1;
			update.viewport = 1;
		}

		furi_mutex_release(instance->scene.timer.mutex);
		break;
	case TIMER_STATE_TIMING:
		update.viewport = 1;
		break;
	}

	if(update.light) {
		updateLight(instance);
	}

	if(update.viewport) {
		view_port_update(instance->scene.timer.viewport);
	}
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

static void renderScramble(Canvas* const canvas) {
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "F2 R2 F R' F' R U' F U2");
}

void SceneTimerDraw(Canvas* const canvas, const PCUBERZERO instance) {
	if(!canvas || !instance) {
		return;
	}

	canvas_clear(canvas);
	renderScramble(canvas);
}

void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance) {
	const uint32_t tick = furi_get_tick();

	if(!event || !instance) {
		return;
	}

	furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);

	if(event->type == InputTypePress) {
		if(instance->scene.timer.state == TIMER_STATE_TIMING) {
			instance->scene.timer.state = TIMER_STATE_STOP;
			instance->scene.timer.stopTimer = tick;
		} else {
			if(event->key == InputKeyOk) {
				switch(instance->scene.timer.state) {
				case TIMER_STATE_DEFAULT:
					instance->scene.timer.state = TIMER_STATE_WAIT_FOR_READY;
					instance->scene.timer.pressedTime = tick;
					break;
				case TIMER_STATE_WAIT_FOR_READY:
					break;
				case TIMER_STATE_READY:
					break;
				case TIMER_STATE_TIMING:
					break;
				case TIMER_STATE_STOP:
					break;
				}
			}
		}
	} else if(event->type == InputTypeRelease) {
		if(instance->scene.timer.state == TIMER_STATE_STOP) {
			instance->scene.timer.state = TIMER_STATE_DEFAULT;
		} else {
			if(event->key == InputKeyOk) {
				switch(instance->scene.timer.state) {
				case TIMER_STATE_DEFAULT:
					break;
				case TIMER_STATE_WAIT_FOR_READY:
					instance->scene.timer.state = TIMER_STATE_DEFAULT;
					break;
				case TIMER_STATE_READY:
					instance->scene.timer.state = TIMER_STATE_TIMING;
					instance->scene.timer.startTimer = tick;
					break;
				case TIMER_STATE_TIMING:
					break;
				case TIMER_STATE_STOP:
					break;
				}
			}
		}
	}

	furi_mutex_release(instance->scene.timer.mutex);
	updateLight(instance);

	if(event->key == InputKeyBack) {
		furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
	}
}
