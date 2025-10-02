/**
 * @file miband_nfc_scene_diff_viewer.c
 * @brief Detailed difference viewer for verify results
 * 
 * This scene displays detailed information about blocks that differ
 * between the expected dump and the actual Mi Band data, showing
 * hex values and highlighting exact byte differences.
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

/**
 * @brief Structure to store block difference information
 */
typedef struct {
    size_t block_index;
    uint8_t expected[16];
    uint8_t found[16];
    uint8_t diff_mask[16]; // 1 = different, 0 = same
    uint8_t diff_count;
} BlockDifference;

/**
 * @brief Format hex string with differences highlighted
 * 
 * @param output Output string buffer
 * @param data Data bytes
 * @param mask Difference mask
 * @param length Number of bytes
 */
static void format_hex_with_diff(
    FuriString* output,
    const uint8_t* data,
    const uint8_t* mask,
    size_t length) {
    for(size_t i = 0; i < length; i++) {
        if(mask && mask[i]) {
            furi_string_cat_printf(output, "[%02X]", data[i]); // Different
        } else {
            furi_string_cat_printf(output, " %02X ", data[i]); // Same
        }
        if((i + 1) % 8 == 0 && i < length - 1) {
            furi_string_cat_str(output, "\n     ");
        }
    }
}

/**
 * @brief Generate detailed difference report
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @param differences Array of block differences
 * @param diff_count Number of differences
 * @return Allocated FuriString with report text
 */
static FuriString*
    generate_difference_report(MiBandNfcApp* app, BlockDifference* differences, size_t diff_count) {
    FuriString* report = furi_string_alloc();
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    // Header
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
        furi_string_cat_str(report, "All blocks match perfectly!\n\n");
        return report;
    }

    furi_string_cat_str(report, "Differences Found:\n");
    furi_string_cat_str(report, "==================\n\n");

    // Detail each difference
    for(size_t i = 0; i < diff_count; i++) {
        BlockDifference* diff = &differences[i];

        // Block header
        const char* block_type = "Data";
        if(diff->block_index == 0) {
            block_type = "UID";
        } else if(mf_classic_is_sector_trailer(diff->block_index)) {
            block_type = "Trailer";
        }

        furi_string_cat_printf(
            report,
            "Block %zu (%s): %u bytes differ\n",
            diff->block_index,
            block_type,
            diff->diff_count);

        // Expected data
        furi_string_cat_str(report, "Exp: ");
        format_hex_with_diff(report, diff->expected, diff->diff_mask, 16);
        furi_string_cat_str(report, "\n");

        // Found data
        furi_string_cat_str(report, "Got: ");
        format_hex_with_diff(report, diff->found, diff->diff_mask, 16);
        furi_string_cat_str(report, "\n\n");

        // Limit report length
        if(i >= 9 && diff_count > 10) {
            furi_string_cat_printf(report, "... and %zu more differences\n\n", diff_count - i - 1);
            break;
        }
    }

    // Summary
    furi_string_cat_str(
        report,
        "Legend: [XX] = different\n"
        "         XX  = same\n\n"
        "Press Back to exit");

    return report;
}

/**
 * @brief Scene entry point
 * 
 * Expects app->target_data to contain read data and app->mf_classic_data
 * to contain expected data. Compares and generates detailed report.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_diff_viewer_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(!app->is_valid_nfc_data) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    BlockDifference* differences = malloc(sizeof(BlockDifference) * total_blocks);
    size_t diff_count = 0;

    // Compara tutti i blocchi
    for(size_t i = 0; i < total_blocks; i++) {
        bool blocks_differ = false;
        uint8_t diff_mask[16] = {0};
        uint8_t diff_bytes = 0;

        if(i == 0) {
            // Block 0: Skip - UID preserved
            continue;
        } else if(mf_classic_is_sector_trailer(i)) {
            // Trailer: Skip - keys differ
            continue;
        } else {
            // Data block: Full comparison
            for(size_t j = 0; j < 16; j++) {
                if(app->mf_classic_data->block[i].data[j] != app->target_data->block[i].data[j]) {
                    blocks_differ = true;
                    diff_mask[j] = 1;
                    diff_bytes++;
                }
            }
        }

        if(blocks_differ) {
            BlockDifference* diff = &differences[diff_count++];
            diff->block_index = i;
            diff->diff_count = diff_bytes;
            memcpy(diff->expected, app->mf_classic_data->block[i].data, 16);
            memcpy(diff->found, app->target_data->block[i].data, 16);
            memcpy(diff->diff_mask, diff_mask, 16);
        }
    }

    // Genera report
    FuriString* report = generate_difference_report(app, differences, diff_count);

    // RESET text_box PRIMA di usarlo
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(report));
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    // Switch to text_box view
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdAbout);

    // Cleanup
    furi_string_free(report);
    free(differences);

    FURI_LOG_I(TAG, "Difference viewer: %zu differences found", diff_count);
}

/**
 * @brief Scene event handler
 */
bool miband_nfc_scene_diff_viewer_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Return to main menu
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

/**
 * @brief Scene exit handler
 */
void miband_nfc_scene_diff_viewer_on_exit(void* context) {
    MiBandNfcApp* app = context;
    text_box_reset(app->text_box);
}
