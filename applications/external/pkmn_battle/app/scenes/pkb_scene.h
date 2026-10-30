#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) PkbScene##id,
typedef enum {
#include "pkb_scene_config.h"
    PkbSceneNum,
} PkbScene;
#undef ADD_SCENE

extern const SceneManagerHandlers pkb_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "pkb_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "pkb_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "pkb_scene_config.h"
#undef ADD_SCENE
