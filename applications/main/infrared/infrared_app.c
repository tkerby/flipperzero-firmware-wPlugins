#include "infrared_app_i.h"

#include "infrared_settings.h"

#include <furi_hal_power.h>

#include <string.h>
#include <toolbox/path.h>
#include <toolbox/saved_struct.h>
#include <dolphin/dolphin.h>

#define TAG "InfraredApp"

#define INFRARED_TX_MIN_INTERVAL_MS (50U)
#define INFRARED_TASK_STACK_SIZE    (2048UL)

static const NotificationSequence*
    infrared_notification_sequences[InfraredNotificationMessageCount] = {
        &sequence_success,
        &sequence_set_only_green_255,
        &sequence_reset_green,
        &sequence_solid_yellow,
        &sequence_reset_rgb,
        &sequence_blink_start_cyan,
        &sequence_blink_start_magenta,
        &sequence_blink_stop,
};

static void infrared_make_app_folder(InfraredApp* infrared) {
    if(!storage_simply_mkdir(infrared->storage, INFRARED_APP_FOLDER)) {
        infrared_show_error_message(infrared, "Cannot create\napp folder");
    }
}

static bool infrared_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    InfraredApp* infrared = context;
    return scene_manager_handle_custom_event(infrared->scene_manager, event);
}

static bool infrared_back_event_callback(void* context) {
    furi_assert(context);
    InfraredApp* infrared = context;
    return scene_manager_handle_back_event(infrared->scene_manager);
}

static void infrared_tick_event_callback(void* context) {
    furi_assert(context);
    InfraredApp* infrared = context;
    scene_manager_handle_tick_event(infrared->scene_manager);
}

static void infrared_rpc_command_callback(const RpcAppSystemEvent* event, void* context) {
    furi_assert(context);
    InfraredApp* infrared = context;
    furi_assert(infrared->rpc_ctx);

    if(event->type == RpcAppEventTypeSessionClose) {
        view_dispatcher_send_custom_event(
            infrared->view_dispatcher, InfraredCustomEventTypeRpcSessionClose);
        rpc_system_app_set_callback(infrared->rpc_ctx, NULL, NULL);
        infrared->rpc_ctx = NULL;
    } else if(event->type == RpcAppEventTypeAppExit) {
        view_dispatcher_send_custom_event(
            infrared->view_dispatcher, InfraredCustomEventTypeRpcExit);
    } else if(event->type == RpcAppEventTypeLoadFile) {
        furi_assert(event->data.type == RpcAppSystemEventDataTypeString);
        furi_string_set(infrared->file_path, event->data.string);
        view_dispatcher_send_custom_event(
            infrared->view_dispatcher, InfraredCustomEventTypeRpcLoadFile);
    } else if(event->type == RpcAppEventTypeButtonPress) {
        furi_assert(
            event->data.type == RpcAppSystemEventDataTypeString ||
            event->data.type == RpcAppSystemEventDataTypeInt32);
        if(event->data.type == RpcAppSystemEventDataTypeString) {
            furi_string_set(infrared->button_name, event->data.string);
            view_dispatcher_send_custom_event(
                infrared->view_dispatcher, InfraredCustomEventTypeRpcButtonPressName);
        } else {
            infrared->app_state.current_button_index = event->data.i32;
            view_dispatcher_send_custom_event(
                infrared->view_dispatcher, InfraredCustomEventTypeRpcButtonPressIndex);
        }
    } else if(event->type == RpcAppEventTypeButtonPressRelease) {
        furi_assert(
            event->data.type == RpcAppSystemEventDataTypeString ||
            event->data.type == RpcAppSystemEventDataTypeInt32);
        if(event->data.type == RpcAppSystemEventDataTypeString) {
            furi_string_set(infrared->button_name, event->data.string);
            view_dispatcher_send_custom_event(
                infrared->view_dispatcher, InfraredCustomEventTypeRpcButtonPressReleaseName);
        } else {
            infrared->app_state.current_button_index = event->data.i32;
            view_dispatcher_send_custom_event(
                infrared->view_dispatcher, InfraredCustomEventTypeRpcButtonPressReleaseIndex);
        }
    } else if(event->type == RpcAppEventTypeButtonRelease) {
        view_dispatcher_send_custom_event(
            infrared->view_dispatcher, InfraredCustomEventTypeRpcButtonRelease);
    } else {
        rpc_system_app_confirm(infrared->rpc_ctx, false);
    }
}

