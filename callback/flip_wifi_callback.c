#include <callback/flip_wifi_callback.h>

static char *ssid_list[64];
static uint32_t ssid_index = 0;
static char current_ssid[64];
static char current_password[64];

static void flip_wifi_redraw_submenu_saved(void *context);
static void flip_wifi_view_draw_callback_scan(Canvas *canvas, void *model);
static void flip_wifi_view_draw_callback_saved(Canvas *canvas, void *model);
static bool flip_wifi_view_input_callback_scan(InputEvent *event, void *context);
static bool flip_wifi_view_input_callback_saved(InputEvent *event, void *context);
static uint32_t callback_to_submenu_saved(void *context);
static uint32_t callback_to_submenu_scan(void *context);
static uint32_t callback_to_submenu_main(void *context);

void flip_wifi_text_updated_password_scan(void *context);
void flip_wifi_text_updated_password_saved(void *context);
void flip_wifi_text_updated_add_ssid(void *context);
void flip_wifi_text_updated_add_password(void *context);

static bool flip_wifi_alloc_playlist(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    if (!wifi_playlist)
    {
        wifi_playlist = (WiFiPlaylist *)malloc(sizeof(WiFiPlaylist));
        if (!wifi_playlist)
        {
            FURI_LOG_E(TAG, "Failed to allocate playlist");
            return false;
        }
        wifi_playlist->count = 0;
    }
    // Load the playlist from storage
    if (!load_playlist(wifi_playlist))
    {
        FURI_LOG_E(TAG, "Failed to load playlist");

        // playlist is empty?
        submenu_reset(app->submenu_wifi);
        submenu_set_header(app->submenu_wifi, "Saved APs");
        submenu_add_item(app->submenu_wifi, "[Add Network]", FlipWiFiSubmenuIndexWiFiSavedAddSSID, callback_submenu_choices, app);
    }
    else
    {
        // Update the submenu
        flip_wifi_redraw_submenu_saved(app);
    }
    return true;
}
static void flip_wifi_free_playlist(void)
{
    if (wifi_playlist)
    {
        free(wifi_playlist);
        wifi_playlist = NULL;
    }
}

static bool flip_wifi_alloc_views(void *context, uint32_t view)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    switch (view)
    {
    case FlipWiFiViewWiFiScan:
        if (!app->view_wifi)
        {
            if (!easy_flipper_set_view(&app->view_wifi, FlipWiFiViewGeneric, flip_wifi_view_draw_callback_scan, flip_wifi_view_input_callback_scan, callback_to_submenu_scan, &app->view_dispatcher, app))
            {
                return false;
            }
            if (!app->view_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate view for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewWiFiSaved:
        if (!app->view_wifi)
        {
            if (!easy_flipper_set_view(&app->view_wifi, FlipWiFiViewGeneric, flip_wifi_view_draw_callback_saved, flip_wifi_view_input_callback_saved, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                return false;
            }
            if (!app->view_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate view for WiFi Scan");
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}
static void flip_wifi_free_views(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    if (app->view_wifi)
    {
        free(app->view_wifi);
        app->view_wifi = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewGeneric);
    }
}
static bool flip_wifi_alloc_widgets(void *context, uint32_t widget)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    switch (widget)
    {
    case FlipWiFiViewAbout:
        if (!app->widget_info)
        {
            if (!easy_flipper_set_widget(&app->widget_info, FlipWiFiViewAbout, "FlipWiFi v1.3\n-----\nFlipperHTTP companion app.\nScan and save WiFi networks.\n-----\nwww.github.com/jblanked", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->widget_info)
            {
                FURI_LOG_E(TAG, "Failed to allocate widget for About");
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}
static void flip_wifi_free_widgets(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    if (app->widget_info)
    {
        free(app->widget_info);
        app->widget_info = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewAbout);
    }
}
static bool flip_wifi_alloc_submenus(void *context, uint32_t view)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    switch (view)
    {
    case FlipWiFiViewSubmenuScan:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "WiFi Nearby", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewSubmenuSaved:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "Saved APs", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for WiFi Saved");
                return false;
            }
            if (!flip_wifi_alloc_playlist(app))
            {
                FURI_LOG_E(TAG, "Failed to allocate playlist");
                return false;
            }
        }
        return true;
    case FlipWiFiViewSubmenuCommands:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "Fast Commands", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for Commands");
                return false;
            }
            //  PING, LIST, WIFI/LIST, IP/ADDRESS, and WIFI/IP.
            submenu_add_item(app->submenu_wifi, "[CUSTOM]", FlipWiFiSubmenuIndexFastCommandStart + 0, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "PING", FlipWiFiSubmenuIndexFastCommandStart + 1, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "LIST", FlipWiFiSubmenuIndexFastCommandStart + 2, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "IP/ADDRESS", FlipWiFiSubmenuIndexFastCommandStart + 3, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "WIFI/IP", FlipWiFiSubmenuIndexFastCommandStart + 4, callback_submenu_choices, app);
        }
        return true;
    }
    return false;
}
static void flip_wifi_free_submenus(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    if (app->submenu_wifi)
    {
        free(app->submenu_wifi);
        app->submenu_wifi = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewSubmenu);
    }
}

