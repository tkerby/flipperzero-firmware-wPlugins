#include "../sigroam.h"

#include <stdio.h>

/*
 * The Settings page. Enterable in any state. This file has zero hardware side effects: it does
 * not touch the serial port, the worker, or OTG, and sends no session commands. app->settings is
 * GUI-thread exclusive, so the callbacks take no lock.
 */

static const char* const k_source_text[SR_SETTINGS_SOURCE_CHOICES] = {
    "Auto",
    "Marauder",
};

static const char* const k_off_on[2] = {"Off", "On"};
static const char* const k_backlight[2] = {"Auto", "Always"};

enum {
    SetBaud = 0,
    SetSource,
    SetSound,
    SetVibro,
    SetLight,
    SetStealth,
    SetApply,
    SetProbe,
    SetRaw,
    SetDebug,
    SetNew,
};

static uint8_t s_settings_row;

enum {
    SettingsEvProbe = 1,
    SettingsEvRaw = 2,
};

static void settings_refresh_apply(SigRoamApp* app) {
    const char* text = "now";

    if(app->settings.baud != app->settings_entry_snapshot.baud ||
       app->settings.source != app->settings_entry_snapshot.source) {
        text = "restart";
    }
    if(app->var_item_apply != NULL) {
        variable_item_set_current_value_text(app->var_item_apply, text);
    }
}

static void settings_baud_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t baud = sr_settings_baud_choice(idx);

    if(baud == 0u) {
        return;
    }
    app->settings.baud = baud;
    snprintf(app->baud_text, sizeof(app->baud_text), "%lu", (unsigned long)baud);
    variable_item_set_current_value_text(item, app->baud_text);
    settings_refresh_apply(app);
}

static void settings_source_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    SrSourceKind k = sr_settings_source_choice(idx);

    app->settings.source = k;
    if(idx < (uint8_t)SR_SETTINGS_SOURCE_CHOICES) {
        variable_item_set_current_value_text(item, k_source_text[idx]);
    }
    settings_refresh_apply(app);
}

static void settings_sound_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > 1u) {
        idx = 1u;
    }
    app->settings.sound = (idx != 0u);
    variable_item_set_current_value_text(item, k_off_on[idx]);
}

static void settings_vibro_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > 1u) {
        idx = 1u;
    }
    app->settings.vibro = (idx != 0u);
    variable_item_set_current_value_text(item, k_off_on[idx]);
}

static void settings_backlight_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > 1u) {
        idx = 1u;
    }
    app->settings.backlight_always = (idx != 0u);
    variable_item_set_current_value_text(item, k_backlight[idx]);
}

static void settings_stealth_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > 1u) {
        idx = 1u;
    }
    app->settings.stealth = (idx != 0u);
    variable_item_set_current_value_text(item, k_off_on[idx]);
}

/* A pure presentation bit: it decides only whether the Dash tab draws the two debug rows, and
 * touches neither the serial port, the worker, nor OTG.
 * It takes effect immediately (the next dash_fill reads the new value), so it does not enter the
 * "needs restart" decision in settings_refresh_apply, which compares only baud / source (:21-24). */
static void settings_debug_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > 1u) {
        idx = 1u;
    }
    app->settings.debug_rows = (idx != 0u);
    variable_item_set_current_value_text(item, k_off_on[idx]);
}

static void settings_enter(void* context, uint32_t index) {
    SigRoamApp* app = context;

    if(app == NULL) {
        return;
    }
    if(index == (uint32_t)SetProbe) {
        view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)SettingsEvProbe);
    } else if(index == (uint32_t)SetRaw) {
        view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)SettingsEvRaw);
    }
}

static void settings_newnet_changed(VariableItem* item) {
    SigRoamApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > 1u) {
        idx = 1u;
    }
    app->settings.newnet = (idx != 0u);
    variable_item_set_current_value_text(item, k_off_on[idx]);
}

