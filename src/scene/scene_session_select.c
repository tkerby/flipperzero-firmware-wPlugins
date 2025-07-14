#include "src/cuberzero.h"
#include <gui/elements.h>

#define TEXT_SELECT "Select"
#define TEXT_NEW	"New"
#define TEXT_DELETE "Delete"

typedef struct {
	PCUBERZERO instance;
	FuriMessageQueue* queue;
} SESSIONSELECTSCENE, *PSESSIONSELECTSCENE;

enum SESSIONSELECTBUTTON {
	BUTTON_SELECT,
	BUTTON_NEW,
	BUTTPN_DELETE
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

static void callbackDraw(Canvas* const canvas, void* const context) {
	furi_check(canvas);
	UNUSED(context);
	canvas_clear(canvas);
	canvas_set_color(canvas, ColorBlack);
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 0, 8, "Current Session:");
	elements_text_box(canvas, 0, 11, 128, 39, AlignLeft, AlignTop, "3x3 Blindfolded Test 2025 at my home, this is a very long session name", 1);
	canvas_set_font(canvas, FontSecondary);
	drawButton(canvas, 10, 51, 1, "Select");
	drawButton(canvas, 52, 51, 0, "New");
	drawButton(canvas, 86, 51, 0, "Delete");
}

static void callbackInput(InputEvent* const event, void* const context) {
	if(!event || !context) {
		return;
	}

	if(event->type == InputTypeShort) {
		switch(event->key) {
		case InputKeyUp:
		case InputKeyLeft:
			break;
		case InputKeyDown:
		case InputKeyRight:
			break;
		case InputKeyOk:
			break;
		default:
			furi_message_queue_put(((PSESSIONSELECTSCENE) context)->queue, event, FuriWaitForever);
			break;
		}
	}
}

void SceneSessionSelectEnter(void* const context) {
	furi_check(context);
	const PSESSIONSELECTSCENE instance = malloc(sizeof(SESSIONSELECTSCENE));
	instance->instance = context;
	ViewPort* const viewport = view_port_alloc();
	instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
	view_port_draw_callback_set(viewport, callbackDraw, instance);
	view_port_input_callback_set(viewport, callbackInput, instance);
	view_dispatcher_stop(((PCUBERZERO) context)->dispatcher);
	gui_remove_view_port(((PCUBERZERO) context)->interface, ((PCUBERZERO) context)->dispatcher->viewport);
	gui_add_view_port(((PCUBERZERO) context)->interface, viewport, GuiLayerFullscreen);
	const InputEvent* event;

	while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
		break;
	}

	gui_remove_view_port(((PCUBERZERO) context)->interface, viewport);
	gui_add_view_port(((PCUBERZERO) context)->interface, ((PCUBERZERO) context)->dispatcher->viewport, GuiLayerFullscreen);
	furi_message_queue_free(instance->queue);
	view_port_free(viewport);
	free(instance);
	scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
	view_dispatcher_run(((PCUBERZERO) context)->dispatcher);
}
