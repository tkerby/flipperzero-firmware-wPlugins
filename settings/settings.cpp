#include "settings.hpp"
#include "app.hpp"

FlipDownloaderSettings::FlipDownloaderSettings()
{
    // nothing to do
}

FlipDownloaderSettings::~FlipDownloaderSettings()
{
    free();
}

void FlipDownloaderSettings::settings_item_selected_callback(void *context, uint32_t index)
{
    FlipDownloaderSettings *settings = (FlipDownloaderSettings *)context;
    settings->settingsItemSelected(index);
}

uint32_t FlipDownloaderSettings::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipDownloaderViewSubmenu;
}

uint32_t FlipDownloaderSettings::callback_to_settings(void *context)
{
    UNUSED(context);
    return FlipDownloaderViewSettings;
}

bool FlipDownloaderSettings::init(ViewDispatcher **view_dispatcher, void *appContext)
{
    view_dispatcher_ref = view_dispatcher;
    this->appContext = appContext;

    if (!easy_flipper_set_variable_item_list(&variable_item_list, FlipDownloaderViewSettings,
                                             settings_item_selected_callback, callbackToSubmenu, view_dispatcher, this))
    {
        return false;
    }

    variable_item_wifi_ssid = variable_item_list_add(variable_item_list, "WiFi SSID", 1, nullptr, nullptr);
    variable_item_wifi_pass = variable_item_list_add(variable_item_list, "WiFi Password", 1, nullptr, nullptr);

    variable_item_set_current_value_text(variable_item_wifi_ssid, "");
    variable_item_set_current_value_text(variable_item_wifi_pass, "");

    return true;
}

void FlipDownloaderSettings::free()
{
    // Free text input first
    free_text_input();

    if (variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipDownloaderViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_wifi_ssid = nullptr;
        variable_item_wifi_pass = nullptr;
    }
}

// Text input callback wrappers
void FlipDownloaderSettings::text_updated_ssid_callback(void *context)
{
    FlipDownloaderSettings *settings = (FlipDownloaderSettings *)context;
    settings->text_updated(TextInputWiFiSSID);
}

void FlipDownloaderSettings::text_updated_pass_callback(void *context)
{
    FlipDownloaderSettings *settings = (FlipDownloaderSettings *)context;
    settings->text_updated(TextInputWiFiPassword);
}

void FlipDownloaderSettings::text_updated(uint32_t view)
{
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);

    switch (view)
    {
    case TextInputWiFiSSID:
        if (variable_item_wifi_ssid)
        {
            variable_item_set_current_value_text(variable_item_wifi_ssid, text_input_buffer.get());
        }
        app->save_char("wifi_ssid", text_input_buffer.get());
        break;
    case TextInputWiFiPassword:
        if (variable_item_wifi_pass)
        {
            variable_item_set_current_value_text(variable_item_wifi_pass, text_input_buffer.get());
        }
        app->save_char("wifi_pass", text_input_buffer.get());
        break;
    default:
        break;
    }

    // switch to the settings view
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipDownloaderViewSettings);
    }
}

bool FlipDownloaderSettings::init_text_input(uint32_t view)
{
    // check if already initialized
    if (text_input_buffer || text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "init_text_input: already initialized");
        return false;
    }

    // init buffers
    text_input_buffer_size = 128;
    if (!easy_flipper_set_buffer(reinterpret_cast<char **>(&text_input_buffer), text_input_buffer_size))
    {
        return false;
    }
    if (!easy_flipper_set_buffer(reinterpret_cast<char **>(&text_input_temp_buffer), text_input_buffer_size))
    {
        return false;
    }

    // app context
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    char loaded[256];

    if (view == TextInputWiFiSSID)
    {
        if (app->load_char("wifi_ssid", loaded, sizeof(loaded)))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
        return easy_flipper_set_text_input(&text_input, FlipDownloaderViewTextInput,
                                           "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                           text_updated_ssid_callback, callback_to_settings, view_dispatcher_ref, this);
    }
    else if (view == TextInputWiFiPassword)
    {
        if (app->load_char("wifi_pass", loaded, sizeof(loaded)))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
        return easy_flipper_set_text_input(&text_input, FlipDownloaderViewTextInput,
                                           "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           text_updated_pass_callback, callback_to_settings, view_dispatcher_ref, this);
    }
    return false;
}

void FlipDownloaderSettings::free_text_input()
{
    if (text_input && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipDownloaderViewTextInput);
        text_input_free(text_input);
        text_input = nullptr;
    }
    text_input_buffer.reset();
    text_input_temp_buffer.reset();
}

bool FlipDownloaderSettings::start_text_input(uint32_t view)
{
    free_text_input();
    if (!init_text_input(view))
    {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipDownloaderViewTextInput);
        return true;
    }
    else
    {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

void FlipDownloaderSettings::settingsItemSelected(uint32_t index)
{
    start_text_input(index);
}