static void flip_wifi_custom_command_updated(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    if (!app->uart_text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "Text input buffer is NULL");
        return;
    }
    if (!app->uart_text_input_temp_buffer[0])
    {
        FURI_LOG_E(TAG, "Text input buffer is empty");
        return;
    }
    // Send the custom command
    flipper_http_send_data(app->uart_text_input_temp_buffer);
    while (fhttp.last_response == NULL || strlen(fhttp.last_response) == 0)
    {
        furi_delay_ms(100);
    }
    // Switch to the view
    char response[100];
    snprintf(response, sizeof(response), "%s", fhttp.last_response);
    easy_flipper_dialog("", response);
    flipper_http_deinit();
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}

static bool flip_wifi_alloc_text_inputs(void *context, uint32_t view)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    app->uart_text_input_buffer_size = MAX_SSID_LENGTH;
    if (!app->uart_text_input_buffer)
    {
        if (!easy_flipper_set_buffer(&app->uart_text_input_buffer, app->uart_text_input_buffer_size))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input buffer");
            return false;
        }
        if (!app->uart_text_input_buffer)
        {
            FURI_LOG_E(TAG, "Failed to allocate text input buffer");
            return false;
        }
    }
    if (!app->uart_text_input_temp_buffer)
    {
        if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input temp buffer");
            return false;
        }
        if (!app->uart_text_input_temp_buffer)
        {
            FURI_LOG_E(TAG, "Failed to allocate text input temp buffer");
            return false;
        }
    }
    switch (view)
    {
    case FlipWiFiViewTextInputScan:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter WiFi Password", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, flip_wifi_text_updated_password_scan, callback_to_submenu_scan, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Scan");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputSaved:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter WiFi Password", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, flip_wifi_text_updated_password_saved, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputSavedAddSSID:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter SSID", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, flip_wifi_text_updated_add_ssid, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add SSID");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add SSID");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputSavedAddPassword:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter Password", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, flip_wifi_text_updated_add_password, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
                return false;
            }
        }
        return true;
    case FlipWiFiSubmenuIndexFastCommandStart:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter Command", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, flip_wifi_custom_command_updated, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return false;
            }
        }
        return true;
    }
    return false;
}
static void flip_wifi_free_text_inputs(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    if (app->uart_text_input)
    {
        free(app->uart_text_input);
        app->uart_text_input = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewTextInput);
    }
    if (app->uart_text_input_buffer)
    {
        free(app->uart_text_input_buffer);
        app->uart_text_input_buffer = NULL;
    }
    if (app->uart_text_input_temp_buffer)
    {
        free(app->uart_text_input_temp_buffer);
        app->uart_text_input_temp_buffer = NULL;
    }
}

void flip_wifi_free_all(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    flip_wifi_free_views(app);
    flip_wifi_free_widgets(app);
    flip_wifi_free_submenus(app);
    flip_wifi_free_text_inputs(app);
    flip_wifi_free_playlist();
}

static void flip_wifi_redraw_submenu_saved(void *context)
{
    // re draw the saved submenu
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    if (!app->submenu_wifi)
    {
        FURI_LOG_E(TAG, "Submenu is NULL");
        return;
    }
    if (!wifi_playlist)
    {
        FURI_LOG_E(TAG, "WiFi Playlist is NULL");
        return;
    }
    submenu_reset(app->submenu_wifi);
    submenu_set_header(app->submenu_wifi, "Saved APs");
    submenu_add_item(app->submenu_wifi, "[Add Network]", FlipWiFiSubmenuIndexWiFiSavedAddSSID, callback_submenu_choices, app);
    for (size_t i = 0; i < wifi_playlist->count; i++)
    {
        submenu_add_item(app->submenu_wifi, wifi_playlist->ssids[i], FlipWiFiSubmenuIndexWiFiSavedStart + i, callback_submenu_choices, app);
    }
}

