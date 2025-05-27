#include "../ac_remote_app_i.h"

#include <inttypes.h>

typedef enum {
    button_power,
    button_mode,
    button_temp_up,
    button_fan,
    button_temp_down,
    button_vane,
    label_temperature,
} button_id;

static const Icon* POWER_ICONS[POWER_STATE_MAX][2] = {
    [PowerButtonOff] = {&I_on_19x20, &I_on_hover_19x20},
    [PowerButtonOn] = {&I_off_19x20, &I_off_hover_19x20},
};

static const Icon* MODE_BUTTON_ICONS[MODE_BUTTON_STATE_MAX][2] = {
    [ModeButtonHeating] = {&I_heat_19x20, &I_heat_hover_19x20},
    [ModeButtonCooling] = {&I_cold_19x20, &I_cold_hover_19x20},
    [ModeButtonDehumidifying] = {&I_dry_19x20, &I_dry_hover_19x20},
    [ModeButtonFan] = {&I_fan_speed_4_19x20, &I_fan_speed_4_hover_19x20},
};

static const Icon* FAN_SPEED_BUTTON_ICONS[FAN_SPEED_BUTTON_STATE_MAX][2] = {
    [FanSpeedButtonLow] = {&I_fan_speed_2_19x20, &I_fan_speed_2_hover_19x20},
    [FanSpeedButtonMedium] = {&I_fan_speed_3_19x20, &I_fan_speed_3_hover_19x20},
    [FanSpeedButtonHigh] = {&I_fan_speed_4_19x20, &I_fan_speed_4_hover_19x20},
};

static const Icon* VANE_BUTTON_ICONS[VANE_BUTTON_STATE_MAX][2] = {
    [VaneButtonPos0] = {&I_vane_0_19x20, &I_vane_0_hover_19x20},
    [VaneButtonPos1] = {&I_vane_1_19x20, &I_vane_1_hover_19x20},
    [VaneButtonPos2] = {&I_vane_2_19x20, &I_vane_2_hover_19x20},
    [VaneButtonPos3] = {&I_vane_3_19x20, &I_vane_3_hover_19x20},
    [VaneButtonPos4] = {&I_vane_4_19x20, &I_vane_4_hover_19x20},
    [VaneButtonPos5] = {&I_vane_5_19x20, &I_vane_5_hover_19x20},
    [VaneButtonPos6] = {&I_vane_6_19x20, &I_vane_6_hover_19x20},
    [VaneButtonAuto] = {&I_vane_auto_move_19x20, &I_vane_auto_move_hover_19x20},
};

static const HvacHitachiControl POWER_LUT[POWER_STATE_MAX] = {
    [PowerButtonOff] = HvacHitachiControlPowerOff,
    [PowerButtonOn] = HvacHitachiControlPowerOn,
};

static const HvacHitachiMode MODE_LUT[MODE_BUTTON_STATE_MAX] = {
    [ModeButtonHeating] = HvacHitachiModeHeating,
    [ModeButtonCooling] = HvacHitachiModeCooling,
    [ModeButtonDehumidifying] = HvacHitachiModeDehumidifying,
    [ModeButtonFan] = HvacHitachiModeFan,
};

static const HvacHitachiFanSpeed FAN_SPEED_LUT[FAN_SPEED_BUTTON_STATE_MAX] = {
    [FanSpeedButtonLow] = HvacHitachiFanSpeedLow,
    [FanSpeedButtonMedium] = HvacHitachiFanSpeedMedium,
    [FanSpeedButtonHigh] = HvacHitachiFanSpeedHigh,
};

static const HvacHitachiVane VANE_LUT[VANE_BUTTON_STATE_MAX] = {
    [VaneButtonPos0] = HvacHitachiVanePos0,
    [VaneButtonPos1] = HvacHitachiVanePos1,
    [VaneButtonPos2] = HvacHitachiVanePos2,
    [VaneButtonPos3] = HvacHitachiVanePos3,
    [VaneButtonPos4] = HvacHitachiVanePos4,
    [VaneButtonPos5] = HvacHitachiVanePos5,
    [VaneButtonPos6] = HvacHitachiVanePos6,
    [VaneButtonAuto] = HvacHitachiVaneAuto,
};

