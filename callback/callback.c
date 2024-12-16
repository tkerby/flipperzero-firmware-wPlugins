#include <callback/callback.h>

// Below added by Derek Jamison
// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

static void frame_cb(GameEngine *engine, Canvas *canvas, InputState input, void *context)
{
    UNUSED(engine);
    GameManager *game_manager = context;
    game_manager_input_set(game_manager, input);
    game_manager_update(game_manager);
    game_manager_render(game_manager, canvas);
}

static int32_t game_app(void *p)
{
    UNUSED(p);
    GameManager *game_manager = game_manager_alloc();
    if (!game_manager)
    {
        FURI_LOG_E("Game", "Failed to allocate game manager");
        return -1;
    }
    GameEngineSettings settings = game_engine_settings_init();
    settings.target_fps = game.target_fps;
    settings.show_fps = game.show_fps;
    settings.always_backlight = game.always_backlight;
    settings.frame_callback = frame_cb;
    settings.context = game_manager;

    GameEngine *engine = game_engine_alloc(settings);
    if (!engine)
    {
        FURI_LOG_E("Game", "Failed to allocate game engine");
        game_manager_free(game_manager);
        return -1;
    }
    game_manager_engine_set(game_manager, engine);

    void *game_context = NULL;
    if (game.context_size > 0)
    {
        game_context = malloc(game.context_size);
        game_manager_game_context_set(game_manager, game_context);
    }
    game.start(game_manager, game_context);

    game_engine_run(engine);
    game_engine_free(engine);

    game_manager_free(game_manager);

    game.stop(game_context);
    if (game_context)
    {
        free(game_context);
    }

    int32_t entities = entities_get_count();
    if (entities != 0)
    {
        FURI_LOG_E("Game", "Memory leak detected: %ld entities still allocated", entities);
        return -1;
    }

    return 0;
}

static void flip_world_request_error_draw(Canvas *canvas)
{
    if (canvas == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_request_error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if (fhttp.last_response != NULL)
    {
        if (strstr(fhttp.last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[ERROR] Failed to connect to Wifi.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[ERROR] GET request failed or returned empty data.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] WiFi error.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
        }
        else
        {
            canvas_clear(canvas);
            FURI_LOG_E(TAG, "Received an error: %s", fhttp.last_response);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Unusual error...");
            canvas_draw_str(canvas, 0, 60, "Press BACK and retry.");
        }
    }
    else
    {
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "[ERROR] Unknown error.");
        canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
    }
}

static bool alloc_about_view(void *context);
static bool alloc_text_input_view(void *context, char *title);
static bool alloc_variable_item_list(void *context, uint32_t view_id);
//
static void wifi_settings_item_selected(void *context, uint32_t index);
static void text_updated_ssid(void *context);
static void text_updated_pass(void *context);
//
static void flip_world_game_fps_change(VariableItem *item);
static void game_settings_item_selected(void *context, uint32_t index);

uint32_t callback_to_submenu(void *context)
{
    UNUSED(context);
    return FlipWorldViewSubmenu;
}
static uint32_t callback_to_wifi_settings(void *context)
{
    UNUSED(context);
    return FlipWorldViewVariableItemList;
}
static uint32_t callback_to_settings(void *context)
{
    UNUSED(context);
    return FlipWorldViewSettings;
}

static void flip_world_view_about_draw_callback(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font_custom(canvas, FONT_SIZE_XLARGE);
    canvas_draw_str(canvas, 0, 10, VERSION_TAG);
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    canvas_draw_str(canvas, 0, 20, "- @JBlanked @codeallnight");
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, 0, 30, "- github.com/JBlanked/FlipWorld");

    canvas_draw_str_multi(canvas, 0, 55, "The first open world multiplayer\ngame on the Flipper Zero.");
}

// alloc
static bool alloc_about_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return false;
    }
    if (!app->view_about)
    {
        if (!easy_flipper_set_view(&app->view_about, FlipWorldViewAbout, flip_world_view_about_draw_callback, NULL, callback_to_submenu, &app->view_dispatcher, app))
        {
            return false;
        }
        if (!app->view_about)
        {
            return false;
        }
    }
    return true;
}

