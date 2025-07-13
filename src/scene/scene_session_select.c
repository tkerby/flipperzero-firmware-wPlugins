#include "src/cuberzero.h"
#include <storage/storage.h>
#include <gui/modules/widget.h>

#define MAX_NAME_LENGTH 256

/*static void callbackItem(void* const context, const uint32_t index) {
	UNUSED(context);
	UNUSED(index);
}*/

void SceneSessionSelectEnter(void* const context) {
	furi_check(context);
	Widget* widget = widget_alloc();
	widget_add_text_box_element(widget, 10, 10, 50, 50, AlignCenter, AlignCenter, "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of \" de Finibus Bonorum et Malorum \" (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, \" Lorem ipsum dolor sit amet..\", comes from a line in section 1.10.32.", 1);
	view_dispatcher_add_view(((PCUBERZERO) context)->dispatcher, 10, widget_get_view(widget));
	view_dispatcher_switch_to_view(((PCUBERZERO) context)->dispatcher, 10);

	while(1);

	view_dispatcher_remove_view(((PCUBERZERO) context)->dispatcher, 10);
	widget_free(widget);
	/*if(!context) {
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
	furi_record_close(RECORD_STORAGE);*/
}
