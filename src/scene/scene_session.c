#include "src/cuberzero.h"
#include <dialogs/dialogs.h>
#include <gui/elements.h>
#include <storage/storage.h>

typedef struct {
	PCUBERZERO instance;
	ViewPort* viewport;
	FuriMessageQueue* queue;
	struct {
		uint8_t action	   : 2;
		uint8_t button	   : 2;
		uint8_t renderText : 1;
		uint8_t text	   : 2;
	};
} SESSIONSCENE, *PSESSIONSCENE;

enum ACTION {
	ACTION_EXIT,
	ACTION_SELECT,
	ACTION_OPEN_ANYWAY,
	ACTION_DELETE
};

enum BUTTONSESSION {
	BUTTON_SESSION_SELECT,
	BUTTON_SESSION_NEW,
	BUTTON_SESSION_DELETE,
	COUNT_BUTTON_SESSION
};

enum BUTTONTEXT {
	BUTTON_TEXT_DELETE_SELECT,
	BUTTON_TEXT_CANCEL_OK_OPEN,
	COUNT_BUTTON_TEXT
};

enum TEXT {
	TEXT_NO_FILE_SELECTED,
	TEXT_APPEARS_INCORRECT_TYPE,
	TEXT_NOT_SESSION_FILE,
	TEXT_DELETE_CONFIRM
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

// [Select] [Ok]     No session file was selected.
// [Select] [Open]   The selected file does not\nappear to be a session file.\nOpen anyway?
// [Select] [Ok]     The selected file\nis not a session file.\nIt might be corrupted.
// [Delete] [Cancel] Are you sure you want to\ndelete the current session?
static void callbackRender(Canvas* const canvas, void* const context) {
	furi_check(canvas && context);
	const PSESSIONSCENE instance = context;
	canvas_clear(canvas);

	if(!instance->renderText) {
		canvas_set_font(canvas, FontPrimary);
		canvas_draw_str(canvas, 0, 8, "Current Session:");
		elements_text_box(canvas, 0, 11, 128, 39, AlignLeft, AlignTop, "Session 8192", 1);
		drawButton(canvas, 10, 51, instance->button == BUTTON_SESSION_SELECT, "Select");
		drawButton(canvas, 52, 51, instance->button == BUTTON_SESSION_NEW, "New");
		drawButton(canvas, 86, 51, instance->button == BUTTON_SESSION_DELETE, "Delete");
		return;
	}

	const char* text;
	uint8_t firstX;
	uint8_t secondX;
	const char* firstText;
	const char* secondText;

	switch(instance->text) {
	case TEXT_NO_FILE_SELECTED:
		text = "No session file was selected.";
		firstX = 27;
		secondX = 85;
		firstText = "Select";
		secondText = "Ok";
		break;
	case TEXT_APPEARS_INCORRECT_TYPE:
		text = "The selected file does not\nappear to be a session file.\nOpen anyway?";
		firstX = 23;
		secondX = 79;
		firstText = "Select";
		secondText = "Open";
		break;
	case TEXT_NOT_SESSION_FILE:
		text = "The selected file\nis not a session file.\nIt might be corrupted.";
		firstX = 27;
		secondX = 85;
		firstText = "Select";
		secondText = "Ok";
		break;
	case TEXT_DELETE_CONFIRM:
		text = "Are you sure you want to\ndelete the current session?";
		firstX = 21;
		secondX = 74;
		firstText = "Delete";
		secondText = "Cancel";
		break;
	default:
		return;
	}

	elements_text_box(canvas, 0, 0, 128, 51, AlignCenter, AlignCenter, text, 1);
	drawButton(canvas, firstX, 51, instance->button == BUTTON_TEXT_DELETE_SELECT, firstText);
	drawButton(canvas, secondX, 51, instance->button == BUTTON_TEXT_CANCEL_OK_OPEN, secondText);
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
		instance->button = (instance->button + 1) % (instance->renderText ? COUNT_BUTTON_TEXT : COUNT_BUTTON_SESSION);
		goto updateViewport;
	case InputKeyDown:
	case InputKeyLeft:
		instance->button = (instance->button ? instance->button : (instance->renderText ? COUNT_BUTTON_TEXT : COUNT_BUTTON_SESSION)) - 1;
		goto updateViewport;
	case InputKeyBack:
		if(instance->renderText) {
			instance->button = BUTTON_SESSION_SELECT;
			instance->renderText = 0;
			goto updateViewport;
		}

		instance->action = ACTION_EXIT;
		furi_message_queue_put(instance->queue, event, FuriWaitForever);
		return;
	default:
		if(event->key != InputKeyOk) {
			return;
		}

		break;
	}

