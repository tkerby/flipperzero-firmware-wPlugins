#include "ami_tool_i.h"
#include <string.h>
#include <stdio.h>
#include <furi_hal.h>
#include <furi_hal_nfc.h>

#define AMI_TOOL_READ_THREAD_STACK_SIZE 2048

static void ami_tool_scene_read_reset_result(AmiToolApp* app) {
    furi_assert(app);
    app->read_result.type = AmiToolReadResultNone;
    app->read_result.error = MfUltralightErrorNone;
    app->read_result.tag_type = MfUltralightTypeOrigin;
    app->read_result.uid_len = 0;
    memset(app->read_result.uid, 0, sizeof(app->read_result.uid));
    app->tag_data_valid = false;
    memset(&app->tag_password, 0, sizeof(app->tag_password));
    app->tag_password_valid = false;
}

static void ami_tool_scene_read_stop_thread(AmiToolApp* app) {
    if(app->read_thread) {
        furi_thread_join(app->read_thread);
        furi_thread_free(app->read_thread);
        app->read_thread = NULL;
    }
}

static const char* ami_tool_scene_read_error_to_string(MfUltralightError error) {
    switch(error) {
    case MfUltralightErrorNone:
        return "No error";
    case MfUltralightErrorNotPresent:
        return "Tag lost during read.";
    case MfUltralightErrorProtocol:
        return "Protocol error while reading.";
    case MfUltralightErrorAuth:
        return "Authentication failed.";
    case MfUltralightErrorTimeout:
        return "Timed out waiting for tag.";
    default:
        return "Unknown read error.";
    }
}

static const char* ami_tool_scene_read_type_to_string(MfUltralightType type) {
    switch(type) {
    case MfUltralightTypeOrigin:
        return "Ultralight";
    case MfUltralightTypeNTAG203:
        return "NTAG203";
    case MfUltralightTypeMfulC:
        return "Ultralight C";
    case MfUltralightTypeUL11:
        return "UL11";
    case MfUltralightTypeUL21:
        return "UL21";
    case MfUltralightTypeNTAG213:
        return "NTAG213";
    case MfUltralightTypeNTAG215:
        return "NTAG215";
    case MfUltralightTypeNTAG216:
        return "NTAG216";
    case MfUltralightTypeNTAGI2C1K:
        return "NTAG I2C 1K";
    case MfUltralightTypeNTAGI2C2K:
        return "NTAG I2C 2K";
    case MfUltralightTypeNTAGI2CPlus1K:
        return "NTAG I2C Plus 1K";
    case MfUltralightTypeNTAGI2CPlus2K:
        return "NTAG I2C Plus 2K";
    default:
        return "Unknown";
    }
}

