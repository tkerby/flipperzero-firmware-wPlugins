#include "../firestring.h"

/** indicies for menu items*/
typedef enum {
    FireStringStepTwoSelection_USB,
    // FireStringStepTwoSelection_BT        // TODO
    // FireStringStepTwoSelection_NFC,      // TODO?
    // FireStringStepTwoSelection_RFID,     // TODO?? Not sure if this has any uses?
    // FireStringStepTwoSelection_SUBGHZ    // TODO??? Is this getting too crazy?
    // FireStringStepTwoSelection_Infrared  // TODO???? Does this even make any sense to do?
    FireStringStepTwoSelection_Save,
    FireStringStepTwoSelection_Restart,
    FireStringStepTwoSelection_Exit
} FireStringStepTwoSelection;

void fire_string_menu_callback_step_two_menu(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "fire_string_menu_callback_step_two_menu");
    furi_check(context);

    FireString* app = context;

    switch(index) {
    case FireStringStepTwoSelection_USB:
        scene_manager_next_scene(app->scene_manager, FireStringScene_LoadingUSB);
        break;
    case FireStringStepTwoSelection_Save:
        break;
    case FireStringStepTwoSelection_Restart:
        furi_string_reset(app->fire_string);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FireStringScene_Generate);
        break;
    case FireStringStepTwoSelection_Exit:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }
}

void fire_string_scene_on_enter_step_two_menu(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_step_two_menu");
    furi_check(context);

    FireString* app = context;

    submenu_add_item(
        app->submenu,
        "Send through USB",
        FireStringStepTwoSelection_USB,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Save to internal storage",
        FireStringStepTwoSelection_Save,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Restart",
        FireStringStepTwoSelection_Restart,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Exit",
        FireStringStepTwoSelection_Exit,
        fire_string_menu_callback_step_two_menu,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_SubMenu);
}

/** main menu event handler - switches scene based on the event */
bool fire_string_scene_on_event_step_two_menu(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "fire_string_scene_on_event_step_two_menu");
    furi_check(context);

    FireString* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case FireStringEvent_ShowBadUSBPopup:
            scene_manager_next_scene(app->scene_manager, FireStringScene_LoadingUSB);
            consumed = true;
            break;
        case FireStringEvent_ShowSavedPopup: // DEBUG: popup, text_input or both?
            // scene_manager_next_scene(app->scene_manager, FireStringScene_Generate);
            consumed = true;
            break;
        case FireStringEvent_Exit:
            consumed = true;
            view_dispatcher_stop(app->view_dispatcher);
            break;
        default:
            break;
        }
        break;
    case SceneManagerEventTypeBack:
        consumed = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    default:
        break;
    }
    return consumed;
}

void fire_string_scene_on_exit_step_two_menu(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_step_two_menu");
    furi_check(context);

    FireString* app = context;
    submenu_reset(app->submenu);
}
