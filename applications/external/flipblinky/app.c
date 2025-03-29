#include "app_icons.h"
#include "app_i.h"
#include "app_config.h"
#include "app_settings.h"

#include <gui/modules/widget.h>
#include <furi.h>

// The screen only refreshes a few times a second, so this is the minimum time the message
// will be displayed after an action.
#define MESSAGE_DISPLAY_TIME_MS 1900

// The effect to initially use when the "Flipboard Blinky" view is shown.
#define INITIAL_EFFECT_ID 2

/**
 * @brief This method handles Flipper D-Pad input when in the FlipboardBlinky mode.
 * @param _event The InputEvent* to handle.
 * @param _context The Flipboard* context.
 * @return true if the key event was handled, false otherwise.
 */
bool flipboard_view_flip_keyboard_input(InputEvent* _event, void* _context) {
    UNUSED(_event);
    UNUSED(_context);
    return false;
}

/**
 * @brief This method handles drawing when in the FlipboardBlinky mode.
 * @param canvas The canvas to draw on.
 * @param model The FlipboardModelRef* context.
 */
void flipboard_view_flip_keyboard_draw(Canvas* canvas, void* model) {
    FlipboardModelRef* my_model = (FlipboardModelRef*)model;
    FlipboardBlinkyModel* fbm = flipboard_model_get_custom_data(my_model->model);
    if(fbm->render_model.source == FlipboardBlinkySourceAssets) {
        uint16_t h = icon_get_height(&I_nametag);
        if(h > 64) {
            h = 64;
        }
        uint16_t w = icon_get_width(&I_nametag);
        if(w > 128) {
            w = 128;
        }
        uint16_t x = 0;
        if(fbm->render_model.justification == FlipboardBlinkyJustificationCenter) {
            x = (128 - w) / 2;
        } else if(fbm->render_model.justification == FlipboardBlinkyJustificationRight) {
            x = 128 - w;
        }
        canvas_draw_icon(canvas, x, (64 - h) / 2, &I_nametag);
    } else if(fbm->render_model.source == FlipboardBlinkySourceFXBM) {
        if(fbm->fxbm) {
            uint16_t h = fxbm_get_height(fbm->fxbm);
            if(h > 64) {
                h = 64;
            }
            uint16_t w = fxbm_get_width(fbm->fxbm);
            if(w > 128) {
                w = 128;
            }
            uint16_t x = 0;
            if(fbm->render_model.justification == FlipboardBlinkyJustificationCenter) {
                x = (128 - w) / 2;
            } else if(fbm->render_model.justification == FlipboardBlinkyJustificationRight) {
                x = 128 - w;
            }
            canvas_draw_xbm(canvas, x, (64 - h) / 2, w, h, fxbm_get_data(fbm->fxbm));
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 0, 10, "FXBM file not found!");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 30, "Save FXBM file on SD card at");
            canvas_draw_str(canvas, 0, 40, "/apps_data/flipboard/");
            canvas_draw_str(canvas, 0, 50, "blinky.fxbm");
        }
    } else if(fbm->render_model.source == FlipboardBlinkySourceText) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        Align align = AlignLeft;
        uint32_t x = 0;
        if(fbm->render_model.justification == FlipboardBlinkyJustificationCenter) {
            align = AlignCenter;
            x = 64;
        } else if(fbm->render_model.justification == FlipboardBlinkyJustificationRight) {
            align = AlignRight;
            x = 127;
        }
        canvas_draw_str_aligned(canvas, x, 0, align, AlignTop, fbm->render_model.line[0]);
        canvas_draw_str_aligned(canvas, x, 16, align, AlignTop, fbm->render_model.line[1]);
        canvas_draw_str_aligned(canvas, x, 32, align, AlignTop, fbm->render_model.line[2]);
        canvas_draw_str_aligned(canvas, x, 48, align, AlignTop, fbm->render_model.line[3]);
    }

    if(fbm->render_model.show_details && fbm->show_details_until &&
       (furi_get_tick() < fbm->show_details_until)) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 48, 128, 64 - 48);
        canvas_set_color(canvas, ColorBlack);
        FuriString* str = furi_string_alloc();
        furi_string_printf(
            str,
            "Speed:%lu    Effect:%d of %d",
            fbm->period_ms,
            fbm->effect_id,
            fbm->max_effect_id);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 1, 61, furi_string_get_cstr(str));
        furi_string_free(str);
    }
}

/**
 * @brief This method sets the effect to its initial state.
 * @param model The FlipboardModel* context.
 */
