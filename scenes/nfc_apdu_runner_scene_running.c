#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 运行场景弹出窗口回调
static void nfc_apdu_runner_scene_running_popup_callback(void* context) {
    NfcApduRunner* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcApduRunnerCustomEventPopupClosed);
}

// 运行场景进入回调
void nfc_apdu_runner_scene_running_on_enter(void* context) {
    NfcApduRunner* app = context;
    Popup* popup = app->popup;

    // 解析脚本文件
    app->script = nfc_apdu_script_parse(app->storage, furi_string_get_cstr(app->file_path));

    if(app->script == NULL) {
        popup_reset(popup);
        popup_set_text(popup, "Failed to parse script file", 89, 44, AlignCenter, AlignCenter);
        popup_set_timeout(popup, 2000);
        popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
        popup_set_context(popup, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        return;
    }

    // 检查卡类型是否支持
    if(app->script->card_type == CardTypeUnknown) {
        popup_reset(popup);
        popup_set_text(popup, "Unsupported card type", 89, 44, AlignCenter, AlignCenter);
        popup_set_timeout(popup, 2000);
        popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
        popup_set_context(popup, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        return;
    }

    // 显示正在运行的弹出窗口
    popup_reset(popup);
    popup_set_text(
        popup, "Running APDU commands...\nPlease wait", 89, 44, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 0);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);

    // 重置卡片丢失标志
    app->card_lost = false;

    // 执行APDU命令
    app->responses = NULL;
    app->response_count = 0;

    bool success = nfc_apdu_run_commands(app, app->script, &app->responses, &app->response_count);

    if(app->card_lost) {
        popup_reset(popup);
        popup_set_text(
            popup, "Card was removed\nCheck the log for details", 89, 44, AlignCenter, AlignCenter);
        popup_set_timeout(popup, 2000);
        popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
        popup_set_context(popup, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        return;
    }

    if(!success || app->response_count == 0) {
        popup_reset(popup);
        popup_set_text(
            popup,
            "Failed to run APDU commands\nCheck the log for details",
            89,
            44,
            AlignCenter,
            AlignCenter);
        popup_set_timeout(popup, 2000);
        popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
        popup_set_context(popup, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        return;
    }

    // 保存响应结果
    success = nfc_apdu_save_responses(
        app->storage, furi_string_get_cstr(app->file_path), app->responses, app->response_count);

    if(!success) {
        popup_reset(popup);
        popup_set_text(popup, "Failed to save responses", 89, 44, AlignCenter, AlignCenter);
        popup_set_timeout(popup, 2000);
        popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
        popup_set_context(popup, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        return;
    }

    // 切换到结果场景
    scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneResults);
}

// 运行场景事件回调
bool nfc_apdu_runner_scene_running_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcApduRunnerCustomEventPopupClosed) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

// 运行场景退出回调
void nfc_apdu_runner_scene_running_on_exit(void* context) {
    NfcApduRunner* app = context;
    popup_reset(app->popup);

    // 释放脚本资源
    if(app->script != NULL) {
        nfc_apdu_script_free(app->script);
        app->script = NULL;
    }
}
