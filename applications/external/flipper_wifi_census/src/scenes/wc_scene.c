#include "include/scenes/wc_scene.h"

static void (*const wc_on_enter_handlers[])(void *) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
#include "include/scenes/wc_scene_config.h"
#undef ADD_SCENE
};

static bool (*const wc_on_event_handlers[])(void *, SceneManagerEvent) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
#include "include/scenes/wc_scene_config.h"
#undef ADD_SCENE
};

static void (*const wc_on_exit_handlers[])(void *) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
#include "include/scenes/wc_scene_config.h"
#undef ADD_SCENE
};

const SceneManagerHandlers wc_scene_handlers = {
    .on_enter_handlers = wc_on_enter_handlers,
    .on_event_handlers = wc_on_event_handlers,
    .on_exit_handlers = wc_on_exit_handlers,
    .scene_num = WcSceneCount,
};
