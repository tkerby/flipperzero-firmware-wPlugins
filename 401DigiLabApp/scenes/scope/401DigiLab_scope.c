/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "scenes/scope/401DigiLab_scope.h"
static const char* TAG = "401_DigiLabScope";

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_speaker.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>
#include <limits.h>
#define SPEAKER_ACQUIRE_TIMEOUT 100
#include <401_config.h>
#include <drivers/sk6805.h>
uint16_t redraw_cnt = 0;

static void scope_sound_stop() {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

static void scope_LED_stop() {
    SK6805_off();
}

static void scope_vibro_stop() {
    furi_hal_gpio_write(&gpio_vibro, false);
}

static void scope_adc_start(FuriHalAdcHandle** adc) {
    *adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(*adc);
}

static void scope_adc_stop(FuriHalAdcHandle** adc) {
    if(*adc) {
        furi_hal_adc_release(*adc);
    }
}

static bool CheckAlert(ScopeModel* model, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    float voltage = model->voltage;
    switch(app->data->config->ScopeAlert) {
    case DigiLab_ScopeAlert_lt3V:
        return ((voltage) < 3.3);
        break;
    case DigiLab_ScopeAlert_gt3V:
        return ((voltage) > 3.3);
        break;
    case DigiLab_ScopeAlert_lt5V:
        return ((voltage) < 5.0);
        break;
    case DigiLab_ScopeAlert_gt5V:
        return ((voltage) > 5.0);
        break;
    case DigiLab_ScopeAlert_osc:
        if(model->inversion) {
            model->inversion = false;
            return true;
        }
        return false;
        break;
    case DigiLab_ScopeAlert_maxV:
        return ((voltage) > 11);
        break;
    case DigiLab_ScopeAlert_0V:
        return ((voltage) < 0.3);
        break;
    default:
        return false;
        break;
    }
}

static void adc_scopeSound(AppContext* app, ScopeModel* model, bool alert) {
    switch(app->data->config->ScopeSound) {
    case Digilab_ScopeSoundOn:
        if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(SPEAKER_ACQUIRE_TIMEOUT)) {
            furi_hal_speaker_start((float)(model->voltage * 75), 1.0F);
        }
        break;
    case DigiLab_ScopeSoundAlert:
        if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(SPEAKER_ACQUIRE_TIMEOUT)) {
            if(app->data->config->ScopeAlert == DigiLab_ScopeAlert_osc) {
                if(alert) {
                    furi_hal_speaker_start(model->inversion_count * 30, 1);
                } else {
                    furi_hal_speaker_start(0, 0);
                }
            } else {
                if(alert) {
                    furi_hal_speaker_start(500, 1);
                } else {
                    furi_hal_speaker_start(0, 0);
                }
            }
        }
        break;
    default:
    case DigiLab_ScopeSoundOff:
        break;
    }
}

static void adc_scopeVibro(AppContext* app, ScopeModel* model, bool alert) {
    UNUSED(model);
    switch(app->data->config->ScopeVibro) {
    case DigiLab_ScopeVibroAlert:
        if(alert) {
            furi_hal_gpio_write(&gpio_vibro, true);
        } else {
            furi_hal_gpio_write(&gpio_vibro, false);
        }
        break;
    default:
    case DigiLab_ScopeVibroOff:
        break;
    }
}

static void adc_scopeLed_voltageColor(uint32_t voltage, uint8_t* r, uint8_t* g, uint8_t* b) {
    // Limites de tension en mV
    const uint32_t max_voltage = 12; // Max 12V
    if(voltage > max_voltage) {
        voltage = max_voltage;
    }
    uint32_t ratio = (voltage * 255) / max_voltage;
    *r = (ratio * 255) / 255; // Rouge de 0 à 255
    *g = ((255 - ratio) * 255) / 255; // Vert de 255 à 0
    *b = 0; // Bleu fixé à 0
}