static bool alloc_text_input_view(void *context, char *title)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return false;
    }
    if (!title)
    {
        FURI_LOG_E(TAG, "Title is NULL");
        return false;
    }
    app->text_input_buffer_size = 64;
    if (!app->text_input_buffer)
    {
        if (!easy_flipper_set_buffer(&app->text_input_buffer, app->text_input_buffer_size))
        {
            return false;
        }
    }
    if (!app->text_input_temp_buffer)
    {
        if (!easy_flipper_set_buffer(&app->text_input_temp_buffer, app->text_input_buffer_size))
        {
            return false;
        }
    }
    if (!app->text_input)
    {
        if (!easy_flipper_set_uart_text_input(
                &app->text_input,
                FlipWorldViewTextInput,
                title,
                app->text_input_temp_buffer,
                app->text_input_buffer_size,
                strcmp(title, "SSID") == 0 ? text_updated_ssid : text_updated_pass,
                callback_to_wifi_settings,
                &app->view_dispatcher,
                app))
        {
            return false;
        }
        if (!app->text_input)
        {
            return false;
        }
        char ssid[64];
        char pass[64];
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass)))
        {
            if (strcmp(title, "SSID") == 0)
            {
                strncpy(app->text_input_temp_buffer, ssid, app->text_input_buffer_size);
            }
            else
            {
                strncpy(app->text_input_temp_buffer, pass, app->text_input_buffer_size);
            }
        }
    }
    return true;
}
static bool alloc_variable_item_list(void *context, uint32_t view_id)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return false;
    }
    if (!app->variable_item_list)
    {
        switch (view_id)
        {
        case FlipWorldSubmenuIndexWiFiSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, wifi_settings_item_selected, callback_to_settings, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return false;
            }

            if (!app->variable_item_list)
            {
                FURI_LOG_E(TAG, "Variable item list is NULL");
                return false;
            }

            if (!app->variable_item_wifi_ssid)
            {
                app->variable_item_wifi_ssid = variable_item_list_add(app->variable_item_list, "SSID", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_wifi_ssid, "");
            }
            if (!app->variable_item_wifi_pass)
            {
                app->variable_item_wifi_pass = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_wifi_pass, "");
            }
            char ssid[64];
            char pass[64];
            if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass)))
            {
                variable_item_set_current_value_text(app->variable_item_wifi_ssid, ssid);
                // variable_item_set_current_value_text(app->variable_item_wifi_pass, pass);
                save_char("WiFi-SSID", ssid);
                save_char("WiFi-Password", pass);
            }
            break;
        case FlipWorldSubmenuIndexGameSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, game_settings_item_selected, callback_to_settings, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return false;
            }

            if (!app->variable_item_list)
            {
                FURI_LOG_E(TAG, "Variable item list is NULL");
                return false;
            }

            if (!app->variable_item_game_fps)
            {
                app->variable_item_game_fps = variable_item_list_add(app->variable_item_list, "FPS", 4, flip_world_game_fps_change, NULL);
                app->variable_item_game_download_world = variable_item_list_add(app->variable_item_list, "Install Official World Pack", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_game_download_world, "");
                variable_item_set_current_value_index(app->variable_item_game_fps, 0);
                variable_item_set_current_value_text(app->variable_item_game_fps, game_fps_choices[0]);
            }
            char _game_fps[8];
            if (load_char("Game-FPS", _game_fps, sizeof(_game_fps)))
            {
                int index = strcmp(_game_fps, "30") == 0 ? 0 : strcmp(_game_fps, "60") == 0 ? 1
                                                           : strcmp(_game_fps, "120") == 0  ? 2
                                                           : strcmp(_game_fps, "240") == 0  ? 3
                                                                                            : 0;
                variable_item_set_current_value_text(app->variable_item_game_fps, game_fps_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_fps, index);
                snprintf(game_fps, 8, "%s", _game_fps);
            }
            break;
        }
    }
    return true;
}
static bool alloc_submenu_settings(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return false;
    }
    if (!app->submenu_settings)
    {
        if (!easy_flipper_set_submenu(&app->submenu_settings, FlipWorldViewSettings, "Settings", callback_to_submenu, &app->view_dispatcher))
        {
            return NULL;
        }
        if (!app->submenu_settings)
        {
            return false;
        }
        submenu_add_item(app->submenu_settings, "WiFi", FlipWorldSubmenuIndexWiFiSettings, callback_submenu_choices, app);
        submenu_add_item(app->submenu_settings, "Game", FlipWorldSubmenuIndexGameSettings, callback_submenu_choices, app);
        submenu_add_item(app->submenu_settings, "User", FlipWorldSubmenuIndexUserSettings, callback_submenu_choices, app);
    }
    return true;
}
// free
static void free_about_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->view_about)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewAbout);
        view_free(app->view_about);
        app->view_about = NULL;
    }
}
static void free_main_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->view_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewMain);
        view_free(app->view_main);
        app->view_main = NULL;
    }
}

