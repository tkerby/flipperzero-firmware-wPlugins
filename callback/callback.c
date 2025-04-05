#include <callback/callback.h>
#include <callback/loader.h>
#include "engine/engine.h"
#include "engine/game_engine.h"
#include "engine/game_manager_i.h"
#include "engine/level_i.h"
#include "engine/entity_i.h"
#include "game/storage.h"
#include "alloc/alloc.h"

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
    settings.target_fps = atof_(fps_choices_str[fps_index]);
    settings.show_fps = game.show_fps;
    settings.always_backlight = strstr(yes_or_no_choices[screen_always_on_index], "Yes") != NULL;
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

static bool alloc_message_view(void *context, MessageState state);
static bool alloc_text_input_view(void *context, char *title);
static bool alloc_variable_item_list(void *context, uint32_t view_id);
//
static void callback_submenu_lobby_choices(void *context, uint32_t index);
//
static void wifi_settings_select(void *context, uint32_t index);
static void updated_wifi_ssid(void *context);
static void updated_wifi_pass(void *context);
static void updated_username(void *context);
static void updated_password(void *context);
//
static void fps_change(VariableItem *item);
static void game_settings_select(void *context, uint32_t index);
static void user_settings_select(void *context, uint32_t index);
static void screen_on_change(VariableItem *item);
static void sound_on_change(VariableItem *item);
static void vibration_on_change(VariableItem *item);
static void player_on_change(VariableItem *item);
static void vgm_x_change(VariableItem *item);
static void vgm_y_change(VariableItem *item);
//
static uint8_t timer_iteration = 0; // timer iteration for the loading screen
static uint8_t timer_refresh = 5;   // duration for timer to refresh
//
void waiting_loader_process_callback(FlipperHTTP *fhttp, void *context);
static void waiting_lobby(void *context);
static uint32_t lobby_index = -1;
static char *lobby_list[10];
static bool fetch_lobby(FlipperHTTP *fhttp, char *lobby_name);
bool user_hit_back = false;
static bool message_input_callback(InputEvent *event, void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (event->key == InputKeyBack)
    {
        FURI_LOG_I(TAG, "Message view - BACK pressed");
        user_hit_back = true;
    }
    return true;
}
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
    return FlipWorldViewSubmenuOther;
}
static int32_t waiting_app_callback(void *p)
{
    FlipWorldApp *app = (FlipWorldApp *)p;
    furi_check(app);
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP");
        return -1;
    }
    user_hit_back = false;
    timer_iteration = 0;
    while (timer_iteration < 60 && !user_hit_back)
    {
        FURI_LOG_I(TAG, "Waiting for more players...");
        waiting_loader_process_callback(fhttp, app);
        FURI_LOG_I(TAG, "Waiting for more players... %d", timer_iteration);
        timer_iteration++;
        furi_delay_ms(1000 * timer_refresh);
    }
    // if we reach here, it means we timed out or the user hit back
    FURI_LOG_E(TAG, "No players joined within the timeout or user hit back");
    remove_player_from_lobby(fhttp);
    flipper_http_free(fhttp);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
    return 0;
}
static void message_draw_callback(Canvas *canvas, void *model)
{
    MessageModel *message_model = model;
    canvas_clear(canvas);
    if (message_model->message_state == MessageStateAbout)
    {
        canvas_draw_str(canvas, 0, 10, VERSION_TAG);
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str(canvas, 0, 20, "Dev: JBlanked, codeallnight");
        canvas_draw_str(canvas, 0, 30, "GFX: the1anonlypr3");
        canvas_draw_str(canvas, 0, 40, "github.com/jblanked/FlipWorld");

        canvas_draw_str_multi(canvas, 0, 55, "The first open world multiplayer\ngame on the Flipper Zero.");
    }
    else if (message_model->message_state == MessageStateLoading)
    {
        canvas_set_font(canvas, FontPrimary);
        if (game_mode_index != 1)
        {
            canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Starting FlipWorld");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 50, "Please wait while your");
            canvas_draw_str(canvas, 0, 60, "game is started.");
        }
        else
        {
            canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Loading Lobbies");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 60, "Please wait....");
        }
    }
    // well this is only called once so let's make a while loop
    else if (message_model->message_state == MessageStateWaitingLobby)
    {
        canvas_draw_str(canvas, 0, 10, "Waiting for more players...");
        // time elapsed based on timer_iteration and timer_refresh
        // char str[32];
        // snprintf(str, sizeof(str), "Time elapsed: %d seconds", timer_iteration * timer_refresh);
        // canvas_draw_str(canvas, 0, 50, str);
        canvas_draw_str(canvas, 0, 60, "Press BACK to cancel.");
        canvas_commit(canvas); // make sure message is drawn
    }
    else
    {
        canvas_draw_str(canvas, 0, 10, "Unknown message state");
    }
}

// alloc
static bool alloc_message_view(void *context, MessageState state)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->view_message)
    {
        FURI_LOG_E(TAG, "Message view already allocated");
        return false;
    }
    switch (state)
    {
    case MessageStateAbout:
        easy_flipper_set_view(&app->view_message, FlipWorldViewMessage, message_draw_callback, NULL, callback_to_submenu, &app->view_dispatcher, app);
        break;
    case MessageStateLoading:
        easy_flipper_set_view(&app->view_message, FlipWorldViewMessage, message_draw_callback, NULL, NULL, &app->view_dispatcher, app);
        break;
    case MessageStateWaitingLobby:
        easy_flipper_set_view(&app->view_message, FlipWorldViewMessage, message_draw_callback, message_input_callback, NULL, &app->view_dispatcher, app);
        break;
    }
    if (!app->view_message)
    {
        FURI_LOG_E(TAG, "Failed to allocate message view");
        return false;
    }
    view_allocate_model(app->view_message, ViewModelTypeLockFree, sizeof(MessageModel));
    MessageModel *model = view_get_model(app->view_message);
    model->message_state = state;
    return true;
}

