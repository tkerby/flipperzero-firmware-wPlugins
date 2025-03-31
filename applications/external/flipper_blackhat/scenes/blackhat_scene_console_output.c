#include "../blackhat_app_i.h"

void blackhat_console_output_handle_rx_data_cb(
    uint8_t* buf, size_t len, void* context
)
{
    furi_assert(context);
    BlackhatApp* app = context;

    // If text box store gets too big, then truncate it
    app->text_box_store_strlen += len;
    if (app->text_box_store_strlen >= BLACKHAT_TEXT_BOX_STORE_SIZE - 1) {
        furi_string_right(app->text_box_store, app->text_box_store_strlen / 2);
        app->text_box_store_strlen =
            furi_string_size(app->text_box_store) + len;
    }

    // Null-terminate buf and append to text box store
    buf[len] = '\0';
    furi_string_cat_printf(app->text_box_store, "%s", buf);

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

void blackhat_scene_console_output_on_enter(void* context)
{
    BlackhatApp* app = context;

    TextBox* text_box = app->text_box;
    text_box_reset(app->text_box);
    text_box_set_font(text_box, TextBoxFontText);

    text_box_set_focus(text_box, TextBoxFocusEnd);

    furi_string_reset(app->text_box_store);
    app->text_box_store_strlen = 0;

    if (app->text_input_req) {
        scene_manager_next_scene(app->scene_manager, BlackhatSceneRename);
        return;
    }

    snprintf(
        app->text_store,
        sizeof(app->text_store),
        "%s %s\n",
        app->selected_tx_string,
        app->selected_option_item_text
    );

    FURI_LOG_I("tag/app name", "%s", app->text_store);

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    scene_manager_set_scene_state(
        app->scene_manager, BlackhatSceneConsoleOutput, 0
    );
    view_dispatcher_switch_to_view(
        app->view_dispatcher, BlackhatAppViewConsoleOutput
    );

    // Register callback to receive data
    blackhat_uart_set_handle_rx_data_cb(
        app->uart, blackhat_console_output_handle_rx_data_cb
    );

    blackhat_uart_tx(app->uart, app->text_store, strlen(app->text_store));
}

bool blackhat_scene_console_output_on_event(
    void* context, SceneManagerEvent event
)
{
    UNUSED(context);

    bool consumed = false;

    if (event.type == SceneManagerEventTypeTick) {
        consumed = true;
    }

    return consumed;
}

void blackhat_scene_console_output_on_exit(void* context)
{
    BlackhatApp* app = context;

    // Unregister rx callback
    blackhat_uart_set_handle_rx_data_cb(app->uart, NULL);
}