static uint32_t callback_to_submenu_main(void *context)
{
    UNUSED(context);
    ssid_index = 0;
    return FlipWiFiViewSubmenuMain;
}
static uint32_t callback_to_submenu_scan(void *context)
{
    UNUSED(context);
    ssid_index = 0;
    return FlipWiFiViewSubmenu;
}
static uint32_t callback_to_submenu_saved(void *context)
{
    UNUSED(context);
    ssid_index = 0;
    return FlipWiFiViewSubmenu;
}
uint32_t callback_exit_app(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

// Callback for drawing the main screen
static void flip_wifi_view_draw_callback_scan(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, ssid_list[ssid_index]);
    canvas_draw_icon(canvas, 0, 53, &I_ButtonBACK_10x8);
    canvas_draw_str_aligned(canvas, 12, 54, AlignLeft, AlignTop, "Back");
    canvas_draw_icon(canvas, 96, 53, &I_ButtonRight_4x7);
    canvas_draw_str_aligned(canvas, 103, 54, AlignLeft, AlignTop, "Add");
}
static void flip_wifi_view_draw_callback_saved(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, current_ssid);
    canvas_set_font(canvas, FontSecondary);
    char password[72];
    snprintf(password, sizeof(password), "Pass: %s", current_password);
    canvas_draw_str(canvas, 0, 20, password);
    canvas_draw_icon(canvas, 0, 54, &I_ButtonLeft_4x7);
    canvas_draw_str_aligned(canvas, 7, 54, AlignLeft, AlignTop, "Delete");
    canvas_draw_icon(canvas, 37, 53, &I_ButtonBACK_10x8);
    canvas_draw_str_aligned(canvas, 49, 54, AlignLeft, AlignTop, "Back");
    canvas_draw_icon(canvas, 73, 54, &I_ButtonOK_7x7);
    canvas_draw_str_aligned(canvas, 81, 54, AlignLeft, AlignTop, "Set");
    canvas_draw_icon(canvas, 100, 54, &I_ButtonRight_4x7);
    canvas_draw_str_aligned(canvas, 107, 54, AlignLeft, AlignTop, "Edit");
}

// Input callback for the view (async input handling)
static bool flip_wifi_view_input_callback_scan(InputEvent *event, void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (event->type == InputTypePress && event->key == InputKeyRight)
    {
        // switch to text input to set password
        flip_wifi_free_text_inputs(app);
        if (!flip_wifi_alloc_text_inputs(app, FlipWiFiViewTextInputScan))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
            return false;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        return true;
    }
    return false;
}
// Input callback for the view (async input handling)
static bool flip_wifi_view_input_callback_saved(InputEvent *event, void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    if (event->type == InputTypePress && event->key == InputKeyRight)
    {
        // set text input buffer as the selected password
        strncpy(app->uart_text_input_temp_buffer, wifi_playlist->passwords[ssid_index], app->uart_text_input_buffer_size);
        // switch to text input to set password
        flip_wifi_free_text_inputs(app);
        if (!flip_wifi_alloc_text_inputs(app, FlipWiFiViewTextInputSaved))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved");
            return false;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        return true;
    }
    else if (event->type == InputTypePress && event->key == InputKeyOk)
    {
        // save the settings
        save_settings(wifi_playlist->ssids[ssid_index], wifi_playlist->passwords[ssid_index]);

        flipper_http_save_wifi(wifi_playlist->ssids[ssid_index], wifi_playlist->passwords[ssid_index]);

        flipper_http_connect_wifi();

        easy_flipper_dialog("[SUCCESS]", "All FlipperHTTP apps will now\nuse the selected network.");
        return true;
    }
    else if (event->type == InputTypePress && event->key == InputKeyLeft)
    {
        // delete the selected ssid and password
        free(wifi_playlist->ssids[ssid_index]);
        free(wifi_playlist->passwords[ssid_index]);
        free(ssid_list[ssid_index]);
        // shift the remaining ssids and passwords
        for (uint32_t i = ssid_index; i < wifi_playlist->count - 1; i++)
        {
            // Use strncpy to prevent buffer overflows and ensure null termination
            strncpy(wifi_playlist->ssids[i], wifi_playlist->ssids[i + 1], MAX_SSID_LENGTH - 1);
            wifi_playlist->ssids[i][MAX_SSID_LENGTH - 1] = '\0'; // Ensure null-termination

            strncpy(wifi_playlist->passwords[i], wifi_playlist->passwords[i + 1], MAX_SSID_LENGTH - 1);
            wifi_playlist->passwords[i][MAX_SSID_LENGTH - 1] = '\0'; // Ensure null-termination

            // Shift ssid_list
            ssid_list[i] = ssid_list[i + 1];
        }
        wifi_playlist->count--;

        // save the playlist to storage
        save_playlist(wifi_playlist);

        // re draw the saved submenu
        flip_wifi_redraw_submenu_saved(app);
        // switch back to the saved view
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        return true;
    }
    return false;
}

