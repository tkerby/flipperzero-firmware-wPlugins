#include <callback/flip_library_callback.h>

// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) \
    furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

static bool flip_library_random_fact_fetch(FactLoaderModel* model) {
    UNUSED(model);
    return flipper_http_get_request("https://uselessfacts.jsph.pl/api/v2/facts/random");
}

static char* flip_library_random_fact_parse(FactLoaderModel* model) {
    UNUSED(model);
    return get_json_value("text", fhttp.last_response, 128);
}

static void flip_library_random_fact_switch_to_view(FlipLibraryApp* app) {
    flip_library_generic_switch_to_view(
        app,
        "Random Fact",
        flip_library_random_fact_fetch,
        flip_library_random_fact_parse,
        1,
        callback_to_random_facts,
        FlipLibraryViewLoader);
}

static bool flip_library_cat_fact_fetch(FactLoaderModel* model) {
    UNUSED(model);
    return flipper_http_get_request_with_headers(
        "https://catfact.ninja/fact", "{\"Content-Type\":\"application/json\"}");
}

static char* flip_library_cat_fact_parse(FactLoaderModel* model) {
    UNUSED(model);
    return get_json_value("fact", fhttp.last_response, 128);
}

static void flip_library_cat_fact_switch_to_view(FlipLibraryApp* app) {
    flip_library_generic_switch_to_view(
        app,
        "Random Cat Fact",
        flip_library_cat_fact_fetch,
        flip_library_cat_fact_parse,
        1,
        callback_to_random_facts,
        FlipLibraryViewLoader);
}

static bool flip_library_dog_fact_fetch(FactLoaderModel* model) {
    UNUSED(model);
    return flipper_http_get_request_with_headers(
        "https://dog-api.kinduff.com/api/facts", "{\"Content-Type\":\"application/json\"}");
}

static char* flip_library_dog_fact_parse(FactLoaderModel* model) {
    UNUSED(model);
    return get_json_array_value("facts", 0, fhttp.last_response, 256);
}

static void flip_library_dog_fact_switch_to_view(FlipLibraryApp* app) {
    flip_library_generic_switch_to_view(
        app,
        "Random Dog Fact",
        flip_library_dog_fact_fetch,
        flip_library_dog_fact_parse,
        1,
        callback_to_random_facts,
        FlipLibraryViewLoader);
}

static bool flip_library_quote_fetch(FactLoaderModel* model) {
    UNUSED(model);
    return flipper_http_get_request("https://zenquotes.io/api/random");
}

static char* flip_library_quote_parse(FactLoaderModel* model) {
    UNUSED(model);
    // remove [ and ] from the start and end of the string
    char* response = fhttp.last_response;
    if(response[0] == '[') {
        response++;
    }
    if(response[strlen(response) - 1] == ']') {
        response[strlen(response) - 1] = '\0';
    }
    // remove white space from both sides
    while(response[0] == ' ') {
        response++;
    }
    while(response[strlen(response) - 1] == ' ') {
        response[strlen(response) - 1] = '\0';
    }
    return get_json_value("q", response, 128);
}

static void flip_library_quote_switch_to_view(FlipLibraryApp* app) {
    flip_library_generic_switch_to_view(
        app,
        "Random Quote",
        flip_library_quote_fetch,
        flip_library_quote_parse,
        1,
        callback_to_random_facts,
        FlipLibraryViewLoader);
}

static bool flip_library_dictionary_fetch(FactLoaderModel* model) {
    UNUSED(model);
    char payload[128];
    snprintf(
        payload, sizeof(payload), "{\"word\":\"%s\"}", app_instance->uart_text_input_buffer_query);

    return flipper_http_post_request_with_headers(
        "https://www.flipsocial.net/api/define/",
        "{\"Content-Type\":\"application/json\"}",
        payload);
}

static char* flip_library_dictionary_parse(FactLoaderModel* model) {
    UNUSED(model);
    char* defn = get_json_value("definition", fhttp.last_response, 16);
    if(defn == NULL) {
        defn = get_json_value("[ERROR]", fhttp.last_response, 16);
    }
    return defn;
}

