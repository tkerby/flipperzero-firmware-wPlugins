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

uint32_t get_dict_len(FireString* app) {
    size_t dict_len = 0;
    if(app->settings->str_type == StrType_Passphrase) {
        uint32_t i = 0;
        while(app->hid->word_list[i] != NULL && !furi_string_empty(app->hid->word_list[i])) {
            i++;
        }
        dict_len = i;
    } else {
        dict_len = strlen(app->hid->char_list);
    }

    return dict_len;
}

// Get string length or word count if phrase is enabled
uint32_t get_str_len(FireString* app) {
    if(app->settings->str_type == StrType_Passphrase) {
        uint32_t word_count = 0;
        uint32_t string_size = furi_string_size(app->fire_string);
        const char* c_str = malloc(sizeof(char) * string_size);
        c_str = furi_string_get_cstr(app->fire_string);

        if(string_size == 0) {
            return word_count;
        } else {
            word_count = 1;
        }

        for(uint32_t i = 0; i < string_size; i++) {
            if(c_str[i] == '-') {
                word_count++;
            }
        }

        return word_count;
    } else {
        return furi_string_size(app->fire_string);
    }
}

// Use internal rng to append random char or word to fire_string
void random_generator(FireString* app) {
    size_t dict_len = get_dict_len(app);
    furi_hal_random_init();

    if(app->settings->str_type == StrType_Passphrase) {
        uint32_t rnd_buffer = 0;
        rnd_buffer = furi_hal_random_get() & 0xFFF; // Bit mask for 12 bit; max int index 4095
        while(rnd_buffer > dict_len - 1) {
            rnd_buffer = furi_hal_random_get() & 0xFFF;
        }
        if(furi_string_size(app->fire_string) < 1) {
            furi_string_set(app->fire_string, app->hid->word_list[rnd_buffer]);
        } else {
            furi_string_cat_printf(
                app->fire_string, "-%s", furi_string_get_cstr(app->hid->word_list[rnd_buffer]));
        }
    } else {
        uint8_t rnd_byte = 0b00000000;
        furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
        rnd_byte &= 0b01111111; // Bit mask for char
        while(rnd_byte > dict_len - 1) {
            rnd_byte = 0;
            furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
            rnd_byte &= 0b01111111;
        }
        furi_string_push_back(app->fire_string, app->hid->char_list[rnd_byte]);
    }
}

// User internal rng to generate fire_string of str_len
void get_random_str(FireString* app) {
    uint32_t str_len = app->settings->str_len - get_str_len(app);

    for(uint32_t i = 0; i < str_len; i++) {
        random_generator(app);
    }
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
    furi_string_printf(progress, "%ld/%ld", get_str_len(app), app->settings->str_len);

    widget_reset(app->widget);

    widget_add_text_scroll_element(
        app->widget, 0, 1, 128, 45, furi_string_get_cstr(app->fire_string));

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Config", string_generator_btn_callback, app);

    if(get_str_len(app) > 0) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Reset", string_generator_btn_callback, app);
    }

    if(get_str_len(app) == app->settings->str_len) {
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
    size_t dict_len = get_dict_len(app);

    const uint32_t* timings;
    size_t timings_size;

    if(app->ir_worker) {
        infrared_worker_get_raw_signal(signal, &timings, &timings_size);

        uint32_t i = 0;
        if(app->settings->str_type == StrType_Passphrase) {
            while(get_str_len(app) < app->settings->str_len && i < timings_size) {
                if(get_str_len(app) < 1) {
                    furi_string_cat_printf(
                        app->fire_string,
                        "%s",
                        furi_string_get_cstr(app->hid->word_list[timings[i] % dict_len]));
                } else {
                    furi_string_cat_printf(
                        app->fire_string,
                        "-%s",
                        furi_string_get_cstr(app->hid->word_list[timings[i] % dict_len]));
                }
                i++;
            }
        } else {
            while(get_str_len(app) < app->settings->str_len && i < timings_size) {
                furi_string_push_back(
                    app->fire_string, app->hid->char_list[timings[i] % dict_len]);
                i++;
            }
        }
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

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Widget);

    if(app->settings->str_type == StrType_Passphrase && app->hid->word_list == NULL) {
        scene_manager_next_scene(app->scene_manager, FireStringScene_Loading_Word_List);
    }
    if(app->settings->str_type != StrType_Passphrase) {
        get_char_list(app);
    }

    build_string_generator_widget(app);
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
        // Toggle infrared_worker if needed
        if(get_str_len(app) < app->settings->str_len && !app->ir_worker && app->settings->use_ir) {
            infrared_rx_start(app);
        }
        if(get_str_len(app) == app->settings->str_len && app->ir_worker && app->settings->use_ir) {
            infrared_rx_stop(app);
        }
        // animate automatic string generation
        if(get_str_len(app) < app->settings->str_len && !app->settings->use_ir) {
            if(get_str_len(app) > 30) { // arbitrarily skip animation at certain length
                get_random_str(app);
            } else {
                random_generator(app);
                furi_delay_tick(furi_ms_to_ticks(delay_ms));
                if(delay_ms > 1) {
                    delay_ms /= 1.5;
                } else {
                    delay_ms = 0;
                }
            }
            build_string_generator_widget(app);
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
    if(app->settings->str_type == StrType_Passphrase && app->hid->word_list != NULL) { // Phrase
        uint32_t i = 0;
        while(app->hid->word_list[i] != NULL && !furi_string_empty(app->hid->word_list[i])) {
            furi_string_free(app->hid->word_list[i]);
            i++;
        }
        free(app->hid->word_list);
        app->hid->word_list = NULL;
    }
    if(app->settings->str_type != StrType_Passphrase && app->hid->char_list != NULL) {
        free(app->hid->char_list);
    }

    widget_reset(app->widget);
}
