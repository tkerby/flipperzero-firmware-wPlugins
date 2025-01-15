#include <callback/callback.h>
#include "engine/engine.h"
#include "engine/game_engine.h"
#include "engine/game_manager_i.h"
#include "engine/level_i.h"
#include "engine/entity_i.h"
#include "game/storage.h"

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

    // Setup game engine settings...
    GameEngineSettings settings = game_engine_settings_init();
    settings.target_fps = game_fps_choices_2[game_fps_index];
    settings.show_fps = game.show_fps;
    settings.always_backlight = strstr(yes_or_no_choices[game_screen_always_on_index], "Yes") != NULL;
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

    // Allocate custom game context if needed
    void *game_context = NULL;
    if (game.context_size > 0)
    {
        game_context = malloc(game.context_size);
        game_manager_game_context_set(game_manager, game_context);
    }

    // Start the game
    game.start(game_manager, game_context);

    // 1) Run the engine
    game_engine_run(engine);

    // 2) Stop the game FIRST, so it can do any internal cleanup
    game.stop(game_context);

    // 3) Now free the engine
    game_engine_free(engine);

    // 4) Now free the manager
    game_manager_free(game_manager);

    // 5) Finally, free your custom context if it was allocated
    if (game_context)
    {
        free(game_context);
    }

    // 6) Check for leftover entities
    int32_t entities = entities_get_count();
    if (entities != 0)
    {
        FURI_LOG_E("Game", "Memory leak detected: %ld entities still allocated", entities);
        return -1;
    }

    return 0;
}

