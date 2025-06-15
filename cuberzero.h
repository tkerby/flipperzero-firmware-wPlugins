#include <furi.h>
#include <applications/services/gui/view_dispatcher.h>
#include <applications/services/gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>

#define CUBERZERO_TAG "CuberZero"

typedef struct {
	ViewDispatcher* dispatcher;
	SceneManager* manager;
	struct {
		Submenu* submenu;
		VariableItemList* variableList;
	} view;

	struct {
		struct {
			uint32_t selectedItem;
		} home;
	} scene;
} CUBERZERO, *PCUBERZERO;

typedef enum {
	CUBERZERO_VIEW_SUBMENU,
	CUBERZERO_VIEW_VARIABLE_ITEM_LIST
} CUBERZEROVIEW;

typedef enum {
	CUBERZERO_SCENE_ABOUT,
	CUBERZERO_SCENE_HOME,
	CUBERZERO_SCENE_SETTINGS,
	CUBERZERO_SCENE_TIMER,
	CUBERZERO_SCENE_COUNT
} CUBERZEROSCENE;

void SceneAboutEnter(const PCUBERZERO instance);
bool SceneAboutEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneAboutExit(const PCUBERZERO instance);
void SceneHomeEnter(const PCUBERZERO instance);
bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneHomeExit(const PCUBERZERO instance);
void SceneSettingsEnter(const PCUBERZERO instance);
bool SceneSettingsEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneSettingsExit(const PCUBERZERO instance);
void SceneTimerEnter(const PCUBERZERO instance);
bool SceneTimerEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneTimerExit(const PCUBERZERO instance);
