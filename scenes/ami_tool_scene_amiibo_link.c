#include "ami_tool_i.h"
#include <string.h>

static void ami_tool_scene_amiibo_link_show_text(AmiToolApp* app, const char* message) {
    if(!app || !message) {
        return;
    }
    furi_string_set(app->text_box_store, message);
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
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

    static const uint8_t blank_pack[4] = {0x00, 0x00, 0x00, 0x00};
    memcpy(app->tag_pack, blank_pack, sizeof(app->tag_pack));
    app->tag_pack_valid = true;

    return true;
}

void ami_tool_scene_amiibo_link_on_enter(void* context) {
    AmiToolApp* app = context;

    if(!ami_tool_scene_amiibo_link_prepare(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Unable to prepare Amiibo Link memory.\nPlease try again.");
        return;
    }

    if(!ami_tool_info_start_emulation(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Unable to start Amiibo Link emulation.\nPlease try again.");
        return;
    }

    ami_tool_scene_amiibo_link_show_ready(app);
}

bool ami_tool_scene_amiibo_link_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        ami_tool_info_stop_emulation(app);
        return false;
    }

    return false;
}

void ami_tool_scene_amiibo_link_on_exit(void* context) {
    AmiToolApp* app = context;
    ami_tool_info_stop_emulation(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
}
