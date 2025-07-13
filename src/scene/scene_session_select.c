#include "src/cuberzero.h"
#include <storage/storage.h>

#define MAX_NAME_LENGTH 256

static void callbackItem(void* const context, const uint32_t index) {
	UNUSED(context);
	UNUSED(index);
}

void SceneSessionSelectEnter(void* const context) {
	if(!context) {
		return;
	}

	Storage* storage = furi_record_open(RECORD_STORAGE);
	furi_check(storage);
	File* file = storage_file_alloc(storage);
	storage_dir_open(file, APP_DATA_PATH("sessions"));
	FileInfo info;
	char name[MAX_NAME_LENGTH + 1];
	submenu_reset(((PCUBERZERO) context)->view.submenu);

	while(storage_dir_read(file, &info, name, MAX_NAME_LENGTH)) {
		submenu_add_item(((PCUBERZERO) context)->view.submenu, name, SCENE_TIMER, callbackItem, context);
	}

	view_dispatcher_switch_to_view(((PCUBERZERO) context)->dispatcher, VIEW_SUBMENU);
	storage_dir_close(file);
	storage_file_free(file);
	furi_record_close(RECORD_STORAGE);
}