// Function to trim leading and trailing whitespace
// Returns the trimmed start pointer and updates the length
static char *trim_whitespace(char *start, size_t *length)
{
    // Trim leading whitespace
    while (*length > 0 && isspace((unsigned char)*start))
    {
        start++;
        (*length)--;
    }

    // Trim trailing whitespace
    while (*length > 0 && isspace((unsigned char)start[*length - 1]))
    {
        (*length)--;
    }

    return start;
}

static bool flip_wifi_handle_scan(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return false;
    }
    // load the received data from the saved file
    FuriString *scan_data = flipper_http_load_from_file(fhttp.file_path);
    if (scan_data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        fhttp.state = ISSUE;
        easy_flipper_dialog("[ERROR]", "Failed to load received data from file.");
        return false;
    }
    char *data_cstr = (char *)furi_string_get_cstr(scan_data);
    if (data_cstr == NULL)
    {
        FURI_LOG_E(TAG, "Failed to get C-string from FuriString.");
        furi_string_free(scan_data);
        fhttp.state = ISSUE;
        free(data_cstr);
        easy_flipper_dialog("[ERROR]", "Failed to get C-string from FuriString.");
        return false;
    }

    uint32_t ssid_count = 0;

    char *current_position = data_cstr;
    char *next_comma = NULL;

    // Manually split the string on commas
    while ((next_comma = strchr(current_position, ',')) != NULL)
    {
        // Calculate length of the SSID
        size_t ssid_length = next_comma - current_position;

        // Trim leading and trailing whitespace
        size_t trimmed_length = ssid_length;
        char *trim_start = trim_whitespace(current_position, &trimmed_length);

        // Handle empty SSIDs resulting from consecutive commas
        if (trimmed_length == 0)
        {
            current_position = next_comma + 1; // Move past the comma
            continue;
        }

        // Allocate memory for the SSID and copy it
        ssid_list[ssid_count] = malloc(trimmed_length + 1);
        if (ssid_list[ssid_count] == NULL)
        {
            FURI_LOG_E(TAG, "Memory allocation failed");
            easy_flipper_dialog("[ERROR]", "Memory allocation failed");
            free(data_cstr);
            furi_string_free(scan_data);
            return false;
        }
        strncpy(ssid_list[ssid_count], trim_start, trimmed_length);
        ssid_list[ssid_count][trimmed_length] = '\0'; // Null-terminate the string

        ssid_count++;
        if (ssid_count >= MAX_SCAN_NETWORKS)
        {
            FURI_LOG_E(TAG, "Maximum SSID limit reached");
            break;
        }

        current_position = next_comma + 1; // Move past the comma
    }

    // Handle the last SSID after the last comma (if any)
    if (*current_position != '\0' && ssid_count < MAX_SCAN_NETWORKS)
    {
        size_t ssid_length = strlen(current_position);

        // Trim leading and trailing whitespace
        size_t trimmed_length = ssid_length;
        char *trim_start = trim_whitespace(current_position, &trimmed_length);

        // Handle empty SSIDs
        if (trimmed_length > 0)
        {
            ssid_list[ssid_count] = malloc(trimmed_length + 1);
            if (ssid_list[ssid_count] == NULL)
            {
                FURI_LOG_E(TAG, "Memory allocation failed for the last SSID");
                easy_flipper_dialog("[ERROR]", "Memory allocation failed for the last SSID");
                return false;
            }
            strncpy(ssid_list[ssid_count], trim_start, trimmed_length);
            ssid_list[ssid_count][trimmed_length] = '\0'; // Null-terminate the string
            ssid_count++;
        }
    }

    // Add each SSID as a submenu item
    submenu_reset(app->submenu_wifi);
    submenu_set_header(app->submenu_wifi, "WiFi Nearby");
    for (uint32_t i = 0; i < ssid_count; i++)
    {
        char *ssid_item = ssid_list[i];
        if (ssid_item == NULL)
        {
            // skip any NULL entries
            continue;
        }
        char ssid[64];
        snprintf(ssid, sizeof(ssid), "%s", ssid_item);
        submenu_add_item(app->submenu_wifi, ssid, FlipWiFiSubmenuIndexWiFiScanStart + i, callback_submenu_choices, app);
    }
    free(data_cstr);
    furi_string_free(scan_data);
    return true;
}
void callback_submenu_choices(void *context, uint32_t index)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }
    switch (index)
    {
    case FlipWiFiSubmenuIndexWiFiScan:
        flip_wifi_free_all(app);
        if (!flip_wifi_alloc_submenus(app, FlipWiFiViewSubmenuScan))
        {
            easy_flipper_dialog("[ERROR]", "Failed to allocate submenus for WiFi Scan");
            return;
        }
        // initialize uart
        if (!flipper_http_init(flipper_http_rx_callback, app))
        {
            easy_flipper_dialog("[ERROR]", "Failed to initialize flipper http");
            return;
        }
        bool _flip_wifi_handle_scan()
        {
            return flip_wifi_handle_scan(app);
        }
        // scan for wifi ad parse the results
        flipper_http_loading_task(flipper_http_scan_wifi, _flip_wifi_handle_scan, FlipWiFiViewSubmenu, FlipWiFiViewSubmenuMain, &app->view_dispatcher);
        flipper_http_deinit();
        break;
    case FlipWiFiSubmenuIndexWiFiSaved:
        flip_wifi_free_all(app);
        if (!flip_wifi_alloc_submenus(app, FlipWiFiViewSubmenuSaved))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenus for WiFi Saved");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        break;
    case FlipWiFiSubmenuIndexAbout:
        flip_wifi_free_all(app);
        if (!flip_wifi_alloc_widgets(app, FlipWiFiViewAbout))
        {
            FURI_LOG_E(TAG, "Failed to allocate widget for About");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewAbout);
        break;
    case FlipWiFiSubmenuIndexWiFiSavedAddSSID:
        flip_wifi_free_text_inputs(app);
        if (!flip_wifi_alloc_text_inputs(app, FlipWiFiViewTextInputSavedAddSSID))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        break;
    case FlipWiFiSubmenuIndexCommands:
        flip_wifi_free_all(app);
        if (!flip_wifi_alloc_submenus(app, FlipWiFiViewSubmenuCommands))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenus for Commands");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        break;
    case FlipWiFiSubmenuIndexFastCommandStart ... FlipWiFiSubmenuIndexFastCommandStart + 4:
        // initialize uart
        if (!flipper_http_init(flipper_http_rx_callback, app))
        {
            easy_flipper_dialog("[ERROR]", "Failed to initialize flipper http");
            return;
        }
        // Handle fast commands
        switch (index)
        {
        case FlipWiFiSubmenuIndexFastCommandStart + 0:
            // CUSTOM - send to text input and return
            flip_wifi_free_text_inputs(app);
            if (!flip_wifi_alloc_text_inputs(app, FlipWiFiSubmenuIndexFastCommandStart))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
            return;
        case FlipWiFiSubmenuIndexFastCommandStart + 1:
            // PING
            flipper_http_ping();
            break;
        case FlipWiFiSubmenuIndexFastCommandStart + 2:
            // LIST
            flipper_http_list_commands();
            break;
        case FlipWiFiSubmenuIndexFastCommandStart + 3:
            // IP/ADDRESS
            flipper_http_ip_address();
            break;
        case FlipWiFiSubmenuIndexFastCommandStart + 4:
            // WIFI/IP
            flipper_http_ip_wifi();
            break;
        default:
            break;
        }
        while (fhttp.last_response == NULL || strlen(fhttp.last_response) == 0)
        {
            // Wait for the response
            furi_delay_ms(100);
        }
        if (fhttp.last_response != NULL)
        {
            char response[100];
            snprintf(response, sizeof(response), "%s", fhttp.last_response);
            easy_flipper_dialog("", response);
        }
        flipper_http_deinit();
        break;
    case 100 ... 199:
        ssid_index = index - FlipWiFiSubmenuIndexWiFiScanStart;
        flip_wifi_free_views(app);
        if (!flip_wifi_alloc_views(app, FlipWiFiViewWiFiScan))
        {
            FURI_LOG_E(TAG, "Failed to allocate views for WiFi Scan");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewGeneric);
        break;
    case 200 ... 299:
        ssid_index = index - FlipWiFiSubmenuIndexWiFiSavedStart;
        flip_wifi_free_views(app);
        snprintf(current_ssid, sizeof(current_ssid), "%s", wifi_playlist->ssids[ssid_index]);
        snprintf(current_password, sizeof(current_password), "%s", wifi_playlist->passwords[ssid_index]);
        if (!flip_wifi_alloc_views(app, FlipWiFiViewWiFiSaved))
        {
            FURI_LOG_E(TAG, "Failed to allocate views for WiFi Saved");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewGeneric);
        break;
    default:
        break;
    }
}