static void flip_library_dictionary_switch_to_view(FlipLibraryApp* app) {
    uart_text_input_set_header_text(app->uart_text_input_query, "Enter a word");
    flip_library_generic_switch_to_view(
        app,
        "Defining",
        flip_library_dictionary_fetch,
        flip_library_dictionary_parse,
        1,
        callback_to_submenu,
        FlipLibraryViewTextInputQuery);
}

static void flip_library_request_error_draw(Canvas* canvas) {
    if(canvas == NULL) {
        FURI_LOG_E(TAG, "flip_library_request_error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if(fhttp.last_response != NULL) {
        if(strstr(fhttp.last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") !=
           NULL) {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 22, "Failed to reconnect.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        } else if(strstr(fhttp.last_response, "[ERROR] Failed to connect to Wifi.") != NULL) {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        } else if(strstr(fhttp.last_response, "[PONG]") != NULL) {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
        } else {
            canvas_clear(canvas);
            FURI_LOG_E(TAG, "Received an error: %s", fhttp.last_response);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Unusual error...");
            canvas_draw_str(canvas, 0, 60, "Press BACK and retry.");
        }
    } else {
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "Failed to receive data.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
    }
}

static void flip_library_widget_set_text(char* message, Widget** widget) {
    if(widget == NULL) {
        FURI_LOG_E(TAG, "flip_library_set_widget_text - widget is NULL");
        DEV_CRASH();
        return;
    }
    if(message == NULL) {
        FURI_LOG_E(TAG, "flip_library_set_widget_text - message is NULL");
        DEV_CRASH();
        return;
    }
    widget_reset(*widget);

    uint32_t message_length = strlen(message); // Length of the message
    uint32_t i = 0; // Index tracker
    uint32_t formatted_index = 0; // Tracker for where we are in the formatted message
    char* formatted_message; // Buffer to hold the final formatted message
    if(!easy_flipper_set_buffer(&formatted_message, message_length * 2 + 1)) {
        return;
    }

    while(i < message_length) {
        // TODO: Use canvas_glyph_width to calculate the maximum characters for the line
        uint32_t max_line_length = 29; // Maximum characters per line
        uint32_t remaining_length = message_length - i; // Remaining characters
        uint32_t line_length = (remaining_length < max_line_length) ? remaining_length :
                                                                      max_line_length;

        // Temporary buffer to hold the current line
        char line[30];
        strncpy(line, message + i, line_length);
        line[line_length] = '\0';

        // Check if the line ends in the middle of a word and adjust accordingly
        if(line_length == 29 && message[i + line_length] != '\0' &&
           message[i + line_length] != ' ') {
            // Find the last space within the 30-character segment
            char* last_space = strrchr(line, ' ');
            if(last_space != NULL) {
                // Adjust the line length to avoid cutting the word
                line_length = last_space - line;
                line[line_length] = '\0'; // Null-terminate at the space
            }
        }

        // Manually copy the fixed line into the formatted_message buffer
        for(uint32_t j = 0; j < line_length; j++) {
            formatted_message[formatted_index++] = line[j];
        }

        // Add a newline character for line spacing
        formatted_message[formatted_index++] = '\n';

        // Move i forward to the start of the next word
        i += line_length;

        // Skip spaces at the beginning of the next line
        while(message[i] == ' ') {
            i++;
        }
    }

    // Add the formatted message to the widget
    widget_add_text_scroll_element(*widget, 0, 0, 128, 64, formatted_message);
}

void flip_library_loader_draw_callback(Canvas* canvas, void* model) {
    if(!canvas || !model) {
        FURI_LOG_E(TAG, "flip_library_loader_draw_callback - canvas or model is NULL");
        return;
    }

    SerialState http_state = fhttp.state;
    FactLoaderModel* fact_loader_model = (FactLoaderModel*)model;
    FactState fact_state = fact_loader_model->fact_state;
    char* title = fact_loader_model->title;

    canvas_set_font(canvas, FontSecondary);

    if(http_state == INACTIVE) {
        canvas_draw_str(canvas, 0, 7, "Wifi Dev Board disconnected.");
        canvas_draw_str(canvas, 0, 17, "Please connect to the board.");
        canvas_draw_str(canvas, 0, 32, "If your board is connected,");
        canvas_draw_str(canvas, 0, 42, "make sure you have flashed");
        canvas_draw_str(canvas, 0, 52, "your WiFi Devboard with the");
        canvas_draw_str(canvas, 0, 62, "latest FlipperHTTP flash.");
        return;
    }

    if(fact_state == FactStateError || fact_state == FactStateParseError) {
        flip_library_request_error_draw(canvas);
        return;
    }

    canvas_draw_str(canvas, 0, 7, title);
    canvas_draw_str(canvas, 0, 15, "Loading...");

    if(fact_state == FactStateInitial) {
        return;
    }

    if(http_state == SENDING) {
        canvas_draw_str(canvas, 0, 22, "Sending...");
        return;
    }

    if(http_state == RECEIVING || fact_state == FactStateRequested) {
        canvas_draw_str(canvas, 0, 22, "Receiving...");
        return;
    }

    if(http_state == IDLE && fact_state == FactStateReceived) {
        canvas_draw_str(canvas, 0, 22, "Processing...");
        return;
    }

    if(http_state == IDLE && fact_state == FactStateParsed) {
        canvas_draw_str(canvas, 0, 22, "Processed...");
        return;
    }
}

static void flip_library_loader_process_callback(void* context) {
    if(context == NULL) {
        FURI_LOG_E(TAG, "flip_library_loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    FlipLibraryApp* app = (FlipLibraryApp*)context;
    View* view = app->view_loader;

    FactState current_fact_state;
    with_view_model(
        view, FactLoaderModel * model, { current_fact_state = model->fact_state; }, false);

    if(current_fact_state == FactStateInitial) {
        with_view_model(
            view,
            FactLoaderModel * model,
            {
                model->fact_state = FactStateRequested;
                FactLoaderFetch fetch = model->fetcher;
                if(fetch == NULL) {
                    FURI_LOG_E(TAG, "Model doesn't have Fetch function assigned.");
                    model->fact_state = FactStateError;
                    return;
                }

                // Clear any previous responses
                strncpy(fhttp.last_response, "", 1);
                bool request_status = fetch(model);
                if(!request_status) {
                    model->fact_state = FactStateError;
                }
            },
            true);
    } else if(current_fact_state == FactStateRequested || current_fact_state == FactStateError) {
        if(fhttp.state == IDLE && fhttp.last_response != NULL) {
            if(strstr(fhttp.last_response, "[PONG]") != NULL) {
                FURI_LOG_DEV(TAG, "PONG received.");
            } else if(strncmp(fhttp.last_response, "[SUCCESS]", 9) == 0) {
                FURI_LOG_DEV(
                    TAG,
                    "SUCCESS received. %s",
                    fhttp.last_response ? fhttp.last_response : "NULL");
            } else if(strncmp(fhttp.last_response, "[ERROR]", 9) == 0) {
                FURI_LOG_DEV(
                    TAG, "ERROR received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            } else if(strlen(fhttp.last_response) == 0) {
                // Still waiting on response
            } else {
                with_view_model(
                    view,
                    FactLoaderModel * model,
                    { model->fact_state = FactStateReceived; },
                    true);
            }
        } else if(fhttp.state == SENDING || fhttp.state == RECEIVING) {
            // continue waiting
        } else if(fhttp.state == INACTIVE) {
            // inactive. try again
        } else if(fhttp.state == ISSUE) {
            with_view_model(
                view, FactLoaderModel * model, { model->fact_state = FactStateError; }, true);
        } else {
            FURI_LOG_DEV(
                TAG,
                "Unexpected state: %d lastresp: %s",
                fhttp.state,
                fhttp.last_response ? fhttp.last_response : "NULL");
            DEV_CRASH();
        }
    } else if(current_fact_state == FactStateReceived) {
        with_view_model(
            view,
            FactLoaderModel * model,
            {
                char* fact_text;
                if(model->parser == NULL) {
                    fact_text = NULL;
                    FURI_LOG_DEV(TAG, "Parser is NULL");
                    DEV_CRASH();
                } else {
                    fact_text = model->parser(model);
                }
                FURI_LOG_DEV(
                    TAG,
                    "Parsed fact: %s\r\ntext: %s",
                    fhttp.last_response ? fhttp.last_response : "NULL",
                    fact_text ? fact_text : "NULL");
                model->fact_text = fact_text;
                if(fact_text == NULL) {
                    model->fact_state = FactStateParseError;
                } else {
                    model->fact_state = FactStateParsed;
                }
            },
            true);
    } else if(current_fact_state == FactStateParsed) {
        with_view_model(
            view,
            FactLoaderModel * model,
            {
                if(++model->request_index < model->request_count) {
                    model->fact_state = FactStateInitial;
                } else {
                    flip_library_widget_set_text(
                        model->fact_text != NULL ? model->fact_text : "Empty result",
                        &app_instance->widget_result);
                    if(model->fact_text != NULL) {
                        free(model->fact_text);
                        model->fact_text = NULL;
                    }
                    view_set_previous_callback(
                        widget_get_view(app_instance->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(
                        app_instance->view_dispatcher, FlipLibraryViewWidgetResult);
                }
            },
            true);
    }
}

static void flip_library_loader_timer_callback(void* context) {
    if(context == NULL) {
        FURI_LOG_E(TAG, "flip_library_loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipLibraryCustomEventProcess);
}

static void flip_library_loader_on_enter(void* context) {
    if(context == NULL) {
        FURI_LOG_E(TAG, "flip_library_loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    View* view = app->view_loader;
    with_view_model(
        view,
        FactLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if(model->timer == NULL) {
                model->timer = furi_timer_alloc(
                    flip_library_loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void flip_library_loader_on_exit(void* context) {
    if(context == NULL) {
        FURI_LOG_E(TAG, "flip_library_loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    View* view = app->view_loader;
    with_view_model(
        view,
        FactLoaderModel * model,
        {
            if(model->timer) {
                furi_timer_stop(model->timer);
            }
        },
        false);
}

void flip_library_loader_init(View* view) {
    if(view == NULL) {
        FURI_LOG_E(TAG, "flip_library_loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FactLoaderModel));
    view_set_enter_callback(view, flip_library_loader_on_enter);
    view_set_exit_callback(view, flip_library_loader_on_exit);
}

void flip_library_loader_free_model(View* view) {
    if(view == NULL) {
        FURI_LOG_E(TAG, "flip_library_loader_free_model - view is NULL");
        DEV_CRASH();
        return;
    }
    with_view_model(
        view,
        FactLoaderModel * model,
        {
            if(model->timer) {
                furi_timer_free(model->timer);
                model->timer = NULL;
            }
            if(model->parser_context) {
                free(model->parser_context);
                model->parser_context = NULL;
            }
        },
        false);
}

bool flip_library_custom_event_callback(void* context, uint32_t index) {
    if(context == NULL) {
        FURI_LOG_E(TAG, "flip_library_custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch(index) {
    case FlipLibraryCustomEventProcess:
        flip_library_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "flip_library_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void callback_submenu_choices(void* context, uint32_t index) {
    if(context == NULL) {
        FURI_LOG_E(TAG, "callback_submenu_choices - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    switch(index) {
    case FlipLibrarySubmenuIndexRandomFacts:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewRandomFacts);
        break;
    case FlipLibrarySubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewAbout);
        break;
    case FlipLibrarySubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSettings);
        break;
    case FlipLibrarySubmenuIndexDictionary:
        flip_library_dictionary_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsCats:
        flip_library_cat_fact_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsDogs:
        flip_library_dog_fact_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsQuotes:
        flip_library_quote_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsAll:
        flip_library_random_fact_switch_to_view(app);
        break;
    default:
        break;
    }
}

void text_updated_ssid(void* context) {
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    if(!app) {
        FURI_LOG_E(TAG, "text_updated_ssid - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }

    // store the entered text
    strncpy(
        app->uart_text_input_buffer_ssid,
        app->uart_text_input_temp_buffer_ssid,
        app->uart_text_input_buffer_size_ssid);

    // Ensure null-termination
    app->uart_text_input_buffer_ssid[app->uart_text_input_buffer_size_ssid - 1] = '\0';

    // update the variable item text
    if(app->variable_item_ssid) {
        variable_item_set_current_value_text(
            app->variable_item_ssid, app->uart_text_input_buffer_ssid);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password);

    // save wifi settings to devboard
    if(strlen(app->uart_text_input_buffer_ssid) > 0 &&
       strlen(app->uart_text_input_buffer_password) > 0) {
        if(!flipper_http_save_wifi(
               app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password)) {
            FURI_LOG_E(TAG, "Failed to save wifi settings.");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSettings);
}

void text_updated_password(void* context) {
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    if(!app) {
        FURI_LOG_E(TAG, "text_updated_password - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }

    // store the entered text
    strncpy(
        app->uart_text_input_buffer_password,
        app->uart_text_input_temp_buffer_password,
        app->uart_text_input_buffer_size_password);

    // Ensure null-termination
    app->uart_text_input_buffer_password[app->uart_text_input_buffer_size_password - 1] = '\0';

    // update the variable item text
    if(app->variable_item_password) {
        variable_item_set_current_value_text(
            app->variable_item_password, app->uart_text_input_buffer_password);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password);

    // save wifi settings to devboard
    if(strlen(app->uart_text_input_buffer_ssid) > 0 &&
       strlen(app->uart_text_input_buffer_password) > 0) {
        if(!flipper_http_save_wifi(
               app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password)) {
            FURI_LOG_E(TAG, "Failed to save wifi settings");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSettings);
}

void text_updated_query(void* context) {
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    if(!app) {
        FURI_LOG_E(TAG, "text_updated_query - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }

    // store the entered text
    strncpy(
        app->uart_text_input_buffer_query,
        app->uart_text_input_temp_buffer_query,
        app->uart_text_input_buffer_size_query);

    // Ensure null-termination
    app->uart_text_input_buffer_query[app->uart_text_input_buffer_size_query - 1] = '\0';

    // switch to the loader view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewLoader);
}

uint32_t callback_to_submenu(void* context) {
    UNUSED(context);
    return FlipLibraryViewSubmenuMain;
}

uint32_t callback_to_wifi_settings(void* context) {
    UNUSED(context);
    return FlipLibraryViewSettings;
}

uint32_t callback_to_random_facts(void* context) {
    UNUSED(context);
    return FlipLibraryViewRandomFacts;
}

void settings_item_selected(void* context, uint32_t index) {
    FlipLibraryApp* app = (FlipLibraryApp*)context;
    if(!app) {
        FURI_LOG_E(TAG, "settings_item_selected - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }
    switch(index) {
    case 0: // Input SSID
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewTextInputSSID);
        break;
    case 1: // Input Password
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewTextInputPassword);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t callback_exit_app(void* context) {
    // Exit the application
    if(!context) {
        FURI_LOG_E(TAG, "callback_exit_app - Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}

void flip_library_generic_switch_to_view(
    FlipLibraryApp* app,
    char* title,
    FactLoaderFetch fetcher,
    FactLoaderParser parser,
    size_t request_count,
    ViewNavigationCallback back,
    uint32_t view_id) {
    if(app == NULL) {
        FURI_LOG_E(TAG, "flip_library_generic_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View* view = app->view_loader;
    if(view == NULL) {
        FURI_LOG_E(TAG, "flip_library_generic_switch_to_view - view is NULL");
        DEV_CRASH();
        return;
    }

    with_view_model(
        view,
        FactLoaderModel * model,
        {
            model->title = title;
            model->fetcher = fetcher;
            model->parser = parser;
            model->request_index = 0;
            model->request_count = request_count;
            model->back_callback = back;
            model->fact_state = FactStateInitial;
            model->fact_text = NULL;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
