#include "../mizip_balance_editor.h"

static void
    mizip_balance_editor_scene_confirm_dialog_callback(DialogExResult result, void* context) {
    MiZipBalanceEditorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void mizip_balance_editor_show_current_balance(void* context) {
    MiZipBalanceEditorApp* app = context;
    FuriString* message;
    dialog_ex_reset(app->dialog_ex);
    if(app->is_valid_mizip_data) {
        //Get and show UID
        char uid[18];
        snprintf(
            uid,
            sizeof(uid),
            "UID: %02X %02X %02X %02X",
            app->mf_classic_data->block[0].data[0],
            app->mf_classic_data->block[0].data[1],
            app->mf_classic_data->block[0].data[2],
            app->mf_classic_data->block[0].data[3]);
        dialog_ex_set_header(app->dialog_ex, uid, 64, 0, AlignCenter, AlignTop);

        //Get and show current balance
        if(app->mf_classic_data->block[10].data[0] == 0x55) {
            app->previous_credit_pointer = 0x08;
            app->credit_pointer = 0x09;
        } else {
            app->previous_credit_pointer = 0x09;
            app->credit_pointer = 0x08;
        }
        app->previous_balance =
            (app->mf_classic_data->block[app->previous_credit_pointer].data[2] << 8) |
            (app->mf_classic_data->block[app->previous_credit_pointer].data[1]);
        app->current_balance = (app->mf_classic_data->block[app->credit_pointer].data[2] << 8) |
                               (app->mf_classic_data->block[app->credit_pointer].data[1]);
        char str[100];
        snprintf(
            str,
            sizeof(str),
            "Previous balance:  %d.%02d E\nCurrent balance: %d.%02d E\nPress back to write",
            app->previous_balance / 100,
            app->previous_balance % 100,
            app->current_balance / 100,
            app->current_balance % 100);

        //Init message and buttons
        message = furi_string_alloc_set(str);
        dialog_ex_set_left_button_text(app->dialog_ex, "-");
        dialog_ex_set_right_button_text(app->dialog_ex, "+");
        dialog_ex_set_center_button_text(app->dialog_ex, "Custom value");
    } else {
        message = furi_string_alloc_set("No MiZip data found");
        dialog_ex_set_center_button_text(app->dialog_ex, "Retry");
    }
    dialog_ex_set_text(
        app->dialog_ex, furi_string_get_cstr(message), 64, 29, AlignCenter, AlignCenter);
    dialog_ex_set_result_callback(
        app->dialog_ex, mizip_balance_editor_scene_confirm_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    FURI_LOG_T("RESULT", "Result showed");
}

void mizip_balance_editor_scene_show_result_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    mizip_balance_editor_show_current_balance(context);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdShowResult);
}

bool mizip_balance_editor_scene_show_result_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DialogExResultCenter:
            if(app->is_valid_mizip_data) {
                //Open NumberInput for custom value
                scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdNumberInput);
            } else {
                //Get back if data isn't valid
                scene_manager_previous_scene(app->scene_manager);
            }
            consumed = true;
            break;
        case DialogExResultLeft:
            //Remove 100 cents from balance
            mizip_balance_editor_write_new_balance(context, app->current_balance - 100);
            mizip_balance_editor_show_current_balance(context);
            consumed = true;
            break;
        case DialogExResultRight:
            //Add 100 cents from balance
            mizip_balance_editor_write_new_balance(context, app->current_balance + 100);
            mizip_balance_editor_show_current_balance(context);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void mizip_balance_editor_scene_show_result_on_exit(void* context) {
    UNUSED(context);
}
