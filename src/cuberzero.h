#ifndef __CUBERZERO_H__
#define __CUBERZERO_H__

#include <furi.h>
#include <gui/modules/submenu.h>
#include <applications/services/gui/gui.h>
#include <applications/services/gui/view_dispatcher.h>
#include <applications/services/gui/scene_manager.h>

#define CUBERZERO_TAG	  "CuberZero"
#define CUBERZERO_VERSION "0.0.1"

#define CUBERZERO_INFO(text, ...)  furi_log_print_format(FuriLogLevelInfo, CUBERZERO_TAG, text, ##__VA_ARGS__)
#define CUBERZERO_ERROR(text, ...) furi_log_print_format(FuriLogLevelError, CUBERZERO_TAG, text, ##__VA_ARGS__)

typedef struct {
	struct {
		Submenu* submenu;
	} view;

	Gui* interface;
	ViewDispatcher* dispatcher;
	SceneManager* manager;
} CUBERZERO, *PCUBERZERO;

typedef enum {
	VIEW_SUBMENU
} VIEW;

#endif
