#include "../passy_i.h"
#include <dolphin/dolphin.h>

#define ASN_EMIT_DEBUG 0
#include <lib/asn1/DG1.h>

#define TAG "PassySceneReadCardSuccess"

void passy_scene_read_success_on_enter(void* context) {
    Passy* passy = context;

    dolphin_deed(DolphinDeedNfcReadSuccess);
    notification_message(passy->notifications, &sequence_success);

    furi_string_reset(passy->text_box_store);
    FuriString* str = passy->text_box_store;
    if(passy->read_type == PassyReadDG1) {
        DG1_t* dg1 = 0;
        dg1 = calloc(1, sizeof *dg1);
        assert(dg1);
        asn_dec_rval_t rval = asn_decode(
            0,
            ATS_DER,
            &asn_DEF_DG1,
            (void**)&dg1,
            bit_buffer_get_data(passy->DG1),
            bit_buffer_get_size_bytes(passy->DG1));

        if(rval.code == RC_OK) {
            FURI_LOG_I(TAG, "ASN.1 decode success");

            char payloadDebug[384] = {0};
            memset(payloadDebug, 0, sizeof(payloadDebug));
            (&asn_DEF_DG1)
                ->op->print_struct(&asn_DEF_DG1, dg1, 1, print_struct_callback, payloadDebug);
            if(strlen(payloadDebug) > 0) {
                FURI_LOG_D(TAG, "DG1: %s", payloadDebug);
            } else {
                FURI_LOG_D(TAG, "Received empty Payload");
            }

            furi_string_cat_printf(str, "%s\n", dg1->mrz.buf);
        } else {
            FURI_LOG_E(TAG, "ASN.1 decode failed: %d.  %d consumed", rval.code, rval.consumed);
            furi_string_cat_printf(str, "%s\n", bit_buffer_get_data(passy->DG1));
        }

        free(dg1);
        dg1 = 0;

    } else if(passy->read_type == PassyReadDG2) {
        furi_string_cat_printf(str, "Saved to disk");
    }
    text_box_set_font(passy->text_box, TextBoxFontText);
    text_box_set_text(passy->text_box, furi_string_get_cstr(passy->text_box_store));
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewTextBox);
}

bool passy_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            passy->scene_manager, PassySceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void passy_scene_read_success_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    text_box_reset(passy->text_box);
}