static void infrared_find_vacant_remote_name(FuriString* name, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FuriString* base_path;
    base_path = furi_string_alloc_set(path);

    if(furi_string_end_with(base_path, INFRARED_APP_EXTENSION)) {
        size_t filename_start = furi_string_search_rchar(base_path, '/');
        furi_string_left(base_path, filename_start);
    }

    furi_string_printf(
        base_path, "%s/%s%s", path, furi_string_get_cstr(name), INFRARED_APP_EXTENSION);

    FS_Error status = storage_common_stat(storage, furi_string_get_cstr(base_path), NULL);

    if(status == FSE_OK) {
        /* If the suggested name is occupied, try another one (name2, name3, etc) */
        size_t dot = furi_string_search_rchar(base_path, '.');
        furi_string_left(base_path, dot);

        FuriString* path_temp;
        path_temp = furi_string_alloc();

        uint32_t i = 1;
        do {
            furi_string_printf(
                path_temp, "%s%lu%s", furi_string_get_cstr(base_path), ++i, INFRARED_APP_EXTENSION);
            status = storage_common_stat(storage, furi_string_get_cstr(path_temp), NULL);
        } while(status == FSE_OK);

        furi_string_free(path_temp);

        if(status == FSE_NOT_EXIST) {
            furi_string_cat_printf(name, "%lu", i);
        }
    }

    furi_string_free(base_path);
    furi_record_close(RECORD_STORAGE);
}

static InfraredApp* infrared_alloc(void) {
    InfraredApp* infrared = malloc(sizeof(InfraredApp));

    infrared->task_thread =
        furi_thread_alloc_ex("InfraredTask", INFRARED_TASK_STACK_SIZE, NULL, infrared);
    infrared->file_path = furi_string_alloc();
    infrared->button_name = furi_string_alloc();

    InfraredAppState* app_state = &infrared->app_state;
    app_state->is_learning_new_remote = false;
    app_state->is_debug_enabled = furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug);
    app_state->is_transmitting = false;
    app_state->is_otg_enabled = false;
    app_state->is_easy_mode = false;
    app_state->is_decode_enabled = true;
    app_state->edit_target = InfraredEditTargetNone;
    app_state->edit_mode = InfraredEditModeNone;
    app_state->current_button_index = InfraredButtonIndexNone;

    infrared->scene_manager = scene_manager_alloc(&infrared_scene_handlers, infrared);
    infrared->view_dispatcher = view_dispatcher_alloc();

    infrared->gui = furi_record_open(RECORD_GUI);

    ViewDispatcher* view_dispatcher = infrared->view_dispatcher;
    view_dispatcher_enable_queue(view_dispatcher);
    view_dispatcher_set_event_callback_context(view_dispatcher, infrared);
    view_dispatcher_set_custom_event_callback(view_dispatcher, infrared_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(view_dispatcher, infrared_back_event_callback);
    view_dispatcher_set_tick_event_callback(view_dispatcher, infrared_tick_event_callback, 100);

    infrared->storage = furi_record_open(RECORD_STORAGE);
    infrared->dialogs = furi_record_open(RECORD_DIALOGS);
    infrared->notifications = furi_record_open(RECORD_NOTIFICATION);

    infrared->worker = infrared_worker_alloc();
    infrared->remote = infrared_remote_alloc();
    infrared->current_signal = infrared_signal_alloc();
    infrared->brute_force = infrared_brute_force_alloc();

    infrared->submenu = submenu_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewSubmenu, submenu_get_view(infrared->submenu));

    infrared->text_input = text_input_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewTextInput, text_input_get_view(infrared->text_input));

    infrared->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewDialogEx, dialog_ex_get_view(infrared->dialog_ex));

    infrared->button_menu = button_menu_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewButtonMenu, button_menu_get_view(infrared->button_menu));

    infrared->popup = popup_alloc();
    view_dispatcher_add_view(view_dispatcher, InfraredViewPopup, popup_get_view(infrared->popup));

    infrared->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        view_dispatcher,
        InfraredViewVariableList,
        variable_item_list_get_view(infrared->var_item_list));

    infrared->view_stack = view_stack_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewStack, view_stack_get_view(infrared->view_stack));

    infrared->move_view = infrared_move_view_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewMove, infrared_move_view_get_view(infrared->move_view));

    infrared->loading = loading_alloc();
    view_dispatcher_add_view(
        view_dispatcher, InfraredViewLoading, loading_get_view(infrared->loading));

    if(app_state->is_debug_enabled) {
        infrared->debug_view = infrared_debug_view_alloc();
        view_dispatcher_add_view(
            view_dispatcher,
            InfraredViewDebugView,
            infrared_debug_view_get_view(infrared->debug_view));
    }

    infrared->button_panel = button_panel_alloc();
    infrared->progress = infrared_progress_view_alloc();

    return infrared;
}

