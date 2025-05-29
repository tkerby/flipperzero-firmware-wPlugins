#include "../ac_remote_app_i.h"

#include <inttypes.h>

typedef enum {
    button_power,
    button_mode,
    button_temp_up,
    button_fan,
    button_temp_down,
    button_vane,
    button_view_sub,
    label_temperature,

    button_timer_on_h_inc,
    button_timer_on_m_inc,
    button_timer_off_h_inc,
    button_timer_off_m_inc,
    label_timer_on_h,
    label_timer_on_m,
    label_timer_off_h,
    label_timer_off_m,
    button_timer_on_h_dec,
    button_timer_on_m_dec,
    button_timer_off_h_dec,
    button_timer_off_m_dec,
    button_timer_set,
    button_timer_reset,
    button_reset_filter,
    button_settings,
    button_view_main,
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

// Exactly 4095 minutes
#define TIMER_MAX_TIME_H (68u)
#define TIMER_MAX_TIME_M (15u)

#define TIMER_MAX_MIN (59u)

#define BUTTON_TIMER_X (4)
#define BUTTON_TIMER_Y (2)

typedef struct {
    const uint16_t x;
    const uint16_t y;
    const button_id button;
    const Icon* icon;
    const Icon* icon_hover;
} TimerButtonLayout;

static const TimerButtonLayout BUTTON_TIMER_LAYOUT[BUTTON_TIMER_Y][BUTTON_TIMER_X] = {
    [0][0] =
        {
            .x = 2,
            .y = 12,
            .button = button_timer_on_h_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [0][1] =
        {
            .x = 16,
            .y = 12,
            .button = button_timer_on_m_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [0][2] =
        {
            .x = 33,
            .y = 12,
            .button = button_timer_off_h_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [0][3] =
        {
            .x = 47,
            .y = 12,
            .button = button_timer_off_m_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [1][0] =
        {
            .x = 2,
            .y = 30,
            .button = button_timer_on_h_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
    [1][1] =
        {
            .x = 16,
            .y = 30,
            .button = button_timer_on_m_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
    [1][2] =
        {
            .x = 33,
            .y = 30,
            .button = button_timer_off_h_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
    [1][3] =
        {
            .x = 47,
            .y = 30,
            .button = button_timer_off_m_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
};

static bool ac_remote_load_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    uint32_t version = 0;
    bool success = false;
    do {
        uint32_t tmp = 0;
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
        if(!flipper_format_read_uint32(ff, "TimerOn", &tmp, 1)) {
            tmp = 0;
        }
        if(tmp > 0xfff) {
            tmp = 0;
        }
        app_state->timer_on_h = tmp / 60;
        app_state->timer_on_m = tmp % 60;
        if(!flipper_format_read_uint32(ff, "TimerOff", &tmp, 1)) {
            tmp = 0;
        }
        if(tmp > 0xfff) {
            tmp = 0;
        }
        app_state->timer_off_h = tmp / 60;
        app_state->timer_off_m = tmp % 60;
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
        uint32_t tmp = 0;
        if(!flipper_format_file_open_always(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_write_header_cstr(ff, "AC Remote", 1)) break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Power", &app_state->power, 1)) break;
        if(!flipper_format_write_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(!flipper_format_write_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(!flipper_format_write_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(!flipper_format_write_uint32(ff, "Vane", &app_state->vane, 1)) break;
        tmp = app_state->timer_on_h * 60 + app_state->timer_on_m;
        if(!flipper_format_write_uint32(ff, "TimerOn", &tmp, 1)) break;
        tmp = app_state->timer_off_h * 60 + app_state->timer_off_m;
        if(!flipper_format_write_uint32(ff, "TimerOff", &tmp, 1)) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    flipper_format_free(ff);
    return success;
}

static uint32_t inc_hour(const uint32_t hour, const uint32_t minute) {
    if(hour >= TIMER_MAX_TIME_H || (hour >= TIMER_MAX_TIME_H - 1 && minute > TIMER_MAX_TIME_M)) {
        return 0;
    }
    return hour + 1;
}

static uint32_t dec_hour(const uint32_t hour, const uint32_t minute) {
    if(hour > TIMER_MAX_TIME_H) {
        return 0;
    }
    if(hour == 0) {
        if(minute > TIMER_MAX_TIME_M) {
            return TIMER_MAX_TIME_H - 1;
        }
        return TIMER_MAX_TIME_H;
    }
    return hour - 1;
}

static uint32_t inc_minute(const uint32_t hour, const uint32_t minute) {
    if(minute >= TIMER_MAX_MIN) {
        return 0;
    } else if(hour >= TIMER_MAX_TIME_H && minute >= TIMER_MAX_TIME_M) {
        return 0;
    }
    return minute + 1;
}

static uint32_t dec_minute(const uint32_t hour, const uint32_t minute) {
    if(minute > TIMER_MAX_MIN) {
        return 0;
    }
    if(minute == 0) {
        if(hour >= TIMER_MAX_TIME_H) {
            return TIMER_MAX_TIME_M;
        }
        return TIMER_MAX_MIN;
    }
    return minute - 1;
}

void ac_remote_scene_universal_common_item_callback(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonSelected, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

void ac_remote_scene_hitachi_on_enter(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* panel_main = ac_remote->panel_main;
    ACRemotePanel* panel_sub = ac_remote->panel_sub;
    ac_remote->protocol = hvac_hitachi_init();

    if(!ac_remote_load_settings(&ac_remote->app_state)) {
        ac_remote->app_state.power = PowerButtonOff;
        ac_remote->app_state.mode = ModeButtonCooling;
        ac_remote->app_state.fan = FanSpeedButtonLow;
        ac_remote->app_state.vane = VaneButtonPos0;
        ac_remote->app_state.temperature = 23;
        ac_remote->app_state.timer_on_h = 0;
        ac_remote->app_state.timer_on_m = 0;
        ac_remote->app_state.timer_off_h = 0;
        ac_remote->app_state.timer_off_m = 0;
    }

    ac_remote_panel_reserve(panel_main, 2, 4);

    ac_remote_panel_add_item(
        panel_main,
        button_power,
        0,
        0,
        6,
        17,
        POWER_ICONS[ac_remote->app_state.power][0],
        POWER_ICONS[ac_remote->app_state.power][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 5, 39, &I_power_text_21x5);
    ac_remote_panel_add_item(
        panel_main,
        button_mode,
        1,
        0,
        39,
        17,
        MODE_BUTTON_ICONS[ac_remote->app_state.mode][0],
        MODE_BUTTON_ICONS[ac_remote->app_state.mode][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 40, 39, &I_mode_text_17x5);
    ac_remote_panel_add_icon(panel_main, 0, 59, &I_frame_30x39);
    ac_remote_panel_add_item(
        panel_main,
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
        panel_main,
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
        panel_main,
        button_fan,
        1,
        1,
        39,
        50,
        FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][0],
        FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 43, 72, &I_fan_text_11x5);
    ac_remote_panel_add_item(
        panel_main,
        button_vane,
        1,
        2,
        39,
        83,
        VANE_BUTTON_ICONS[ac_remote->app_state.vane][0],
        VANE_BUTTON_ICONS[ac_remote->app_state.vane][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 37, 105, &I_louver_text_23x5);
    ac_remote_panel_add_item(
        panel_main,
        button_view_sub,
        0,
        3,
        6,
        116,
        &I_timer_52x10,
        &I_timer_hover_52x10,
        ac_remote_scene_universal_common_item_callback,
        context);

    ac_remote_panel_add_label(panel_main, 0, 6, 11, FontPrimary, "AC remote");

    snprintf(
        ac_remote->label_string_pool[LabelStringTemp],
        LABEL_STRING_SIZE,
        "%" PRIu32,
        ac_remote->app_state.temperature);
    ac_remote_panel_add_label(
        panel_main,
        label_temperature,
        4,
        82,
        FontKeyboard,
        ac_remote->label_string_pool[LabelStringTemp]);

    view_set_orientation(ac_remote_panel_get_view(panel_main), ViewOrientationVertical);

    ac_remote_panel_reserve(panel_sub, 4, 5);

    // Timer control
    ac_remote_panel_add_icon(panel_sub, 0, 4, &I_timer_frame_64x73);
    for(uint16_t yy = 0; yy < BUTTON_TIMER_Y; yy++) {
        for(uint16_t xx = 0; xx < BUTTON_TIMER_X; xx++) {
            const TimerButtonLayout* layout = &BUTTON_TIMER_LAYOUT[yy][xx];
            ac_remote_panel_add_item(
                panel_sub,
                layout->button,
                xx,
                yy,
                layout->x,
                layout->y,
                layout->icon,
                layout->icon_hover,
                ac_remote_scene_universal_common_item_callback,
                context);
        }
    }

    snprintf(
        ac_remote->label_string_pool[LabelStringTimerOnHour],
        LABEL_STRING_SIZE,
        "%02" PRIu32,
        ac_remote->app_state.timer_on_h);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_on_h,
        4,
        29,
        FontKeyboard,
        ac_remote->label_string_pool[LabelStringTimerOnHour]);

    snprintf(
        ac_remote->label_string_pool[LabelStringTimerOnMinute],
        LABEL_STRING_SIZE,
        "%02" PRIu32,
        ac_remote->app_state.timer_on_m);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_on_m,
        18,
        29,
        FontKeyboard,
        ac_remote->label_string_pool[LabelStringTimerOnMinute]);

    snprintf(
        ac_remote->label_string_pool[LabelStringTimerOffHour],
        LABEL_STRING_SIZE,
        "%02" PRIu32,
        ac_remote->app_state.timer_off_h);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_off_h,
        35,
        29,
        FontKeyboard,
        ac_remote->label_string_pool[LabelStringTimerOffHour]);

    snprintf(
        ac_remote->label_string_pool[LabelStringTimerOffMinute],
        LABEL_STRING_SIZE,
        "%02" PRIu32,
        ac_remote->app_state.timer_off_m);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_off_m,
        49,
        29,
        FontKeyboard,
        ac_remote->label_string_pool[LabelStringTimerOffMinute]);

    ac_remote_panel_add_item(
        panel_sub,
        button_timer_set,
        0,
        2,
        7,
        50,
        &I_timer_set_19x20,
        &I_timer_set_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_item(
        panel_sub,
        button_timer_reset,
        1,
        2,
        38,
        50,
        &I_timer_reset_19x20,
        &I_timer_reset_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);

    // Misc buttons
    ac_remote_panel_add_item(
        panel_sub,
        button_reset_filter,
        0,
        3,
        7,
        82,
        &I_reset_filter_19x20,
        &I_reset_filter_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_sub, 5, 105, &I_reset_filter_text_23x5);

    ac_remote_panel_add_item(
        panel_sub,
        button_settings,
        1,
        3,
        38,
        82,
        &I_settings_19x20,
        &I_settings_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_sub, 36, 105, &I_settings_text_23x5);

    // Back button
    ac_remote_panel_add_item(
        panel_sub,
        button_view_main,
        0,
        4,
        6,
        116,
        &I_back_52x10,
        &I_back_hover_52x10,
        ac_remote_scene_universal_common_item_callback,
        context);

    view_set_orientation(ac_remote_panel_get_view(panel_sub), ViewOrientationVertical);

    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewMain);
}

bool ac_remote_scene_hitachi_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    SceneManager* scene_manager = ac_remote->scene_manager;
    ACRemotePanel* panel_main = ac_remote->panel_main;
    ACRemotePanel* panel_sub = ac_remote->panel_sub;
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
            bool send_on_power_off = false, has_ir_code = true;
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
                    panel_main,
                    button_power,
                    POWER_ICONS[ac_remote->app_state.power][0],
                    POWER_ICONS[ac_remote->app_state.power][1]);
                send_on_power_off = true;
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
                    panel_main,
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
                    panel_main,
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
                    panel_main,
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
                snprintf(
                    ac_remote->label_string_pool[LabelStringTemp],
                    LABEL_STRING_SIZE,
                    "%" PRIu32,
                    ac_remote->app_state.temperature);
                ac_remote_panel_update_view(panel_main);
                break;
            case button_temp_down:
                if(ac_remote->app_state.temperature > HVAC_HITACHI_TEMPERATURE_MIN) {
                    ac_remote->app_state.temperature--;
                }
                hvac_hitachi_set_temperature(
                    ac_remote->protocol, ac_remote->app_state.temperature, false);
                snprintf(
                    ac_remote->label_string_pool[LabelStringTemp],
                    LABEL_STRING_SIZE,
                    "%" PRIu32,
                    ac_remote->app_state.temperature);
                ac_remote_panel_update_view(panel_main);
                break;
            case button_view_sub:
                view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewSub);
                has_ir_code = false;
                break;
            case button_view_main:
                view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewMain);
                has_ir_code = false;
                break;
            case button_timer_on_h_inc:
            case button_timer_on_h_dec:
                ac_remote->app_state.timer_on_h =
                    (event_value == button_timer_on_h_inc) ?
                        inc_hour(
                            ac_remote->app_state.timer_on_h, ac_remote->app_state.timer_on_m) :
                        dec_hour(ac_remote->app_state.timer_on_h, ac_remote->app_state.timer_on_m);
                snprintf(
                    ac_remote->label_string_pool[LabelStringTimerOnHour],
                    LABEL_STRING_SIZE,
                    "%02" PRIu32,
                    ac_remote->app_state.timer_on_h);
                ac_remote_panel_update_view(panel_sub);
                has_ir_code = false;
                break;
            case button_timer_on_m_inc:
            case button_timer_on_m_dec:
                ac_remote->app_state.timer_on_m =
                    (event_value == button_timer_on_m_inc) ?
                        inc_minute(
                            ac_remote->app_state.timer_on_h, ac_remote->app_state.timer_on_m) :
                        dec_minute(
                            ac_remote->app_state.timer_on_h, ac_remote->app_state.timer_on_m);
                snprintf(
                    ac_remote->label_string_pool[LabelStringTimerOnMinute],
                    LABEL_STRING_SIZE,
                    "%02" PRIu32,
                    ac_remote->app_state.timer_on_m);
                ac_remote_panel_update_view(panel_sub);
                has_ir_code = false;
                break;
            case button_timer_off_h_inc:
            case button_timer_off_h_dec:
                ac_remote->app_state.timer_off_h =
                    (event_value == button_timer_off_h_inc) ?
                        inc_hour(
                            ac_remote->app_state.timer_off_h, ac_remote->app_state.timer_off_m) :
                        dec_hour(
                            ac_remote->app_state.timer_off_h, ac_remote->app_state.timer_off_m);
                snprintf(
                    ac_remote->label_string_pool[LabelStringTimerOffHour],
                    LABEL_STRING_SIZE,
                    "%02" PRIu32,
                    ac_remote->app_state.timer_off_h);
                ac_remote_panel_update_view(panel_sub);
                has_ir_code = false;
                break;
            case button_timer_off_m_inc:
            case button_timer_off_m_dec:
                ac_remote->app_state.timer_off_m =
                    (event_value == button_timer_off_m_inc) ?
                        inc_minute(
                            ac_remote->app_state.timer_off_h, ac_remote->app_state.timer_off_m) :
                        dec_minute(
                            ac_remote->app_state.timer_off_h, ac_remote->app_state.timer_off_m);
                snprintf(
                    ac_remote->label_string_pool[LabelStringTimerOffMinute],
                    LABEL_STRING_SIZE,
                    "%02" PRIu32,
                    ac_remote->app_state.timer_off_m);
                ac_remote_panel_update_view(panel_sub);
                has_ir_code = false;
                break;
            case button_reset_filter:
                hvac_hitachi_reset_filter(ac_remote->protocol);
                send_on_power_off = true;
                break;
            default:
                has_ir_code = false;
                break;
            }
            if(has_ir_code && (send_on_power_off || ac_remote->app_state.power == PowerButtonOn)) {
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
    ACRemotePanel* panel_main = ac_remote->panel_main;
    ACRemotePanel* panel_sub = ac_remote->panel_sub;
    ac_remote_store_settings(&ac_remote->app_state);
    hvac_hitachi_deinit(ac_remote->protocol);
    ac_remote_panel_reset(panel_main);
    ac_remote_panel_reset(panel_sub);
}
