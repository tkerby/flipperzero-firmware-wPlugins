#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <applications/services/gui/view_dispatcher.h>
#include <applications/services/gui/scene_manager.h>

#ifndef __CUBERZERO_H__
#define __CUBERZERO_H__

#define CUBERZERO_TAG     "CuberZero"
#define CUBERZERO_VERSION "0.0.1"

typedef struct {
    uint8_t cube;
} CUBERZEROSETTINGS, *PCUBERZEROSETTINGS;

typedef struct {
    CUBERZEROSETTINGS settings;
    struct {
        struct {
            uint8_t selectedItem;
        } home;

        struct {
            FuriMutex* mutex;
            FuriMessageQueue* queue;
            ViewPort* viewport;
            FuriTimer* timer;
            FuriString* string;
            uint32_t pressedTime;
            uint32_t startTimer;
            uint32_t stopTimer;
            uint8_t state;
            uint8_t previousState;
            uint8_t nextScene;
            uint32_t nextSceneIdentifier;
        } timer;
    } scene;

    Gui* interface;
    struct {
        Submenu* submenu;
        VariableItemList* variableList;
        Widget* widget;
    } view;

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
    CUBERZERO_VIEW_VARIABLE_ITEM_LIST,
    CUBERZERO_VIEW_WIDGET
} CUBERZEROVIEW;

#ifdef __cplusplus
extern "C" {
#endif
void CuberZeroSettingsLoad(const PCUBERZEROSETTINGS settings);
void CuberZeroSettingsSave(const PCUBERZEROSETTINGS settings);
void SceneAboutEnter(const PCUBERZERO instance);
void SceneCubeSelectEnter(const PCUBERZERO instance);
bool SceneCubeSelectEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneHomeEnter(const PCUBERZERO instance);
bool SceneHomeEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneSettingsEnter(const PCUBERZERO instance);
void SceneSettingsExit(const PCUBERZERO instance);
void SceneTimerTick(const PCUBERZERO instance);
void SceneTimerEnter(const PCUBERZERO instance);
void SceneTimerDraw(Canvas* const canvas, const PCUBERZERO instance);
void SceneTimerInput(const InputEvent* const event, const PCUBERZERO instance);
#ifdef __cplusplus
}
#endif
#endif