char buffer[4] = {0};

static bool ac_remote_load_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    uint32_t version = 0;
    bool success = false;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(!furi_string_equal(header, "AC Remote") || (version != 1)) break;
        if(!flipper_format_read_uint32(ff, "Power", &app_state->power, 1)) break;
        if(!flipper_format_read_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(app_state->mode >= MODE_BUTTON_STATE_MAX) break;
        if(!flipper_format_read_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(app_state->temperature > HVAC_HITACHI_TEMPERATURE_MAX ||
           app_state->temperature < HVAC_HITACHI_TEMPERATURE_MIN)
            break;
        if(!flipper_format_read_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(app_state->fan >= FAN_SPEED_BUTTON_STATE_MAX) break;
        if(!flipper_format_read_uint32(ff, "Vane", &app_state->vane, 1)) break;
        if(app_state->vane > VANE_BUTTON_STATE_MAX) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(header);
    flipper_format_free(ff);
    return success;
}

bool ac_remote_store_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool success = false;
    do {
        if(!flipper_format_file_open_always(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_write_header_cstr(ff, "AC Remote", 1)) break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Power", &app_state->power, 1)) break;
        if(!flipper_format_write_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(!flipper_format_write_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(!flipper_format_write_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(!flipper_format_write_uint32(ff, "Vane", &app_state->vane, 1)) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    flipper_format_free(ff);
    return success;
}

void ac_remote_scene_universal_common_item_callback(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonSelected, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

void ac_remote_scene_hitachi_on_enter(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    ac_remote->protocol = hvac_hitachi_init();

    if(!ac_remote_load_settings(&ac_remote->app_state)) {
        ac_remote->app_state.power = PowerButtonOff;
        ac_remote->app_state.mode = ModeButtonCooling;
        ac_remote->app_state.fan = FanSpeedButtonLow;
        ac_remote->app_state.vane = VaneButtonPos0;
        ac_remote->app_state.temperature = 23;
    }

    ac_remote_panel_reserve(ac_remote_panel, 2, 3);

    ac_remote_panel_add_item(
        ac_remote_panel,
        button_power,
        0,
        0,
        6,
        17,
        POWER_ICONS[ac_remote->app_state.power][0],
        POWER_ICONS[ac_remote->app_state.power][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 5, 39, &I_power_text_21x5);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_mode,
        1,
        0,
        39,
        17,
        MODE_BUTTON_ICONS[ac_remote->app_state.mode][0],
        MODE_BUTTON_ICONS[ac_remote->app_state.mode][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 39, 39, &I_mode_text_20x5);
    ac_remote_panel_add_icon(ac_remote_panel, 0, 59, &I_frame_30x39);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_temp_up,
        0,
        1,
        3,
        47,
        &I_tempup_24x21,
        &I_tempup_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_temp_down,
        0,
        2,
        3,
        89,
        &I_tempdown_24x21,
        &I_tempdown_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_fan,
        1,
        1,
        39,
        50,
        FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][0],
        FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 41, 72, &I_fan_text_14x5);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_vane,
        1,
        2,
        39,
        83,
        VANE_BUTTON_ICONS[ac_remote->app_state.vane][0],
        VANE_BUTTON_ICONS[ac_remote->app_state.vane][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 38, 105, &I_vane_text_20x5);

    ac_remote_panel_add_label(ac_remote_panel, 0, 6, 11, FontPrimary, "AC remote");

    snprintf(buffer, sizeof(buffer), "%" PRIu32, ac_remote->app_state.temperature);
    ac_remote_panel_add_label(ac_remote_panel, label_temperature, 4, 82, FontKeyboard, buffer);

    view_set_orientation(ac_remote_panel_get_view(ac_remote_panel), ViewOrientationVertical);
    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewStack);
}

bool ac_remote_scene_hitachi_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    SceneManager* scene_manager = ac_remote->scene_manager;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    UNUSED(scene_manager);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        uint16_t event_type;
        int16_t event_value;
        ac_remote_custom_event_unpack(event.event, &event_type, &event_value);
        if(event_type == AC_RemoteCustomEventTypeSendCommand) {
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_blink_magenta_100);
            hvac_hitachi_send(ac_remote->protocol);
            notification_message(notifications, &sequence_blink_stop);
        } else if(event_type == AC_RemoteCustomEventTypeButtonSelected) {
            bool power_state_changed = false;
            hvac_hitachi_reset(ac_remote->protocol);
            switch(event_value) {
            case button_power:
                ac_remote->app_state.power++;
                if(ac_remote->app_state.power >= POWER_STATE_MAX) {
                    ac_remote->app_state.power = PowerButtonOff;
                }
                hvac_hitachi_set_power(
                    ac_remote->protocol,
                    ac_remote->app_state.temperature,
                    FAN_SPEED_LUT[ac_remote->app_state.fan],
                    MODE_LUT[ac_remote->app_state.mode],
                    VANE_LUT[ac_remote->app_state.vane],
                    POWER_LUT[ac_remote->app_state.power]);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_power,
                    POWER_ICONS[ac_remote->app_state.power][0],
                    POWER_ICONS[ac_remote->app_state.power][1]);
                power_state_changed = true;
                break;
            case button_mode:
                ac_remote->app_state.mode++;
                if(ac_remote->app_state.mode >= MODE_BUTTON_STATE_MAX) {
                    ac_remote->app_state.mode = ModeButtonHeating;
                }
                hvac_hitachi_set_mode(
                    ac_remote->protocol,
                    ac_remote->app_state.temperature,
                    FAN_SPEED_LUT[ac_remote->app_state.fan],
                    MODE_LUT[ac_remote->app_state.mode],
                    VANE_LUT[ac_remote->app_state.vane]);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_mode,
                    MODE_BUTTON_ICONS[ac_remote->app_state.mode][0],
                    MODE_BUTTON_ICONS[ac_remote->app_state.mode][1]);
                break;
            case button_fan:
                ac_remote->app_state.fan++;
                if(ac_remote->app_state.fan >= FAN_SPEED_BUTTON_STATE_MAX) {
                    ac_remote->app_state.fan = FanSpeedButtonLow;
                }
                hvac_hitachi_set_fan_speed_mode(
                    ac_remote->protocol,
                    FAN_SPEED_LUT[ac_remote->app_state.fan],
                    MODE_LUT[ac_remote->app_state.mode]);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_fan,
                    FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][0],
                    FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][1]);
                break;
            case button_vane:
                ac_remote->app_state.vane++;
                if(ac_remote->app_state.vane >= VANE_BUTTON_STATE_MAX) {
                    ac_remote->app_state.vane = VaneButtonPos0;
                }
                hvac_hitachi_set_vane(ac_remote->protocol, VANE_LUT[ac_remote->app_state.vane]);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_vane,
                    VANE_BUTTON_ICONS[ac_remote->app_state.vane][0],
                    VANE_BUTTON_ICONS[ac_remote->app_state.vane][1]);
                break;
            case button_temp_up:
                if(ac_remote->app_state.temperature < HVAC_HITACHI_TEMPERATURE_MAX) {
                    ac_remote->app_state.temperature++;
                }
                hvac_hitachi_set_temperature(
                    ac_remote->protocol, ac_remote->app_state.temperature, true);
                snprintf(buffer, sizeof(buffer), "%" PRIu32, ac_remote->app_state.temperature);
                ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
                break;
            case button_temp_down:
                if(ac_remote->app_state.temperature > HVAC_HITACHI_TEMPERATURE_MIN) {
                    ac_remote->app_state.temperature--;
                }
                hvac_hitachi_set_temperature(
                    ac_remote->protocol, ac_remote->app_state.temperature, false);
                snprintf(buffer, sizeof(buffer), "%" PRIu32, ac_remote->app_state.temperature);
                ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
                break;
            default:
                break;
            }
            if(power_state_changed || ac_remote->app_state.power == PowerButtonOn) {
                hvac_hitachi_build_samples(ac_remote->protocol);
                uint32_t event =
                    ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, 0);
                view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
            }
        }
        consumed = true;
    }
    return consumed;
}

void ac_remote_scene_hitachi_on_exit(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    ac_remote_store_settings(&ac_remote->app_state);
    hvac_hitachi_deinit(ac_remote->protocol);
    ac_remote_panel_reset(ac_remote_panel);
}