static bool alloc_text_input_view(void *context, char *title)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
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
                is_str(title, "SSID") ? updated_wifi_ssid : is_str(title, "Password")     ? updated_wifi_pass
                                                        : is_str(title, "Username-Login") ? updated_username
                                                                                          : updated_password,
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
            if (is_str(title, "SSID"))
            {
                strncpy(app->text_input_temp_buffer, ssid, app->text_input_buffer_size);
            }
            else if (is_str(title, "Password"))
            {
                strncpy(app->text_input_temp_buffer, pass, app->text_input_buffer_size);
            }
            else if (is_str(title, "Username-Login"))
            {
                strncpy(app->text_input_temp_buffer, username, app->text_input_buffer_size);
            }
            else if (is_str(title, "Password-Login"))
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
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, wifi_settings_select, callback_to_settings, &app->view_dispatcher, app))
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
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, game_settings_select, callback_to_settings, &app->view_dispatcher, app))
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
            if (!app->variable_item_game_player_sprite)
            {
                app->variable_item_game_player_sprite = variable_item_list_add(app->variable_item_list, "Weapon", 4, player_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_player_sprite, 1);
                variable_item_set_current_value_text(app->variable_item_game_player_sprite, player_sprite_choices[1]);
            }
            if (!app->variable_item_game_fps)
            {
                app->variable_item_game_fps = variable_item_list_add(app->variable_item_list, "FPS", 4, fps_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_fps, 0);
                variable_item_set_current_value_text(app->variable_item_game_fps, fps_choices_str[0]);
            }
            if (!app->variable_item_game_vgm_x)
            {
                app->variable_item_game_vgm_x = variable_item_list_add(app->variable_item_list, "VGM Horizontal", 12, vgm_x_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vgm_x, 2);
                variable_item_set_current_value_text(app->variable_item_game_vgm_x, vgm_levels[2]);
            }
            if (!app->variable_item_game_vgm_y)
            {
                app->variable_item_game_vgm_y = variable_item_list_add(app->variable_item_list, "VGM Vertical", 12, vgm_y_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vgm_y, 2);
                variable_item_set_current_value_text(app->variable_item_game_vgm_y, vgm_levels[2]);
            }
            if (!app->variable_item_game_screen_always_on)
            {
                app->variable_item_game_screen_always_on = variable_item_list_add(app->variable_item_list, "Keep Screen On?", 2, screen_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_screen_always_on, 1);
                variable_item_set_current_value_text(app->variable_item_game_screen_always_on, yes_or_no_choices[1]);
            }
            if (!app->variable_item_game_sound_on)
            {
                app->variable_item_game_sound_on = variable_item_list_add(app->variable_item_list, "Sound On?", 2, sound_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_sound_on, 0);
                variable_item_set_current_value_text(app->variable_item_game_sound_on, yes_or_no_choices[0]);
            }
            if (!app->variable_item_game_vibration_on)
            {
                app->variable_item_game_vibration_on = variable_item_list_add(app->variable_item_list, "Vibration On?", 2, vibration_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vibration_on, 0);
                variable_item_set_current_value_text(app->variable_item_game_vibration_on, yes_or_no_choices[0]);
            }
            char _game_player_sprite[8];
            if (load_char("Game-Player-Sprite", _game_player_sprite, sizeof(_game_player_sprite)))
            {
                int index = is_str(_game_player_sprite, "naked") ? 0 : is_str(_game_player_sprite, "sword") ? 1
                                                                   : is_str(_game_player_sprite, "axe")     ? 2
                                                                   : is_str(_game_player_sprite, "bow")     ? 3
                                                                                                            : 0;
                variable_item_set_current_value_index(app->variable_item_game_player_sprite, index);
                variable_item_set_current_value_text(
                    app->variable_item_game_player_sprite,
                    is_str(player_sprite_choices[index], "naked") ? "None" : player_sprite_choices[index]);
            }
            char _game_fps[8];
            if (load_char("Game-FPS", _game_fps, sizeof(_game_fps)))
            {
                int index = is_str(_game_fps, "30") ? 0 : is_str(_game_fps, "60") ? 1
                                                      : is_str(_game_fps, "120")  ? 2
                                                      : is_str(_game_fps, "240")  ? 3
                                                                                  : 0;
                variable_item_set_current_value_text(app->variable_item_game_fps, fps_choices_str[index]);
                variable_item_set_current_value_index(app->variable_item_game_fps, index);
            }
            char _game_vgm_x[8];
            if (load_char("Game-VGM-X", _game_vgm_x, sizeof(_game_vgm_x)))
            {
                int vgm_x = atoi(_game_vgm_x);
                int index = vgm_x == -2 ? 0 : vgm_x == -1 ? 1
                                          : vgm_x == 0    ? 2
                                          : vgm_x == 1    ? 3
                                          : vgm_x == 2    ? 4
                                          : vgm_x == 3    ? 5
                                          : vgm_x == 4    ? 6
                                          : vgm_x == 5    ? 7
                                          : vgm_x == 6    ? 8
                                          : vgm_x == 7    ? 9
                                          : vgm_x == 8    ? 10
                                          : vgm_x == 9    ? 11
                                          : vgm_x == 10   ? 12
                                                          : 2;
                variable_item_set_current_value_index(app->variable_item_game_vgm_x, index);
                variable_item_set_current_value_text(app->variable_item_game_vgm_x, vgm_levels[index]);
            }
            char _game_vgm_y[8];
            if (load_char("Game-VGM-Y", _game_vgm_y, sizeof(_game_vgm_y)))
            {
                int vgm_y = atoi(_game_vgm_y);
                int index = vgm_y == -2 ? 0 : vgm_y == -1 ? 1
                                          : vgm_y == 0    ? 2
                                          : vgm_y == 1    ? 3
                                          : vgm_y == 2    ? 4
                                          : vgm_y == 3    ? 5
                                          : vgm_y == 4    ? 6
                                          : vgm_y == 5    ? 7
                                          : vgm_y == 6    ? 8
                                          : vgm_y == 7    ? 9
                                          : vgm_y == 8    ? 10
                                          : vgm_y == 9    ? 11
                                          : vgm_y == 10   ? 12
                                                          : 2;
                variable_item_set_current_value_index(app->variable_item_game_vgm_y, index);
                variable_item_set_current_value_text(app->variable_item_game_vgm_y, vgm_levels[index]);
            }
            char _game_screen_always_on[8];
            if (load_char("Game-Screen-Always-On", _game_screen_always_on, sizeof(_game_screen_always_on)))
            {
                int index = is_str(_game_screen_always_on, "No") ? 0 : is_str(_game_screen_always_on, "Yes") ? 1
                                                                                                             : 0;
                variable_item_set_current_value_text(app->variable_item_game_screen_always_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_screen_always_on, index);
            }
            char _game_sound_on[8];
            if (load_char("Game-Sound-On", _game_sound_on, sizeof(_game_sound_on)))
            {
                int index = is_str(_game_sound_on, "No") ? 0 : is_str(_game_sound_on, "Yes") ? 1
                                                                                             : 0;
                variable_item_set_current_value_text(app->variable_item_game_sound_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_sound_on, index);
            }
            char _game_vibration_on[8];
            if (load_char("Game-Vibration-On", _game_vibration_on, sizeof(_game_vibration_on)))
            {
                int index = is_str(_game_vibration_on, "No") ? 0 : is_str(_game_vibration_on, "Yes") ? 1
                                                                                                     : 0;
                variable_item_set_current_value_text(app->variable_item_game_vibration_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_vibration_on, index);
            }
            break;
        case FlipWorldSubmenuIndexUserSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, user_settings_select, callback_to_settings, &app->view_dispatcher, app))
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
static bool alloc_submenu_other(void *context, uint32_t view_id)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->submenu_other)
    {
        FURI_LOG_I(TAG, "Submenu already allocated");
        return true;
    }
    switch (view_id)
    {
    case FlipWorldViewSettings:
        if (!easy_flipper_set_submenu(&app->submenu_other, FlipWorldViewSubmenuOther, "Settings", callback_to_submenu, &app->view_dispatcher))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenu settings");
            return false;
        }
        submenu_add_item(app->submenu_other, "WiFi", FlipWorldSubmenuIndexWiFiSettings, callback_submenu_choices, app);
        submenu_add_item(app->submenu_other, "Game", FlipWorldSubmenuIndexGameSettings, callback_submenu_choices, app);
        submenu_add_item(app->submenu_other, "User", FlipWorldSubmenuIndexUserSettings, callback_submenu_choices, app);
        return true;
    case FlipWorldViewLobby:
        return easy_flipper_set_submenu(&app->submenu_other, FlipWorldViewSubmenuOther, "Lobbies", callback_to_submenu, &app->view_dispatcher);
    default:
        return false;
    }
}

