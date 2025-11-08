
#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/st25tb/st25tb_poller.h>
#include <nfc/protocols/st25tb/st25tb.h>

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"
#include <machine/endian.h>
#define ST25TB_UID_SIZE        (8U)
#define ST25TB_TOTAL_BYTES     64
#define DISTRIBUTIN_DATA_START 0
#define DISTRIBUTIN_DATA_END   19

#define TAG "Metroflip:Scene:Star"

typedef struct {
    int outer_key; // e.g., 0x000
    int inner_key; // e.g., 5
    const char* city;
    const char* system;
    const char* usage; // e.g., "_1_1", "_2", NULL if not specified
} InterticEntry;

InterticEntry FRA_OrganizationalAuthority_Contract_Provider[] = {
    {0x000, 5, "Lille", "Ilévia / Keolis", "_1_1"},
    {0x000, 7, "Lens-Béthune", "Tadao / Transdev", "_1_1"},
    {0x006, 1, "Amiens", "Ametis / Keolis", NULL},
    {0x008, 15, "Angoulême", "STGA", "_1_1"},
    {0x021, 1, "Bordeaux", "TBM / Keolis", "_1_1"},
    {0x057, 1, "Lyon", "TCL / Keolis", "_1"},
    {0x072, 1, "Tours", "filbleu / Keolis", "_1_1"},
    {0x078, 4, "Reims", "Citura / Transdev", "_1_2"},
    {0x091, 1, "Strasbourg", "CTS", "_4"},
    {0x502, 83, "Annecy", "Sibra", "_2"},
    {0x502, 10, "Clermont-Ferrand", "T2C", NULL},
    {0x907, 1, "Dijon", "Divia / Keolis", NULL},
    {0x908, 1, "Rennes", "STAR / Keolis", "_2"},
    {0x908, 8, "Saint-Malo", "MAT / RATP", "_1_1"},
    {0x911, 5, "Besançon", "Ginko / Keolis", NULL},
    {0x912, 3, "Le Havre", "Lia / Transdev", "_1_1"},
    {0x912, 35, "Cherbourg-en-Cotentin", "Cap Cotentin / Transdev", NULL},
    {0x913, 3, "Nîmes", "Tango / Transdev", "_3"},
    {0x915, 1, "Metz", "Le Met' / TAMM", NULL},
    {0x917, 4, "Angers", "Irigo / RATP", "_1_2"},
    {0x917, 7, "Saint-Nazaire", "Stran", NULL},
};

#define NUM_ENTRIES                                          \
    (sizeof(FRA_OrganizationalAuthority_Contract_Provider) / \
     sizeof(FRA_OrganizationalAuthority_Contract_Provider[0]))

InterticEntry* find_entry(int outer, int inner) {
    for(size_t i = 0; i < NUM_ENTRIES; i++) {
        if(FRA_OrganizationalAuthority_Contract_Provider[i].outer_key == outer &&
           FRA_OrganizationalAuthority_Contract_Provider[i].inner_key == inner) {
            return &FRA_OrganizationalAuthority_Contract_Provider[i];
        }
    }
    return NULL; // not found
}

uint8_t st25tb_get_block_byte(const St25tbData* data, size_t block_index, uint8_t byte_index) {
    if(block_index >= st25tb_get_block_count(data->type) || byte_index > 3) return 0;
    uint32_t block = __bswap32(data->blocks[block_index]);
    return (block >> (8 * (3 - byte_index))) & 0xFF;
}

void big_endian_version(
    uint8_t* dst,
    const St25tbData* data,
    size_t total_bytes) { // god I hate endianess
    for(size_t i = 0; i < total_bytes; i += 4) {
        uint8_t b0 = st25tb_get_block_byte(data, i / 4, 0);
        uint8_t b1 = st25tb_get_block_byte(data, i / 4, 1);
        uint8_t b2 = st25tb_get_block_byte(data, i / 4, 2);
        uint8_t b3 = st25tb_get_block_byte(data, i / 4, 3);
        dst[i] = b3;
        dst[i + 1] = b2;
        dst[i + 2] = b1;
        dst[i + 3] = b0;
    }
}

// Converts selected bytes to a bit string
static void
    bytes_to_bit_string(const uint8_t* buffer, size_t start_byte, size_t end_byte, char* bit_str) {
    char* dst = bit_str;
    for(size_t i = start_byte; i <= end_byte; i++) {
        for(int bit = 7; bit >= 0; bit--) {
            *dst++ = (buffer[i] & (1 << bit)) ? '1' : '0';
        }
    }
    *dst = '\0';
}

