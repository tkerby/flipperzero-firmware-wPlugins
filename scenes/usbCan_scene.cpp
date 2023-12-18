#include "usbCan_scene.hpp"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const usb_can_scene_on_enter_handlers[])(void*) = {
#include "usbCan_scene_config.hpp"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const usb_can_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "usbCan_scene_config.hpp"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const usb_can_scene_on_exit_handlers[])(void* context) = {
#include "usbCan_scene_config.hpp"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers usb_can_scene_handlers = {
    .on_enter_handlers = usb_can_scene_on_enter_handlers,
    .on_event_handlers = usb_can_scene_on_event_handlers,
    .on_exit_handlers = usb_can_scene_on_exit_handlers,
    .scene_num = usb_canSceneNum,
};
