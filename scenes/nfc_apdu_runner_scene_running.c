#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 运行场景弹出窗口回调
static void nfc_apdu_runner_scene_running_popup_callback(void* context) {
    NfcApduRunner* app = context;
    FURI_LOG_I("APDU_DEBUG", "RUNNING-1: 弹出窗口回调");
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcApduRunnerCustomEventPopupClosed);
}

// 显示错误消息并设置回调
static void show_error_popup(NfcApduRunner* app, const char* message) {
    FURI_LOG_I("APDU_DEBUG", "RUNNING-2: 显示错误弹窗: %s", message);
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_text(popup, message, 89, 44, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 2000);
    popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
}

// 运行场景进入回调
void nfc_apdu_runner_scene_running_on_enter(void* context) {
    NfcApduRunner* app = context;
    Popup* popup = app->popup;
    FURI_LOG_I("APDU_DEBUG", "RUNNING-3: 进入运行场景");

    // 确保文件路径有效
    if(furi_string_empty(app->file_path)) {
        FURI_LOG_E("APDU_DEBUG", "RUNNING-4: 文件路径为空");
        show_error_popup(app, "Invalid file path");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "RUNNING-5: 文件路径: %s", furi_string_get_cstr(app->file_path));

    // 显示正在运行的弹出窗口
    FURI_LOG_I("APDU_DEBUG", "RUNNING-6: 显示加载脚本弹窗");
    popup_reset(popup);
    popup_set_text(popup, "Loading script...\nPlease wait", 89, 44, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 0);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);

    // 解析脚本文件
    FURI_LOG_I("APDU_DEBUG", "RUNNING-7: 开始解析脚本文件");
    app->script = nfc_apdu_script_parse(app->storage, furi_string_get_cstr(app->file_path));

    if(app->script == NULL) {
        FURI_LOG_E("APDU_DEBUG", "RUNNING-8: 解析脚本文件失败");
        show_error_popup(app, "Failed to parse script file");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "RUNNING-9: 脚本解析成功, 卡类型: %d", app->script->card_type);

    // 检查卡类型是否支持
    if(app->script->card_type == CardTypeUnknown) {
        FURI_LOG_E("APDU_DEBUG", "RUNNING-10: 不支持的卡类型");
        show_error_popup(app, "Unsupported card type");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "RUNNING-11: 卡类型支持");

    // 显示正在运行的弹出窗口
    FURI_LOG_I("APDU_DEBUG", "RUNNING-12: 显示运行APDU命令弹窗");
    popup_reset(popup);
    popup_set_text(
        popup, "Running APDU commands...\nPlease wait", 89, 44, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 0);
    popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);

    // 重置卡片丢失标志
    app->card_lost = false;
    // 设置运行标志为true，允许执行APDU命令
    app->running = true;
    FURI_LOG_I("APDU_DEBUG", "RUNNING-13: 重置卡片丢失标志，设置运行标志");

    // 执行APDU命令
    app->responses = NULL;
    app->response_count = 0;
    FURI_LOG_I("APDU_DEBUG", "RUNNING-14: 开始执行APDU命令");

    bool success = nfc_apdu_run_commands(app, app->script, &app->responses, &app->response_count);
    FURI_LOG_I("APDU_DEBUG", "RUNNING-15: APDU命令执行结果: %s", success ? "成功" : "失败");

    if(!app->running) {
        FURI_LOG_I("APDU_DEBUG", "RUNNING-15.1: 操作被用户取消");
        show_error_popup(app, "Operation cancelled by user");
        return;
    }

    if(app->card_lost) {
        FURI_LOG_E("APDU_DEBUG", "RUNNING-16: 卡片丢失");
        show_error_popup(app, "Card was removed\nCheck the log for details");
        return;
    }

    if(!success || app->response_count == 0) {
        FURI_LOG_E("APDU_DEBUG", "RUNNING-17: 执行APDU命令失败");
        show_error_popup(app, "Failed to run APDU commands\nCheck the log for details");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "RUNNING-18: 响应数量: %lu", app->response_count);

    // 保存响应结果
    FURI_LOG_I("APDU_DEBUG", "RUNNING-19: 开始保存响应结果");
    success = nfc_apdu_save_responses(
        app->storage, furi_string_get_cstr(app->file_path), app->responses, app->response_count);

    if(!success) {
        FURI_LOG_E("APDU_DEBUG", "RUNNING-20: 保存响应结果失败");
        show_error_popup(app, "Failed to save responses");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "RUNNING-21: 响应结果保存成功");

    // 切换到结果场景
    FURI_LOG_I("APDU_DEBUG", "RUNNING-22: 切换到结果场景");
    scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneResults);
}

// 运行场景事件回调
bool nfc_apdu_runner_scene_running_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;
    FURI_LOG_I("APDU_DEBUG", "RUNNING-24: 事件回调, 事件类型: %d", event.type);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcApduRunnerCustomEventPopupClosed) {
            FURI_LOG_I("APDU_DEBUG", "RUNNING-25: 弹出窗口关闭事件");
            // 如果是错误弹窗关闭，返回到文件选择场景
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == NfcApduRunnerCustomEventViewExit) {
            FURI_LOG_I("APDU_DEBUG", "RUNNING-26: 视图退出事件");
            // 用户请求退出，设置运行标志为false
            app->running = false;
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_I("APDU_DEBUG", "RUNNING-27: 返回事件");
        // 用户按下返回键，设置运行标志为false并返回到上一个场景
        app->running = false;
        consumed = false; // 不消费事件，让场景管理器处理返回
    }

    return consumed;
}

// 运行场景退出回调
void nfc_apdu_runner_scene_running_on_exit(void* context) {
    NfcApduRunner* app = context;
    FURI_LOG_I("APDU_DEBUG", "RUNNING-23: 退出运行场景");

    // 设置运行标志为false，停止任何正在进行的操作
    app->running = false;

    // 释放脚本资源
    if(app->script) {
        nfc_apdu_script_free(app->script);
        app->script = NULL;
    }

    popup_reset(app->popup);
}
