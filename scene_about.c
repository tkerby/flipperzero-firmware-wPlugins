#include "cuberzero.h"

void SceneAboutEnter(const PCUBERZERO instance) {
	if(!instance) {
		return;
	}

	widget_reset(instance->view.widget);
	widget_add_text_scroll_element(instance->view.widget, 0, 0, 128, 64, "\e#Cuber Zero " CUBERZERO_VERSION "\nWCA puzzles scrambler\nand timer!\n\nhttps://www.github.com/KHOPAN/Cuber-Zero");
	view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_WIDGET);
}
