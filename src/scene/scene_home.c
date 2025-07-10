#include "src/cuberzero.h"

void SceneHomeEnter(void* const context) {
	if(!context) {
		return;
	}

	submenu_reset(((PCUBERZERO) context)->view.submenu);
	submenu_set_header(((PCUBERZERO) context)->view.submenu, "Cuber Zero");
	view_dispatcher_switch_to_view(((PCUBERZERO) context)->dispatcher, VIEW_SUBMENU);
}