	if(!instance->renderText) {
		switch(instance->button) {
		case BUTTON_SESSION_SELECT:
			instance->action = ACTION_SELECT;
			furi_message_queue_put(instance->queue, event, FuriWaitForever);
			return;
		case BUTTON_SESSION_NEW:
			return;
		case BUTTON_SESSION_DELETE:
			return;
		default:
			return;
		}
	}

	switch(instance->text) {
	case TEXT_NO_FILE_SELECTED:
	case TEXT_NOT_SESSION_FILE:
		switch(instance->button) {
		case BUTTON_TEXT_DELETE_SELECT:
			goto handleSelect;
		case BUTTON_TEXT_CANCEL_OK_OPEN:
			instance->button = BUTTON_SESSION_SELECT;
			instance->renderText = 0;
			goto updateViewport;
		default:
			return;
		}
	case TEXT_APPEARS_INCORRECT_TYPE:
		switch(instance->button) {
		case BUTTON_TEXT_DELETE_SELECT:
			goto handleSelect;
		case BUTTON_TEXT_CANCEL_OK_OPEN:
			instance->action = ACTION_OPEN_ANYWAY;
			furi_message_queue_put(instance->queue, event, FuriWaitForever);
			return;
		default:
			return;
		}
	case TEXT_DELETE_CONFIRM:
		switch(instance->button) {
		case BUTTON_TEXT_DELETE_SELECT:
			instance->action = ACTION_DELETE;
			furi_message_queue_put(instance->queue, event, FuriWaitForever);
			return;
		case BUTTON_TEXT_CANCEL_OK_OPEN:
			goto handleSelect;
		default:
			return;
		}
	default:
		return;
	}
handleSelect:
	instance->action = ACTION_SELECT;
	furi_message_queue_put(instance->queue, event, FuriWaitForever);
	return;
updateViewport:
	view_port_update(instance->viewport);
}

static void actionSelect(const PSESSIONSCENE instance, FuriString* const path) {
	furi_string_set_str(path, APP_DATA_PATH("sessions"));
	DialogsFileBrowserOptions options;
	memset(&options, 0, sizeof(DialogsFileBrowserOptions));
	options.skip_assets = dialog_file_browser_show(furi_record_open(RECORD_DIALOGS), path, path, &options);
	furi_record_close(RECORD_DIALOGS);

	if(!options.skip_assets) {
		instance->renderText = 1;
		instance->text = TEXT_NO_FILE_SELECTED;
		goto updateViewport;
	}

/*Storage* storage = furi_record_open(RECORD_STORAGE);
	File* file = storage_file_alloc(storage);
	instance->renderText = 1;

	if(!furi_string_end_withi_str(path, ".cbzs")) {
		instance->text = TEXT_APPEARS_INCORRECT_TYPE;
	}

	if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
		instance->text = TEXT_NOT_SESSION_FILE;
	}

	uint64_t size = storage_file_size(file);

	if(size < 4) {
		instance->text = TEXT_NOT_SESSION_FILE;
	}

	storage_file_close(file);
	storage_file_free(file);
	furi_record_close(RECORD_STORAGE);
	view_port_update(instance->viewport);*/
updateViewport:
	view_port_update(instance->viewport);
}

void SceneSessionEnter(void* const context) {
	furi_check(context);
	const PSESSIONSCENE instance = malloc(sizeof(SESSIONSCENE));
	instance->instance = context;
	instance->viewport = view_port_alloc();
	instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
	FuriString* const path = furi_string_alloc();
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
			actionSelect(instance, path);
			break;
		case ACTION_OPEN_ANYWAY:
			break;
		case ACTION_DELETE:
			break;
		}
	}
functionExit:
	gui_remove_view_port(instance->instance->interface, instance->viewport);
	gui_add_view_port(instance->instance->interface, instance->instance->dispatcher->viewport, GuiLayerFullscreen);
	furi_string_free(path);
	furi_message_queue_free(instance->queue);
	view_port_free(instance->viewport);
	free(instance);
	scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
}