void flip_wifi_text_updated_password_scan(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }

    // Store the entered text with buffer size limit
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size - 1);
    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    if (!flip_wifi_alloc_playlist(app))
    {
        FURI_LOG_E(TAG, "Failed to allocate playlist");
        return;
    }

    // Ensure ssid_index is valid
    if (ssid_index >= MAX_SCAN_NETWORKS)
    {
        FURI_LOG_E(TAG, "Invalid ssid_index: %ld", ssid_index);
        return;
    }

    // Check if there's space in the playlist
    if (wifi_playlist->count >= MAX_SAVED_NETWORKS)
    {
        FURI_LOG_E(TAG, "Playlist is full. Cannot add more entries.");
        return;
    }
    FURI_LOG_I(TAG, "Adding SSID: %s", ssid_list[ssid_index]);
    FURI_LOG_I(TAG, "Count: %d", wifi_playlist->count);
    // Add the SSID and password to the playlist
    snprintf(wifi_playlist->ssids[wifi_playlist->count], MAX_SSID_LENGTH, "%s", ssid_list[ssid_index]);
    snprintf(wifi_playlist->passwords[wifi_playlist->count], MAX_SSID_LENGTH, "%s", app->uart_text_input_buffer);
    wifi_playlist->count++;

    // Save the updated playlist to storage
    save_playlist(wifi_playlist);

    // Redraw the submenu to reflect changes
    flip_wifi_redraw_submenu_saved(app);

    // Switch back to the scan view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}

