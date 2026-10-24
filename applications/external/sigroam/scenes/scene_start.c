#include "../sigroam.h"

/*
 * Home is the outing path. Dashboard stays first: Back from a running scan returns
 * here without stopping (Stop is OK on the Dash tab).
 * Probe firmware and Raw log are under Settings. Wi-Fi is saved once and opens
 * from Upload. About stays last.
 * Enum values match display order. set_selected_item matches the index field.
 */
typedef enum {
    SigRoamStartItemDash = 0,
    SigRoamStartItemUpload,
    SigRoamStartItemRank,
    SigRoamStartItemTools,
    SigRoamStartItemSettings,
    SigRoamStartItemAbout,
} SigRoamStartItem;

/* Submenu callback only enqueues. No lock, no scene switch here. */
static void sigroam_start_submenu_callback(void* context, uint32_t index) {
    SigRoamApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sigroam_scene_start_on_enter(void* context) {
    SigRoamApp* app = context;

    submenu_reset(app->submenu);
    /*
     * The header is always the product name (user decision, 2026-08-31). Two reasons:
     *  (1) The main menu is a navigation page, not a status page -- serial status appears both
     *      on the Dash tab (hint + the rx= counter) and on the Probe page (full explanation,
     *      including "unplug USB, then exit and reopen"), so nothing is lost;
     *  (2) The Catalog hero image must be taken with qFlipper's built-in screenshot -> USB must
     *      be plugged in -> with USB plugged in OTG necessarily fails closed (V-053) -> the old
     *      code would put `SR [5V OFF]` in the hero image's header bar, which reads like an
     *      error; and the `rx=N d=N` the old code showed while io was open is a debug string,
     *      equally unsuitable.
     * sigroam_start_header() was removed along with it: its first branch `return "SigRoam"` was
     * **dead code** -- its only call site sat in the else branch where io is not open, where
     * sr_io_is_open() is necessarily false, so the main menu header had never once displayed
     * the product name.
     */
    submenu_set_header(app->submenu, "SigRoam Wardriving");
    submenu_add_item(
        app->submenu, "Dashboard", SigRoamStartItemDash, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Upload", SigRoamStartItemUpload, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Rank", SigRoamStartItemRank, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Board tools", SigRoamStartItemTools, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", SigRoamStartItemSettings, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", SigRoamStartItemAbout, sigroam_start_submenu_callback, app);

    /* Must be called after every add_item: it walks items looking for a matching index, and
     * finds nothing on an empty list. */
    submenu_set_selected_item(app->submenu, (uint32_t)app->start_selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewSubmenu);
}

bool sigroam_scene_start_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        /* Remember this selection so the cursor stays put on returning to Start (saving
         * repeated down presses).
         * Range guard: this scene's custom events only come from the submenu callback, but do
         * not take that on trust -- out of range is simply not recorded.
         * app->start_selected is GUI-thread exclusive (the exclusive-field rule at
         * sigroam.h:90-96), so no lock is needed. */
        if(event.event <= (uint32_t)SigRoamStartItemAbout) {
            app->start_selected = (uint8_t)event.event;
        }
        if(event.event == SigRoamStartItemAbout) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneAbout);
            return true;
        }
        if(event.event == SigRoamStartItemDash) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneDash);
            return true;
        }
        if(event.event == SigRoamStartItemUpload) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneUpload);
            return true;
        }
        if(event.event == SigRoamStartItemTools) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneTools);
            return true;
        }
        if(event.event == SigRoamStartItemRank) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneRank);
            return true;
        }
        if(event.event == SigRoamStartItemSettings) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneSettings);
            return true;
        }
    }

    /* Back not consumed: no previous scene → dispatcher exits. */
    return false;
}

void sigroam_scene_start_on_exit(void* context) {
    SigRoamApp* app = context;
    submenu_reset(app->submenu);
}