static void flip_world_request_error_draw(Canvas *canvas, DataLoaderModel *model)
{
    if (canvas == NULL)
    {
        FURI_LOG_E(TAG, "flip_world_request_error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if (model->fhttp->last_response != NULL)
    {
        if (strstr(model->fhttp->last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(model->fhttp->last_response, "[ERROR] Failed to connect to Wifi.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(model->fhttp->last_response, "[ERROR] GET request failed or returned empty data.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] WiFi error.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(model->fhttp->last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
        }
        else
        {
            canvas_clear(canvas);
            FURI_LOG_E(TAG, "Received an error: %s", model->fhttp->last_response);
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
static void text_updated_wifi_ssid(void *context);
static void text_updated_wifi_pass(void *context);
static void text_updated_username(void *context);
static void text_updated_password(void *context);
//
static void flip_world_game_fps_change(VariableItem *item);
static void game_settings_item_selected(void *context, uint32_t index);
static void user_settings_item_selected(void *context, uint32_t index);
static void flip_world_game_screen_always_on_change(VariableItem *item);
static void flip_world_game_sound_on_change(VariableItem *item);
static void flip_world_game_vibration_on_change(VariableItem *item);

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
    // canvas_set_font_custom(canvas, FONT_SIZE_XLARGE);
    canvas_draw_str(canvas, 0, 10, VERSION_TAG);
    // canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, 0, 20, "Dev: JBlanked, codeallnight");
    canvas_draw_str(canvas, 0, 30, "GFX: the1anonlypr3");
    canvas_draw_str(canvas, 0, 40, "github.com/jblanked/FlipWorld");

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
                strcmp(title, "SSID") == 0 ? text_updated_wifi_ssid : strcmp(title, "Password") == 0     ? text_updated_wifi_pass
                                                                  : strcmp(title, "Username-Login") == 0 ? text_updated_username
                                                                                                         : text_updated_password,
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
        char username[64];
        char password[64];
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
        {
            if (strcmp(title, "SSID") == 0)
            {
                strncpy(app->text_input_temp_buffer, ssid, app->text_input_buffer_size);
            }
            else if (strcmp(title, "Password") == 0)
            {
                strncpy(app->text_input_temp_buffer, pass, app->text_input_buffer_size);
            }
            else if (strcmp(title, "Username-Login") == 0)
            {
                strncpy(app->text_input_temp_buffer, username, app->text_input_buffer_size);
            }
            else if (strcmp(title, "Password-Login") == 0)
            {
                strncpy(app->text_input_temp_buffer, password, app->text_input_buffer_size);
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
    char ssid[64];
    char pass[64];
    char username[64];
    char password[64];
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
            if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
            {
                variable_item_set_current_value_text(app->variable_item_wifi_ssid, ssid);
                // variable_item_set_current_value_text(app->variable_item_wifi_pass, pass);
                save_char("WiFi-SSID", ssid);
                save_char("WiFi-Password", pass);
                save_char("Flip-Social-Username", username);
                save_char("Flip-Social-Password", password);
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

            if (!app->variable_item_game_download_world)
            {
                app->variable_item_game_download_world = variable_item_list_add(app->variable_item_list, "Install Official World Pack", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_game_download_world, "");
            }
            if (!app->variable_item_game_fps)
            {
                app->variable_item_game_fps = variable_item_list_add(app->variable_item_list, "FPS", 4, flip_world_game_fps_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_fps, 0);
                variable_item_set_current_value_text(app->variable_item_game_fps, game_fps_choices[0]);
            }
            if (!app->variable_item_game_screen_always_on)
            {
                app->variable_item_game_screen_always_on = variable_item_list_add(app->variable_item_list, "Keep Screen On?", 2, flip_world_game_screen_always_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_screen_always_on, 1);
                variable_item_set_current_value_text(app->variable_item_game_screen_always_on, yes_or_no_choices[1]);
            }
            if (!app->variable_item_game_sound_on)
            {
                app->variable_item_game_sound_on = variable_item_list_add(app->variable_item_list, "Sound On?", 2, flip_world_game_sound_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_sound_on, 0);
                variable_item_set_current_value_text(app->variable_item_game_sound_on, yes_or_no_choices[0]);
            }
            if (!app->variable_item_game_vibration_on)
            {
                app->variable_item_game_vibration_on = variable_item_list_add(app->variable_item_list, "Vibration On?", 2, flip_world_game_vibration_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vibration_on, 0);
                variable_item_set_current_value_text(app->variable_item_game_vibration_on, yes_or_no_choices[0]);
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
            }
            char _game_screen_always_on[8];
            if (load_char("Game-Screen-Always-On", _game_screen_always_on, sizeof(_game_screen_always_on)))
            {
                int index = strcmp(_game_screen_always_on, "No") == 0 ? 0 : strcmp(_game_screen_always_on, "Yes") == 0 ? 1
                                                                                                                       : 0;
                variable_item_set_current_value_text(app->variable_item_game_screen_always_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_screen_always_on, index);
            }
            char _game_sound_on[8];
            if (load_char("Game-Sound-On", _game_sound_on, sizeof(_game_sound_on)))
            {
                int index = strcmp(_game_sound_on, "No") == 0 ? 0 : strcmp(_game_sound_on, "Yes") == 0 ? 1
                                                                                                       : 0;
                variable_item_set_current_value_text(app->variable_item_game_sound_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_sound_on, index);
            }
            char _game_vibration_on[8];
            if (load_char("Game-Vibration-On", _game_vibration_on, sizeof(_game_vibration_on)))
            {
                int index = strcmp(_game_vibration_on, "No") == 0 ? 0 : strcmp(_game_vibration_on, "Yes") == 0 ? 1
                                                                                                               : 0;
                variable_item_set_current_value_text(app->variable_item_game_vibration_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_vibration_on, index);
            }
            break;
        case FlipWorldSubmenuIndexUserSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, user_settings_item_selected, callback_to_settings, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return false;
            }

            if (!app->variable_item_list)
            {
                FURI_LOG_E(TAG, "Variable item list is NULL");
                return false;
            }

            // if logged in, show profile info, otherwise show login/register
            if (is_logged_in() || is_logged_in_to_flip_social())
            {
                if (!app->variable_item_user_username)
                {
                    app->variable_item_user_username = variable_item_list_add(app->variable_item_list, "Username", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_username, "");
                }
                if (!app->variable_item_user_password)
                {
                    app->variable_item_user_password = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_password, "");
                }
                if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
                {
                    variable_item_set_current_value_text(app->variable_item_user_username, username);
                    variable_item_set_current_value_text(app->variable_item_user_password, "*****");
                }
            }
            else
            {
                if (!app->variable_item_user_username)
                {
                    app->variable_item_user_username = variable_item_list_add(app->variable_item_list, "Username", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_username, "");
                }
                if (!app->variable_item_user_password)
                {
                    app->variable_item_user_password = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_password, "");
                }
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
    if (app->variable_item_game_screen_always_on)
    {
        free(app->variable_item_game_screen_always_on);
        app->variable_item_game_screen_always_on = NULL;
    }
    if (app->variable_item_game_download_world)
    {
        free(app->variable_item_game_download_world);
        app->variable_item_game_download_world = NULL;
    }
    if (app->variable_item_game_sound_on)
    {
        free(app->variable_item_game_sound_on);
        app->variable_item_game_sound_on = NULL;
    }
    if (app->variable_item_game_vibration_on)
    {
        free(app->variable_item_game_vibration_on);
        app->variable_item_game_vibration_on = NULL;
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
    free_text_input_view(app);

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
static bool fetch_world_list(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
        return false;
    }
    // Create the directory for saving worlds
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    // free storage
    furi_record_close(RECORD_STORAGE);

    snprintf(
        fhttp->file_path,
        sizeof(fhttp->file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

    fhttp->save_received_data = true;
    return flipper_http_get_request_with_headers(fhttp, "https://www.flipsocial.net/api/world/v3/list/10/", "{\"Content-Type\":\"application/json\"}");
}
// we will load the palyer stats from the API and save them
// in player_spawn game method, it will load the player stats that we saved
static bool fetch_player_stats(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
        return false;
    }
    char username[64];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        easy_flipper_dialog("Error", "Failed to load saved username. Go to settings to update.");
        return false;
    }
    char url[128];
    snprintf(url, sizeof(url), "https://www.flipsocial.net/api/user/game-stats/%s/", username);
    snprintf(
        fhttp->file_path,
        sizeof(fhttp->file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player/player_stats.json");

    fhttp->save_received_data = true;
    return flipper_http_get_request_with_headers(fhttp, url, "{\"Content-Type\":\"application/json\"}");
}

static bool start_game_thread(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "app is NULL");
        easy_flipper_dialog("Error", "app is NULL. Press BACK to return.");
        return false;
    }
    // free game thread
    if (game_thread_running)
    {
        game_thread_running = false;
        furi_thread_flags_set(thread_id, WorkerEvtStop);
        furi_thread_free(thread_id);
    }
    // start game thread
    FuriThread *thread = furi_thread_alloc_ex("game", 2048, game_app, app);
    if (!thread)
    {
        FURI_LOG_E(TAG, "Failed to allocate game thread");
        easy_flipper_dialog("Error", "Failed to allocate game thread. Restart your Flipper.");
        return false;
    }
    furi_thread_start(thread);
    thread_id = furi_thread_get_id(thread);
    game_thread_running = true;
    return true;
}
// combine register, login, and world list fetch into one function to switch to the loader view
static bool flip_world_fetch_game(DataLoaderModel *model)
{
    FlipWorldApp *app = (FlipWorldApp *)model->parser_context;
    if (!app)
    {
        FURI_LOG_E(TAG, "app is NULL");
        easy_flipper_dialog("Error", "app is NULL. Press BACK to return.");
        return false;
    }
    if (model->request_index == 0)
    {
        // login
        char username[64];
        char password[64];
        if (!load_char("Flip-Social-Username", username, sizeof(username)))
        {
            FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
            easy_flipper_dialog("Error", "Failed to load saved username\nGo to user settings to update.");
            return false;
        }
        if (!load_char("Flip-Social-Password", password, sizeof(password)))
        {
            FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
            easy_flipper_dialog("Error", "Failed to load saved password\nGo to settings to update.");
            return false;
        }
        char payload[256];
        snprintf(payload, sizeof(payload), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
        return flipper_http_post_request_with_headers(model->fhttp, "https://www.flipsocial.net/api/user/login/", "{\"Content-Type\":\"application/json\"}", payload);
    }
    else if (model->request_index == 1)
    {
        // check if login was successful
        char is_logged_in[8];
        if (!load_char("is_logged_in", is_logged_in, sizeof(is_logged_in)))
        {
            FURI_LOG_E(TAG, "Failed to load is_logged_in");
            easy_flipper_dialog("Error", "Failed to load is_logged_in\nGo to user settings to update.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
            return false;
        }
        if (strcmp(is_logged_in, "false") == 0 && strcmp(model->title, "Registering...") == 0)
        {
            // register
            char username[64];
            char password[64];
            if (!load_char("Flip-Social-Username", username, sizeof(username)))
            {
                FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
                easy_flipper_dialog("Error", "Failed to load saved username. Go to settings to update.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return false;
            }
            if (!load_char("Flip-Social-Password", password, sizeof(password)))
            {
                FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
                easy_flipper_dialog("Error", "Failed to load saved password. Go to settings to update.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return false;
            }
            char payload[172];
            snprintf(payload, sizeof(payload), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
            model->title = "Registering...";
            return flipper_http_post_request_with_headers(model->fhttp, "https://www.flipsocial.net/api/user/register/", "{\"Content-Type\":\"application/json\"}", payload);
        }
        else
        {
            model->title = "Fetching World List..";
            return fetch_world_list(model->fhttp);
        }
    }
    else if (model->request_index == 2)
    {
        model->title = "Fetching World List..";
        return fetch_world_list(model->fhttp);
    }
    else if (model->request_index == 3)
    {
        snprintf(
            model->fhttp->file_path,
            sizeof(model->fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

        FuriString *world_list = flipper_http_load_from_file(model->fhttp->file_path);
        if (!world_list)
        {
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
            FURI_LOG_E(TAG, "Failed to load world list");
            easy_flipper_dialog("Error", "Failed to load world list. Go to game settings to download packs.");
            return false;
        }
        FuriString *first_world = get_json_array_value_furi("worlds", 0, world_list);
        if (!first_world)
        {
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
            FURI_LOG_E(TAG, "Failed to get first world");
            easy_flipper_dialog("Error", "Failed to get first world. Go to game settings to download packs.");
            furi_string_free(world_list);
            return false;
        }
        if (world_exists(furi_string_get_cstr(first_world)))
        {
            furi_string_free(world_list);
            furi_string_free(first_world);

            if (!start_game_thread(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Failed to start game thread";
            }
            return true;
        }
        snprintf(
            model->fhttp->file_path,
            sizeof(model->fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", furi_string_get_cstr(first_world));

        model->fhttp->save_received_data = true;
        char url[128];
        snprintf(url, sizeof(url), "https://www.flipsocial.net/api/world/v3/get/world/%s/", furi_string_get_cstr(first_world));
        furi_string_free(world_list);
        furi_string_free(first_world);
        return flipper_http_get_request_with_headers(model->fhttp, url, "{\"Content-Type\":\"application/json\"}");
    }
    FURI_LOG_E(TAG, "Unknown request index");
    return false;
}
static char *flip_world_parse_game(DataLoaderModel *model)
{
    FlipWorldApp *app = (FlipWorldApp *)model->parser_context;

    if (model->request_index == 0)
    {
        if (!model->fhttp->last_response)
        {
            save_char("is_logged_in", "false");
            // Go back to the main menu
            easy_flipper_dialog("Error", "Response is empty. Press BACK to return.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
            return "Response is empty...";
        }

        // Check for successful conditions
        if (strstr(model->fhttp->last_response, "[SUCCESS]") != NULL || strstr(model->fhttp->last_response, "User found") != NULL)
        {
            save_char("is_logged_in", "true");
            model->title = "Login successful!";
            model->title = "Fetching World List..";
            return "Login successful!";
        }

        // Check if user not found
        if (strstr(model->fhttp->last_response, "User not found") != NULL)
        {
            save_char("is_logged_in", "false");
            model->title = "Registering...";
            return "Account not found...\nRegistering now.."; // if they see this an issue happened switching to register
        }

        // If not success, not found, check length conditions
        size_t resp_len = strlen(model->fhttp->last_response);
        if (resp_len == 0 || resp_len > 127)
        {
            // Empty or too long means failed login
            save_char("is_logged_in", "false");
            // Go back to the main menu
            easy_flipper_dialog("Error", "Failed to login. Press BACK to return.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
            return "Failed to login...";
        }

        // Handle any other unknown response as a failure
        save_char("is_logged_in", "false");
        // Go back to the main menu
        easy_flipper_dialog("Error", "Failed to login. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
        return "Failed to login...";
    }
    else if (model->request_index == 1)
    {
        if (strcmp(model->title, "Registering...") == 0)
        {
            // check registration response
            if (model->fhttp->last_response != NULL && (strstr(model->fhttp->last_response, "[SUCCESS]") != NULL || strstr(model->fhttp->last_response, "User created") != NULL))
            {
                save_char("is_logged_in", "true");
                char username[64];
                char password[64];
                // load the username and password, then save them
                if (!load_char("Flip-Social-Username", username, sizeof(username)))
                {
                    FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
                    easy_flipper_dialog("Error", "Failed to load Flip-Social-Username");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load Flip-Social-Username";
                }
                if (!load_char("Flip-Social-Password", password, sizeof(password)))
                {
                    FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
                    easy_flipper_dialog("Error", "Failed to load Flip-Social-Password");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load Flip-Social-Password";
                }
                // load wifi ssid,pass then save
                char ssid[64];
                char pass[64];
                if (!load_char("WiFi-SSID", ssid, sizeof(ssid)))
                {
                    FURI_LOG_E(TAG, "Failed to load WiFi-SSID");
                    easy_flipper_dialog("Error", "Failed to load WiFi-SSID");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load WiFi-SSID";
                }
                if (!load_char("WiFi-Password", pass, sizeof(pass)))
                {
                    FURI_LOG_E(TAG, "Failed to load WiFi-Password");
                    easy_flipper_dialog("Error", "Failed to load WiFi-Password");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load WiFi-Password";
                }
                save_settings(ssid, pass, username, password);
                model->title = "Fetching World List..";
                return "Account created!";
            }
            else if (strstr(model->fhttp->last_response, "Username or password not provided") != NULL)
            {
                easy_flipper_dialog("Error", "Please enter your credentials.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Please enter your credentials.";
            }
            else if (strstr(model->fhttp->last_response, "User already exists") != NULL || strstr(model->fhttp->last_response, "Multiple users found") != NULL)
            {
                easy_flipper_dialog("Error", "Registration failed...\nUsername already exists.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Username already exists.";
            }
            else
            {
                easy_flipper_dialog("Error", "Registration failed...\nUpdate your credentials.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Registration failed...";
            }
        }
        else
        {
            if (!start_game_thread(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Failed to start game thread";
            }
            return "Thanks for playing FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
        }
    }
    else if (model->request_index == 2)
    {
        return "Welcome to FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
    }
    else if (model->request_index == 3)
    {
        if (!start_game_thread(app))
        {
            FURI_LOG_E(TAG, "Failed to start game thread");
            easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
            return "Failed to start game thread";
        }
        return "Thanks for playing FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
    }
    easy_flipper_dialog("Error", "Unknown error. Press BACK to return.");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
    return "Unknown error";
}
void flip_world_switch_to_view_get_game(FlipWorldApp *app)
{
    flip_world_generic_switch_to_view(app, "Starting Game..", flip_world_fetch_game, flip_world_parse_game, 5, callback_to_submenu, FlipWorldViewLoader);
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
        free_all_views(app, true, true);
        if (!is_enough_heap(45000)) // lowered from 60k to 45k since we saved 15k bytes
        {
            easy_flipper_dialog("Error", "Not enough heap memory.\nPlease restart your Flipper.");
            return;
        }
        // check if logged in
        if (is_logged_in() || is_logged_in_to_flip_social())
        {
            FlipperHTTP *fhttp = flipper_http_alloc();
            if (!fhttp)
            {
                FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
                easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP. Press BACK to return.");
                return;
            }
            bool fetch_world_list_i()
            {
                return fetch_world_list(fhttp);
            }
            bool parse_world_list_i()
            {
                return fhttp->state != ISSUE;
            }

            bool fetch_player_stats_i()
            {
                return fetch_player_stats(fhttp);
            }

            Loading *loading;
            int32_t loading_view_id = 987654321; // Random ID

            loading = loading_alloc();
            if (!loading)
            {
                FURI_LOG_E(HTTP_TAG, "Failed to allocate loading");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                flipper_http_free(fhttp);
                return;
            }

            view_dispatcher_add_view(app->view_dispatcher, loading_view_id, loading_get_view(loading));

            // Switch to the loading view
            view_dispatcher_switch_to_view(app->view_dispatcher, loading_view_id);

            // Make the request
            if (!flipper_http_process_response_async(fhttp, fetch_world_list_i, parse_world_list_i) ||
                !flipper_http_process_response_async(fhttp, fetch_player_stats_i, set_player_context))
            {
                FURI_LOG_E(HTTP_TAG, "Failed to make request");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
                loading_free(loading);
                flipper_http_free(fhttp);
            }
            else
            {
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
                loading_free(loading);
                flipper_http_free(fhttp);
            }
            if (!start_game_thread(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                return;
            }

            easy_flipper_dialog("Starting Game", "Please wait...");
        }
        else
        {
            flip_world_switch_to_view_get_game(app);
        }
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
        free_all_views(app, true, false);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexUserSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    default:
        break;
    }
}

static void text_updated_wifi_ssid(void *context)
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
        char username[64];
        char password[64];
        if (load_char("WiFi-Password", pass, sizeof(pass)))
        {
            if (strlen(pass) > 0 && strlen(app->text_input_buffer) > 0)
            {
                // save the settings
                load_char("Flip-Social-Username", username, sizeof(username));
                load_char("Flip-Social-Password", password, sizeof(password));
                save_settings(app->text_input_buffer, pass, username, password);

                // initialize the http
                FlipperHTTP *fhttp = flipper_http_alloc();
                if (fhttp)
                {
                    // save the wifi if the device is connected
                    if (!flipper_http_save_wifi(fhttp, app->text_input_buffer, pass))
                    {
                        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
                    }

                    // free the resources
                    flipper_http_free(fhttp);
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
static void text_updated_wifi_pass(void *context)
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
    char username[64];
    char password[64];
    if (load_char("WiFi-SSID", ssid, sizeof(ssid)))
    {
        if (strlen(ssid) > 0 && strlen(app->text_input_buffer) > 0)
        {
            // save the settings
            load_char("Flip-Social-Username", username, sizeof(username));
            load_char("Flip-Social-Password", password, sizeof(password));
            save_settings(ssid, app->text_input_buffer, username, password);

            // initialize the http
            FlipperHTTP *fhttp = flipper_http_alloc();
            if (fhttp)
            {
                // save the wifi if the device is connected
                if (!flipper_http_save_wifi(fhttp, ssid, app->text_input_buffer))
                {
                    easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
                }

                // free the resources
                flipper_http_free(fhttp);
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
static void text_updated_username(void *context)
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
    save_char("Flip-Social-Username", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_user_username)
    {
        variable_item_set_current_value_text(app->variable_item_user_username, app->text_input_buffer);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList); // back to user settings
}
static void text_updated_password(void *context)
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
    save_char("Flip-Social-Password", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_user_password)
    {
        variable_item_set_current_value_text(app->variable_item_user_password, app->text_input_buffer);
    }

    // get value of username
    char username[64];
    char ssid[64];
    char pass[64];
    if (load_char("Flip-Social-Username", username, sizeof(username)))
    {
        if (strlen(username) > 0 && strlen(app->text_input_buffer) > 0)
        {
            // save the settings
            load_char("WiFi-SSID", ssid, sizeof(ssid));
            load_char("WiFi-Password", pass, sizeof(pass));
            save_settings(ssid, pass, username, app->text_input_buffer);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList); // back to user settings
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
    char username[64];
    char password[64];
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
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
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
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
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
    game_fps_index = index;
    variable_item_set_current_value_text(item, game_fps_choices[index]);
    variable_item_set_current_value_index(item, index);

    // save the fps
    save_char("Game-FPS", game_fps_choices[index]);
}
static void flip_world_game_screen_always_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    game_screen_always_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);

    // save the screen always on
    save_char("Game-Screen-Always-On", yes_or_no_choices[index]);
}
static void flip_world_game_sound_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    game_sound_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);

    // save the screen always on
    save_char("Game-Sound-On", yes_or_no_choices[index]);
}
static void flip_world_game_vibration_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    game_vibration_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);

    // save the screen always on
    save_char("Game-Vibration-On", yes_or_no_choices[index]);
}

static bool flip_world_fetch_worlds(DataLoaderModel *model)
{
    if (!model || !model->fhttp)
    {
        FURI_LOG_E(TAG, "model or fhttp is NULL");
        return false;
    }
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
    snprintf(
        model->fhttp->file_path,
        sizeof(model->fhttp->file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list_full.json");
    model->fhttp->save_received_data = true;
    return flipper_http_get_request_with_headers(model->fhttp, "https://www.flipsocial.net/api/world/v3/get/10/", "{\"Content-Type\":\"application/json\"}");
}
static char *flip_world_parse_worlds(DataLoaderModel *model)
{
    UNUSED(model);
    return "World Pack Installed";
}
static void flip_world_switch_to_view_get_worlds(FlipWorldApp *app)
{
    flip_world_generic_switch_to_view(app, "Fetching World Pack..", flip_world_fetch_worlds, flip_world_parse_worlds, 1, callback_to_submenu, FlipWorldViewLoader);
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
    case 0: // Download all world data s one huge json
        flip_world_switch_to_view_get_worlds(app);
    case 1: // Change FPS
        break;
    case 2: // Screen Always On
        break;
    }
}
static void user_settings_item_selected(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    switch (index)
    {
    case 0: // Username
        free_all_views(app, false, false);
        if (!alloc_text_input_view(app, "Username-Login"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    case 1: // Password
        free_all_views(app, false, false);
        if (!alloc_text_input_view(app, "Password-Login"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
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

    DataLoaderModel *data_loader_model = (DataLoaderModel *)model;
    SerialState http_state = data_loader_model->fhttp->state;
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
        flip_world_request_error_draw(canvas, data_loader_model);
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
    DataLoaderModel *loader_model = NULL;
    with_view_model(view, DataLoaderModel * model, { current_data_state = model->data_state; loader_model = model; }, false);
    if (!loader_model || !loader_model->fhttp)
    {
        FURI_LOG_E(TAG, "Model or fhttp is NULL");
        DEV_CRASH();
        return;
    }

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
                strncpy(model->fhttp->last_response, "", 1);
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
        if (loader_model->fhttp->state == IDLE && loader_model->fhttp->last_response != NULL)
        {
            if (strstr(loader_model->fhttp->last_response, "[PONG]") != NULL)
            {
                FURI_LOG_DEV(TAG, "PONG received.");
            }
            else if (strncmp(loader_model->fhttp->last_response, "[SUCCESS]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "SUCCESS received. %s", loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            }
            else if (strncmp(loader_model->fhttp->last_response, "[ERROR]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "ERROR received. %s", loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            }
            else if (strlen(loader_model->fhttp->last_response) == 0)
            {
                // Still waiting on response
            }
            else
            {
                with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateReceived; }, true);
            }
        }
        else if (loader_model->fhttp->state == SENDING || loader_model->fhttp->state == RECEIVING)
        {
            // continue waiting
        }
        else if (loader_model->fhttp->state == INACTIVE)
        {
            // inactive. try again
        }
        else if (loader_model->fhttp->state == ISSUE)
        {
            with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", loader_model->fhttp->state, loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
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
                FURI_LOG_DEV(TAG, "Parsed data: %s\r\ntext: %s", model->fhttp->last_response ? model->fhttp->last_response : "NULL", data_text ? data_text : "NULL");
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
                // do not free the context here, it is the app context
                // free(model->parser_context);
                // model->parser_context = NULL;
            }
            if (model->fhttp)
            {
                flipper_http_free(model->fhttp);
                model->fhttp = NULL;
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
            //
            model->parser_context = app;
            if (!model->fhttp)
            {
                model->fhttp = flipper_http_alloc();
            }
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