static void free_text_input_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->text_input)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewTextInput);
        uart_text_input_free(app->text_input);
        app->text_input = NULL;
    }
    if (app->text_input_buffer)
    {
        free(app->text_input_buffer);
        app->text_input_buffer = NULL;
    }
    if (app->text_input_temp_buffer)
    {
        free(app->text_input_temp_buffer);
        app->text_input_temp_buffer = NULL;
    }
}
static void free_variable_item_list(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->variable_item_list)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        variable_item_list_free(app->variable_item_list);
        app->variable_item_list = NULL;
    }
    if (app->variable_item_wifi_ssid)
    {
        free(app->variable_item_wifi_ssid);
        app->variable_item_wifi_ssid = NULL;
    }
    if (app->variable_item_wifi_pass)
    {
        free(app->variable_item_wifi_pass);
        app->variable_item_wifi_pass = NULL;
    }
    if (app->variable_item_game_fps)
    {
        free(app->variable_item_game_fps);
        app->variable_item_game_fps = NULL;
    }
    if (app->variable_item_user_username)
    {
        free(app->variable_item_user_username);
        app->variable_item_user_username = NULL;
    }
    if (app->variable_item_user_password)
    {
        free(app->variable_item_user_password);
        app->variable_item_user_password = NULL;
    }
}
static void free_submenu_settings(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->submenu_settings)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewSettings);
        submenu_free(app->submenu_settings);
        app->submenu_settings = NULL;
    }
}
static FuriThreadId thread_id;
static bool game_thread_running = false;
void free_all_views(void *context, bool should_free_variable_item_list, bool should_free_submenu_settings)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (should_free_variable_item_list)
    {
        free_variable_item_list(app);
    }
    free_about_view(app);
    free_main_view(app);
    free_text_input_view(app);
    if (app->view_main)
    {
        view_free(app->view_main);
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewMain);
        app->view_main = NULL;
    }
    // free game thread
    if (game_thread_running)
    {
        game_thread_running = false;
        furi_thread_flags_set(thread_id, WorkerEvtStop);
        furi_thread_free(thread_id);
    }

    if (should_free_submenu_settings)
        free_submenu_settings(app);
}