static void adc_scopeLed(AppContext* app, ScopeModel* model, bool alert) {
    switch(app->data->config->ScopeLed) {
    case DigiLab_ScopeLedOff:
        SK6805_set_led_color(0, 0, 0, 0);
        SK6805_update();
        break;
    case DigiLab_ScopeLedAlert:
        if(app->data->config->ScopeAlert == DigiLab_ScopeAlert_osc) {
            if(alert) {
                SK6805_set_led_color(0, 0, 0x88, model->inversion_count * 3);
            } else {
                SK6805_set_led_color(0, 0, 0, 0);
            }
            SK6805_update();
        } else {
            if(alert) {
                SK6805_set_led_color(0, 0xFF, 0xFF, 0);
            } else {
                SK6805_set_led_color(0, 0, 0, 0);
            }
            SK6805_update();
        }
        break;
    case DigiLab_ScopeLedFollow:
        if(model->voltage > 0.5) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            adc_scopeLed_voltageColor(model->voltage, &r, &g, &b);
            SK6805_set_led_color(0, r, g, b);
            SK6805_update();
        } else {
            SK6805_set_led_color(0, 0, 0, 0);
            SK6805_update();
        }
        break;
    case DigiLab_ScopeLedVariance:
        uint8_t mappedVariance = RingBuffer_getStandardDeviationMapped(
            model->oscWindow->samples, model->oscWindow->vMin, model->oscWindow->vMax);
        SK6805_set_led_color(
            0, mappedVariance, 255 - mappedVariance, (uint8_t)(255 / model->oscWindow->vMax));
        SK6805_update();
        break;
    case DigiLab_ScopeLedTrigger:
        if(model->voltage > (model->oscWindow->vTrig / OSC_SCALE_V)) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            adc_scopeLed_voltageColor(model->voltage, &r, &g, &b);
            SK6805_set_led_color(0, r, g, b);
            SK6805_update();
        } else {
            SK6805_set_led_color(0, 0xFF, 0, 0);
            SK6805_update();
        }
        break;
    }
}

bool previous_int = false;
bool current_int = false;

void adc_timer_cb(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    ScopeModel* model = view_get_model(appScope->view);

    redraw_cnt++;
    model->raw = furi_hal_adc_read(appScope->adc, FuriHalAdcChannel4);
    current_int = !furi_hal_gpio_read(L401DIGILAB_BOARD_PIN_INT);

    if(current_int) {
        model->voltage =
            (double)((double)furi_hal_adc_convert_to_voltage(appScope->adc, model->raw) *
                     (double)(model->BridgeFactor));
    } else {
        model->voltage = (double)0.0;
    }

    if(previous_int != current_int) {
        model->inversion = true;
        model->inversion_count++;
        previous_int = current_int;
    }

    OscWindow_add(model->oscWindow, (uint32_t)(model->voltage * OSC_SCALE_MV));

    if(!(redraw_cnt % 100)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ScopeEventRedraw);
    }
}

void osc_draw_screen(Canvas* canvas, ScopeModel* model) {
    OscWindow_draw(canvas, model->oscWindow);
    if(model->inversion_count > 0) {
        canvas_draw_str_aligned(canvas, 0, 32, AlignLeft, AlignCenter, "OSC");
        model->inversion_count = 0;
    }

    canvas_set_font(canvas, FontSecondary);
    elements_button_right(canvas, "Probe");
    if(model->capture) {
        elements_button_center(canvas, "Stop");
    } else {
        elements_button_center(canvas, "Start");
    }
    elements_button_left(canvas, "Config");
}

/**
 * @brief Render callback for the app osc.
 *
 * @param canvas The canvas to be used for rendering.
 * @param model The model passed to the callback.
 */
void app_scope_render_callback(Canvas* canvas, void* ctx) {
    // UNUSED(model);
    ScopeModel* model = (ScopeModel*)ctx;
    // char str[64];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    osc_draw_screen(canvas, model);
}

/**
 * @brief Allocate memory and initialize the app osc.
 *
 * @return Returns a pointer to the allocated AppScope.
 */
AppScope* app_scope_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = malloc(sizeof(AppScope));

    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    appScope->view = view_alloc();
    appScope->list_config = variable_item_list_alloc();
    appScope->timer = furi_timer_alloc(adc_timer_cb, FuriTimerTypePeriodic, app);

    view_allocate_model(appScope->view, ViewModelTypeLockFree, sizeof(ScopeModel));
    with_view_model(
        appScope->view,
        ScopeModel * model,
        {
            model->oscWindow = OscWindow_create(256 - 30, 0, 10, 128, 40);
            model->capture = true;
        },
        false);

    view_set_context(appScope->view, app);
    view_set_draw_callback(appScope->view, app_scope_render_callback);
    view_set_input_callback(appScope->view, app_scope_input_callback);
    view_set_custom_callback(appScope->view, app_scene_scope_on_custom_event);

    furi_hal_gpio_init(L401DIGILAB_BOARD_PIN_ADC, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_vibro, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_vibro, false);

    scope_vibro_stop();

    return appScope;
}

