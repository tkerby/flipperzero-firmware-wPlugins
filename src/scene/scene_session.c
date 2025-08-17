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
		uint8_t dialog : 1;
	};
} SESSIONSCENE, *PSESSIONSCENE;

enum ACTION {
	ACTION_EXIT,
	ACTION_SELECT
};

enum BUTTONSESSION {
	BUTTON_SESSION_SELECT,
	BUTTON_SESSION_RENAME,
	BUTTON_SESSION_NEW,
	BUTTON_SESSION_DELETE,
	COUNT_BUTTON_SESSION
};

static inline void renderButton(Canvas* const canvas, const uint8_t x, const uint8_t y, const uint8_t pressed, const char* const text) {
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

	if(!instance->dialog) {
		canvas_set_font(canvas, FontPrimary);
		canvas_draw_str(canvas, 0, 8, "Current Session:");
		elements_text_box(canvas, 0, 11, 128, 39, AlignLeft, AlignTop, "Session 8192", 1);
		renderButton(canvas, 2, 51, instance->button == BUTTON_SESSION_SELECT, "Select");
		renderButton(canvas, 33, 51, instance->button == BUTTON_SESSION_RENAME, "Rename");
		renderButton(canvas, 71, 51, instance->button == BUTTON_SESSION_NEW, "New");
		renderButton(canvas, 94, 51, instance->button == BUTTON_SESSION_DELETE, "Delete");
		return;
	}
}

static inline void handleKeyOk(const PSESSIONSCENE instance) {
	switch(instance->button) {
	case BUTTON_SESSION_SELECT:
		break;
	case BUTTON_SESSION_RENAME:
		break;
	case BUTTON_SESSION_NEW:
		break;
	case BUTTON_SESSION_DELETE:
		break;
	}
}

static inline void handleKeyBack(const PSESSIONSCENE instance, const InputEvent* const event) {
	if(instance->dialog) {
		return;
	}

	instance->action = ACTION_EXIT;
	furi_message_queue_put(instance->queue, event, FuriWaitForever);
}

static void callbackInput(InputEvent* const event, void* const context) {
	furi_check(event && context);

	if(event->type != InputTypeShort) {
		return;
	}

	const PSESSIONSCENE instance = context;

	switch(event->key) {
	case InputKeyUp:
	case InputKeyRight:
		instance->button = (instance->button + 1) % COUNT_BUTTON_SESSION; //(instance->dialog ? COUNT_BUTTON_TEXT : COUNT_BUTTON_SESSION);
		goto updateViewport;
	case InputKeyDown:
	case InputKeyLeft:
		instance->button = (instance->button ? instance->button : COUNT_BUTTON_SESSION) - 1; //(instance->dialog ? COUNT_BUTTON_TEXT : COUNT_BUTTON_SESSION)) - 1;
		goto updateViewport;
	case InputKeyOk:
		handleKeyOk(instance);
		return;
	case InputKeyBack:
		handleKeyBack(instance, event);
		return;
	default:
		return;
	}
updateViewport:
	view_port_update(instance->viewport);
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
		case ACTION_SELECT:
			break;
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
