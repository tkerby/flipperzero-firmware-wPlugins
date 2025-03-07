#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 结果场景进入回调
void nfc_apdu_runner_scene_results_on_enter(void* context) {
    NfcApduRunner* app = context;
    TextBox* text_box = app->text_box;

    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);

    FuriString* text = furi_string_alloc();

    furi_string_cat_str(text, "APDU Responses:\n\n");

    for(uint32_t i = 0; i < app->response_count; i++) {
        furi_string_cat_printf(text, "Command: %s\n", app->responses[i].command);
        furi_string_cat_str(text, "Response: ");

        for(uint16_t j = 0; j < app->responses[i].response_length; j++) {
            furi_string_cat_printf(text, "%02X", app->responses[i].response[j]);
        }

        furi_string_cat_str(text, "\n\n");
    }

    text_box_set_text(text_box, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewTextBox);
}

// 结果场景事件回调
bool nfc_apdu_runner_scene_results_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// 结果场景退出回调
void nfc_apdu_runner_scene_results_on_exit(void* context) {
    NfcApduRunner* app = context;
    text_box_reset(app->text_box);

    // 释放响应资源
    if(app->responses != NULL) {
        nfc_apdu_responses_free(app->responses, app->response_count);
        app->responses = NULL;
        app->response_count = 0;
    }
}
