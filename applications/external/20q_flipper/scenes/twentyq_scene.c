#include "twentyq_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const twentyq_scene_on_enter_handlers[])(void*) = {
#include "twentyq_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const twentyq_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "twentyq_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const twentyq_scene_on_exit_handlers[])(void* context) = {
#include "twentyq_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers twentyq_scene_handlers = {
    .on_enter_handlers = twentyq_scene_on_enter_handlers,
    .on_event_handlers = twentyq_scene_on_event_handlers,
    .on_exit_handlers = twentyq_scene_on_exit_handlers,
    .scene_num = TwentyQSceneCount,
};
