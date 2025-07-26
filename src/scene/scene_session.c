#include "src/cuberzero.h"
#include <gui/elements.h>
#include <dialogs/dialogs.h>

typedef enum {
	ACTION_SELECT,
	ACTION_EXIT
} ACTION;

typedef struct {
	PCUBERZERO instance;
	ViewPort* viewport;
	FuriMessageQueue* queue;
	uint8_t selectedButton;
	ACTION action;
} SESSIONSELECTSCENE, *PSESSIONSELECTSCENE;

enum SESSIONSELECTSCREEN {
	SCREEN_VIEW_SESSION,
	SCREEN_CONFIRM_DELETE
};

enum SESSIONSELECTBUTTON {
	BUTTON_SELECT,
	BUTTON_NEW,
	BUTTON_DELETE,
	COUNT_BUTTON
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
	furi_check(canvas);
	furi_check(context);
	const PSESSIONSELECTSCENE instance = context;
	canvas_clear(canvas);
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 0, 8, "Current Session:");
	elements_text_box(canvas, 0, 11, 128, 39, AlignLeft, AlignTop, "Ahhhhhhhhhhhhhhhhh, long tttttttttteeeeeeeeeexxxxxxxxxxtttttttttt", 1);
	canvas_set_font(canvas, FontSecondary);
	drawButton(canvas, 10, 51, instance->selectedButton == BUTTON_SELECT, "Select");
	drawButton(canvas, 52, 51, instance->selectedButton == BUTTON_NEW, "New");
	drawButton(canvas, 86, 51, instance->selectedButton == BUTTON_DELETE, "Delete");
}

static void callbackInput(InputEvent* const event, void* const context) {
	furi_check(event);
	furi_check(context);

	if(event->type != InputTypeShort) {
		return;
	}

	const PSESSIONSELECTSCENE instance = context;

	switch(event->key) {
	case InputKeyUp:
	case InputKeyRight:
		instance->selectedButton = (instance->selectedButton + 1) % COUNT_BUTTON;
		view_port_update(instance->viewport);
		break;
	case InputKeyDown:
	case InputKeyLeft:
		instance->selectedButton = (instance->selectedButton ? instance->selectedButton : COUNT_BUTTON) - 1;
		view_port_update(instance->viewport);
		break;
	case InputKeyOk:
		switch(instance->selectedButton) {
		case BUTTON_SELECT:
			instance->action = ACTION_SELECT;
			furi_message_queue_put(instance->queue, event, FuriWaitForever);
			break;
		}

		break;
	default:
		instance->action = ACTION_EXIT;
		furi_message_queue_put(instance->queue, event, FuriWaitForever);
		break;
	}
}

void SceneSessionSelectEnter(void* const context) {
	furi_check(context);
	const PSESSIONSELECTSCENE instance = malloc(sizeof(SESSIONSELECTSCENE));
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
		case ACTION_SELECT: {
			DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
			FuriString* path = furi_string_alloc_set_str("");
			DialogsFileBrowserOptions options = {0};
			dialog_file_browser_show(dialogs, path, path, &options);
			furi_string_free(path);
			furi_record_close(RECORD_DIALOGS);
			break;
		}
		case ACTION_EXIT:
			goto exit;
		}
	}

exit:
	gui_remove_view_port(instance->instance->interface, instance->viewport);
	gui_add_view_port(instance->instance->interface, instance->instance->dispatcher->viewport, GuiLayerFullscreen);
	furi_message_queue_free(instance->queue);
	view_port_free(instance->viewport);
	scene_manager_handle_back_event(instance->instance->manager);
	free(instance);
}
