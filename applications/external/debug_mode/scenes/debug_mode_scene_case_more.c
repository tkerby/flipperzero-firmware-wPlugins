#include "../debug_mode.h"
#include <string.h>

typedef enum {
    CaseMoreActionLinkNfc,
    CaseMoreActionExport,
    CaseMoreActionExportStudy,
    CaseMoreActionArchive,
    CaseMoreActionDelete,
} CaseMoreAction;

static void debug_mode_scene_case_more_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_case_more_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "More");
    submenu_add_item(
        app->submenu,
        "Link NFC Tag",
        CaseMoreActionLinkNfc,
        debug_mode_scene_case_more_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Export Case",
        CaseMoreActionExport,
        debug_mode_scene_case_more_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Export Case Study",
        CaseMoreActionExportStudy,
        debug_mode_scene_case_more_callback,
        app);
    if(strcmp(app->current_case_status, "Archived") != 0) {
        submenu_add_item(
            app->submenu,
            "Archive Case",
            CaseMoreActionArchive,
            debug_mode_scene_case_more_callback,
            app);
    }
    submenu_add_item(
        app->submenu,
        "Delete Case",
        CaseMoreActionDelete,
        debug_mode_scene_case_more_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_case_more_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case CaseMoreActionLinkNfc:
            app->nfc_mode = DebugModeNfcModeLink;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneNfcScan);
            break;
        case CaseMoreActionExport:
            if(case_store_export_txt(app->storage, app->current_case_id)) {
                snprintf(app->message_title, sizeof(app->message_title), "Exported");
                snprintf(
                    app->message_body,
                    sizeof(app->message_body),
                    "Report saved to the exports folder.");
            } else {
                snprintf(app->message_title, sizeof(app->message_title), "Export Failed");
                snprintf(
                    app->message_body,
                    sizeof(app->message_body),
                    "Could not write the report file.");
            }
            app->message_return_scene = DebugModeSceneCaseView;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
            break;
        case CaseMoreActionExportStudy:
            if(case_store_export_story_md(app->storage, app->current_case_id)) {
                snprintf(app->message_title, sizeof(app->message_title), "Case Study Exported");
                snprintf(
                    app->message_body,
                    sizeof(app->message_body),
                    "Markdown case study saved to exports.");
            } else {
                snprintf(app->message_title, sizeof(app->message_title), "Export Failed");
                snprintf(
                    app->message_body,
                    sizeof(app->message_body),
                    "Could not write the story file.");
            }
            app->message_return_scene = DebugModeSceneCaseView;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
            break;
        case CaseMoreActionArchive:
            app->pending_case_action = DebugModeCaseActionArchive;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseActionConfirm);
            break;
        case CaseMoreActionDelete:
            app->pending_case_action = DebugModeCaseActionDelete;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseActionConfirm);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void debug_mode_scene_case_more_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
