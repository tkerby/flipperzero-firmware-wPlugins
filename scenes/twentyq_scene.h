#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) TwentyQScene##id,
typedef enum {
#include "twentyq_scene_config.h"
    TwentyQSceneCount,
} TwentyQScene;
#undef ADD_SCENE

extern const SceneManagerHandlers twentyq_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                  \
    void prefix##_scene_##name##_on_enter(void*);                    \
    bool prefix##_scene_##name##_on_event(void*, SceneManagerEvent); \
    void prefix##_scene_##name##_on_exit(void*);
#include "twentyq_scene_config.h"
#undef ADD_SCENE