static void scope_timer_start(FuriTimer* timer) {
    furi_timer_start(timer, (furi_kernel_get_tick_frequency() / 1000));
}

static void scope_timer_stop(FuriTimer* timer) {
    furi_timer_stop(timer);
}

/**
 * @brief Callback function to handle input events for the app osc.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_scope_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    UNUSED(input_event);
    bool handled = false;

    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyLeft:
            scope_sound_stop();
            scene_manager_next_scene(app->scene_manager, AppSceneScopeConfig);
            break;
        case InputKeyRight:

            scene_manager_search_and_switch_to_another_scene(app->scene_manager, AppSceneProbe);
            break;
        case InputKeyOk:

            AppScope* appScope = (AppScope*)app->sceneScope;

            with_view_model(
                appScope->view,
                ScopeModel * model,
                {
                    model->capture = !model->capture;
                    if(model->capture == true)
                        scope_timer_start(appScope->timer);
                    else
                        scope_timer_stop(appScope->timer);
                },
                true);
            break;
        default:

            FURI_LOG_I(TAG, "Resume to not handled");
            break;
        }
    }
    return handled;
}

/**
 * @brief Get the view associated with the app osc.
 *
 * @param appScope The AppScope for which the view is to be fetched.
 * @return Returns a pointer to the View.
 */
View* app_scope_get_view(AppScope* appScope) {
    furi_assert(appScope);
    return appScope->view;
}

/**
 * @brief Handle scene manager events for the app osc scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_scope_on_event(void* ctx, SceneManagerEvent event) {
    bool consumed = false;
    UNUSED(ctx);
    UNUSED(event);

    return consumed;
}

bool app_scene_scope_on_custom_event(uint32_t event, void* ctx) {
    bool consumed = false;
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    ScopeModel* model = view_get_model(appScope->view);

    switch(event) {
    case ScopeEventRedraw:
        RingBuffer_getMinMax(
            model->oscWindow->samples, &model->oscWindow->vMin, &model->oscWindow->vMax);
        model->oscWindow->vAvg = RingBuffer_getAverage(model->oscWindow->samples) / OSC_SCALE_V;
        bool alert = CheckAlert(model, app);
        adc_scopeSound(app, model, alert);
        adc_scopeVibro(app, model, alert);
        adc_scopeLed(app, model, alert);
        view_commit_model(appScope->view, true);
        break;
    default:
        break;
    }
    return consumed;
}

/**
 * @brief Callback when the app osc scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_scope_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    ScopeModel* model = view_get_model(appScope->view);

    scope_adc_start(&appScope->adc);
    SK6805_init();
    scope_timer_start(appScope->timer);
    furi_hal_gpio_init(L401DIGILAB_BOARD_PIN_INT, GpioModeInput, GpioPullUp, GpioSpeedLow);

    //Reload latest Bridge Factor
    model->BridgeFactor = (app->data->config->BridgeFactor / 100);
    model->inversion = false;
    model->inversion_count = 0;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewScope);
}

/**
 * @brief Callback when the app osc scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_scope_on_exit(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    scope_timer_stop(appScope->timer);
    furi_delay_ms(100);
    scope_adc_stop(&appScope->adc);
    scope_sound_stop();
    scope_vibro_stop();
    scope_LED_stop();
}

/**
 * @brief Free the memory occupied by the app osc.
 *
 * @param appScope The AppScope to be freed.
 */
void app_scope_free(AppScope* appScope) {
    furi_assert(appScope);
    variable_item_list_free(appScope->list_config);
    with_view_model(
        appScope->view, ScopeModel * model, { OscWindow_free(model->oscWindow); }, false);
    furi_timer_free(appScope->timer);
    view_free_model(appScope->view);
    view_free(appScope->view);
    free(appScope);
}