static bool alloc_game_submenu(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (!app->submenu_game)
    {
        if (!easy_flipper_set_submenu(&app->submenu_game, FlipWorldViewGameSubmenu, "Play", callback_to_submenu, &app->view_dispatcher))
        {
            return false;
        }
        if (!app->submenu_game)
        {
            return false;
        }
        submenu_add_item(app->submenu_game, "Tutorial", FlipWorldSubmenuIndexStory, callback_submenu_choices, app);
        submenu_add_item(app->submenu_game, "PvP (Beta)", FlipWorldSubmenuIndexPvP, callback_submenu_choices, app);
        submenu_add_item(app->submenu_game, "PvE", FlipWorldSubmenuIndexPvE, callback_submenu_choices, app);
    }
    return true;
}

void free_game_submenu(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->submenu_game)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewGameSubmenu);
        submenu_free(app->submenu_game);
        app->submenu_game = NULL;
    }
}

static void free_submenu_other(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->submenu_other)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        submenu_free(app->submenu_other);
        app->submenu_other = NULL;
    }
    for (int i = 0; i < 10; i++)
    {
        if (lobby_list[i])
        {
            free(lobby_list[i]);
            lobby_list[i] = NULL;
        }
    }
}
// free
static void free_message_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (app->view_message)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewMessage);
        view_free(app->view_message);
        app->view_message = NULL;
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
    if (app->variable_item_game_player_sprite)
    {
        free(app->variable_item_game_player_sprite);
        app->variable_item_game_player_sprite = NULL;
    }
    if (app->variable_item_game_vgm_x)
    {
        free(app->variable_item_game_vgm_x);
        app->variable_item_game_vgm_x = NULL;
    }
    if (app->variable_item_game_vgm_y)
    {
        free(app->variable_item_game_vgm_y);
        app->variable_item_game_vgm_y = NULL;
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

static FuriThread *game_thread;
static FuriThread *waiting_thread;
static bool game_thread_running = false;
static bool waiting_thread_running = false;
static bool start_waiting_thread(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    // free game thread
    if (waiting_thread_running)
    {
        waiting_thread_running = false;
        if (waiting_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(waiting_thread), WorkerEvtStop);
            furi_thread_join(waiting_thread);
            furi_thread_free(waiting_thread);
        }
    }
    // start waiting thread
    FuriThread *thread = furi_thread_alloc_ex("waiting_thread", 2048, waiting_app_callback, app);
    if (!thread)
    {
        FURI_LOG_E(TAG, "Failed to allocate waiting thread");
        easy_flipper_dialog("Error", "Failed to allocate waiting thread. Restart your Flipper.");
        return false;
    }
    furi_thread_start(thread);
    waiting_thread = thread;
    waiting_thread_running = true;
    return true;
}
void free_all_views(void *context, bool free_variable_list, bool free_settings_other, bool free_submenu_game)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (free_variable_list)
    {
        free_variable_item_list(app);
    }
    free_message_view(app);
    free_text_input_view(app);

    // free game thread
    if (game_thread_running)
    {
        game_thread_running = false;
        if (game_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(game_thread), WorkerEvtStop);
            furi_thread_join(game_thread);
            furi_thread_free(game_thread);
            game_thread = NULL;
        }
    }

    if (free_settings_other)
    {
        free_submenu_other(app);
    }

    // free Derek's loader
    free_view_loader(app);

    if (free_submenu_game)
    {
        free_game_submenu(app);
    }

    // free waiting thread
    if (waiting_thread_running)
    {
        waiting_thread_running = false;
        if (waiting_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(waiting_thread), WorkerEvtStop);
            furi_thread_join(waiting_thread);
            furi_thread_free(waiting_thread);
            waiting_thread = NULL;
        }
    }
}
static bool fetch_world_list(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
        return false;
    }

    // ensure flip_world directory exists
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);

    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

    fhttp->save_received_data = true;
    return flipper_http_request(fhttp, GET, "https://www.jblanked.com/flipper/api/world/v5/list/10/", "{\"Content-Type\":\"application/json\"}", NULL);
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
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/user/game-stats/%s/", username);

    // ensure the folders exist
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);

    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player/player_stats.json");
    fhttp->save_received_data = true;
    return flipper_http_request(fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL);
}

