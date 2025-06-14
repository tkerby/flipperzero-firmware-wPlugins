#include <furi.h>
#include <applications/services/gui/scene_manager.h>
#include <applications/services/gui/view_dispatcher.h>

#define CUBERZERO_LOG(message) FURI_LOG_I("CuberZero", message)

typedef struct {
	SceneManager* manager;
	ViewDispatcher* dispatcher;
} CUBERZERO, *PCUBERZERO;
