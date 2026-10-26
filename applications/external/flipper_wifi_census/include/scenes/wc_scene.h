#pragma once

#include <gui/scene_manager.h>

typedef enum {
#define ADD_SCENE(prefix, name, id) WcScene##id,
#include "include/scenes/wc_scene_config.h"
#undef ADD_SCENE
    WcSceneCount,
} WcScene;

extern const SceneManagerHandlers wc_scene_handlers;

// Per-scene handler prototypes (generated from the scene list).
#define ADD_SCENE(prefix, name, id)                                                                \
    void prefix##_scene_##name##_on_enter(void *context);                                          \
    bool prefix##_scene_##name##_on_event(void *context, SceneManagerEvent event);                 \
    void prefix##_scene_##name##_on_exit(void *context);
#include "include/scenes/wc_scene_config.h"
#undef ADD_SCENE