static void ami_tool_scene_read_show_waiting(AmiToolApp* app) {
    furi_string_printf(
        app->text_box_store,
        "NFC Read\n\nPlace an NTAG215 tag on the back of the Flipper.\nPress Back to cancel.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

static void ami_tool_scene_read_show_wrong_type(AmiToolApp* app) {
    furi_string_printf(
        app->text_box_store,
        "Unsupported tag.\nExpected NTAG215 but detected %s.\n\nPress Back to exit.",
        ami_tool_scene_read_type_to_string(app->read_result.tag_type));
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

static void ami_tool_scene_read_show_error(AmiToolApp* app) {
    furi_string_printf(
        app->text_box_store,
        "Read failed.\n%s\n\nPress Back to exit.",
        ami_tool_scene_read_error_to_string(app->read_result.error));
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

static bool ami_tool_scene_read_compute_password(
    const uint8_t* uid,
    size_t uid_len,
    MfUltralightAuthPassword* password) {
    if((uid_len < 7) || (uid == NULL) || (password == NULL)) {
        return false;
    }

    password->data[0] = 0xAA ^ (uid[1] ^ uid[3]);
    password->data[1] = 0x55 ^ (uid[2] ^ uid[4]);
    password->data[2] = 0xAA ^ (uid[3] ^ uid[5]);
    password->data[3] = 0x55 ^ (uid[4] ^ uid[6]);

    return true;
}

static int32_t ami_tool_scene_read_worker(void* context);
static bool ami_tool_scene_read_extract_amiibo_id(AmiToolApp* app, char* buffer, size_t buffer_size);
static void ami_tool_scene_read_show_info(AmiToolApp* app);

static void ami_tool_scene_read_start_worker(AmiToolApp* app) {
    if(app->read_thread) {
        return;
    }
    ami_tool_scene_read_reset_result(app);
    app->read_thread = furi_thread_alloc_ex(
        "AmiToolNfcRead", AMI_TOOL_READ_THREAD_STACK_SIZE, ami_tool_scene_read_worker, app);
    furi_thread_start(app->read_thread);
}

static int32_t ami_tool_scene_read_worker(void* context) {
    AmiToolApp* app = context;
    MfUltralightData* data = mf_ultralight_alloc();
    bool waiting_for_tag = true;

    while(app->read_scene_active) {
        MfUltralightError error = mf_ultralight_poller_sync_read_card(app->nfc, data, NULL);

        if(!app->read_scene_active) {
            break;
        }

        if((error == MfUltralightErrorNotPresent) && waiting_for_tag) {
            furi_delay_ms(100);
            continue;
        }

        AmiToolReadResult result = {
            .type = AmiToolReadResultError,
            .error = error,
            .tag_type = MfUltralightTypeOrigin,
            .uid_len = 0,
        };

        AmiToolCustomEvent event = AmiToolEventReadError;

        if(error == MfUltralightErrorNone) {
            waiting_for_tag = false;
            const uint8_t* uid = mf_ultralight_get_uid(data, &result.uid_len);
            if(uid && result.uid_len > 0) {
                if(result.uid_len > sizeof(result.uid)) {
                    result.uid_len = sizeof(result.uid);
                }
                memcpy(result.uid, uid, result.uid_len);
            }
            result.tag_type = data->type;

            if(data->type != MfUltralightTypeNTAG215) {
                result.type = AmiToolReadResultWrongType;
                event = AmiToolEventReadWrongType;
            } else {
                if(app->tag_data) {
                    mf_ultralight_copy(app->tag_data, data);
                    app->tag_data_valid = true;
                }

                if(ami_tool_scene_read_compute_password(
                       uid, result.uid_len, &app->tag_password)) {
                    app->tag_password_valid = true;
                } else {
                    app->tag_password_valid = false;
                    memset(&app->tag_password, 0, sizeof(app->tag_password));
                }

                result.type = AmiToolReadResultSuccess;
                result.error = MfUltralightErrorNone;
                event = AmiToolEventReadSuccess;
            }
        } else {
            result.type = AmiToolReadResultError;
            event = AmiToolEventReadError;
        }

        app->read_result = result;
        view_dispatcher_send_custom_event(app->view_dispatcher, event);
        break;
    }

    mf_ultralight_free(data);
    return 0;
}

void ami_tool_scene_read_on_enter(void* context) {
    AmiToolApp* app = context;

    app->read_scene_active = true;
    ami_tool_scene_read_reset_result(app);
    ami_tool_scene_read_show_waiting(app);
    ami_tool_scene_read_start_worker(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

bool ami_tool_scene_read_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventReadSuccess:
            ami_tool_scene_read_stop_thread(app);
            app->read_scene_active = false;
            ami_tool_scene_read_show_info(app);
            return true;
        case AmiToolEventReadWrongType:
            ami_tool_scene_read_stop_thread(app);
            ami_tool_scene_read_show_wrong_type(app);
            return true;
        case AmiToolEventReadError:
            ami_tool_scene_read_stop_thread(app);
            ami_tool_scene_read_show_error(app);
            return true;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        app->read_scene_active = false;
        furi_hal_nfc_abort();
        ami_tool_scene_read_stop_thread(app);
        return false;
    }

    return false;
}

void ami_tool_scene_read_on_exit(void* context) {
    AmiToolApp* app = context;
    app->read_scene_active = false;
    furi_hal_nfc_abort();
    ami_tool_scene_read_stop_thread(app);
}
static bool ami_tool_scene_read_extract_amiibo_id(AmiToolApp* app, char* buffer, size_t buffer_size) {
    if(!app || !buffer || buffer_size < 17) {
        return false;
    }
    if(!app->tag_data_valid || !app->tag_data) {
        return false;
    }

    const size_t start_page = 21;
    if(app->tag_data->pages_total <= (start_page + 1)) {
        return false;
    }

    uint8_t raw[8];
    for(size_t i = 0; i < 4; i++) {
        raw[i] = app->tag_data->page[start_page].data[i];
        raw[i + 4] = app->tag_data->page[start_page + 1].data[i];
    }

    for(size_t i = 0; i < 8; i++) {
        snprintf(buffer + (i * 2), buffer_size - (i * 2), "%02X", raw[i]);
    }
    buffer[16] = '\0';

    return true;
}

static void ami_tool_scene_read_show_info(AmiToolApp* app) {
    char id_hex[17] = {0};
    bool has_id = ami_tool_scene_read_extract_amiibo_id(app, id_hex, sizeof(id_hex));
    ami_tool_info_show_page(app, has_id ? id_hex : NULL, true);
}
