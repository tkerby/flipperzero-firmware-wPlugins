#include "../fire_string.h"
#include "furi_hal_random.h"

#define ALPH_ROWS 2
#define NUMS_ROWS 1
#define SYMB_ROWS 4
#define BIN_ROWS  1
#define COLS      2

void build_string_generator_widget(FireString* app);

#define DEFAULT_DELAY 250
uint32_t delay_ms = DEFAULT_DELAY;

void get_char_list(FireString* app) {
    FuriString* list = furi_string_alloc();

    // Alphabet: Uppercase (65-90) and Lowercase (97-122)
    const int alphabet_ranges[ALPH_ROWS][COLS] = {
        {65, 90}, // Uppercase A-Z
        {97, 122} // Lowercase a-z
    };

    // Numbers: Digits (48-57)
    int numbers_ranges[NUMS_ROWS][COLS] = {
        {48, 57} // Digits 0-9
    };

    // Symbols: Multiple ranges
    int symbols_ranges[SYMB_ROWS][COLS] = {
        {33, 47}, // Exclamation mark to Slash (! to /)
        {58, 64}, // Colon to At Symbol (: to @)
        {91, 96}, // Opening Bracket to Backtick ([ to `)
        {123, 126} // Opening Curly Brace to Tilde ({ to ~)
    };

    // Binary: (48-49)
    int binary_range[BIN_ROWS][COLS] = {
        {48, 49} // Zero to One (0 to 1)
    };

    switch(app->settings->str_type) {
    case(StrType_AlphaNumSymb):
        // cat alphabetic chars
        for(uint8_t i = 0; i < ALPH_ROWS; i++) {
            for(uint8_t j = alphabet_ranges[i][0]; j <= alphabet_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        // cat numerical chars
        for(uint8_t i = 0; i < NUMS_ROWS; i++) {
            for(uint8_t j = numbers_ranges[i][0]; j <= numbers_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        // cat symbol chars
        for(uint8_t i = 0; i < SYMB_ROWS; i++) {
            for(uint8_t j = symbols_ranges[i][0]; j <= symbols_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        break;
    case(StrType_AlphaNum):
        // cat alphabetic chars
        for(uint8_t i = 0; i < ALPH_ROWS; i++) {
            for(uint8_t j = alphabet_ranges[i][0]; j <= alphabet_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        // cat numerical chars
        for(uint8_t i = 0; i < NUMS_ROWS; i++) {
            for(uint8_t j = numbers_ranges[i][0]; j <= numbers_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        break;
    case(StrType_Alpha):
        // cat alphabetic chars
        for(uint8_t i = 0; i < ALPH_ROWS; i++) {
            for(uint8_t j = alphabet_ranges[i][0]; j <= alphabet_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        break;
    case(StrType_Symb):
        // cat symbol chars
        for(uint8_t i = 0; i < SYMB_ROWS; i++) {
            for(uint8_t j = symbols_ranges[i][0]; j <= symbols_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        break;
    case(StrType_Num):
        // cat numerical chars
        for(uint8_t i = 0; i < NUMS_ROWS; i++) {
            for(uint8_t j = numbers_ranges[i][0]; j <= numbers_ranges[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        break;
    case(StrType_Bin):
        // cat binary chars
        for(uint8_t i = 0; i < BIN_ROWS; i++) {
            for(uint8_t j = binary_range[i][0]; j <= binary_range[i][COLS - 1]; j++) {
                furi_string_cat_printf(list, "%c", j);
            }
        }
        break;
    }

    app->hid->char_list = (char*)malloc(sizeof(char) * furi_string_size(list));
    memcpy(app->hid->char_list, furi_string_get_cstr(list), furi_string_size(list));
    furi_string_free(list);
}

void get_random_str(FireString* app) {
    uint8_t rnd_byte = 0b00000000;
    uint32_t str_len = app->settings->str_len - furi_string_size(app->fire_string);
    char* buffer = malloc(sizeof(char) * str_len + 1);
    const size_t char_list_len = strlen(app->hid->char_list);

    furi_hal_random_init();

    for(uint32_t i = 0; i < str_len; i++) {
        furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
        rnd_byte &= 0b01111111;
        while(rnd_byte > char_list_len - 1) {
            rnd_byte = 0;
            furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
            rnd_byte &= 0b01111111;
        }
        buffer[i] = app->hid->char_list[rnd_byte];
    }
    buffer[str_len + 1] = '\0';

    furi_string_cat_str(app->fire_string, buffer);

    free(buffer);
}

void get_random_char(FireString* app) {
    uint8_t rnd_byte = 0b00000000;
    char buffer;
    const size_t char_list_len = strlen(app->hid->char_list);

    furi_hal_random_init();

    furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
    rnd_byte &= 0b01111111;
    while(rnd_byte > char_list_len - 1) {
        rnd_byte = 0;
        furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
        rnd_byte &= 0b01111111;
    }
    buffer = app->hid->char_list[rnd_byte];

    furi_string_cat_printf(app->fire_string, "%c", buffer);
}

void string_generator_btn_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(TAG, "string_generator_btn_callback");
    furi_assert(context);

    FireString* app = context;

    switch(type) {
    case InputTypeShort:
        switch(result) {
        case GuiButtonTypeRight:
            scene_manager_next_scene(app->scene_manager, FireStringScene_GenerateStepTwo);
            break;
        case GuiButtonTypeLeft:
            scene_manager_next_scene(app->scene_manager, FireStringScene_Settings);
            break;
        case GuiButtonTypeCenter:
            furi_string_reset(app->fire_string);
            delay_ms = DEFAULT_DELAY;
            if(app->settings->use_ir == true) {
                build_string_generator_widget(app);
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void build_string_generator_widget(FireString* app) {
    FURI_LOG_T(TAG, "build_string_generator_widget");

    FuriString* progress = furi_string_alloc();
    furi_string_printf(
        progress, "%d/%ld", furi_string_size(app->fire_string), app->settings->str_len);

    widget_reset(app->widget);

    widget_add_text_scroll_element(
        app->widget, 0, 1, 128, 45, furi_string_get_cstr(app->fire_string));

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Config", string_generator_btn_callback, app);

    if(furi_string_size(app->fire_string) > 0) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Reset", string_generator_btn_callback, app);
    }

    if(furi_string_size(app->fire_string) == app->settings->str_len) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Next", string_generator_btn_callback, app);
    } else {
        widget_add_string_element(
            app->widget,
            85,
            56,
            AlignLeft,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(progress));
    }

    furi_string_free(progress);
}

static void ir_received_callback(void* context, InfraredWorkerSignal* signal) {
    FURI_LOG_T(TAG, "ir_received_callback");
    furi_assert(context);

    FireString* app = context;
    const size_t char_list_len = strlen(app->hid->char_list);

    const uint32_t* timings;
    size_t timings_size;

    if(app->ir_worker) {
        infrared_worker_get_raw_signal(signal, &timings, &timings_size);

        char* tmpStr = malloc(sizeof(char) * app->settings->str_len + 1);
        uint32_t i = 0;
        while((furi_string_size(app->fire_string) + strlen(tmpStr)) < app->settings->str_len &&
              i < timings_size) {
            tmpStr[i] = app->hid->char_list[timings[i] % char_list_len];
            i++;
        }
        if(furi_string_size(app->fire_string) > 0) {
            if(furi_string_size(app->fire_string) < app->settings->str_len) {
                furi_string_cat_str(app->fire_string, tmpStr);
            }
        } else {
            furi_string_set_str(app->fire_string, tmpStr);
        }
        free(tmpStr);
    }

    build_string_generator_widget(app);
}

void infrared_rx_start(FireString* app) {
    FURI_LOG_T(TAG, "infrared_rx_start");

    app->ir_worker = infrared_worker_alloc();

    infrared_worker_rx_enable_signal_decoding(app->ir_worker, false);
    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, true);
    infrared_worker_rx_set_received_signal_callback(app->ir_worker, ir_received_callback, app);
    infrared_worker_rx_start(app->ir_worker);
}

void infrared_rx_stop(FireString* app) {
    FURI_LOG_T(TAG, "infrared_rx_stop");

    infrared_worker_rx_stop(app->ir_worker);
    infrared_worker_free(app->ir_worker);
    app->ir_worker = NULL;
}

void fire_string_scene_on_enter_string_generator(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_string_generator");
    furi_check(context);

    FireString* app = context;

    get_char_list(app);

    build_string_generator_widget(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Widget);
}

bool fire_string_scene_on_event_string_generator(void* context, SceneManagerEvent event) {
    // FURI_LOG_T(TAG, "fire_string_scene_on_event_string_generator");
    furi_check(context);

    FireString* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        break;
    case SceneManagerEventTypeBack:
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FireStringScene_MainMenu);
        consumed = true;
        break;
    case SceneManagerEventTypeTick:
        // animate automatic string generation
        if(furi_string_size(app->fire_string) < app->settings->str_len && !app->settings->use_ir) {
            if(furi_string_size(app->fire_string) > 30) {
                get_random_str(app);
            } else {
                get_random_char(app);
                furi_delay_tick(furi_ms_to_ticks(delay_ms));
                if(delay_ms > 1) {
                    delay_ms /= 1.5;
                } else {
                    delay_ms = 0;
                }
            }
            build_string_generator_widget(app);
        }
        // Toggle infrared_worker if needed
        if(furi_string_size(app->fire_string) < app->settings->str_len && !app->ir_worker) {
            infrared_rx_start(app);
        }
        if(furi_string_size(app->fire_string) == app->settings->str_len && app->ir_worker) {
            infrared_rx_stop(app);
        }
        break;
    }

    return consumed;
}

void fire_string_scene_on_exit_string_generator(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_string_generator");
    furi_check(context);

    FireString* app = context;
    if(app->ir_worker != NULL) {
        infrared_rx_stop(app);
    }
    free(app->hid->char_list);

    widget_reset(app->widget);
}