// static bool fetch_app_update(FlipperHTTP *fhttp)
// {
//     if (!fhttp)
//     {
//         FURI_LOG_E(TAG, "fhttp is NULL");
//         easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
//         return false;
//     }

//     return flipper_http_get_request_with_headers(fhttp, "https://www.jblanked.com/flipper/api/app/last-updated/flip_world/", "{\"Content-Type\":\"application/json\"}");
// }

// static bool parse_app_update(FlipperHTTP *fhttp)
// {
//     if (!fhttp)
//     {
//         FURI_LOG_E(TAG, "fhttp is NULL");
//         easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
//         return false;
//     }
//     if (fhttp->last_response == NULL || strlen(fhttp->last_response) == 0)
//     {
//         FURI_LOG_E(TAG, "fhttp->last_response is NULL or empty");
//         easy_flipper_dialog("Error", "fhttp->last_response is NULL or empty. Press BACK to return.");
//         return false;
//     }
//     bool last_update_available = false;
//     char last_updated_old[32];
//     // load the previous last_updated
//     if (!load_char("last_updated", last_updated_old, sizeof(last_updated_old)))
//     {
//         FURI_LOG_E(TAG, "Failed to load last_updated");
//         // it's okay, we'll just update it
//     }
//     // save the new last_updated
//     save_char("last_updated", fhttp->last_response);

//     // compare the two
//     if (strlen(last_updated_old) == 0 || !is_str(last_updated_old, fhttp->last_response))
//     {
//         last_update_available = true;
//     }

//     if (last_update_available)
//     {
//         easy_flipper_dialog("Update Available", "An update is available. Press OK to update.");
//         return true;
//     }
//     else
//     {
//         easy_flipper_dialog("No Update Available", "No update is available. Press OK to continue.");
//         return false;
//     }
// }