void sigroam_scene_settings_on_enter(void* context) {
    SigRoamApp* app = context;
    VariableItemList* list = app->var_list;
    VariableItem* item;
    size_t idx;
    uint8_t bidx;

    app->settings_entry_snapshot = app->settings;
    variable_item_list_reset(list);

    idx = sr_settings_baud_index(app->settings.baud);
    if(idx >= (size_t)SR_SETTINGS_BAUD_CHOICES) {
        idx = 0;
    }
    item = variable_item_list_add(
        list, "Baud", (uint8_t)SR_SETTINGS_BAUD_CHOICES, settings_baud_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)idx);
    snprintf(
        app->baud_text,
        sizeof(app->baud_text),
        "%lu",
        (unsigned long)sr_settings_baud_choice(idx));
    variable_item_set_current_value_text(item, app->baud_text);

    idx = sr_settings_source_index(app->settings.source);
    if(idx >= (size_t)SR_SETTINGS_SOURCE_CHOICES) {
        /* GhostESP was CUT and Native is illegal: values outside the table are still kept in
         * settings, the display falls back to Auto, and the stored value is not rewritten until
         * the user changes Source. */
        idx = 0;
    }
    item = variable_item_list_add(
        list, "Source", (uint8_t)SR_SETTINGS_SOURCE_CHOICES, settings_source_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)idx);
    variable_item_set_current_value_text(item, k_source_text[idx]);

    bidx = app->settings.sound ? 1u : 0u;
    item = variable_item_list_add(list, "Sound", 2, settings_sound_changed, app);
    variable_item_set_current_value_index(item, bidx);
    variable_item_set_current_value_text(item, k_off_on[bidx]);

    bidx = app->settings.vibro ? 1u : 0u;
    item = variable_item_list_add(list, "Vibro", 2, settings_vibro_changed, app);
    variable_item_set_current_value_index(item, bidx);
    variable_item_set_current_value_text(item, k_off_on[bidx]);

    bidx = app->settings.backlight_always ? 1u : 0u;
    item = variable_item_list_add(list, "Backlight", 2, settings_backlight_changed, app);
    variable_item_set_current_value_index(item, bidx);
    variable_item_set_current_value_text(item, k_backlight[bidx]);

    bidx = app->settings.stealth ? 1u : 0u;
    item = variable_item_list_add(list, "Stealth", 2, settings_stealth_changed, app);
    variable_item_set_current_value_index(item, bidx);
    variable_item_set_current_value_text(item, k_off_on[bidx]);

    /* Everyday rows end at Apply. settings_refresh_apply uses the pointer, not the position. */
    app->var_item_apply = variable_item_list_add(list, "Apply", 1, NULL, app);
    variable_item_set_current_value_index(app->var_item_apply, 0);
    settings_refresh_apply(app);

    item = variable_item_list_add(list, "Probe firmware", 1, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(list, "Raw log", 1, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, ">");

    bidx = app->settings.debug_rows ? 1u : 0u;
    item = variable_item_list_add(list, "Debug rows", 2, settings_debug_changed, app);
    variable_item_set_current_value_index(item, bidx);
    variable_item_set_current_value_text(item, k_off_on[bidx]);

    bidx = app->settings.newnet ? 1u : 0u;
    item = variable_item_list_add(list, "New net tick", 2, settings_newnet_changed, app);
    variable_item_set_current_value_index(item, bidx);
    variable_item_set_current_value_text(item, k_off_on[bidx]);

    variable_item_list_set_enter_callback(list, settings_enter, app);
    if(s_settings_row > (uint8_t)SetNew) {
        s_settings_row = 0u;
    }
    variable_item_list_set_selected_item(list, s_settings_row);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewVarList);
}

bool sigroam_scene_settings_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        s_settings_row = 0u;
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }
    if(event.event == (uint32_t)SettingsEvProbe) {
        s_settings_row = (uint8_t)SetProbe;
        scene_manager_next_scene(app->scene_manager, SigRoamSceneProbe);
        return true;
    }
    if(event.event == (uint32_t)SettingsEvRaw) {
        s_settings_row = (uint8_t)SetRaw;
        scene_manager_next_scene(app->scene_manager, SigRoamSceneRaw);
        return true;
    }
    return false;
}

void sigroam_scene_settings_on_exit(void* context) {
    SigRoamApp* app = context;

    if(!sr_settings_equal(&app->settings, &app->settings_entry_snapshot)) {
        if(!sr_settings_is_valid(&app->settings)) {
            FURI_LOG_E(SR_TAG, "settings: invalid, not saved");
        } else if(!sr_settings_store_save(&app->settings)) {
            FURI_LOG_E(SR_TAG, "settings: save FAILED");
        }
    }

    variable_item_list_reset(app->var_list);
    app->var_item_apply = NULL;
}
