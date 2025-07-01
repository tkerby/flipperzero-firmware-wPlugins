#include <furi_hal_light.h>
#include "cuberzero.h"
#include "scramble/puzzle.h"
#include <gui/elements.h>

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

	uint8_t update = 0;

	switch(instance->scene.timer.state) {
	case TIMER_STATE_WAIT_FOR_READY:
		furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);

		if(instance->scene.timer.state == TIMER_STATE_WAIT_FOR_READY && tick - instance->scene.timer.pressedTime >= 500) {
			instance->scene.timer.state = TIMER_STATE_READY;
			update = 1;
		}

		furi_mutex_release(instance->scene.timer.mutex);

		if(update) {
			updateLight(instance);
			view_port_update(instance->scene.timer.viewport);
		}

		break;
	case TIMER_STATE_TIMING:
		view_port_update(instance->scene.timer.viewport);
		break;
	}
}

void SceneTimerEnter(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);
	furi_mutex_release(instance->scene.timer.mutex);
	furi_message_queue_reset(instance->scene.timer.queue);
	view_dispatcher_stop(instance->dispatcher);
	gui_remove_view_port(instance->interface, instance->dispatcher->viewport);
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

	if(instance->scene.timer.nextScene) {
		scene_manager_next_scene(instance->manager, instance->scene.timer.nextSceneIdentifier);
	} else {
		scene_manager_handle_back_event(instance->manager);
	}

	view_dispatcher_run(instance->dispatcher);
}

void SceneTimerDraw(Canvas* const canvas, const PCUBERZERO instance) {
	if(!canvas || !instance) {
		return;
	}

	canvas_clear(canvas);
	uint32_t tick;
	uint32_t seconds;

	switch(instance->scene.timer.state) {
	case TIMER_STATE_DEFAULT:
	case TIMER_STATE_WAIT_FOR_READY:
		canvas_set_font(canvas, FontSecondary);
		canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "F2 R2 F R' F' R U' F U2");
		break;
	case TIMER_STATE_READY:
		canvas_set_font(canvas, FontPrimary);
		canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "READY!");
		break;
	case TIMER_STATE_TIMING:
	case TIMER_STATE_STOP:
	case TIMER_STATE_HALT:
		tick = (instance->scene.timer.state == TIMER_STATE_TIMING ? furi_get_tick() : instance->scene.timer.stopTimer) - instance->scene.timer.startTimer;
		seconds = tick / 1000;
		furi_string_printf(instance->scene.timer.string, "%lu:%02lu.%03lu", seconds / 60, seconds % 60, tick % 1000);
		canvas_set_font(canvas, FontBigNumbers);
		canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(instance->scene.timer.string));
		break;
	}
}

void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance) {
	const uint32_t tick = furi_get_tick();

	if(!event || !instance) {
		return;
	}

	furi_mutex_acquire(instance->scene.timer.mutex, FuriWaitForever);

	switch(instance->scene.timer.state) {
	case TIMER_STATE_DEFAULT:
		if(event->type == InputTypePress && event->key == InputKeyOk) {
			instance->scene.timer.state = TIMER_STATE_WAIT_FOR_READY;
			instance->scene.timer.pressedTime = tick;
		} else if(event->type == InputTypeShort && event->key == InputKeyBack) {
			instance->scene.timer.nextScene = 0;
			furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
		} else if(event->type == InputTypePress && event->key == InputKeyLeft) {
			instance->scene.timer.nextScene = 1;
			instance->scene.timer.nextSceneIdentifier = CUBERZERO_SCENE_ABOUT;
			furi_message_queue_put(instance->scene.timer.queue, event, FuriWaitForever);
		}

		break;
	case TIMER_STATE_WAIT_FOR_READY:
		if(event->type == InputTypeRelease && event->key == InputKeyOk) {
			instance->scene.timer.state = TIMER_STATE_DEFAULT;
		}

		break;
	case TIMER_STATE_READY:
		if(event->type == InputTypeRelease && event->key == InputKeyOk) {
			instance->scene.timer.state = TIMER_STATE_TIMING;
			instance->scene.timer.startTimer = tick;
		}

		break;
	case TIMER_STATE_TIMING:
		if(event->type == InputTypePress) {
			instance->scene.timer.state = TIMER_STATE_STOP;
			instance->scene.timer.stopTimer = tick;
		}

		break;
	case TIMER_STATE_STOP:
		if(event->type == InputTypeRelease) {
			instance->scene.timer.state = TIMER_STATE_HALT;
		}

		break;
	case TIMER_STATE_HALT:
		if(event->type == InputTypePress && event->key != InputKeyBack) {
			instance->scene.timer.state = TIMER_STATE_STOP;
		} else if(event->type == InputTypeShort && event->key == InputKeyBack) {
			instance->scene.timer.state = TIMER_STATE_DEFAULT;
		}

		break;
	}

	furi_mutex_release(instance->scene.timer.mutex);
	updateLight(instance);
	view_port_update(instance->scene.timer.viewport);
}
