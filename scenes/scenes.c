#include "scenes.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const flipper_spi_terminal_on_enter_handlers[])(void*) = {
#include "scenes_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const flipper_spi_terminal_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "scenes_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const flipper_spi_terminal_on_exit_handlers[])(void* context) = {
#include "scenes_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers flipper_spi_terminal_scene_handlers = {
    .on_enter_handlers = flipper_spi_terminal_on_enter_handlers,
    .on_event_handlers = flipper_spi_terminal_on_event_handlers,
    .on_exit_handlers = flipper_spi_terminal_on_exit_handlers,
    .scene_num = FlipperSPITerminalAppSceneNum,
};

void flipper_spi_terminal_scenes_alloc(FlipperSPITerminalApp* app) {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_alloc(app);
#include "scenes_config.h"
#undef ADD_SCENE
}

void flipper_spi_terminal_scenes_free(FlipperSPITerminalApp* app) {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_free(app);
#include "scenes_config.h"
#undef ADD_SCENE
}