void flipboard_reset_effect(FlipboardModel* model) {
    FlipboardBlinkyModel* fbm = flipboard_model_get_custom_data(model);

    switch(fbm->effect_id) {
    case 1:
        // Off
        for(size_t i = 0; i < COUNT_OF(fbm->colors); i++) {
            fbm->colors[i] = LedColorBlack;
        }
        FlipboardLeds* leds = flipboard_model_get_leds(model);
        flipboard_leds_set(leds, LedId1, fbm->colors[0]);
        flipboard_leds_set(leds, LedId2, fbm->colors[1]);
        flipboard_leds_set(leds, LedId3, fbm->colors[2]);
        flipboard_leds_set(leds, LedId4, fbm->colors[3]);
        flipboard_leds_update(leds);
        break;

    case 2:
        // LEDs will be set in effect.  Populate Fill+Highligh colors.
        fbm->colors[4] = LedColorViolet; // Fill color
        fbm->colors[5] = LedColorRed; // Highlight color
        fbm->effect_counter = 0; // Set effect counter to initial value.
        break;

    case 3:
        // Random colors, set in the effect
        break;

    case 4:
        fbm->colors[0] = LedColorRed;
        fbm->colors[1] = LedColorWhite;
        fbm->colors[2] = LedColorRed;
        fbm->colors[3] = LedColorBlue;
        break;

    case 5:
        fbm->colors[0] = LedColorYellow;
        fbm->colors[1] = LedColorOrange;
        fbm->colors[2] = LedColorYellow;
        fbm->colors[3] = LedColorOrange;
        break;

    case 6:
        fbm->colors[0] = LedColorGreen;
        fbm->colors[1] = LedColorBlue;
        fbm->colors[2] = LedColorMagenta;
        fbm->colors[3] = LedColorBlack;
        break;

    default:
        FURI_LOG_D(TAG, "Unknown effect_id=%d", fbm->effect_id);
        fbm->colors[0] = LedColorRed;
        fbm->colors[1] = LedColorRed;
        fbm->colors[2] = LedColorRed;
        fbm->colors[3] = LedColorRed;
        break;
    }

    if(fbm->effect_id == 1) {
        flipboard_model_set_backlight(model, false);
    } else {
        flipboard_model_set_backlight(model, true);
    }
}

/**
 * @brief This method updates the effect to the step.
 * @param model The FlipboardModel* context.
 */
void flipboard_do_effect(FlipboardModel* model) {
    FlipboardBlinkyModel* fbm = flipboard_model_get_custom_data(model);
    FlipboardLeds* leds = flipboard_model_get_leds(model);

    switch(fbm->effect_id) {
    case 1:
        // All LEDs are off, so don't do any effect.
        return;

    case 2:
        // Set all of the LEDs to the color specified in the fill color (colors[4])
        for(int i = 0; i < 4; i++) {
            fbm->colors[i] = fbm->colors[4];
        }
        // For 0 set 1st LED, 1 set 2nd LED, 2 set 3rd LED, 3 set 4th LED.
        if(fbm->effect_counter < 4) {
            fbm->colors[fbm->effect_counter] = fbm->colors[5];
        } else {
            // 4 set 3rd LED (6-4=index 2), 5 set 2nd LED (6-5=index 1)
            fbm->colors[6 - fbm->effect_counter] = fbm->colors[5];
        }
        // Increment effect counter, then make sure counter is 0 to 5.
        fbm->effect_counter++;
        if(fbm->effect_counter >= 6) {
            fbm->effect_counter = 0;
        }
        break;

    case 3:
        // Set each of the 4 LEDs to a random color
        for(int i = 0; i < 4; i++) {
            uint8_t red = rand() % 0xFF;
            uint8_t green = rand() % 0xFF;
            uint8_t blue = rand() % 0xFF;
            fbm->colors[i] = red << 16 | green << 8 | blue;
        }
        break;

    default:
        // By default, the effects scroll the visible colors
        {
            uint32_t tmp = fbm->colors[0];
            for(int i = 0; i < 3; i++) {
                fbm->colors[i] = fbm->colors[i + 1];
            }
            fbm->colors[3] = tmp;
            break;
        }
    }

    flipboard_leds_set(leds, LedId1, fbm->colors[0]);
    flipboard_leds_set(leds, LedId2, fbm->colors[1]);
    flipboard_leds_set(leds, LedId3, fbm->colors[2]);
    flipboard_leds_set(leds, LedId4, fbm->colors[3]);
    flipboard_leds_update(leds);
}

/**
 * @brief This method handles FlipBoard key input when in the FlipboardBlinky mode.
 * @param context The Flipboard* context.
 * @param old_key The previous key state.
 * @param new_key The new key state.
 */
