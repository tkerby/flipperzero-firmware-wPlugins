#include <furi.h>
#include <applications/services/gui/view_dispatcher.h>
#include <applications/services/gui/scene_manager.h>

#define CUBERZERO_LOG(message) FURI_LOG_I("CuberZero", message)

typedef struct {
	ViewDispatcher* dispatcher;
	SceneManager* manager;
	struct {
		View* home;
	} scene;
} CUBERZERO, *PCUBERZERO;

typedef enum {
	CUBERZERO_SCENE_HOME,
	CUBERZERO_SCENE_COUNT
} CUBERZEROSCENE;

void SceneHomeInitialize(const PCUBERZERO instance, char* const* const message);
void SceneHomeOnEnter(const PCUBERZERO instance);
bool SceneHomeOnEvent(const PCUBERZERO instance, const SceneManagerEvent event);
void SceneHomeOnExit(const PCUBERZERO instance);
void SceneHomeFree(const PCUBERZERO instance);