static void infrared_free(InfraredApp* infrared) {
    furi_assert(infrared);

    furi_thread_join(infrared->task_thread);
    furi_thread_free(infrared->task_thread);

    ViewDispatcher* view_dispatcher = infrared->view_dispatcher;
    InfraredAppState* app_state = &infrared->app_state;

    if(infrared->rpc_ctx) {
        rpc_system_app_set_callback(infrared->rpc_ctx, NULL, NULL);
        rpc_system_app_send_exited(infrared->rpc_ctx);
        infrared->rpc_ctx = NULL;
    }

    view_dispatcher_remove_view(view_dispatcher, InfraredViewSubmenu);
    submenu_free(infrared->submenu);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewTextInput);
    text_input_free(infrared->text_input);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewDialogEx);
    dialog_ex_free(infrared->dialog_ex);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewButtonMenu);
    button_menu_free(infrared->button_menu);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewPopup);
    popup_free(infrared->popup);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewVariableList);
    variable_item_list_free(infrared->var_item_list);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewStack);
    view_stack_free(infrared->view_stack);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewMove);
    infrared_move_view_free(infrared->move_view);

    view_dispatcher_remove_view(view_dispatcher, InfraredViewLoading);
    loading_free(infrared->loading);

    if(app_state->is_debug_enabled) {
        view_dispatcher_remove_view(view_dispatcher, InfraredViewDebugView);
        infrared_debug_view_free(infrared->debug_view);
    }

    button_panel_free(infrared->button_panel);
    infrared_progress_view_free(infrared->progress);

    view_dispatcher_free(view_dispatcher);
    scene_manager_free(infrared->scene_manager);

    infrared_brute_force_free(infrared->brute_force);
    infrared_signal_free(infrared->current_signal);
    infrared_remote_free(infrared->remote);
    infrared_worker_free(infrared->worker);

    furi_record_close(RECORD_NOTIFICATION);
    infrared->notifications = NULL;

    furi_record_close(RECORD_DIALOGS);
    infrared->dialogs = NULL;

    furi_record_close(RECORD_GUI);
    infrared->gui = NULL;

    furi_string_free(infrared->file_path);
    furi_string_free(infrared->button_name);

    free(infrared);
}

InfraredErrorCode infrared_add_remote_with_button(
    const InfraredApp* infrared,
    const char* button_name,
    const InfraredSignal* signal) {
    InfraredRemote* remote = infrared->remote;

    FuriString* new_name = furi_string_alloc_set(INFRARED_DEFAULT_REMOTE_NAME);
    FuriString* new_path = furi_string_alloc_set(INFRARED_APP_FOLDER);

    infrared_find_vacant_remote_name(new_name, furi_string_get_cstr(new_path));
    furi_string_cat_printf(
        new_path, "/%s%s", furi_string_get_cstr(new_name), INFRARED_APP_EXTENSION);

    InfraredErrorCode error = InfraredErrorCodeNone;

    do {
        error = infrared_remote_create(remote, furi_string_get_cstr(new_path));
        if(INFRARED_ERROR_PRESENT(error)) break;

        error = infrared_remote_append_signal(remote, signal, button_name);
    } while(false);

    furi_string_free(new_name);
    furi_string_free(new_path);

    return error;
}

InfraredErrorCode
    infrared_rename_current_remote(const InfraredApp* infrared, const char* new_name) {
    InfraredRemote* remote = infrared->remote;
    const char* old_path = infrared_remote_get_path(remote);

    if(!strcmp(infrared_remote_get_name(remote), new_name)) {
        return true;
    }

    FuriString* new_name_fstr = furi_string_alloc_set(new_name);
    FuriString* new_path_fstr = furi_string_alloc_set(old_path);

    infrared_find_vacant_remote_name(new_name_fstr, old_path);

    if(furi_string_end_with(new_path_fstr, INFRARED_APP_EXTENSION)) {
        path_extract_dirname(old_path, new_path_fstr);
    }

    path_append(new_path_fstr, furi_string_get_cstr(new_name_fstr));
    furi_string_cat(new_path_fstr, INFRARED_APP_EXTENSION);

    const InfraredErrorCode error =
        infrared_remote_rename(remote, furi_string_get_cstr(new_path_fstr));

    furi_string_free(new_name_fstr);
    furi_string_free(new_path_fstr);

    return error;
}

