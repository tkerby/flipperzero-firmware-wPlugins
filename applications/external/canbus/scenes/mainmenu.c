#include "../app_user.h"

// This variable works to get the value of the selector and set it when the user
// enter at the Scene
static uint8_t menu_selector = 0;

// Function to display init
void draw_start(App* app) {
    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignCenter, FontPrimary, "CANBUS APP");
    widget_add_string_element(
        app->widget, 64, 55, AlignCenter, AlignCenter, FontSecondary, "Electronic Cats");

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// This is to open the files
bool OpenLogFile(App* app) {
    // browse for files
    FuriString* predefined_filepath = furi_string_alloc_set_str(PATHLOGS);
    FuriString* selected_filepath = furi_string_alloc();
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".log", NULL);
    if(!dialog_file_browser_show(
           app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
        return false;
    }
    if(storage_file_open(
           app->log_file, furi_string_get_cstr(selected_filepath), FSAM_READ, FSOM_OPEN_EXISTING)) {
        app->size_of_storage = storage_file_size(app->log_file);
    } else {
        dialog_message_show_storage_error(app->dialogs, "Cannot open File");
        return false;
    }

    if(app->size_of_storage > 20000) {
        dialog_message_show_storage_error(
            app->dialogs, "Cannot open File\nLarge Memory size\nOpen in a computer");
        storage_file_close(app->log_file);
        return false;
    }

    furi_string_reset(app->text);
    char buf[storage_file_size(app->log_file)];
    storage_file_read(app->log_file, buf, sizeof(buf));
    buf[sizeof(buf)] = '\0';
    furi_string_cat_str(app->text, buf);

    storage_file_close(app->log_file);
    furi_string_free(selected_filepath);
    furi_string_free(predefined_filepath);
    return true;
}

void basic_scenes_menu_callback(void* context, uint32_t index) {
    App* app = context;

    menu_selector = index;

    switch(index) {
    case SniffingTestOption:
        scene_manager_handle_custom_event(app->scene_manager, SniffingOptionEvent);
        break;

    case SpeedDetectorOption:
        scene_manager_handle_custom_event(app->scene_manager, SpeedDetectorEvent);
        break;

    case SenderOption:
        scene_manager_handle_custom_event(app->scene_manager, SenderOptionEvent);
        break;

    case PlayLOGOption:
        scene_manager_handle_custom_event(app->scene_manager, PlayLOGOptionEvent);
        break;

    case SettingsOption:
        scene_manager_handle_custom_event(app->scene_manager, SettingsOptionEvent);
        break;

    case ObdiiOption:
        scene_manager_handle_custom_event(app->scene_manager, ObdiiOptionEvent);
        break;

    case UDSOption:
        scene_manager_handle_custom_event(app->scene_manager, UDSOptionEvent);
        break;

    case ReadLOGOption:
        if(OpenLogFile(app)) {
            scene_manager_next_scene(app->scene_manager, app_scene_read_logs);
        }
        break;

    case AboutUsOption:
        scene_manager_handle_custom_event(app->scene_manager, AboutUsEvent);
        break;

    default:
        break;
    }
}

void app_scene_menu_on_enter(void* context) {
    App* app = context;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, app_scene_main_menu);

    if(state == 0) {
        draw_start(app);
        furi_delay_ms(START_TIME);
        scene_manager_set_scene_state(app->scene_manager, app_scene_main_menu, 1);
    }

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "MENU CANBUS");

    submenu_add_item(
        app->submenu, "Sniffing", SniffingTestOption, basic_scenes_menu_callback, app);

    submenu_add_item(
        app->submenu, "Speed Detector", SpeedDetectorOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "Sender", SenderOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "Player", PlayLOGOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "Scanner OBD2", ObdiiOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "UDS Services", UDSOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "Read LOG", ReadLOGOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "Settings", SettingsOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "About Us", AboutUsOption, basic_scenes_menu_callback, app);

    submenu_set_selected_item(app->submenu, menu_selector);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

    app->obdii_aux_index = 0;
}

bool app_scene_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case SniffingOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_sniffing_option);
            consumed = true;
            break;

        case SpeedDetectorEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_speed_detector_option);
            consumed = true;
            break;

        case SenderOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_sender_option);
            break;

        case PlayLOGOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_play_logs);
            break;

        case SettingsOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_settings_option);
            consumed = true;
            break;

        case ObdiiOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_obdii_option);
            break;

        case ReadLOGOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_read_logs);
            consumed = true;
            break;

        case UDSOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_uds_menu_option);
            consumed = true;
            break;

        case AboutUsEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_about_us);
            consumed = true;
            break;

        default:
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void app_scene_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
