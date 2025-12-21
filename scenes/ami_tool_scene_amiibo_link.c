#include "ami_tool_i.h"
#include <string.h>

#define AMI_TOOL_AMIIBO_LINK_COMPLETION_DELAY_MS (1000U)

static void ami_tool_scene_amiibo_link_handle_completion(AmiToolApp* app);
static uint32_t ami_tool_scene_amiibo_link_hash(const AmiToolApp* app);
static void ami_tool_scene_amiibo_link_check_completion(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_reset_config_tracking(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_monitor_config(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_restore_pending_auth0(AmiToolApp* app);

static size_t ami_tool_scene_amiibo_link_config_start_page(const AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total < 4) {
        return 0;
    }
    return app->tag_data->pages_total - 4;
}

static void ami_tool_scene_amiibo_link_reset_config_tracking(AmiToolApp* app) {
    if(!app) {
        return;
    }
    app->amiibo_link_auth0_override_active = false;
    app->amiibo_link_pending_auth0 = 0xFF;
    app->amiibo_link_current_auth0 = 0xFF;
    app->amiibo_link_access_snapshot_valid = false;
    memset(app->amiibo_link_access_snapshot, 0, sizeof(app->amiibo_link_access_snapshot));

    if(app->tag_data && app->tag_data->pages_total >= 4) {
        size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
        app->amiibo_link_current_auth0 = app->tag_data->page[config_start].data[3];
        memcpy(
            app->amiibo_link_access_snapshot,
            app->tag_data->page[config_start + 1].data,
            MF_ULTRALIGHT_PAGE_SIZE);
        app->amiibo_link_access_snapshot_valid = true;
    }
}

static void ami_tool_scene_amiibo_link_monitor_config(AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total < 4) {
        return;
    }

    size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
    uint8_t* config_page = app->tag_data->page[config_start].data;
    uint8_t desired_auth0 = config_page[3];

    if(!app->amiibo_link_auth0_override_active) {
        if(desired_auth0 != app->amiibo_link_current_auth0) {
            if(desired_auth0 == 0xFF) {
                app->amiibo_link_current_auth0 = desired_auth0;
            } else {
                app->amiibo_link_pending_auth0 = desired_auth0;
                app->amiibo_link_auth0_override_active = true;
                app->amiibo_link_current_auth0 = 0xFF;
                config_page[3] = 0xFF;
            }
        }
    } else if(app->amiibo_link_access_snapshot_valid) {
        uint8_t* access_page = app->tag_data->page[config_start + 1].data;
        if(memcmp(
               access_page,
               app->amiibo_link_access_snapshot,
               MF_ULTRALIGHT_PAGE_SIZE) != 0) {
            memcpy(
                app->amiibo_link_access_snapshot,
                access_page,
                MF_ULTRALIGHT_PAGE_SIZE);
            config_page[3] = app->amiibo_link_pending_auth0;
            app->amiibo_link_current_auth0 = app->amiibo_link_pending_auth0;
            app->amiibo_link_auth0_override_active = false;
        }
    }
}

static void ami_tool_scene_amiibo_link_restore_pending_auth0(AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total < 4) {
        return;
    }
    if(!app->amiibo_link_auth0_override_active) {
        return;
    }
    size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
    app->tag_data->page[config_start].data[3] = app->amiibo_link_pending_auth0;
    app->amiibo_link_current_auth0 = app->amiibo_link_pending_auth0;
    app->amiibo_link_auth0_override_active = false;
}

static void ami_tool_scene_amiibo_link_show_text(AmiToolApp* app, const char* message) {
    if(!app || !message) {
        return;
    }
    furi_string_set(app->text_box_store, message);
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    app->usage_info_visible = false;
}

static void ami_tool_scene_amiibo_link_show_ready(AmiToolApp* app) {
    ami_tool_scene_amiibo_link_show_text(
        app,
        "Amiibo Link\n\n"
        "Flipper is emulating a blank NTAG215.\n"
        "Use compatible application to write data.\n"
        "Press Back to stop.");
}