void flip_wifi_text_updated_password_saved(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);

    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    // update the password_saved in the playlist
    snprintf(wifi_playlist->passwords[ssid_index], MAX_SSID_LENGTH, app->uart_text_input_buffer);

    // save the playlist to storage
    save_playlist(wifi_playlist);

    // switch to back to the saved view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}

void flip_wifi_text_updated_add_ssid(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }

    // check if empty
    if (strlen(app->uart_text_input_temp_buffer) == 0)
    {
        easy_flipper_dialog("[ERROR]", "SSID cannot be empty");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);

    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';
    save_char("wifi-ssid", app->uart_text_input_buffer);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
    uart_text_input_reset(app->uart_text_input);
    uart_text_input_set_header_text(app->uart_text_input, "Enter Password");
    app->uart_text_input_buffer_size = MAX_SSID_LENGTH;
    free(app->uart_text_input_buffer);
    free(app->uart_text_input_temp_buffer);
    easy_flipper_set_buffer(&app->uart_text_input_buffer, app->uart_text_input_buffer_size);
    easy_flipper_set_buffer(&app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);
    uart_text_input_set_result_callback(app->uart_text_input, flip_wifi_text_updated_add_password, app, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
}
void flip_wifi_text_updated_add_password(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }

    // check if empty
    if (strlen(app->uart_text_input_temp_buffer) == 0)
    {
        easy_flipper_dialog("[ERROR]", "Password cannot be empty");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);

    save_char("wifi-password", app->uart_text_input_buffer);

    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    char wifi_ssid[64];
    if (!load_char("wifi-ssid", wifi_ssid, sizeof(wifi_ssid)))
    {
        FURI_LOG_E(TAG, "Failed to load wifi ssid");
        return;
    }

    // add the SSID and password_scan to the playlist
    snprintf(wifi_playlist->ssids[wifi_playlist->count], MAX_SSID_LENGTH, wifi_ssid);
    snprintf(wifi_playlist->passwords[wifi_playlist->count], MAX_SSID_LENGTH, app->uart_text_input_buffer);
    wifi_playlist->count++;

    // save the playlist to storage
    save_playlist(wifi_playlist);

    flip_wifi_redraw_submenu_saved(app);

    // switch to back to the saved view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}