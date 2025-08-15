#include "src/cuberzero.h"
#include <dialogs/dialogs.h>
#include <gui/elements.h>

typedef struct {
	PCUBERZERO instance;
	ViewPort* viewport;
	FuriMessageQueue* queue;
	struct {
		uint8_t action : 1;
		uint8_t button : 2;
	};
} SESSIONSCENE, *PSESSIONSCENE;

enum ACTION {
	ACTION_EXIT
};

enum BUTTONSESSION {
	BUTTON_SESSION_SELECT,
	BUTTON_SESSION_RENAME,
	BUTTON_SESSION_NEW,
	BUTTON_SESSION_DELETE,
	COUNT_BUTTON_SESSION
};

static inline void drawButton(Canvas* const canvas, const uint8_t x, const uint8_t y, const uint8_t pressed, const char* const text) {
	const uint16_t width = canvas_string_width(canvas, text);
	canvas_set_color(canvas, ColorBlack);
	canvas_draw_line(canvas, x + 1, y, x + width + 4, y);
	canvas_draw_line(canvas, x + 1, y + 12, x + width + 4, y + 12);
	canvas_draw_line(canvas, x, y + 1, x, y + 11);
	canvas_draw_line(canvas, x + width + 5, y + 1, x + width + 5, y + 11);

	if(pressed) {
		canvas_draw_box(canvas, x + 1, y + 1, width + 4, 11);
		canvas_set_color(canvas, ColorWhite);
	}

	canvas_draw_str(canvas, x + 3, y + 10, text);
}

static void callbackRender(Canvas* const canvas, void* const context) {
	furi_check(canvas && context);
	const PSESSIONSCENE instance = context;
	canvas_clear(canvas);

	if(1) {
		canvas_set_font(canvas, FontPrimary);
		canvas_draw_str(canvas, 0, 8, "Current Session:");
		elements_text_box(canvas, 0, 11, 128, 39, AlignLeft, AlignTop, "Session 8192", 1);
		drawButton(canvas, 2, 51, instance->button == BUTTON_SESSION_SELECT, "Select");
		drawButton(canvas, 33, 51, instance->button == BUTTON_SESSION_RENAME, "Rename");
		drawButton(canvas, 71, 51, instance->button == BUTTON_SESSION_NEW, "New");
		drawButton(canvas, 94, 51, instance->button == BUTTON_SESSION_DELETE, "Delete");
		return;
	}
}

static void callbackInput(InputEvent* const event, void* const context) {
	UNUSED(event);
	UNUSED(context);
}

void SceneSessionEnter(void* const context) {
	furi_check(context);
	const PSESSIONSCENE instance = malloc(sizeof(SESSIONSCENE));
	instance->instance = context;
	instance->viewport = view_port_alloc();
	instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
	view_port_draw_callback_set(instance->viewport, callbackRender, instance);
	view_port_input_callback_set(instance->viewport, callbackInput, instance);
	gui_remove_view_port(instance->instance->interface, instance->instance->dispatcher->viewport);
	gui_add_view_port(instance->instance->interface, instance->viewport, GuiLayerFullscreen);
	const InputEvent* event;

	while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
		switch(instance->action) {
		case ACTION_EXIT:
			goto functionExit;
		}
	}
functionExit:
	gui_remove_view_port(instance->instance->interface, instance->viewport);
	gui_add_view_port(instance->instance->interface, instance->instance->dispatcher->viewport, GuiLayerFullscreen);
	furi_message_queue_free(instance->queue);
	view_port_free(instance->viewport);
	free(instance);
	scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
}