static bool start_game_thread(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "app is NULL");
        easy_flipper_dialog("Error", "app is NULL. Press BACK to return.");
        return false;
    }

    // free everything but message_view
    free_variable_item_list(app);
    free_text_input_view(app);
    // free_submenu_other(app); // free lobby list or settings
    free_view_loader(app);
    free_game_submenu(app);

    // free game thread
    if (game_thread_running)
    {
        game_thread_running = false;
        if (game_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(game_thread), WorkerEvtStop);
            furi_thread_join(game_thread);
            furi_thread_free(game_thread);
        }
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
    game_thread = thread;
    game_thread_running = true;
    return true;
}
// combine register, login, and world list fetch into one function to switch to the loader view
static bool _fetch_game(DataLoaderModel *model)
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
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            easy_flipper_dialog("Error", "Failed to load saved username\nGo to user settings to update.");
            return false;
        }
        if (!load_char("Flip-Social-Password", password, sizeof(password)))
        {
            FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            easy_flipper_dialog("Error", "Failed to load saved password\nGo to settings to update.");
            return false;
        }
        char payload[256];
        snprintf(payload, sizeof(payload), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
        return flipper_http_request(model->fhttp, POST, "https://www.jblanked.com/flipper/api/user/login/", "{\"Content-Type\":\"application/json\"}", payload);
    }
    else if (model->request_index == 1)
    {
        // check if login was successful
        char is_logged_in[8];
        if (!load_char("is_logged_in", is_logged_in, sizeof(is_logged_in)))
        {
            FURI_LOG_E(TAG, "Failed to load is_logged_in");
            easy_flipper_dialog("Error", "Failed to load is_logged_in\nGo to user settings to update.");
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            return false;
        }
        if (is_str(is_logged_in, "false") && is_str(model->title, "Registering..."))
        {
            // register
            char username[64];
            char password[64];
            if (!load_char("Flip-Social-Username", username, sizeof(username)))
            {
                FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
                easy_flipper_dialog("Error", "Failed to load saved username. Go to settings to update.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return false;
            }
            if (!load_char("Flip-Social-Password", password, sizeof(password)))
            {
                FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
                easy_flipper_dialog("Error", "Failed to load saved password. Go to settings to update.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return false;
            }
            char payload[172];
            snprintf(payload, sizeof(payload), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
            model->title = "Registering...";
            return flipper_http_request(model->fhttp, POST, "https://www.jblanked.com/flipper/api/user/register/", "{\"Content-Type\":\"application/json\"}", payload);
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
        snprintf(model->fhttp->file_path, sizeof(model->fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

        FuriString *world_list = flipper_http_load_from_file(model->fhttp->file_path);
        if (!world_list)
        {
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            FURI_LOG_E(TAG, "Failed to load world list");
            easy_flipper_dialog("Error", "Failed to load world list. Go to game settings to download packs.");
            return false;
        }
        FuriString *first_world = get_json_array_value_furi("worlds", 0, world_list);
        if (!first_world)
        {
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
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
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Failed to start game thread";
            }
            return true;
        }
        snprintf(model->fhttp->file_path, sizeof(model->fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", furi_string_get_cstr(first_world));

        model->fhttp->save_received_data = true;
        char url[128];
        snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/v5/get/world/%s/", furi_string_get_cstr(first_world));
        furi_string_free(world_list);
        furi_string_free(first_world);
        return flipper_http_request(model->fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL);
    }
    FURI_LOG_E(TAG, "Unknown request index");
    return false;
}
static char *_parse_game(DataLoaderModel *model)
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
        if (is_str(model->title, "Registering..."))
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
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Please enter your credentials.";
            }
            else if (strstr(model->fhttp->last_response, "User already exists") != NULL || strstr(model->fhttp->last_response, "Multiple users found") != NULL)
            {
                easy_flipper_dialog("Error", "Registration failed...\nUsername already exists.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Username already exists.";
            }
            else
            {
                easy_flipper_dialog("Error", "Registration failed...\nUpdate your credentials.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Registration failed...";
            }
        }
        else
        {
            if (!start_game_thread(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
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
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            return "Failed to start game thread";
        }
        return "Thanks for playing FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
    }
    easy_flipper_dialog("Error", "Unknown error. Press BACK to return.");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
    return "Unknown error";
}
static void switch_to_view_get_game(FlipWorldApp *app)
{
    if (!alloc_view_loader(app))
    {
        FURI_LOG_E(TAG, "Failed to allocate view loader");
        return;
    }
    loader_switch_to_view(app, "Starting Game..", _fetch_game, _parse_game, 5, callback_to_submenu, FlipWorldViewLoader);
}
static void run(FlipWorldApp *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    free_all_views(app, true, true, false);
    // only need to check if they have 50k free (game needs about 38k currently)
    if (!is_enough_heap(50000, false))
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

        if (!alloc_message_view(app, MessageStateLoading))
        {
            FURI_LOG_E(TAG, "Failed to allocate message view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMessage);

        // Make the request
        if (game_mode_index != 1) // not GAME_MODE_PVP
        {
            if (!flipper_http_process_response_async(fhttp, fetch_world_list_i, parse_world_list_i) || !flipper_http_process_response_async(fhttp, fetch_player_stats_i, set_player_context))
            {
                FURI_LOG_E(HTTP_TAG, "Failed to make request");
                flipper_http_free(fhttp);
            }
            else
            {
                flipper_http_free(fhttp);
            }

            if (!start_game_thread(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                return;
            }
        }
        else
        {
            // load pvp info (this returns the lobbies available)
            bool fetch_pvp_lobbies()
            {
                // ensure flip_world directory exists
                char directory_path[128];
                snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
                Storage *storage = furi_record_open(RECORD_STORAGE);
                storage_common_mkdir(storage, directory_path);
                snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/pvp");
                storage_common_mkdir(storage, directory_path);
                snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/pvp/lobbies");
                storage_common_mkdir(storage, directory_path);
                furi_record_close(RECORD_STORAGE);
                snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/pvp/pvp_lobbies.json");
                storage_simply_remove_recursive(storage, fhttp->file_path); // ensure the file is empty
                fhttp->save_received_data = true;
                // 2 players max, 10 lobbies
                return flipper_http_request(fhttp, GET, "https://www.jblanked.com/flipper/api/world/pvp/lobbies/2/10/", "{\"Content-Type\":\"application/json\"}", NULL);
            }

            bool parse_pvp_lobbies()
            {

                free_submenu_other(app);
                if (!alloc_submenu_other(app, FlipWorldViewLobby))
                {
                    FURI_LOG_E(TAG, "Failed to allocate lobby submenu");
                    return false;
                }

                // add the lobbies to the submenu
                FuriString *lobbies = flipper_http_load_from_file(fhttp->file_path);
                if (!lobbies)
                {
                    FURI_LOG_E(TAG, "Failed to load lobbies");
                    return false;
                }

                // parse the lobbies
                for (uint32_t i = 0; i < 10; i++)
                {
                    FuriString *lobby = get_json_array_value_furi("lobbies", i, lobbies);
                    if (!lobby)
                    {
                        FURI_LOG_I(TAG, "No more lobbies");
                        break;
                    }
                    FuriString *lobby_id = get_json_value_furi("id", lobby);
                    if (!lobby_id)
                    {
                        FURI_LOG_E(TAG, "Failed to get lobby id");
                        furi_string_free(lobby);
                        return false;
                    }
                    // add the lobby to the submenu
                    submenu_add_item(app->submenu_other, furi_string_get_cstr(lobby_id), FlipWorldSubmenuIndexLobby + i, callback_submenu_lobby_choices, app);
                    // add the lobby to the list
                    if (!easy_flipper_set_buffer(&lobby_list[i], 64))
                    {
                        FURI_LOG_E(TAG, "Failed to allocate lobby list");
                        furi_string_free(lobby);
                        furi_string_free(lobby_id);
                        return false;
                    }
                    snprintf(lobby_list[i], 64, "%s", furi_string_get_cstr(lobby_id));
                    furi_string_free(lobby);
                    furi_string_free(lobby_id);
                }
                furi_string_free(lobbies);
                return true;
            }

            // load pvp lobbies and player stats
            if (!flipper_http_process_response_async(fhttp, fetch_pvp_lobbies, parse_pvp_lobbies) || !flipper_http_process_response_async(fhttp, fetch_player_stats_i, set_player_context))
            {
                // unlike the pve/story, receiving data is necessary
                // so send the user back to the main menu if it fails
                FURI_LOG_E(HTTP_TAG, "Failed to make request");
                easy_flipper_dialog("Error", "Failed to make request. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu);
                flipper_http_free(fhttp);
            }
            else
            {
                flipper_http_free(fhttp);
            }

            // switch to the lobby submenu
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        }
    }
    else
    {
        switch_to_view_get_game(app);
    }
}

void callback_submenu_choices(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    switch (index)
    {
    case FlipWorldSubmenuIndexGameSubmenu:
        free_all_views(app, true, true, true);
        if (!alloc_game_submenu(app))
        {
            FURI_LOG_E(TAG, "Failed to allocate game submenu");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewGameSubmenu);
        break;
    case FlipWorldSubmenuIndexStory:
        game_mode_index = 2; // GAME_MODE_STORY
        run(app);
        break;
    case FlipWorldSubmenuIndexPvP:
        game_mode_index = 1; // GAME_MODE_PVP
        run(app);
        break;
    case FlipWorldSubmenuIndexPvE:
        game_mode_index = 0; // GAME_MODE_PVE
        run(app);
        break;
    case FlipWorldSubmenuIndexMessage:
        // About menu.
        free_all_views(app, true, true, true);
        if (!alloc_message_view(app, MessageStateAbout))
        {
            FURI_LOG_E(TAG, "Failed to allocate message view");
            return;
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMessage);
        break;
    case FlipWorldSubmenuIndexSettings:
        free_all_views(app, true, true, true);
        if (!alloc_submenu_other(app, FlipWorldViewSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate settings view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        break;
    case FlipWorldSubmenuIndexWiFiSettings:
        free_all_views(app, true, false, true);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexWiFiSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    case FlipWorldSubmenuIndexGameSettings:
        free_all_views(app, true, false, true);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexGameSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    case FlipWorldSubmenuIndexUserSettings:
        free_all_views(app, true, false, true);
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
static bool fetch_lobby(FlipperHTTP *fhttp, char *lobby_name)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby_name || strlen(lobby_name) == 0)
    {
        FURI_LOG_E(TAG, "Lobby name is NULL or empty");
        return false;
    }
    char username[64];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        return false;
    }
    // send the request to fetch the lobby details, with player_username
    char url[128];
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/pvp/lobby/get/%s/%s/", lobby_name, username);
    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/pvp/lobbies/%s.json", lobby_name);
    fhttp->save_received_data = true;
    if (!flipper_http_request(fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL))
    {
        FURI_LOG_E(TAG, "Failed to fetch lobby details");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
static bool join_lobby(FlipperHTTP *fhttp, char *lobby_name)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby_name || strlen(lobby_name) == 0)
    {
        FURI_LOG_E(TAG, "Lobby name is NULL or empty");
        return false;
    }
    char username[64];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        return false;
    }
    char url[128];
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"username\":\"%s\", \"game_id\":\"%s\"}", username, lobby_name);
    save_char("pvp_lobby_name", lobby_name); // save the lobby name
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/pvp/lobby/join/");
    if (!flipper_http_request(fhttp, POST, url, "{\"Content-Type\":\"application/json\"}", payload))
    {
        FURI_LOG_E(TAG, "Failed to join lobby");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
static bool create_pvp_enemy(FuriString *lobby_details)
{
    if (!lobby_details)
    {
        FURI_LOG_E(TAG, "Failed to load lobby details");
        return false;
    }

    char current_user[64];
    if (!load_char("Flip-Social-Username", current_user, sizeof(current_user)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        save_char("create_pvp_error", "Failed to load Flip-Social-Username");
        return false;
    }

    for (uint8_t i = 0; i < 2; i++)
    {
        // parse the lobby details
        FuriString *player_stats = get_json_array_value_furi("player_stats", i, lobby_details);
        if (!player_stats)
        {
            FURI_LOG_E(TAG, "Failed to get player stats");
            save_char("create_pvp_error", "Failed to get player stats array");
            return false;
        }

        // available keys from player_stats
        FuriString *username = get_json_value_furi("username", player_stats);
        if (!username)
        {
            FURI_LOG_E(TAG, "Failed to get username");
            save_char("create_pvp_error", "Failed to get username");
            furi_string_free(player_stats);
            return false;
        }

        // check if the username is the same as the current user
        if (is_str(furi_string_get_cstr(username), current_user))
        {
            furi_string_free(player_stats);
            furi_string_free(username);
            continue; // skip the current user
        }

        FuriString *strength = get_json_value_furi("strength", player_stats);
        FuriString *health = get_json_value_furi("health", player_stats);
        FuriString *attack_timer = get_json_value_furi("attack_timer", player_stats);

        if (!strength || !health || !attack_timer)
        {
            FURI_LOG_E(TAG, "Failed to get player stats");
            save_char("create_pvp_error", "Failed to get player stats");
            furi_string_free(player_stats);
            furi_string_free(username);
            if (strength)
                furi_string_free(strength);
            if (health)
                furi_string_free(health);
            if (attack_timer)
                furi_string_free(attack_timer);
            return false;
        }

        // create enemy data
        FuriString *enemy_data = furi_string_alloc();
        furi_string_printf(
            enemy_data,
            "{\"enemy_data\":[{\"id\":\"sword\",\"is_user\":\"true\",\"username\":\"%s\","
            "\"index\":0,\"start_position\":{\"x\":350,\"y\":210},\"end_position\":{\"x\":350,\"y\":210},"
            "\"move_timer\":1,\"speed\":1,\"attack_timer\":%f,\"strength\":%f,\"health\":%f}]}",
            furi_string_get_cstr(username),
            (double)atof_furi(attack_timer),
            (double)atof_furi(strength),
            (double)atof_furi(health));

        char directory_path[128];
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
        Storage *storage = furi_record_open(RECORD_STORAGE);
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/pvp_world");
        storage_common_mkdir(storage, directory_path);
        furi_record_close(RECORD_STORAGE);

        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/pvp_world/pvp_world_enemy_data.json");

        // remove the enemy_data file if it exists
        storage_simply_remove_recursive(storage, directory_path);

        File *file = storage_file_alloc(storage);
        if (!storage_file_open(file, directory_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        {
            FURI_LOG_E("Game", "Failed to open file for writing: %s", directory_path);
            save_char("create_pvp_error", "Failed to open file for writing");
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            furi_string_free(enemy_data);
            furi_string_free(player_stats);
            furi_string_free(username);
            furi_string_free(strength);
            furi_string_free(health);
            furi_string_free(attack_timer);
            return false;
        }

        size_t data_size = furi_string_size(enemy_data);
        if (storage_file_write(file, furi_string_get_cstr(enemy_data), data_size) != data_size)
        {
            FURI_LOG_E("Game", "Failed to write enemy_data");
            save_char("create_pvp_error", "Failed to write enemy_data");
        }
        storage_file_close(file);

        furi_string_free(enemy_data);
        furi_string_free(player_stats);
        furi_string_free(username);
        furi_string_free(strength);
        furi_string_free(health);
        furi_string_free(attack_timer);

        // player is found so break
        break;
    }

    return true;
}
// since we aren't using FURI_LOG, we will use easy_flipper_dialog and the last_error_message
// char last_error_message[64];
static size_t lobby_count(FlipperHTTP *fhttp, FuriString *lobby)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return -1;
    }
    if (!lobby)
    {
        FURI_LOG_E(TAG, "Lobby details are NULL");
        return -1;
    }
    // check if the player is in the lobby
    FuriString *player_count = get_json_value_furi("player_count", lobby);
    if (!player_count)
    {
        FURI_LOG_E(TAG, "Failed to get player count");
        return -1;
    }
    const size_t count = atoi(furi_string_get_cstr(player_count));
    furi_string_free(player_count);
    return count;
}
static bool in_lobby(FlipperHTTP *fhttp, FuriString *lobby)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby)
    {
        FURI_LOG_E(TAG, "Lobby details are NULL");
        return false;
    }
    // check if the player is in the lobby
    FuriString *is_in_game = get_json_value_furi("is_in_game", lobby);
    if (!is_in_game)
    {
        FURI_LOG_E(TAG, "Failed to get is_in_game");
        furi_string_free(is_in_game);
        return false;
    }
    const bool in_game = is_str(furi_string_get_cstr(is_in_game), "true");
    furi_string_free(is_in_game);
    return in_game;
}

static bool start_ws(FlipperHTTP *fhttp, char *lobby_name)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby_name || strlen(lobby_name) == 0)
    {
        FURI_LOG_E(TAG, "Lobby name is NULL or empty");
        return false;
    }
    fhttp->state = IDLE; // ensure it's set to IDLE for the next request
    char websocket_url[128];
    snprintf(websocket_url, sizeof(websocket_url), "ws://www.jblanked.com/ws/game/%s/", lobby_name);
    if (!flipper_http_websocket_start(fhttp, websocket_url, 80, "{\"Content-Type\":\"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to start websocket");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
// this will free both the fhttp and lobby
static void start_pvp(FlipperHTTP *fhttp, FuriString *lobby, void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    // only thing left to do is create the enemy data and start the websocket session
    if (!create_pvp_enemy(lobby))
    {
        FURI_LOG_E(TAG, "Failed to create pvp enemy context.");
        easy_flipper_dialog("Error", "Failed to create pvp enemy context. Press BACK to return.");
        flipper_http_free(fhttp);
        furi_string_free(lobby);
        return;
    }

    furi_string_free(lobby);

    // start the websocket session
    if (!start_ws(fhttp, lobby_list[lobby_index]))
    {
        FURI_LOG_E(TAG, "Failed to start websocket session");
        easy_flipper_dialog("Error", "Failed to start websocket session. Press BACK to return.");
        flipper_http_free(fhttp);
        return;
    }

    flipper_http_free(fhttp);

    // start the game thread
    if (!start_game_thread(app))
    {
        FURI_LOG_E(TAG, "Failed to start game thread");
        easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
        return;
    }
};
void waiting_loader_process_callback(FlipperHTTP *fhttp, void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        return;
    }
    // fetch the lobby details
    if (!fetch_lobby(fhttp, lobby_list[lobby_index]))
    {
        FURI_LOG_E(TAG, "Failed to fetch lobby details");
        flipper_http_free(fhttp);
        easy_flipper_dialog("Error", "Failed to fetch lobby details. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        return;
    }
    // load the lobby details
    FuriString *lobby = flipper_http_load_from_file(fhttp->file_path);
    if (!lobby)
    {
        FURI_LOG_E(TAG, "Failed to load lobby details");
        flipper_http_free(fhttp);
        easy_flipper_dialog("Error", "Failed to load lobby details. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        return;
    }
    // get the player count
    const size_t count = lobby_count(fhttp, lobby);
    if (count == 2)
    {
        // break out of this and start the game
        start_pvp(fhttp, lobby, app); // this will free both the fhttp and lobby
        return;
    }
    furi_string_free(lobby);
}

static void waiting_lobby(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "waiting_lobby: FlipWorldApp is NULL");
    if (!start_waiting_thread(app))
    {
        FURI_LOG_E(TAG, "Failed to start waiting thread");
        easy_flipper_dialog("Error", "Failed to start waiting thread. Press BACK to return.");
        return;
    }
    free_message_view(app);
    if (!alloc_message_view(app, MessageStateWaitingLobby))
    {
        FURI_LOG_E(TAG, "Failed to allocate message view");
        return;
    }
    // finally, switch to the waiting lobby view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMessage);
};
static void callback_submenu_lobby_choices(void *context, uint32_t index)
{
    /* Handle other game lobbies
             1. when clicked on, send request to fetch the selected game lobby details
             2. start the websocket session
             3. start the game thread (the rest will be handled by game_start and player_update)
             */
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    if (index >= FlipWorldSubmenuIndexLobby && index < FlipWorldSubmenuIndexLobby + 10)
    {
        lobby_index = index - FlipWorldSubmenuIndexLobby;

        FlipperHTTP *fhttp = flipper_http_alloc();
        if (!fhttp)
        {
            FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
            easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP. Press BACK to return.");
            return;
        }

        // fetch the lobby details
        if (!fetch_lobby(fhttp, lobby_list[lobby_index]))
        {
            FURI_LOG_E(TAG, "Failed to fetch lobby details");
            easy_flipper_dialog("Error", "Failed to fetch lobby details. Press BACK to return.");
            flipper_http_free(fhttp);
            return;
        }

        // load the lobby details
        FuriString *lobby = flipper_http_load_from_file(fhttp->file_path);
        if (!lobby)
        {
            FURI_LOG_E(TAG, "Failed to load lobby details");
            flipper_http_free(fhttp);
            return;
        }

        // if there are no players, add the user to the lobby and make the user wait until another player joins
        // if there is one player and it's the user, make the user wait until another player joins
        // if there is one player and it's not the user, parse_lobby and start websocket
        // if there are 2 players (which there shouldn't be at this point), show an error message saying the lobby is full
        switch (lobby_count(fhttp, lobby))
        {
        case -1:
            FURI_LOG_E(TAG, "Failed to get player count");
            easy_flipper_dialog("Error", "Failed to get player count. Press BACK to return.");
            flipper_http_free(fhttp);
            furi_string_free(lobby);
            return;
        case 0:
            // add the user to the lobby
            if (!join_lobby(fhttp, lobby_list[lobby_index]))
            {
                FURI_LOG_E(TAG, "Failed to join lobby");
                easy_flipper_dialog("Error", "Failed to join lobby. Press BACK to return.");
                flipper_http_free(fhttp);
                furi_string_free(lobby);
                return;
            }
            // send the user to the waiting screen
            waiting_lobby(app);
            return;
        case 1:
            // check if the user is in the lobby
            if (in_lobby(fhttp, lobby))
            {
                // send the user to the waiting screen
                FURI_LOG_I(TAG, "User is in the lobby");
                flipper_http_free(fhttp);
                furi_string_free(lobby);
                waiting_lobby(app);
                return;
            }
            // add the user to the lobby
            if (!join_lobby(fhttp, lobby_list[lobby_index]))
            {
                FURI_LOG_E(TAG, "Failed to join lobby");
                easy_flipper_dialog("Error", "Failed to join lobby. Press BACK to return.");
                flipper_http_free(fhttp);
                furi_string_free(lobby);
                return;
            }
            break;
        case 2:
            // show an error message saying the lobby is full
            FURI_LOG_E(TAG, "Lobby is full");
            easy_flipper_dialog("Error", "Lobby is full. Press BACK to return.");
            flipper_http_free(fhttp);
            furi_string_free(lobby);
            return;
        };

        start_pvp(fhttp, lobby, app); // this will free both the fhttp and lobby, and start the game
    }
}

static void updated_wifi_ssid(void *context)
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
static void updated_wifi_pass(void *context)
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
static void updated_username(void *context)
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
static void updated_password(void *context)
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

static void wifi_settings_select(void *context, uint32_t index)
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
        free_all_views(app, false, false, true);
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
        free_all_views(app, false, false, true);
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
static void fps_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    fps_index = index;
    variable_item_set_current_value_text(item, fps_choices_str[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-FPS", fps_choices_str[index]);
}
static void screen_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    screen_always_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Screen-Always-On", yes_or_no_choices[index]);
}
static void sound_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    sound_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Sound-On", yes_or_no_choices[index]);
}
static void vibration_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    vibration_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Vibration-On", yes_or_no_choices[index]);
}
static void player_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    player_sprite_index = index;
    variable_item_set_current_value_text(item, is_str(player_sprite_choices[index], "naked") ? "None" : player_sprite_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Player-Sprite", player_sprite_choices[index]);
}
static void vgm_x_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    vgm_x_index = index;
    variable_item_set_current_value_text(item, vgm_levels[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-VGM-X", vgm_levels[index]);
}
static void vgm_y_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    vgm_y_index = index;
    variable_item_set_current_value_text(item, vgm_levels[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-VGM-Y", vgm_levels[index]);
}

static bool _fetch_worlds(DataLoaderModel *model)
{
    if (!model || !model->fhttp)
    {
        FURI_LOG_E(TAG, "model or fhttp is NULL");
        return false;
    }
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
    snprintf(model->fhttp->file_path, sizeof(model->fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list_full.json");
    model->fhttp->save_received_data = true;
    return flipper_http_request(model->fhttp, GET, "https://www.jblanked.com/flipper/api/world/v5/get/10/", "{\"Content-Type\":\"application/json\"}", NULL);
}
static char *_parse_worlds(DataLoaderModel *model)
{
    UNUSED(model);
    return "World Pack Installed";
}
static void switch_to_view_get_worlds(FlipWorldApp *app)
{
    if (!alloc_view_loader(app))
    {
        FURI_LOG_E(TAG, "Failed to allocate view loader");
        return;
    }
    loader_switch_to_view(app, "Fetching World Pack..", _fetch_worlds, _parse_worlds, 1, callback_to_submenu, FlipWorldViewLoader);
}
static void game_settings_select(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }
    switch (index)
    {
    case 0: // Download all world data as one huge json
        switch_to_view_get_worlds(app);
    case 1: // Player Sprite
        break;
    case 2: // Change FPS
        break;
    case 3: // VGM X
        break;
    case 4: // VGM Y
        break;
    case 5: // Screen Always On
        break;
    case 6: // Sound On
        break;
    case 7: // Vibration On
        break;
    }
}
static void user_settings_select(void *context, uint32_t index)
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
        free_all_views(app, false, false, true);
        if (!alloc_text_input_view(app, "Username-Login"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    case 1: // Password
        free_all_views(app, false, false, true);
        if (!alloc_text_input_view(app, "Password-Login"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    }
}