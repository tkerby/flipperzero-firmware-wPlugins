#include <furi.h>
#include <applications/services/gui/view_dispatcher.h>
#include <applications/services/gui/scene_manager.h>
#include <gui/modules/submenu.h>

#define CUBERZERO_TAG "CuberZero"

typedef struct {
	ViewDispatcher* dispatcher;
	SceneManager* manager;
	struct {
		Submenu* submenu;
	} view;
} CUBERZERO, *PCUBERZERO;

typedef enum {
	CUBERZERO_VIEW_SUBMENU
} CUBERZEROVIEW;

typedef enum {
	CUBERZERO_SCENE_HOME,
	CUBERZERO_SCENE_COUNT
} CUBERZEROSCENE;

void SceneHomeEnter(const PCUBERZERO instance);
bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneHomeExit(const PCUBERZERO instance);