void flipboard_debounced_switch(void* context, uint8_t old_key, uint8_t new_key) {
    Flipboard* app = (Flipboard*)context;
    FlipboardModel* model = flipboard_get_model(app);
    uint8_t reduced_new_key = flipboard_model_reduce(model, new_key, false);
    uint32_t detail_counter_ticks = furi_get_tick() + MESSAGE_DISPLAY_TIME_MS;

    FURI_LOG_D(TAG, "SW EVENT: old=%d new=%d reduced=%d", old_key, new_key, reduced_new_key);

    FlipboardBlinkyModel* fbm = flipboard_model_get_custom_data(model);
    if((new_key == (1 | 0 | 0 | 0)) || (new_key == (1 | 2 | 0 | 0))) {
        // Faster by 5ms is left button pressed.  Faster by 20ms if both (button 1+button 2) pressed.
        uint32_t delay = (new_key == 1) ? 5 : 20;
        if(fbm->period_ms > delay) {
            fbm->period_ms -= delay;

            // Don't go faster than 50ms
            if(fbm->period_ms < 50) {
                fbm->period_ms = 50;
            }

            furi_timer_start(fbm->timer, furi_ms_to_ticks(fbm->period_ms));
            fbm->show_details_until = detail_counter_ticks;
        }
    } else if(new_key == (0 | 2 | 0 | 0)) {
        // Slower by 20ms
        uint32_t delay = 20;
        fbm->period_ms += delay;
        furi_timer_start(fbm->timer, furi_ms_to_ticks(fbm->period_ms));
        fbm->show_details_until = detail_counter_ticks;
    } else if(new_key == (0 | 0 | 4 | 0)) {
        // Previous effect
        fbm->effect_id--;
        if(fbm->effect_id < 1) {
            fbm->effect_id = fbm->max_effect_id;
        }
        fbm->show_details_until = detail_counter_ticks;
        flipboard_reset_effect(model);
    } else if(new_key == (0 | 0 | 0 | 8)) {
        // Next effect
        fbm->effect_id++;
        if(fbm->effect_id > fbm->max_effect_id) {
            fbm->effect_id = 1;
        }
        fbm->show_details_until = detail_counter_ticks;
        flipboard_reset_effect(model);
    } else if(new_key == (0 | 0 | 4 | 8)) {
        // Switch to our first effect ("off" when both right buttons are pressed).
        fbm->effect_id = 1;
        fbm->show_details_until = detail_counter_ticks;
        flipboard_reset_effect(model);
    }

    flipboard_model_update_gui(model);
}

/**
 * @brief This method is invoked when the timer ticks.  Do the next step in the effect.
 * @param context The FlipboardModel* context.
 */
void flipboard_tick_callback(void* context) {
    FlipboardModel* model = (FlipboardModel*)context;
    flipboard_do_effect(model);
}

/**
 * @brief This method is invoked when entering the FlipboardBlinky mode.
 * @param context The Flipboard* context.
 * @return true if the key event was handled, false otherwise.
 */
void flipboard_enter_callback(void* context) {
    FlipboardModel* fm = flipboard_get_model((Flipboard*)context);
    FlipboardBlinkyModel* fbm = flipboard_model_get_custom_data(fm);
    fbm->effect_id = INITIAL_EFFECT_ID;
    flipboard_reset_effect(fm);
    flipboard_model_set_button_monitor(fm, flipboard_debounced_switch, (Flipboard*)context);
    furi_timer_start(fbm->timer, furi_ms_to_ticks(fbm->period_ms));
    // flipboard_model_set_gui_refresh_speed_ms(fm, 500);
}

/**
 * @brief This method is invoked when exiting the FlipboardBlinky mode.
 * @param context The Flipboard* context.
 * @return true if the key event was handled, false otherwise.
 */
void flipboard_exit_callback(void* context) {
    FlipboardModel* fm = flipboard_get_model((Flipboard*)context);
    FlipboardBlinkyModel* fbm = flipboard_model_get_custom_data(fm);
    furi_timer_stop(fbm->timer);
    flipboard_model_set_button_monitor(fm, NULL, NULL);
    flipboard_model_set_gui_refresh_speed_ms(fm, 0);
    flipboard_model_set_colors(fm, NULL, 0x0);
}

/**
 * @brief This method configures the View* used for FlipboardBlinky mode.
 * @param context The Flipboard* context.
 * @return View* for FlipboardBlinky mode.
 */