void callback_submenu_choices(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    switch (index)
    {
    case FlipWorldSubmenuIndexRun:
        // free game thread
        if (game_thread_running)
        {
            game_thread_running = false;
            furi_thread_flags_set(thread_id, WorkerEvtStop);
            furi_thread_free(thread_id);
        }
        free_all_views(app, true, true);
        if (!app->view_main)
        {
            if (!easy_flipper_set_view(&app->view_main, FlipWorldViewMain, NULL, NULL, callback_to_submenu, &app->view_dispatcher, app))
            {
                return;
            }
            if (!app->view_main)
            {
                return;
            }
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMain);
        FuriThread *thread = furi_thread_alloc_ex("game", 4096, game_app, app);
        if (!thread)
        {
            FURI_LOG_E(TAG, "Failed to allocate game thread");
            return;
        }
        furi_thread_start(thread);
        thread_id = furi_thread_get_id(thread);
        game_thread_running = true;
        break;
    case FlipWorldSubmenuIndexAbout:
        free_all_views(app, true, true);
        if (!alloc_about_view(app))
        {
            FURI_LOG_E(TAG, "Failed to allocate about view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewAbout);
        break;
    case FlipWorldSubmenuIndexSettings:
        free_all_views(app, true, true);
        if (!alloc_submenu_settings(app))
        {
            FURI_LOG_E(TAG, "Failed to allocate settings view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSettings);
        break;
    case FlipWorldSubmenuIndexWiFiSettings:
        free_all_views(app, true, false);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexWiFiSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    case FlipWorldSubmenuIndexGameSettings:
        free_all_views(app, true, false);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexGameSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    case FlipWorldSubmenuIndexUserSettings:
        easy_flipper_dialog("User Settings", "Coming soon...");
        break;
    default:
        break;
    }
}

static void text_updated_ssid(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }

    // store the entered text
    strncpy(app->text_input_buffer, app->text_input_temp_buffer, app->text_input_buffer_size);

    // Ensure null-termination
    app->text_input_buffer[app->text_input_buffer_size - 1] = '\0';

    // save the setting
    save_char("WiFi-SSID", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_wifi_ssid)
    {
        variable_item_set_current_value_text(app->variable_item_wifi_ssid, app->text_input_buffer);

        // get value of password
        char pass[64];
        if (load_char("WiFi-Password", pass, sizeof(pass)))
        {
            if (strlen(pass) > 0 && strlen(app->text_input_buffer) > 0)
            {
                // save the settings
                save_settings(app->text_input_buffer, pass);

                // initialize the http
                if (flipper_http_init(flipper_http_rx_callback, app))
                {
                    // save the wifi if the device is connected
                    if (!flipper_http_save_wifi(app->text_input_buffer, pass))
                    {
                        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
                    }

                    // free the resources
                    flipper_http_deinit();
                }
                else
                {
                    easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
                }
            }
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
}
static void text_updated_pass(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }

    // store the entered text
    strncpy(app->text_input_buffer, app->text_input_temp_buffer, app->text_input_buffer_size);

    // Ensure null-termination
    app->text_input_buffer[app->text_input_buffer_size - 1] = '\0';

    // save the setting
    save_char("WiFi-Password", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_wifi_pass)
    {
        // variable_item_set_current_value_text(app->variable_item_wifi_pass, app->text_input_buffer);
    }

    // get value of ssid
    char ssid[64];
    if (load_char("WiFi-SSID", ssid, sizeof(ssid)))
    {
        if (strlen(ssid) > 0 && strlen(app->text_input_buffer) > 0)
        {
            // save the settings
            save_settings(ssid, app->text_input_buffer);

            // initialize the http
            if (flipper_http_init(flipper_http_rx_callback, app))
            {
                // save the wifi if the device is connected
                if (!flipper_http_save_wifi(ssid, app->text_input_buffer))
                {
                    easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
                }

                // free the resources
                flipper_http_deinit();
            }
            else
            {
                easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
            }
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
}

static void wifi_settings_item_selected(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    char ssid[64];
    char pass[64];
    switch (index)
    {
    case 0: // Input SSID
        free_all_views(app, false, false);
        if (!alloc_text_input_view(app, "SSID"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        // load SSID
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass)))
        {
            strncpy(app->text_input_temp_buffer, ssid, app->text_input_buffer_size - 1);
            app->text_input_temp_buffer[app->text_input_buffer_size - 1] = '\0';
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    case 1: // Input Password
        free_all_views(app, false, false);
        if (!alloc_text_input_view(app, "Password"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        // load password
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass)))
        {
            strncpy(app->text_input_temp_buffer, pass, app->text_input_buffer_size - 1);
            app->text_input_temp_buffer[app->text_input_buffer_size - 1] = '\0';
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}
static void flip_world_game_fps_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, game_fps_choices[index]);

    // save the fps
    snprintf(game_fps, 8, "%s", game_fps_choices[index]);
    save_char("Game-FPS", game_fps);
}

static bool flip_world_fetch_worlds(DataLoaderModel *model)
{
    UNUSED(model);
    snprintf(
        fhttp.file_path,
        sizeof(fhttp.file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds.json");
    fhttp.save_received_data = true;
    return flipper_http_get_request_with_headers("https://www.flipsocial.net/api/world/get/10/", "{\"Content-Type\":\"application/json\"}");
}
static char *flip_world_parse_worlds(DataLoaderModel *model)
{
    UNUSED(model);
    // load the received data from the saved file
    FuriString *world_data = flipper_http_load_from_file(fhttp.file_path);
    flipper_http_deinit();
    if (!world_data)
    {
        FURI_LOG_E(TAG, "Failed to load world data");
        return "Failed to load world data";
    }
    char *data = (char *)furi_string_get_cstr(world_data);
    if (!data)
    {
        FURI_LOG_E(TAG, "Failed to get world data");
        return "Failed to get world data";
    }
    // we used 10 since we passed 10 in the request
    for (int i = 0; i < 10; i++)
    {
        char *json = get_json_array_value("worlds", i, data, 1024);
        if (!json)
        {
            FURI_LOG_E(TAG, "Failed to get worlds. Data likely empty");
            break;
        }
        char *world_name = get_json_value("name", json, 1024);
        if (!world_name)
        {
            FURI_LOG_E(TAG, "Failed to get world name");
            break;
        }
        if (!save_world(world_name, json))
        {
            FURI_LOG_E(TAG, "Failed to save world");
        }
    }
    return "World Pack Installed";
}
static void flip_world_switch_to_view_get_worlds(FlipWorldApp *app)
{
    flip_world_generic_switch_to_view(app, "Downlading Worlds..", flip_world_fetch_worlds, flip_world_parse_worlds, 1, callback_to_submenu, FlipWorldViewLoader);
}
static void game_settings_item_selected(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    switch (index)
    {
    case 0:    // Game FPS
        break; // handled by flip_world_game_fps_change
    case 1:    // Download Worlds
        // TODO: Implement download worlds
        if (!flipper_http_init(flipper_http_rx_callback, app))
        {
            FURI_LOG_E(TAG, "Failed to initialize FlipperHTTP");
            return;
        }
        flip_world_switch_to_view_get_worlds(app);
        break;
    }
}

static void flip_world_widget_set_text(char *message, Widget **widget)
{
    if (widget == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_set_widget_text - widget is NULL");
        DEV_CRASH();
        return;
    }
    if (message == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_set_widget_text - message is NULL");
        DEV_CRASH();
        return;
    }
    widget_reset(*widget);

    uint32_t message_length = strlen(message); // Length of the message
    uint32_t i = 0;                            // Index tracker
    uint32_t formatted_index = 0;              // Tracker for where we are in the formatted message
    char *formatted_message;                   // Buffer to hold the final formatted message

    // Allocate buffer with double the message length plus one for safety
    if (!easy_flipper_set_buffer(&formatted_message, message_length * 2 + 1))
    {
        return;
    }

    while (i < message_length)
    {
        uint32_t max_line_length = 31;                  // Maximum characters per line
        uint32_t remaining_length = message_length - i; // Remaining characters
        uint32_t line_length = (remaining_length < max_line_length) ? remaining_length : max_line_length;

        // Check for newline character within the current segment
        uint32_t newline_pos = i;
        bool found_newline = false;
        for (; newline_pos < i + line_length && newline_pos < message_length; newline_pos++)
        {
            if (message[newline_pos] == '\n')
            {
                found_newline = true;
                break;
            }
        }

        if (found_newline)
        {
            // If newline found, set line_length up to the newline
            line_length = newline_pos - i;
        }

        // Temporary buffer to hold the current line
        char line[32];
        strncpy(line, message + i, line_length);
        line[line_length] = '\0';

        // If newline was found, skip it for the next iteration
        if (found_newline)
        {
            i += line_length + 1; // +1 to skip the '\n' character
        }
        else
        {
            // Check if the line ends in the middle of a word and adjust accordingly
            if (line_length == max_line_length && message[i + line_length] != '\0' && message[i + line_length] != ' ')
            {
                // Find the last space within the current line to avoid breaking a word
                char *last_space = strrchr(line, ' ');
                if (last_space != NULL)
                {
                    // Adjust the line_length to avoid cutting the word
                    line_length = last_space - line;
                    line[line_length] = '\0'; // Null-terminate at the space
                }
            }

            // Move the index forward by the determined line_length
            i += line_length;

            // Skip any spaces at the beginning of the next line
            while (i < message_length && message[i] == ' ')
            {
                i++;
            }
        }

        // Manually copy the fixed line into the formatted_message buffer
        for (uint32_t j = 0; j < line_length; j++)
        {
            formatted_message[formatted_index++] = line[j];
        }

        // Add a newline character for line spacing
        formatted_message[formatted_index++] = '\n';
    }

    // Null-terminate the formatted_message
    formatted_message[formatted_index] = '\0';

    // Add the formatted message to the widget
    widget_add_text_scroll_element(*widget, 0, 0, 128, 64, formatted_message);
}

void flip_world_loader_draw_callback(Canvas *canvas, void *model)
{
    if (!canvas || !model)
    {
        FURI_LOG_E(TAG, "flip_world_loader_draw_callback - canvas or model is NULL");
        return;
    }

    SerialState http_state = fhttp.state;
    DataLoaderModel *data_loader_model = (DataLoaderModel *)model;
    DataState data_state = data_loader_model->data_state;
    char *title = data_loader_model->title;

    canvas_set_font(canvas, FontSecondary);

    if (http_state == INACTIVE)
    {
        canvas_draw_str(canvas, 0, 7, "Wifi Dev Board disconnected.");
        canvas_draw_str(canvas, 0, 17, "Please connect to the board.");
        canvas_draw_str(canvas, 0, 32, "If your board is connected,");
        canvas_draw_str(canvas, 0, 42, "make sure you have flashed");
        canvas_draw_str(canvas, 0, 52, "your WiFi Devboard with the");
        canvas_draw_str(canvas, 0, 62, "latest FlipperHTTP flash.");
        return;
    }

    if (data_state == DataStateError || data_state == DataStateParseError)
    {
        flip_world_request_error_draw(canvas);
        return;
    }

    canvas_draw_str(canvas, 0, 7, title);
    canvas_draw_str(canvas, 0, 17, "Loading...");

    if (data_state == DataStateInitial)
    {
        return;
    }

    if (http_state == SENDING)
    {
        canvas_draw_str(canvas, 0, 27, "Fetching...");
        return;
    }

    if (http_state == RECEIVING || data_state == DataStateRequested)
    {
        canvas_draw_str(canvas, 0, 27, "Receiving...");
        return;
    }

    if (http_state == IDLE && data_state == DataStateReceived)
    {
        canvas_draw_str(canvas, 0, 27, "Processing...");
        return;
    }

    if (http_state == IDLE && data_state == DataStateParsed)
    {
        canvas_draw_str(canvas, 0, 27, "Processed...");
        return;
    }
}

static void flip_world_loader_process_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    FlipWorldApp *app = (FlipWorldApp *)context;
    View *view = app->view_loader;

    DataState current_data_state;
    with_view_model(view, DataLoaderModel * model, { current_data_state = model->data_state; }, false);

    if (current_data_state == DataStateInitial)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                model->data_state = DataStateRequested;
                DataLoaderFetch fetch = model->fetcher;
                if (fetch == NULL)
                {
                    FURI_LOG_E(TAG, "Model doesn't have Fetch function assigned.");
                    model->data_state = DataStateError;
                    return;
                }

                // Clear any previous responses
                strncpy(fhttp.last_response, "", 1);
                bool request_status = fetch(model);
                if (!request_status)
                {
                    model->data_state = DataStateError;
                }
            },
            true);
    }
    else if (current_data_state == DataStateRequested || current_data_state == DataStateError)
    {
        if (fhttp.state == IDLE && fhttp.last_response != NULL)
        {
            if (strstr(fhttp.last_response, "[PONG]") != NULL)
            {
                FURI_LOG_DEV(TAG, "PONG received.");
            }
            else if (strncmp(fhttp.last_response, "[SUCCESS]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "SUCCESS received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            }
            else if (strncmp(fhttp.last_response, "[ERROR]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "ERROR received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            }
            else if (strlen(fhttp.last_response) == 0)
            {
                // Still waiting on response
            }
            else
            {
                with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateReceived; }, true);
            }
        }
        else if (fhttp.state == SENDING || fhttp.state == RECEIVING)
        {
            // continue waiting
        }
        else if (fhttp.state == INACTIVE)
        {
            // inactive. try again
        }
        else if (fhttp.state == ISSUE)
        {
            with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", fhttp.state, fhttp.last_response ? fhttp.last_response : "NULL");
            DEV_CRASH();
        }
    }
    else if (current_data_state == DataStateReceived)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                char *data_text;
                if (model->parser == NULL)
                {
                    data_text = NULL;
                    FURI_LOG_DEV(TAG, "Parser is NULL");
                    DEV_CRASH();
                }
                else
                {
                    data_text = model->parser(model);
                }
                FURI_LOG_DEV(TAG, "Parsed data: %s\r\ntext: %s", fhttp.last_response ? fhttp.last_response : "NULL", data_text ? data_text : "NULL");
                model->data_text = data_text;
                if (data_text == NULL)
                {
                    model->data_state = DataStateParseError;
                }
                else
                {
                    model->data_state = DataStateParsed;
                }
            },
            true);
    }
    else if (current_data_state == DataStateParsed)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                if (++model->request_index < model->request_count)
                {
                    model->data_state = DataStateInitial;
                }
                else
                {
                    flip_world_widget_set_text(model->data_text != NULL ? model->data_text : "", &app->widget_result);
                    if (model->data_text != NULL)
                    {
                        free(model->data_text);
                        model->data_text = NULL;
                    }
                    view_set_previous_callback(widget_get_view(app->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewWidgetResult);
                }
            },
            true);
    }
}

