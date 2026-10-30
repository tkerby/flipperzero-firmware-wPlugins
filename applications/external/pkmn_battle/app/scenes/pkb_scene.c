#include "pkb_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const pkb_on_enter_handlers[])(void*) = {
#include "pkb_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const pkb_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "pkb_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const pkb_on_exit_handlers[])(void* context) = {
#include "pkb_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers pkb_scene_handlers = {
    .on_enter_handlers = pkb_on_enter_handlers,
    .on_event_handlers = pkb_on_event_handlers,
    .on_exit_handlers = pkb_on_exit_handlers,
    .scene_num = PkbSceneNum,
};
