#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <applications/services/gui/view_dispatcher.h>
#include <applications/services/gui/scene_manager.h>

#ifndef __CUBERZERO_H__
#define __CUBERZERO_H__

#define CUBERZERO_TAG "CuberZero"

typedef struct {
	uint8_t cube;
} CUBERZEROSETTINGS, *PCUBERZEROSETTINGS;

typedef struct {
	CUBERZEROSETTINGS settings;
	struct {
		struct {
			uint8_t selectedItem;
		} home;
	} scene;

	Gui* interface;
	struct {
		Submenu* submenu;
		VariableItemList* variableList;
	} view;

	ViewPort* viewport;
	ViewDispatcher* dispatcher;
	SceneManager* manager;
} CUBERZERO, *PCUBERZERO;

typedef enum {
	WCA_3X3,
	WCA_2X2,
	WCA_4X4,
	WCA_5X5,
	WCA_6X6,
	WCA_7X7,
	WCA_3X3_BLINDFOLDED,
	WCA_3X3_FEWEST_MOVES,
	WCA_3X3_ONE_HANDED,
	WCA_CLOCK,
	WCA_MEGAMINX,
	WCA_PYRAMINX,
	WCA_SKEWB,
	WCA_SQUARE1,
	WCA_4X4_BLINDFOLDED,
	WCA_5X5_BLINDFOLDED,
	WCA_3X3_MULTIBLIND,
	COUNT_CUBERZEROCUBE
} CUBERZEROCUBE;

typedef enum {
	CUBERZERO_SCENE_ABOUT,
	CUBERZERO_SCENE_CUBE_SELECT,
	CUBERZERO_SCENE_HOME,
	CUBERZERO_SCENE_SETTINGS,
	CUBERZERO_SCENE_TIMER,
	COUNT_CUBERZEROSCENE
} CUBERZEROSCENE;

typedef enum {
	CUBERZERO_VIEW_SUBMENU,
	CUBERZERO_VIEW_VARIABLE_ITEM_LIST
} CUBERZEROVIEW;

#ifdef __cplusplus
extern "C" {
#endif
void CuberZeroSettingsLoad(const PCUBERZEROSETTINGS settings);
void CuberZeroSettingsSave(const PCUBERZEROSETTINGS settings);
void SceneAboutEnter(const PCUBERZERO instance);
bool SceneAboutEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneAboutExit(const PCUBERZERO instance);
void SceneCubeSelectEnter(const PCUBERZERO instance);
bool SceneCubeSelectEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneHomeEnter(const PCUBERZERO instance);
bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneSettingsEnter(const PCUBERZERO instance);
void SceneSettingsExit(const PCUBERZERO instance);
void SceneTimerEnter(const PCUBERZERO instance);
bool SceneTimerEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneTimerExit(const PCUBERZERO instance);
#ifdef __cplusplus
}
#endif
#endif