void infrared_tx_start(InfraredApp* infrared) {
    if(infrared->app_state.is_transmitting) {
        return;
    }

    const uint32_t time_elapsed = furi_get_tick() - infrared->app_state.last_transmit_time;

    if(time_elapsed < INFRARED_TX_MIN_INTERVAL_MS) {
        return;
    }

    if(infrared_signal_is_raw(infrared->current_signal)) {
        const InfraredRawSignal* raw = infrared_signal_get_raw_signal(infrared->current_signal);
        infrared_worker_set_raw_signal(
            infrared->worker, raw->timings, raw->timings_size, raw->frequency, raw->duty_cycle);
    } else {
        const InfraredMessage* message = infrared_signal_get_message(infrared->current_signal);
        infrared_worker_set_decoded_signal(infrared->worker, message);
    }

    dolphin_deed(DolphinDeedIrSend);
    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStartSend);

    infrared_worker_tx_set_get_signal_callback(
        infrared->worker, infrared_worker_tx_get_signal_steady_callback, infrared);
    infrared_worker_tx_start(infrared->worker);

    infrared->app_state.is_transmitting = true;
}

InfraredErrorCode infrared_tx_start_button_index(InfraredApp* infrared, size_t button_index) {
    furi_assert(button_index < infrared_remote_get_signal_count(infrared->remote));

    InfraredErrorCode error =
        infrared_remote_load_signal(infrared->remote, infrared->current_signal, button_index);

    if(!INFRARED_ERROR_PRESENT(error)) {
        infrared_tx_start(infrared);
    }
    return error;
}

void infrared_tx_stop(InfraredApp* infrared) {
    if(!infrared->app_state.is_transmitting) {
        return;
    }

    infrared_worker_tx_stop(infrared->worker);
    infrared_worker_tx_set_get_signal_callback(infrared->worker, NULL, NULL);

    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStop);

    infrared->app_state.is_transmitting = false;
    infrared->app_state.last_transmit_time = furi_get_tick();
}

void infrared_tx_send_once(InfraredApp* infrared) {
    if(infrared->app_state.is_transmitting) {
        return;
    }

    dolphin_deed(DolphinDeedIrSend);
    infrared_signal_transmit(infrared->current_signal);
}

InfraredErrorCode infrared_tx_send_once_button_index(InfraredApp* infrared, size_t button_index) {
    furi_assert(button_index < infrared_remote_get_signal_count(infrared->remote));

    InfraredErrorCode error = infrared_remote_load_signal(
        infrared->remote, infrared->current_signal, infrared->app_state.current_button_index);
    if(!INFRARED_ERROR_PRESENT(error)) {
        infrared_tx_send_once(infrared);
    }

    return error;
}
void infrared_blocking_task_start(InfraredApp* infrared, FuriThreadCallback callback) {
    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewLoading);
    furi_thread_set_callback(infrared->task_thread, callback);
    furi_thread_start(infrared->task_thread);
}

InfraredErrorCode infrared_blocking_task_finalize(InfraredApp* infrared) {
    furi_thread_join(infrared->task_thread);
    return furi_thread_get_return_code(infrared->task_thread);
}

void infrared_text_store_set(InfraredApp* infrared, uint32_t bank, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vsnprintf(infrared->text_store[bank], INFRARED_TEXT_STORE_SIZE, fmt, args);

    va_end(args);
}

void infrared_text_store_clear(InfraredApp* infrared, uint32_t bank) {
    memset(infrared->text_store[bank], 0, INFRARED_TEXT_STORE_SIZE + 1);
}

void infrared_play_notification_message(
    const InfraredApp* infrared,
    InfraredNotificationMessage message) {
    furi_assert(message < InfraredNotificationMessageCount);
    notification_message(infrared->notifications, infrared_notification_sequences[message]);
}

void infrared_show_error_message(const InfraredApp* infrared, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    FuriString* message = furi_string_alloc_vprintf(fmt, args);
    dialog_message_show_storage_error(infrared->dialogs, furi_string_get_cstr(message));

    furi_string_free(message);
    va_end(args);
}

void infrared_set_tx_pin(InfraredApp* infrared, FuriHalInfraredTxPin tx_pin) {
    if(tx_pin < FuriHalInfraredTxPinMax) {
        furi_hal_infrared_set_tx_output(tx_pin);
    } else {
        FuriHalInfraredTxPin tx_pin_detected = furi_hal_infrared_detect_tx_output();
        furi_hal_infrared_set_tx_output(tx_pin_detected);
        if(tx_pin_detected != FuriHalInfraredTxPinInternal) {
            infrared_enable_otg(infrared, true);
        }
    }

    infrared->app_state.tx_pin = tx_pin;
}

