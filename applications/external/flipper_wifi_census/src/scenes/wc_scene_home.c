#include "include/domain/wc_version_info.h"
#include "include/scenes/wc_scenes_common.h"

// Persist the user options after any change, so the next run of the app starts where this one
// left off instead of silently reverting to the defaults.
static void wc_settings_persist(WcApp *app) {
    WcSettings s = {.baud = app->baud, .autosave = app->autosave};
    wc_settings_service_save(&app->store, &s);
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

typedef enum {
    StartScan,
    StartFiles,
    StartCompare,
    StartMerge,
    StartImport,
    StartKnown,
    StartSettings,
    StartSerialDebug,
    StartAbout,
} StartItem;

static void start_populate(WcApp *app);

// Auto-save rides on the Scan row itself: it is the one option you decide in the moment, so it
// sits where the decision is taken rather than in a menu you have to remember to visit.
static void autosave_nudge(void *context, uint32_t index, int8_t delta) {
    UNUSED(index);
    UNUSED(delta); // two states: either direction flips it
    WcApp *app = context;
    app->autosave = !app->autosave;
    wc_settings_persist(app);
    start_populate(app);
}

static void start_populate(WcApp *app) {
    snprintf(app->menu_autosave_label, sizeof(app->menu_autosave_label), "Auto-save: %s",
             app->autosave ? "On" : "Off");
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "WiFi Census");
    wc_scroll_list_add_item(app->list_view, "Scan", StartScan, wc_list_cb, app);
    wc_scroll_list_set_item_value(app->list_view, StartScan, app->menu_autosave_label,
                                  autosave_nudge, app);
    wc_scroll_list_add_item(app->list_view, "Files", StartFiles, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Compare", StartCompare, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Merge", StartMerge, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Import pcap", StartImport, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Known devices", StartKnown, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Settings", StartSettings, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Serial debug", StartSerialDebug, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "About", StartAbout, wc_list_cb, app);
}

void wc_scene_start_on_enter(void *context) {
    WcApp *app = context;
    start_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_start_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type != SceneManagerEventTypeCustom) {
        return false;
    }
    switch (event.event) {
        case StartScan:
            scene_manager_next_scene(app->scene_manager, WcSceneScan);
            return true;
        case StartFiles:
            scene_manager_next_scene(app->scene_manager, WcSceneFiles);
            return true;
        case StartCompare:
            scene_manager_next_scene(app->scene_manager, WcSceneCompareA);
            return true;
        case StartMerge:
            scene_manager_next_scene(app->scene_manager, WcSceneMergeA);
            return true;
        case StartImport:
            scene_manager_next_scene(app->scene_manager, WcSceneImportPick);
            return true;
        case StartKnown:
            scene_manager_next_scene(app->scene_manager, WcSceneKnown);
            return true;
        case StartSettings:
            scene_manager_next_scene(app->scene_manager, WcSceneSettings);
            return true;
        case StartSerialDebug:
            scene_manager_next_scene(app->scene_manager, WcSceneSerialDebug);
            return true;
        case StartAbout:
            scene_manager_next_scene(app->scene_manager, WcSceneAbout);
            return true;
        default:
            return false;
    }
}

void wc_scene_start_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Settings (UART baud)
// ---------------------------------------------------------------------------

static const char *const k_baud_names[] = {"115200", "230400"};
static const uint32_t k_baud_values[] = {115200, 230400};

static void settings_baud_changed(VariableItem *item) {
    WcApp *app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, k_baud_names[idx]);
    app->baud = k_baud_values[idx];
}

static const char *const k_onoff[] = {"Off", "On"};

static void settings_autosave_changed(VariableItem *item) {
    WcApp *app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, k_onoff[idx]);
    app->autosave = (idx == 1);
}

void wc_scene_settings_on_enter(void *context) {
    WcApp *app = context;
    variable_item_list_reset(app->var_item_list);
    VariableItem *item =
        variable_item_list_add(app->var_item_list, "UART baud", 2, settings_baud_changed, app);
    uint8_t idx = (app->baud == k_baud_values[1]) ? 1 : 0;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, k_baud_names[idx]);

    VariableItem *as = variable_item_list_add(app->var_item_list, "Auto-save (rotate)", 2,
                                              settings_autosave_changed, app);
    uint8_t as_idx = app->autosave ? 1 : 0;
    variable_item_set_current_value_index(as, as_idx);
    variable_item_set_current_value_text(as, k_onoff[as_idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewVarList);
}

bool wc_scene_settings_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_settings_on_exit(void *context) {
    WcApp *app = context;
    wc_settings_persist(app);
    variable_item_list_reset(app->var_item_list);
}

// ---------------------------------------------------------------------------
// About
// ---------------------------------------------------------------------------

void wc_scene_about_on_enter(void *context) {
    WcApp *app = context;
    snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
             "WiFi Census\nv%s  by Endika\ngithub.com/Endika/flipper-wifi-census\n\n"
             "Passive 2.4GHz device census via an ESP32 Marauder board. Counts and types nearby "
             "devices, saves each scan, and compares captures across places.\n\n"
             "Honest limits: stable-MAC gear and devices probing a named network cross reliably; "
             "modern phones randomize their MAC and are counted but not crossable (use Known "
             "devices).\n\nPassive only. Everything stays on the SD card.\n\n"
             // The free heap decides how far the ceiling can move: a scan reserves the whole
             // census up front, and captures are streamed, so nothing else of that size is
             // held alongside it.
             "Ceiling: %u devices\nFree heap now: %u B\nLowest seen: %u B",
             wc_version(), (unsigned)WC_CENSUS_MAX_DEVICES, (unsigned)memmgr_get_free_heap(),
             (unsigned)memmgr_get_minimum_free_heap());
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);
}

bool wc_scene_about_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_about_on_exit(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
}
