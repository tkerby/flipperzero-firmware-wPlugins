#include "cligui_main_i.h"
#include "cli_control.h"
#include "text_input.h"
#include "console_output.h"
#include <dialogs/dialogs.h>

static bool cligui_custom_event_cb(void* context, uint32_t event) {
    UNUSED(event);
    CliguiApp* app = context;
    UNUSED(app);
    return true;
}
static bool cligui_back_event_cb(void* context) {
    CliguiApp* app = context;
    UNUSED(app);
    return true;
}
static void cligui_tick_event_cb(void* context) {
    CliguiApp* app = context;
    size_t available = clicontrol_bytes_available();
    for(size_t i = 0; i < available; i++) {
        char c = 0;
        size_t len = clicontrol_receive(&c, 1, 100);
        if(len > 0) {
            furi_string_push_back(app->text_box_store, c);
        }
    }
    if(available > 0) {
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    }
    // Set input header stuff
    size_t len = furi_string_size(app->text_box_store);
    // A newly started pipe shell may not have produced its prompt yet.
    size_t idx = len >= 2 ? len - 2 : 0;
    while(idx > 0) {
        if(furi_string_get_char(app->text_box_store, idx) == '\n') {
            idx++;
            break;
        }
        idx--;
    }
    text_input_set_header_text(app->text_input, furi_string_get_cstr(app->text_box_store) + idx);
    UNUSED(app);
}

volatile bool persistent_exit = false;
static void input_callback(const void* event_ptr, void* context) {
    InputEvent* event = (InputEvent*)event_ptr;
    CliguiApp* app = context;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        persistent_exit = false;
        view_dispatcher_stop(app->view_dispatcher);
    }
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->data->state == ViewConsoleOutput) {
            persistent_exit = true;
            view_dispatcher_stop(app->view_dispatcher);
        }
    }
    if(app->data->state == ViewTextInput) {
        text_input_input_handler(app, event);
    } else {
        console_output_input_handler(app, event);
    }
}

int32_t cligui_main(void* p) {
    UNUSED(p);
    if(!clicontrol_hijack(512, 512)) {
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Firmware required", 64, 3, AlignCenter, AlignTop);
        dialog_message_set_text(
            message, "Update RM firmware for\nCLI-GUI Bridge.", 64, 32, AlignCenter, AlignCenter);
        dialog_message_set_buttons(message, NULL, "OK", NULL);
        dialog_message_show(dialogs, message);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);
        return 0;
    }

    CliguiApp* cligui = malloc(sizeof(CliguiApp));
    cligui->data = malloc(sizeof(CliguiData));

    cligui->gui = furi_record_open(RECORD_GUI);
    cligui->view_dispatcher = view_dispatcher_alloc();
    FuriPubSub* input_events = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_events_sub =
        furi_pubsub_subscribe(input_events, input_callback, (void*)cligui);
    view_dispatcher_enable_queue(cligui->view_dispatcher);
    view_dispatcher_set_event_callback_context(cligui->view_dispatcher, cligui);
    view_dispatcher_set_custom_event_callback(cligui->view_dispatcher, cligui_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(cligui->view_dispatcher, cligui_back_event_cb);
    view_dispatcher_set_tick_event_callback(cligui->view_dispatcher, cligui_tick_event_cb, 100);

    view_dispatcher_attach_to_gui(
        cligui->view_dispatcher, cligui->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_send_to_front(cligui->view_dispatcher);

    cligui->text_box = text_box_alloc();
    view_dispatcher_add_view(
        cligui->view_dispatcher, ViewConsoleOutput, text_box_get_view(cligui->text_box));
    cligui->text_box_store = furi_string_alloc();
    furi_string_reserve(cligui->text_box_store, TEXT_BOX_STORE_SIZE);
    furi_string_reset(cligui->text_box_store);
    text_box_set_text(cligui->text_box, furi_string_get_cstr(cligui->text_box_store));
    text_box_set_focus(cligui->text_box, TextBoxFocusEnd);

    cligui->text_input = text_input_alloc();
    text_input_set_result_callback(
        cligui->text_input,
        text_input_result_callback,
        cligui,
        cligui->text_input_store,
        TEXT_INPUT_STORE_SIZE,
        true);
    view_dispatcher_add_view(
        cligui->view_dispatcher, ViewTextInput, text_input_get_view(cligui->text_input));

    view_dispatcher_switch_to_view(cligui->view_dispatcher, ViewTextInput);
    cligui->data->state = ViewTextInput;

    view_dispatcher_run(cligui->view_dispatcher);

    // Stop input callbacks before releasing their views and the shared session handle.
    furi_pubsub_unsubscribe(input_events, input_events_sub);

    view_dispatcher_remove_view(cligui->view_dispatcher, ViewConsoleOutput);
    view_dispatcher_remove_view(cligui->view_dispatcher, ViewTextInput);
    text_box_free(cligui->text_box);
    furi_string_free(cligui->text_box_store);
    text_input_free(cligui->text_input);
    view_dispatcher_free(cligui->view_dispatcher);

    clicontrol_unhijack(persistent_exit);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);

    free(cligui->data);
    free(cligui);

    return 0;
}