View* get_primary_view(void* context) {
    FlipboardModel* model = flipboard_get_model((Flipboard*)context);
    View* view_primary = view_alloc();
    view_set_draw_callback(view_primary, flipboard_view_flip_keyboard_draw);
    view_set_input_callback(view_primary, flipboard_view_flip_keyboard_input);
    view_set_previous_callback(view_primary, flipboard_navigation_show_app_menu);
    view_set_enter_callback(view_primary, flipboard_enter_callback);
    view_set_exit_callback(view_primary, flipboard_exit_callback);
    view_set_context(view_primary, context);
    view_allocate_model(view_primary, ViewModelTypeLockFree, sizeof(FlipboardModelRef));
    FlipboardModelRef* ref = (FlipboardModelRef*)view_get_model(view_primary);
    ref->model = model;
    return view_primary;
}

/**
 * @brief This method is invoked when the app menu is loaded.
 * @details This method is invoked when the app menu is loaded.  It is used to
 *         display a startup animation.
 * @param model The FlipboardModel* context.
 */
static void loaded_app_menu(FlipboardModel* model) {
    static bool initial_load = true;
    FlipboardLeds* leds = flipboard_model_get_leds(model);
    if(initial_load) {
        for(int i = 0; i < 2; i++) {
            flipboard_leds_set(leds, LedId1, LedColorRed);
            flipboard_leds_set(leds, LedId2, LedColorCyan);
            flipboard_leds_set(leds, LedId3, LedColorMagenta);
            flipboard_leds_set(leds, LedId4, LedColorViolet);
            flipboard_leds_update(leds);
            furi_delay_ms(100);
            flipboard_leds_set(leds, LedId2, LedColorRed);
            flipboard_leds_set(leds, LedId3, LedColorCyan);
            flipboard_leds_set(leds, LedId4, LedColorMagenta);
            flipboard_leds_set(leds, LedId1, LedColorViolet);
            flipboard_leds_update(leds);
            furi_delay_ms(100);
            flipboard_leds_set(leds, LedId3, LedColorRed);
            flipboard_leds_set(leds, LedId4, LedColorCyan);
            flipboard_leds_set(leds, LedId1, LedColorMagenta);
            flipboard_leds_set(leds, LedId2, LedColorViolet);
            flipboard_leds_update(leds);
            furi_delay_ms(100);
            flipboard_leds_set(leds, LedId4, LedColorRed);
            flipboard_leds_set(leds, LedId1, LedColorCyan);
            flipboard_leds_set(leds, LedId2, LedColorMagenta);
            flipboard_leds_set(leds, LedId3, LedColorViolet);
            flipboard_leds_update(leds);
            furi_delay_ms(100);
        }
        initial_load = false;
    }

    flipboard_leds_reset(leds);
    flipboard_leds_update(leds);
}

/**
 * @brief This method is invoked when a custom event is received.
 * @param context The Flipboard* context.
 * @param event The event to handle.
 * @return true if the event was handled, false otherwise.
 */
static bool custom_event_handler(void* context, uint32_t event) {
    FlipboardModel* model = flipboard_get_model((Flipboard*)context);

    if(event == CustomEventAppMenuEnter) {
        loaded_app_menu(model);
    }

    return true;
}

/**
 * @brief This method is invoked when the FlipboardBlinky app is launched.
 * @param _p Unused.
 * @return 0.
*/
int32_t flipboard_blinky_app(void* _p) {
    UNUSED(_p);

    ActionModelFields fields = ActionModelFieldNone;
    bool single_mode_button = true;

    Flipboard* app = flipboard_alloc(
        FLIPBOARD_APP_NAME,
        &I_qr_github,
        ABOUT_TEXT,
        fields,
        NULL, // no default button settings needed.
        single_mode_button,
        NULL,
        NULL,
        0,
        get_primary_view);

    FlipboardModel* model = flipboard_get_model(app);
    FlipboardBlinkyModel* fbm = flipboard_blinky_model_alloc(model);
    flipboard_model_set_custom_data(model, fbm);

    AppSettings* settings = app_settings_alloc(app);
    view_set_previous_callback(
        app_settings_get_view(settings), flipboard_navigation_show_app_menu);
    flipboard_override_config_view(app, app_settings_get_view(settings));

    view_dispatcher_set_event_callback_context(flipboard_get_view_dispatcher(app), app);
    view_dispatcher_set_custom_event_callback(
        flipboard_get_view_dispatcher(app), custom_event_handler);

    view_dispatcher_run(flipboard_get_view_dispatcher(app));

    app_settings_free(settings);
    flipboard_blinky_model_free(fbm);
    flipboard_free(app);

    return 0;
}