static void flip_world_loader_timer_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipWorldCustomEventProcess);
}

static void flip_world_loader_on_enter(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if (model->timer == NULL)
            {
                model->timer = furi_timer_alloc(flip_world_loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void flip_world_loader_on_exit(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_stop(model->timer);
            }
        },
        false);
}

void flip_world_loader_init(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DataLoaderModel));
    view_set_enter_callback(view, flip_world_loader_on_enter);
    view_set_exit_callback(view, flip_world_loader_on_exit);
}

void flip_world_loader_free_model(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_loader_free_model - view is NULL");
        DEV_CRASH();
        return;
    }
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_free(model->timer);
                model->timer = NULL;
            }
            if (model->parser_context)
            {
                free(model->parser_context);
                model->parser_context = NULL;
            }
        },
        false);
}

bool flip_world_custom_event_callback(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch (index)
    {
    case FlipWorldCustomEventProcess:
        flip_world_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "flip_world_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void flip_world_generic_switch_to_view(FlipWorldApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id)
{
    if (app == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_generic_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View *view = app->view_loader;
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_generic_switch_to_view - view is NULL");
        DEV_CRASH();
        return;
    }

    with_view_model(
        view,
        DataLoaderModel * model,
        {
            model->title = title;
            model->fetcher = fetcher;
            model->parser = parser;
            model->request_index = 0;
            model->request_count = request_count;
            model->back_callback = back;
            model->data_state = DataStateInitial;
            model->data_text = NULL;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
