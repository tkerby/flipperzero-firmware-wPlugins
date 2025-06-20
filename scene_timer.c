#include "cuberzero.h"
#include <gui/elements.h>
#include <furi_hal_light.h>

struct ViewDispatcher {
	bool eventLoopOwned;
	FuriEventLoop* eventLoop;
	FuriMessageQueue* queueInput;
	FuriMessageQueue* queueEvent;
	Gui* interface;
	ViewPort* viewport;
};

/*static void callbackDraw(Canvas* const canvas, const void* const model) {
	UNUSED(model);
	FuriString* string = furi_string_alloc();
	uint32_t tick = furi_get_tick();
	uint32_t seconds = tick / 1000;
	furi_string_printf(string, "%lu:%02lu.%03lu", seconds / 60, seconds % 60, tick % 1000);
	canvas_set_font(canvas, FontBigNumbers);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(string));
	furi_string_free(string);
}*/

static uint32_t pressed;
static FuriTimer* timer;
static bool ready;
static volatile bool timing;
static volatile bool timed;
static volatile uint32_t startTime;
static volatile uint32_t endTime;

static void callbackTimer(void* const instance) {
	UNUSED(instance);
	furi_hal_light_set(LightRed, 0);
	furi_hal_light_set(LightGreen, 255);
	furi_hal_light_set(LightBlue, 0);
	ready = true;
}

static void callbackInput(const InputEvent* const event, const PCUBERZERO instance) {
	if(event->key != InputKeyOk) {
		furi_message_queue_put((FuriMessageQueue*) instance, event, 0);
		return;
	}

	if(event->type != InputTypePress && event->type != InputTypeRelease) {
		return;
	}

	if(event->type == InputTypePress) {
		pressed = furi_get_tick();
		ready = false;

		if(!timing) {
			furi_timer_start(timer, 500);
		} else {
			endTime = pressed;
		}

		timing = false;
		furi_hal_light_blink_stop();
		furi_hal_light_set(LightRed, 255);
		furi_hal_light_set(LightGreen, 0);
		furi_hal_light_set(LightBlue, 0);
		ready = true;
	} else {
		furi_timer_stop(timer);
		furi_hal_light_set(LightRed, 0);
		furi_hal_light_set(LightGreen, 0);
		furi_hal_light_set(LightBlue, 0);

		if(ready) {
			ready = false;
			timing = true;
			timed = true;
			startTime = furi_get_tick();
			furi_hal_light_blink_start(LightRed, 255, 25, 50);
		}
	}
}

static void callbackRender(Canvas* const canvas, const PCUBERZERO instance) {
	UNUSED(instance);
	FuriString* string = furi_string_alloc();
	uint32_t tick = timed ? timing ? furi_get_tick() - startTime : endTime - startTime : 0;
	uint32_t seconds = tick / 1000;
	furi_string_printf(string, "%lu:%02lu.%03lu", seconds / 60, seconds % 60, tick % 1000);
	canvas_set_font(canvas, FontBigNumbers);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(string));
	furi_string_free(string);
	canvas_set_font(canvas, FontSecondary);
	elements_button_left(canvas, "Exit");
	view_port_update(instance->viewport);
}

void SceneTimerEnter(const PCUBERZERO instance) {
	//view_set_draw_callback(instance->view.view, (ViewDrawCallback) callbackDraw);
	//view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VIEW);
	timer = furi_timer_alloc(callbackTimer, FuriTimerTypeOnce, NULL);
	view_dispatcher_stop(instance->dispatcher);
	gui_remove_view_port(instance->interface, instance->dispatcher->viewport);

	FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
	view_port_input_callback_set(instance->viewport, (ViewPortInputCallback) callbackInput, queue);
	view_port_draw_callback_set(instance->viewport, (ViewPortDrawCallback) callbackRender, instance);

	gui_add_view_port(instance->interface, instance->viewport, GuiLayerFullscreen);
	InputEvent event;

	while(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
		if(event.key == InputKeyBack) {
			break;
		}
	}

	furi_message_queue_free(queue);
	gui_remove_view_port(instance->interface, instance->viewport);
	gui_add_view_port(instance->interface, instance->dispatcher->viewport, GuiLayerFullscreen);
	scene_manager_handle_back_event(instance->manager);
	view_dispatcher_run(instance->dispatcher);
	furi_timer_free(timer);
}