// Extract arbitrary bits from buffer using a string of bits
uint64_t extract_bits(
    const uint8_t* buffer,
    size_t start_byte,
    size_t end_byte,
    size_t start_bit,
    size_t num_bits) {
    size_t bytes_count = end_byte - start_byte + 1;
    char bit_string[bytes_count * 8 + 1];
    bytes_to_bit_string(buffer, start_byte, end_byte, bit_string);

    uint64_t val = 0;
    for(size_t i = 0; i < num_bits; i++) {
        val <<= 1;
        if(bit_string[start_bit + i] == '1') val |= 1;
    }
    return val;
}

static bool intertic_parse(FuriString* parsed_data, const St25tbData* data) {
    bool parsed = false;

    do {
        //do some check here
        //let's get distribution data:
        uint8_t big_endian_file_buffer[64];
        big_endian_version(big_endian_file_buffer, data, ST25TB_TOTAL_BYTES);
        uint64_t countryISOCode = extract_bits(
            big_endian_file_buffer, DISTRIBUTIN_DATA_START, DISTRIBUTIN_DATA_END, 32, 12);
        FURI_LOG_I(TAG, "Country ISO Code: %02llX", countryISOCode);

        if(countryISOCode != 0x250) break; //it needs to be france
        furi_string_cat_printf(parsed_data, "\e#Star\n");
        furi_string_cat_printf(parsed_data, "UID:");

        for(size_t i = 0; i < ST25TB_UID_SIZE; i++) {
            furi_string_cat_printf(parsed_data, " %02X", data->uid[i]);
        }

        furi_string_cat_printf(
            parsed_data, "\nCountry Code: \n0x%02llX (France)\n", countryISOCode);
        uint64_t organizationalAuthority = extract_bits(
            big_endian_file_buffer, DISTRIBUTIN_DATA_START, DISTRIBUTIN_DATA_END, 44, 12);
        furi_string_cat_printf(
            parsed_data, "\nOrganizational Authority: \n0x%02llX\n", organizationalAuthority);
        uint64_t contractApplicationVersionNumber = extract_bits(
            big_endian_file_buffer, DISTRIBUTIN_DATA_START, DISTRIBUTIN_DATA_END, 56, 6);

        furi_string_cat_printf(
            parsed_data,
            "\nContract Application \nVersion Number: %lld\n",
            contractApplicationVersionNumber);

        uint64_t contractProvider = extract_bits(
            big_endian_file_buffer, DISTRIBUTIN_DATA_START, DISTRIBUTIN_DATA_END, 62, 8);

        furi_string_cat_printf(parsed_data, "\nContract Provider: %lld\n", contractProvider);

        InterticEntry* entry = find_entry(organizationalAuthority, contractProvider);
        if(entry) {
            furi_string_cat_printf(parsed_data, "\nCity: \n%s\n", entry->city);
            furi_string_cat_printf(parsed_data, "\nSystem: \n%s\n", entry->system);
        } else {
            furi_string_cat_printf(parsed_data, "\nCity code not indexed\n");
        }

        size_t remaining_trips_block = 5;
        uint8_t r0 = st25tb_get_block_byte(data, remaining_trips_block, 0);
        uint8_t r1 = st25tb_get_block_byte(data, remaining_trips_block, 1);
        uint8_t r2 = st25tb_get_block_byte(data, remaining_trips_block, 2);

        // reverse order: r2 r1 r0
        uint32_t remaining_total = (r2 << 16) | (r1 << 8) | r0;

        furi_string_cat_printf(parsed_data, "\nRemaining Trips: \n%ld\n", remaining_total);
        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand intertic_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolSt25tb);

    Metroflip* app = context;
    const St25tbPollerEvent* st25tb_event = event.event_data;

    if(st25tb_event->type == St25tbPollerEventTypeRequestMode) {
        st25tb_event->data->mode_request.mode = St25tbPollerModeRead;
    } else if(st25tb_event->type == St25tbPollerEventTypeSuccess) {
        nfc_device_set_data(app->nfc_device, NfcProtocolSt25tb, nfc_poller_get_data(app->poller));
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void intertic_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        FURI_LOG_I(TAG, "Star loaded");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            St25tbData* st25tb_data = st25tb_alloc();
            st25tb_load(st25tb_data, ff, 4);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!intertic_parse(parsed_data, st25tb_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            st25tb_free(st25tb_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        FURI_LOG_I(TAG, "Star not loaded");
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolSt25tb);
        nfc_poller_start(app->poller, intertic_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool intertic_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            const St25tbData* st25tb_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolSt25tb);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!intertic_parse(parsed_data, st25tb_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
            widget_add_button_element(
                widget, GuiButtonTypeLeft, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Save", metroflip_save_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            metroflip_app_blink_stop(app);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void intertic_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin intertic_plugin = {
    .card_name = "Star",
    .plugin_on_enter = intertic_on_enter,
    .plugin_on_event = intertic_on_event,
    .plugin_on_exit = intertic_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor intertic_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &intertic_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* intertic_plugin_ep(void) {
    return &intertic_plugin_descriptor;
}
