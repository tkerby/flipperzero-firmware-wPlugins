/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401DigiLab_config.h"
#include "401_config.h"

static const char* TAG = "401_DigiLabConfig";
/**
 * @brief Render callback for the app configuration.
 *
 * @param canvas The canvas to be used for rendering.
 * @param ctx The context passed to the callback.
 */
void app_config_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Calibration");
    canvas_draw_icon(canvas, 0, (64 - 53), &I_calibration);
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text(canvas, 52, 30, "Probe the 5V\ntest pad and\nclick OK.");
}

/**
 * @brief Update led according to update_counter
 *
 * @param ctx The context passed to the callback.
 */
void on_tick(void* ctx) {
    UNUSED(ctx);
}

/**
 * @brief Allocate memory and initialize the app configuration.
 *
 * @return Returns a pointer to the allocated AppConfig.
 */

AppConfig* app_config_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppConfig* appConfig = malloc(sizeof(AppConfig));
    appConfig->view = view_alloc();
    view_set_context(appConfig->view, app);
    view_set_draw_callback(appConfig->view, app_config_render_callback);
    view_set_input_callback(appConfig->view, app_config_input_callback);
    return appConfig;
}

static void config_adc_start(FuriHalAdcHandle** adc) {
    *adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(*adc);
}

static void config_adc_stop(FuriHalAdcHandle** adc) {
    if(*adc) {
        furi_hal_adc_release(*adc);
    }
}

/**
 * @brief Callback function to handle input events for the app configuration.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_config_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    FuriHalAdcHandle* adc;
    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyOk:
            config_adc_start(&adc);
            uint16_t raw = furi_hal_adc_read(adc, FuriHalAdcChannel4);
            float voltage = furi_hal_adc_convert_to_voltage(adc, raw);
            FURI_LOG_W("ADC", "Calibration from %f", (double)voltage);
            FURI_LOG_W("ADC", "Calibration Value: %f", (double)(5 / voltage));
            app->data->config->BridgeFactor = (double)(5 / voltage) * 1000;
            config_adc_stop(&adc);
            break;
        default:
            break;
        }
    }
    bool handled = false;
    return handled;
}

/**
 * @brief Free the memory occupied by the app configuration.
 *
 * @param appConfig The AppConfig to be freed.
 */
void app_config_free(AppConfig* appConfig) {
    furi_assert(appConfig);
    view_free_model(appConfig->view);
    view_free(appConfig->view);

    free(appConfig);
}

/**
 * @brief Get the view associated with the app configuration.
 *
 * @param appConfig The AppConfig for which the view is to be fetched.
 * @return Returns a pointer to the View.
 */
View* app_config_get_view(AppConfig* appConfig) {
    furi_assert(appConfig);
    return appConfig->view;
}

/**
 * @brief Callback when the app configuration scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_config_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewConfig);
}

/**
 * @brief Handle scene manager events for the app configuration scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_config_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
 * @brief Callback when the app configuration scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_config_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    //  AppConfig *appConfig = (AppConfig *)app->sceneConfig;

    l401_err res = config_save_json(DIGILABCONF_CONFIG_FILE, app->data->config);

    if(res == L401_OK) {
        FURI_LOG_I(TAG, "Successfully saved configuration to %s.", DIGILABCONF_CONFIG_FILE);
    } else {
        FURI_LOG_E(TAG, "Error while saving to %s: %d", DIGILABCONF_CONFIG_FILE, res);
    }
}
