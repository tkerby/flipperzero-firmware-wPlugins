#include "cfw_app.h"

static bool cfw_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    CfwApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void callback_reboot(void* context) {
    UNUSED(context);
    power_reboot(PowerBootModeNormal);
}

bool cfw_app_apply(CfwApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(app->save_mainmenu_apps) {
        Stream* stream = file_stream_alloc(storage);
        if(file_stream_open(stream, CFW_MENU_PATH, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
            stream_write_format(stream, "MainMenuList Version %u\n", 0);
            CharList_it_t it;
            CharList_it(it, app->mainmenu_app_paths);
            for(size_t i = 0; i < CharList_size(app->mainmenu_app_paths); i++) {
                stream_write_format(stream, "%s\n", *CharList_get(app->mainmenu_app_paths, i));
            }
        }
        file_stream_close(stream);
        stream_free(stream);
    }

    if(app->save_gamemenu_apps) {
        Stream* stream = file_stream_alloc(storage);
        if(file_stream_open(stream, CFW_MENU_GAMESMODE_PATH, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
            stream_write_format(stream, "GamesMenuList Version %u\n", 0);
            CharList_it_t it;
            CharList_it(it, app->gamemenu_app_paths);
            for(size_t i = 0; i < CharList_size(app->gamemenu_app_paths); i++) {
                stream_write_format(stream, "%s\n", *CharList_get(app->gamemenu_app_paths, i));
            }
        }
        file_stream_close(stream);
        stream_free(stream);
    }

    if(app->save_subghz_frequencies) {
        FlipperFormat* file = flipper_format_file_alloc(storage);
        do {
            FrequencyList_it_t it;
            if(!flipper_format_file_open_always(file, EXT_PATH("subghz/assets/setting_user.txt")))
                break;

            if(!flipper_format_write_header_cstr(
                   file, SUBGHZ_SETTING_FILE_TYPE, SUBGHZ_SETTING_FILE_VERSION))
                break;

            while(flipper_format_delete_key(file, "Add_standard_frequencies"))
                ;
            flipper_format_write_bool(
                file, "Add_standard_frequencies", &app->subghz_use_defaults, 1);

            if(!flipper_format_rewind(file)) break;
            while(flipper_format_delete_key(file, "Frequency"))
                ;
            FrequencyList_it(it, app->subghz_static_freqs);
            for(size_t i = 0; i < FrequencyList_size(app->subghz_static_freqs); i++) {
                flipper_format_write_uint32(
                    file, "Frequency", FrequencyList_get(app->subghz_static_freqs, i), 1);
            }

            if(!flipper_format_rewind(file)) break;
            while(flipper_format_delete_key(file, "Hopper_frequency"))
                ;
            for(size_t i = 0; i < FrequencyList_size(app->subghz_hopper_freqs); i++) {
                flipper_format_write_uint32(
                    file, "Hopper_frequency", FrequencyList_get(app->subghz_hopper_freqs, i), 1);
            }
        } while(false);
        flipper_format_free(file);
    }

    if(app->save_subghz) {
        FlipperFormat* file = flipper_format_file_alloc(storage);
        do {
            if(!flipper_format_file_open_always(file, "/ext/subghz/assets/extend_range.txt"))
                break;
            if(!flipper_format_write_header_cstr(file, "Flipper SubGhz Setting File", 1)) break;
            if(!flipper_format_write_comment_cstr(
                   file, "Whether to allow extended ranges that can break your flipper"))
                break;
            if(!flipper_format_write_bool(
                   file, "use_ext_range_at_own_risk", &app->subghz_extend, 1))
                break;
            if(!flipper_format_write_comment_cstr(
                   file, "Whether to ignore the default TX region settings"))
                break;
            if(!flipper_format_write_bool(file, "ignore_default_tx_region", &app->subghz_bypass, 1))
                break;
        } while(0);
        flipper_format_free(file);
    }

    if(app->save_name) {
        if(strcmp(app->device_name, "") == 0) {
            storage_simply_remove(storage, NAMESPOOF_PATH);
        } else {
            FlipperFormat* file = flipper_format_file_alloc(storage);

            do {
                if(!flipper_format_file_open_always(file, NAMESPOOF_PATH)) break;

                if(!flipper_format_write_header_cstr(file, NAMESPOOF_HEADER, NAMESPOOF_VERSION))
                    break;

                if(!flipper_format_write_comment_cstr(
                       file, "Changing the value below will change your FlipperZero device name."))
                    break;
                if(!flipper_format_write_comment_cstr(
                       file,
                       "Note: This is limited to 8 characters using the following: a-z, A-Z, 0-9, and _"))
                    break;
                if(!flipper_format_write_comment_cstr(
                       file, "It cannot contain any other characters."))
                    break;

                if(!flipper_format_write_string_cstr(file, "Name", app->device_name)) break;

            } while(0);

            flipper_format_free(file);
        }
    }

    if(app->save_backlight) {
        rgb_backlight_save_settings();
    }

    if(app->save_settings) {
        cfw_settings_save();
    }

    if(app->require_reboot) {
        popup_set_header(app->popup, "Rebooting...", 64, 26, AlignCenter, AlignCenter);
        popup_set_text(app->popup, "Applying changes...", 64, 40, AlignCenter, AlignCenter);
        popup_set_callback(app->popup, callback_reboot);
        popup_set_context(app->popup, app);
        popup_set_timeout(app->popup, 1000);
        popup_enable_timeout(app->popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, CfwAppViewPopup);
        return true;
    }

    furi_record_close(RECORD_STORAGE);
    return false;
}

static bool cfw_app_back_event_callback(void* context) {
    furi_assert(context);
    CfwApp* app = context;

    if(!scene_manager_has_previous_scene(app->scene_manager, CfwAppSceneStart)) {
        if(cfw_app_apply(app)) {
            return true;
        }
    }

    return scene_manager_handle_back_event(app->scene_manager);
}

CfwApp* cfw_app_alloc() {
    CfwApp* app = malloc(sizeof(CfwApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->expansion = furi_record_open(RECORD_EXPANSION);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // View Dispatcher and Scene Manager
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&cfw_app_scene_handlers, app);
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(app->view_dispatcher, cfw_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, cfw_app_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Gui Modules
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        CfwAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfwAppViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfwAppViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfwAppViewByteInput, byte_input_get_view(app->byte_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, CfwAppViewPopup, popup_get_view(app->popup));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfwAppViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    //Main Menu Add/Remove list + Start Point

    CharList_init(app->mainmenu_app_names);
    CharList_init(app->mainmenu_app_paths);

    Loader* loader = furi_record_open(RECORD_LOADER);
    MainMenuList_t* mainmenu_apps = loader_get_mainmenu_apps(loader);

    for(size_t i = 0; i < MainMenuList_size(*mainmenu_apps); i++) {
        const MainMenuApp* menu_item = MainMenuList_get(*mainmenu_apps, i);
        CharList_push_back(app->mainmenu_app_names, strdup(menu_item->name));
        CharList_push_back(app->mainmenu_app_paths, strdup(menu_item->path));
    }

    //Game Menu Add/Remove list + Start Point

    CharList_init(app->gamemenu_app_names);
    CharList_init(app->gamemenu_app_paths);

    GamesMenuList_t* gamemenu_apps = loader_get_gamesmenu_apps(loader);
    furi_record_close(RECORD_LOADER);

    for(size_t i = 0; i < GamesMenuList_size(*gamemenu_apps); i++) {
        const GamesMenuApp* game_menu_item = GamesMenuList_get(*gamemenu_apps, i);
        CharList_push_back(app->gamemenu_app_names, strdup(game_menu_item->name));
        CharList_push_back(app->gamemenu_app_paths, strdup(game_menu_item->path));
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FrequencyList_init(app->subghz_static_freqs);
    FrequencyList_init(app->subghz_hopper_freqs);
    app->subghz_use_defaults = true;
    do {
        uint32_t temp;
        if(!flipper_format_file_open_existing(file, EXT_PATH("subghz/assets/setting_user.txt")))
            break;

        flipper_format_read_bool(file, "Add_standard_frequencies", &app->subghz_use_defaults, 1);

        if(!flipper_format_rewind(file)) break;
        while(flipper_format_read_uint32(file, "Frequency", &temp, 1)) {
            if(furi_hal_subghz_is_frequency_valid(temp)) {
                FrequencyList_push_back(app->subghz_static_freqs, temp);
            }
        }

        if(!flipper_format_rewind(file)) break;
        while(flipper_format_read_uint32(file, "Hopper_frequency", &temp, 1)) {
            if(furi_hal_subghz_is_frequency_valid(temp)) {
                FrequencyList_push_back(app->subghz_hopper_freqs, temp);
            }
        }
    } while(false);
    flipper_format_free(file);

    file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_existing(file, "/ext/subghz/assets/extend_range.txt")) {
        flipper_format_read_bool(file, "use_ext_range_at_own_risk", &app->subghz_extend, 1);
        flipper_format_read_bool(file, "ignore_default_tx_region", &app->subghz_bypass, 1);
    }
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    strlcpy(app->device_name, furi_hal_version_get_name_ptr(), FURI_HAL_VERSION_ARRAY_NAME_LENGTH);

    app->version_tag =
        furi_string_alloc_printf("%s  %s", version_get_version(NULL), version_get_builddate(NULL));

    return app;
}

void cfw_app_free(CfwApp* app) {
    furi_assert(app);

    // Gui modules
    view_dispatcher_remove_view(app->view_dispatcher, CfwAppViewVarItemList);
    variable_item_list_free(app->var_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, CfwAppViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, CfwAppViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, CfwAppViewByteInput);
    byte_input_free(app->byte_input);
    view_dispatcher_remove_view(app->view_dispatcher, CfwAppViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, CfwAppViewDialogEx);
    dialog_ex_free(app->dialog_ex);

    // View Dispatcher and Scene Manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Settings deinit

    CharList_it_t it;

    for(CharList_it(it, app->mainmenu_app_names); !CharList_end_p(it); CharList_next(it)) {
        free(*CharList_cref(it));
    }
    CharList_clear(app->mainmenu_app_names);
    for(CharList_it(it, app->mainmenu_app_paths); !CharList_end_p(it); CharList_next(it)) {
        free(*CharList_cref(it));
    }
    CharList_clear(app->mainmenu_app_paths);

    CharList_it_t it2;

    for(CharList_it(it2, app->gamemenu_app_names); !CharList_end_p(it2); CharList_next(it2)) {
        free(*CharList_cref(it2));
    }
    CharList_clear(app->gamemenu_app_names);
    for(CharList_it(it2, app->gamemenu_app_paths); !CharList_end_p(it2); CharList_next(it2)) {
        free(*CharList_cref(it2));
    }
    CharList_clear(app->gamemenu_app_paths);

    FrequencyList_clear(app->subghz_static_freqs);
    FrequencyList_clear(app->subghz_hopper_freqs);

    furi_string_free(app->version_tag);

    // Records
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_EXPANSION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

extern int32_t cfw_app(void* p) {
    UNUSED(p);
    CfwApp* app = cfw_app_alloc();
    DESKTOP_SETTINGS_LOAD(&app->desktop);
    passport_settings_load(&app->passport);
    scene_manager_next_scene(app->scene_manager, CfwAppSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    cfw_app_free(app);
    return 0;
}
