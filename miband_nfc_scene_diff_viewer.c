/**
 * @file miband_nfc_scene_diff_viewer.c
 * @brief Detailed difference viewer - FIXED
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

typedef struct {
    size_t block_index;
    uint8_t expected[16];
    uint8_t found[16];
    uint8_t diff_mask[16];
    uint8_t diff_count;
} BlockDifference;

static void format_hex_with_diff(
    FuriString* output,
    const uint8_t* data,
    const uint8_t* mask,
    size_t length) {
    for(size_t i = 0; i < length; i++) {
        if(mask && mask[i]) {
            furi_string_cat_printf(output, "[%02X]", data[i]);
        } else {
            furi_string_cat_printf(output, " %02X ", data[i]);
        }
        if((i + 1) % 8 == 0 && i < length - 1) {
            furi_string_cat_str(output, "\n     ");
        }
    }
}

static FuriString*
    generate_difference_report(MiBandNfcApp* app, BlockDifference* differences, size_t diff_count) {
    FuriString* report = furi_string_alloc();
    if(!report) return NULL;

    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    furi_string_printf(
        report,
        "Verification Results\n"
        "====================\n\n"
        "Total Blocks: %zu\n"
        "Blocks OK: %zu\n"
        "Blocks DIFFER: %zu\n\n",
        total_blocks,
        total_blocks - diff_count,
        diff_count);

    if(diff_count == 0) {
        furi_string_cat_str(report, "All blocks match!\n\n");
        furi_string_cat_str(report, "Press Back to exit");
        return report;
    }

    furi_string_cat_str(report, "Differences:\n");
    furi_string_cat_str(report, "============\n\n");

    for(size_t i = 0; i < diff_count && i < 10; i++) {
        BlockDifference* diff = &differences[i];

        const char* block_type = "Data";
        if(diff->block_index == 0) {
            block_type = "UID";
        } else if(mf_classic_is_sector_trailer(diff->block_index)) {
            block_type = "Trailer";
        }

        furi_string_cat_printf(
            report,
            "Block %zu (%s)\n%u bytes differ\n",
            diff->block_index,
            block_type,
            diff->diff_count);

        furi_string_cat_str(report, "Exp: ");
        format_hex_with_diff(report, diff->expected, diff->diff_mask, 16);
        furi_string_cat_str(report, "\n");

        furi_string_cat_str(report, "Got: ");
        format_hex_with_diff(report, diff->found, diff->diff_mask, 16);
        furi_string_cat_str(report, "\n\n");
    }

    if(diff_count > 10) {
        furi_string_cat_printf(report, "... %zu more\n\n", diff_count - 10);
    }

    furi_string_cat_str(report, "[XX]=diff  XX=same\n\nPress Back");
    return report;
}

void miband_nfc_scene_diff_viewer_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    FURI_LOG_I(TAG, "DiffViewer: Starting");

    // RESET FORZATO di tutte le altre view PRIMA
    popup_reset(app->popup);
    dialog_ex_reset(app->dialog_ex);

    if(!app->is_valid_nfc_data) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    // 1. GENERA TUTTO IL TESTO PRIMA
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);
    BlockDifference* differences = malloc(sizeof(BlockDifference) * total_blocks);

    if(!differences) {
        FURI_LOG_E(TAG, "Alloc failed");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    size_t diff_count = 0;

    // Trova differenze
    for(size_t i = 0; i < total_blocks; i++) {
        if(i == 0 || mf_classic_is_sector_trailer(i)) continue;

        bool blocks_differ = false;
        uint8_t diff_mask[16] = {0};
        uint8_t diff_bytes = 0;

        for(size_t j = 0; j < 16; j++) {
            if(app->mf_classic_data->block[i].data[j] != app->target_data->block[i].data[j]) {
                blocks_differ = true;
                diff_mask[j] = 1;
                diff_bytes++;
            }
        }

        if(blocks_differ && diff_count < total_blocks) {
            differences[diff_count].block_index = i;
            differences[diff_count].diff_count = diff_bytes;
            memcpy(differences[diff_count].expected, app->mf_classic_data->block[i].data, 16);
            memcpy(differences[diff_count].found, app->target_data->block[i].data, 16);
            memcpy(differences[diff_count].diff_mask, diff_mask, 16);
            diff_count++;
        }
    }

    FURI_LOG_I(TAG, "Found %zu differences", diff_count);

    // 2. GENERA IL REPORT COMPLETO
    FuriString* report = generate_difference_report(app, differences, diff_count);
    free(differences);

    if(!report) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(app->diff_viewer_text) {
        free(app->diff_viewer_text);
    }

    app->diff_viewer_text = malloc(furi_string_size(report) + 1);
    if(!app->diff_viewer_text) {
        furi_string_free(report);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    strcpy(app->diff_viewer_text, furi_string_get_cstr(report));
    furi_string_free(report);

    // DOPPIO RESET della text_box
    text_box_reset(app->text_box_report);
    furi_delay_ms(10); // Dai tempo alla view
    text_box_reset(app->text_box_report);

    text_box_set_text(app->text_box_report, app->diff_viewer_text);
    text_box_set_font(app->text_box_report, TextBoxFontText);
    text_box_set_focus(app->text_box_report, TextBoxFocusStart);

    FURI_LOG_I(TAG, "Text set, switching view");
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdUidReport);
}

bool miband_nfc_scene_diff_viewer_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return true;
    }
    return false;
}

void miband_nfc_scene_diff_viewer_on_exit(void* context) {
    MiBandNfcApp* app = context;
    text_box_reset(app->text_box_report);
}
