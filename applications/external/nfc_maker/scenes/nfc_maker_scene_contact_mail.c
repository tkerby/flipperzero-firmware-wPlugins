#include "../nfc_maker.h"

enum TextInputResult {
    TextInputResultOk,
};

static void nfc_maker_scene_contact_mail_text_input_callback(void* context) {
    NfcMaker* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, TextInputResultOk);
}

void nfc_maker_scene_contact_mail_on_enter(void* context) {
    NfcMaker* app = context;
    NFCMaker_TextInput* text_input = app->text_input;

    nfc_maker_text_input_set_header_text(text_input, "Enter Email Address:");

    strlcpy(app->mail_buf, "johnsmith@email.com", sizeof(app->mail_buf));

    nfc_maker_text_input_set_result_callback(
        text_input,
        nfc_maker_scene_contact_mail_text_input_callback,
        app,
        app->mail_buf,
        sizeof(app->mail_buf),
        true);

    nfc_maker_text_input_set_minimum_length(text_input, 0);

    nfc_maker_text_input_show_illegal_symbols(text_input, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcMakerViewTextInput);
}

bool nfc_maker_scene_contact_mail_on_event(void* context, SceneManagerEvent event) {
    NfcMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case TextInputResultOk:
            scene_manager_next_scene(app->scene_manager, NfcMakerSceneContactPhone);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void nfc_maker_scene_contact_mail_on_exit(void* context) {
    NfcMaker* app = context;
    nfc_maker_text_input_reset(app->text_input);
}