void infrared_enable_otg(InfraredApp* infrared, bool enable) {
    if(enable) {
        if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    } else {
        if(furi_hal_power_is_otg_enabled()) furi_hal_power_disable_otg();
    }
    infrared->app_state.is_otg_enabled = enable;
}

static void infrared_load_settings(InfraredApp* infrared) {
    InfraredSettings settings = {0};

    if(!infrared_settings_load(&settings)) {
        FURI_LOG_D(TAG, "Failed to load settings, using defaults");
        // infrared_save_settings(infrared);
    }

    infrared_set_tx_pin(infrared, settings.tx_pin);
    if(settings.tx_pin < FuriHalInfraredTxPinMax) {
        infrared_enable_otg(infrared, settings.otg_enabled);
    }
    infrared->app_state.is_easy_mode = settings.easy_mode;
}

void infrared_save_settings(InfraredApp* infrared) {
    InfraredSettings settings = {
        .tx_pin = infrared->app_state.tx_pin,
        .otg_enabled = infrared->app_state.is_otg_enabled,
        .easy_mode = infrared->app_state.is_easy_mode,
    };

    if(!infrared_settings_save(&settings)) {
        FURI_LOG_E(TAG, "Failed to save settings");
    }
}

void infrared_signal_received_callback(void* context, InfraredWorkerSignal* received_signal) {
    furi_assert(context);
    InfraredApp* infrared = context;

    if(infrared_worker_signal_is_decoded(received_signal)) {
        infrared_signal_set_message(
            infrared->current_signal, infrared_worker_get_decoded_signal(received_signal));
    } else {
        const uint32_t* timings;
        size_t timings_size;
        infrared_worker_get_raw_signal(received_signal, &timings, &timings_size);
        infrared_signal_set_raw_signal(
            infrared->current_signal,
            timings,
            timings_size,
            INFRARED_COMMON_CARRIER_FREQUENCY,
            INFRARED_COMMON_DUTY_CYCLE);
    }

    view_dispatcher_send_custom_event(
        infrared->view_dispatcher, InfraredCustomEventTypeSignalReceived);
}

void infrared_text_input_callback(void* context) {
    furi_assert(context);
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(
        infrared->view_dispatcher, InfraredCustomEventTypeTextEditDone);
}

void infrared_popup_closed_callback(void* context) {
    furi_assert(context);
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(
        infrared->view_dispatcher, InfraredCustomEventTypePopupClosed);
}

int32_t infrared_app(void* p) {
    InfraredApp* infrared = infrared_alloc();

    infrared_load_settings(infrared);
    infrared_make_app_folder(infrared);

    bool is_remote_loaded = false;
    bool is_rpc_mode = false;

    if(p && strlen(p)) {
        uint32_t rpc_ctx = 0;
        if(sscanf(p, "RPC %lX", &rpc_ctx) == 1) {
            infrared->rpc_ctx = (void*)rpc_ctx;
            rpc_system_app_set_callback(
                infrared->rpc_ctx, infrared_rpc_command_callback, infrared);
            rpc_system_app_send_started(infrared->rpc_ctx);
            is_rpc_mode = true;
        } else {
            const char* file_path = (const char*)p;
            InfraredErrorCode error = infrared_remote_load(infrared->remote, file_path);

            if(!INFRARED_ERROR_PRESENT(error)) {
                is_remote_loaded = true;
            } else {
                is_remote_loaded = false;
                bool wrong_file_type = INFRARED_ERROR_CHECK(error, InfraredErrorCodeWrongFileType);
                const char* format = wrong_file_type ?
                                         "Library file\n\"%s\" can't be opened as a remote" :
                                         "Failed to load\n\"%s\"";

                infrared_show_error_message(infrared, format, file_path);
                return -1;
            }

            furi_string_set(infrared->file_path, file_path);
        }
    }

    if(is_rpc_mode) {
        view_dispatcher_attach_to_gui(
            infrared->view_dispatcher, infrared->gui, ViewDispatcherTypeDesktop);
        scene_manager_next_scene(infrared->scene_manager, InfraredSceneRpc);
    } else {
        view_dispatcher_attach_to_gui(
            infrared->view_dispatcher, infrared->gui, ViewDispatcherTypeFullscreen);
        if(is_remote_loaded) { //-V547
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneRemote);
        } else {
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneStart);
        }
    }

    view_dispatcher_run(infrared->view_dispatcher);

    infrared_set_tx_pin(infrared, FuriHalInfraredTxPinInternal);
    infrared_enable_otg(infrared, false);
    infrared_free(infrared);

    return 0;
}