static bool ami_tool_scene_amiibo_link_prepare(AmiToolApp* app) {
    if(!app || !app->tag_data) {
        return false;
    }

    ami_tool_clear_cached_tag(app);

    if(amiibo_prepare_blank_tag(app->tag_data) != RFIDX_OK) {
        return false;
    }

    app->tag_data_valid = true;

    size_t uid_len = 0;
    const uint8_t* uid = mf_ultralight_get_uid(app->tag_data, &uid_len);
    ami_tool_store_uid(app, uid, uid_len);

    static const uint8_t blank_password[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(app->tag_password.data, blank_password, sizeof(blank_password));
    app->tag_password_valid = true;

    if(uid && uid_len >= 7) {
        app->last_uid_valid = true;
        app->last_uid_len = uid_len;
    }

    static const uint8_t blank_pack[4] = {0x00, 0x00, 0x00, 0x00};
    memcpy(app->tag_pack, blank_pack, sizeof(app->tag_pack));
    app->tag_pack_valid = true;

    app->amiibo_link_initial_hash = ami_tool_scene_amiibo_link_hash(app);
    app->amiibo_link_last_hash = app->amiibo_link_initial_hash;
    app->amiibo_link_last_change_tick = 0;
    app->amiibo_link_completion_pending = false;
    ami_tool_scene_amiibo_link_reset_config_tracking(app);

    return true;
}

static bool ami_tool_scene_amiibo_link_start_session(AmiToolApp* app) {
    if(!ami_tool_scene_amiibo_link_prepare(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Amiibo Link\n\nUnable to prepare memory.\nPress Back to exit.");
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        app->amiibo_link_completion_pending = false;
        app->amiibo_link_last_change_tick = 0;
        return false;
    }

    if(!ami_tool_info_start_emulation(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Amiibo Link\n\nUnable to start emulation.\nPress Back to exit.");
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        app->amiibo_link_completion_pending = false;
        app->amiibo_link_last_change_tick = 0;
        return false;
    }

    app->amiibo_link_active = true;
    app->amiibo_link_waiting_for_completion = true;
    app->amiibo_link_completion_pending = false;
    app->amiibo_link_last_change_tick = 0;
    app->amiibo_link_last_hash = app->amiibo_link_initial_hash;
    ami_tool_scene_amiibo_link_show_ready(app);
    return true;
}

static bool ami_tool_scene_amiibo_link_get_valid_id(
    AmiToolApp* app,
    char* id_hex,
    size_t buffer_size) {
    if(!ami_tool_extract_amiibo_id(app->tag_data, id_hex, buffer_size)) {
        return false;
    }
    if(strlen(id_hex) != 16) {
        return false;
    }
    bool all_zero = true;
    for(size_t i = 0; i < 16; i++) {
        if(id_hex[i] != '0') {
            all_zero = false;
            break;
        }
    }
    return !all_zero;
}

static bool ami_tool_scene_amiibo_link_finalize(AmiToolApp* app) {
    if(!app || !app->tag_data || !app->tag_data_valid || !app->last_uid_valid) {
        return false;
    }

    ami_tool_info_stop_emulation(app);

    if(amiibo_set_uid(app->tag_data, app->last_uid, app->last_uid_len) != RFIDX_OK) {
        return false;
    }

    amiibo_configure_rf_interface(app->tag_data);

    if(app->tag_data->pages_total < 2) {
        return false;
    }

    size_t password_page = app->tag_data->pages_total - 2;
    size_t pack_page = app->tag_data->pages_total - 1;
    memcpy(app->tag_password.data, app->tag_data->page[password_page].data, sizeof(app->tag_password.data));
    app->tag_password_valid = true;
    memcpy(app->tag_pack, app->tag_data->page[pack_page].data, sizeof(app->tag_pack));
    app->tag_pack_valid = true;

    char id_hex[17] = {0};
    if(!ami_tool_scene_amiibo_link_get_valid_id(app, id_hex, sizeof(id_hex))) {
        return false;
    }

    ami_tool_store_uid(app, app->last_uid, app->last_uid_len);
    ami_tool_info_show_page(app, id_hex, true);
    return true;
}

static uint32_t ami_tool_scene_amiibo_link_hash(const AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total == 0) {
        return 0;
    }

    uint32_t hash = 0;
    const size_t start_page = 4;
    size_t end_page = app->tag_data->pages_total;
    if(end_page > 4) {
        end_page -= 4;
    }
    for(size_t page = start_page; page < end_page; page++) {
        const uint8_t* data = app->tag_data->page[page].data;
        for(size_t i = 0; i < MF_ULTRALIGHT_PAGE_SIZE; i++) {
            hash = (hash * 131) ^ data[i];
        }
    }
    return hash;
}

static void ami_tool_scene_amiibo_link_check_completion(AmiToolApp* app) {
    if(!app || !app->amiibo_link_waiting_for_completion) {
        return;
    }

    uint32_t current_hash = ami_tool_scene_amiibo_link_hash(app);
    if(current_hash != app->amiibo_link_last_hash) {
        app->amiibo_link_last_hash = current_hash;
        app->amiibo_link_last_change_tick = furi_get_tick();
    }
    if(current_hash == app->amiibo_link_initial_hash) {
        return;
    }

    char id_hex[17] = {0};
    if(!ami_tool_scene_amiibo_link_get_valid_id(app, id_hex, sizeof(id_hex))) {
        return;
    }

    if(!app->amiibo_link_completion_pending) {
        app->amiibo_link_completion_pending = true;
        if(app->amiibo_link_last_change_tick == 0) {
            app->amiibo_link_last_change_tick = furi_get_tick();
        }
        return;
    }

    if(app->amiibo_link_last_change_tick == 0) {
        return;
    }

    uint32_t now = furi_get_tick();
    uint32_t delay_ticks = furi_ms_to_ticks(AMI_TOOL_AMIIBO_LINK_COMPLETION_DELAY_MS);
    if((now - app->amiibo_link_last_change_tick) < delay_ticks) {
        return;
    }

    app->amiibo_link_waiting_for_completion = false;
    app->amiibo_link_completion_pending = false;
    ami_tool_scene_amiibo_link_handle_completion(app);
}

static void ami_tool_scene_amiibo_link_handle_completion(AmiToolApp* app) {
    if(!app) {
        return;
    }

    ami_tool_scene_amiibo_link_restore_pending_auth0(app);
    app->amiibo_link_completion_pending = false;
    app->amiibo_link_last_change_tick = 0;

    bool success = ami_tool_scene_amiibo_link_finalize(app);
    if(success) {
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        return;
    }

    ami_tool_scene_amiibo_link_show_text(
        app,
        "Amiibo Link\n\nNo valid Amiibo data detected.\n"
        "Hold the phone in place and try again.");

    if(!ami_tool_scene_amiibo_link_start_session(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Amiibo Link\n\nUnable to restart emulation.\nPress Back to exit.");
    }
}

void ami_tool_scene_amiibo_link_on_enter(void* context) {
    AmiToolApp* app = context;
    ami_tool_scene_amiibo_link_start_session(app);
}

bool ami_tool_scene_amiibo_link_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventAmiiboLinkWriteComplete:
            ami_tool_scene_amiibo_link_handle_completion(app);
            return true;
        case AmiToolEventInfoShowActions:
            ami_tool_info_show_actions_menu(app);
            return true;
        case AmiToolEventInfoActionEmulate:
            if(!ami_tool_info_start_emulation(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to start emulation.\nRead or generate an Amiibo first.");
            }
            return true;
        case AmiToolEventInfoActionUsage:
            ami_tool_info_show_usage(app);
            return true;
        case AmiToolEventInfoActionChangeUid:
            if(!ami_tool_info_change_uid(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to change UID.\nInstall key_retail.bin and try again.");
            }
            return true;
        case AmiToolEventInfoActionWriteTag:
            if(!ami_tool_info_write_to_tag(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to write tag.\nUse a blank NTAG215 and make\nsure key_retail.bin is installed.");
            }
            return true;
        case AmiToolEventInfoActionSaveToStorage:
            if(!ami_tool_info_save_to_storage(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to save Amiibo.\nRead or generate one first\nand check storage access.");
            }
            return true;
        case AmiToolEventUsagePrevPage:
            ami_tool_info_navigate_usage(app, -1);
            return true;
        case AmiToolEventUsageNextPage:
            ami_tool_info_navigate_usage(app, 1);
            return true;
        case AmiToolEventInfoWriteStarted:
        case AmiToolEventInfoWriteSuccess:
        case AmiToolEventInfoWriteFailed:
        case AmiToolEventInfoWriteCancelled:
            ami_tool_info_handle_write_event(app, event.event);
            return true;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->amiibo_link_active && app->amiibo_link_waiting_for_completion) {
            ami_tool_scene_amiibo_link_monitor_config(app);
            ami_tool_scene_amiibo_link_check_completion(app);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->write_in_progress) {
            if(app->write_waiting_for_tag) {
                ami_tool_info_request_write_cancel(app);
            }
            return true;
        }
        if(app->info_emulation_active) {
            ami_tool_info_stop_emulation(app);
            if(app->amiibo_link_active) {
                app->amiibo_link_active = false;
                app->amiibo_link_waiting_for_completion = false;
                app->amiibo_link_completion_pending = false;
                app->amiibo_link_last_change_tick = 0;
                ami_tool_scene_amiibo_link_restore_pending_auth0(app);
                return false;
            }
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->usage_info_visible) {
            app->usage_info_visible = false;
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_action_message_visible) {
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_actions_visible) {
            app->info_actions_visible = false;
            ami_tool_info_refresh_current_page(app);
            return true;
        }
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        app->amiibo_link_completion_pending = false;
        app->amiibo_link_last_change_tick = 0;
        ami_tool_scene_amiibo_link_restore_pending_auth0(app);
        ami_tool_info_stop_emulation(app);
        return false;
    }

    return false;
}

void ami_tool_scene_amiibo_link_on_exit(void* context) {
    AmiToolApp* app = context;
    ami_tool_info_stop_emulation(app);
    ami_tool_info_abort_write(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->amiibo_link_active = false;
    app->amiibo_link_waiting_for_completion = false;
    app->amiibo_link_initial_hash = 0;
    app->amiibo_link_last_hash = 0;
    app->amiibo_link_last_change_tick = 0;
    app->amiibo_link_completion_pending = false;
}
