// Parser for CTA Ventra Ultralight cards - Metroflip Plugin Version
// Made by @hazardousvoltage
// Based on my own research, with...
// Credit to https://www.lenrek.net/experiments/compass-tickets/ & MetroDroid project for underlying info
// Credit to FatherDivine for adding the "stop IDs & stop names" database (& code tweaks).
// Additional improvements by FatherDivine:
//   - Added month validation (1-12) to prevent invalid date structures
//   - Added out_size parameter validation in ventra_lookup_stop_name_str
//   - Added skip for empty/whitespace-only lines in CSV parsing
//   - Removed unused dt_diff function
//   - Fixed unnecessary storage_file_close on failed file open
//   - Added two-tiered station lookup (bus/train specific files with CSV fallback)
//   - Added auto_mode support to use already-read data from auto-detect scan
//   - Added trailing newline to parsed output to prevent text obscured by buttons
//
// This parser can decode the paper single-use and single/multi-day paper passes using Ultralight EV1
// The plastic cards are DESFire and fully locked down, not much useful info extractable
//
// Adapted for Metroflip plugin API

/*
 * STOP DATABASE FORMAT (IMPORTANT)
 *
 * This parser supports lookup for BOTH bus and train stops using a two-tiered system.
 *
 * PRIMARY LOOKUP (Metroflip station files - separate files for bus and train):
 *   Bus stations:   /ext/apps_assets/metroflip/ventra/stations/bus/stations.txt
 *   Train stations: /ext/apps_assets/metroflip/ventra/stations/train/stations.txt
 *
 * FALLBACK LOOKUP (unified CSV - for users without Metroflip):
 *   /ext/apps_data/ventra/cta_stops.csv
 *
 * Store IDs EXACTLY as strings:
 *
 *   Bus stops  → decimal Cubic stop IDs
 *       Example: 16959,Harlem & Addison
 *
 *   Train stops → 4‑digit uppercase hex Cubic station IDs
 *       Example: 003B,Jefferson Park
 *
 * The parser will:
 *   - Determine transport type (B=Bus line 2, T=Train line 1)
 *   - First try the type-specific file (bus/train stations.txt)
 *   - Fall back to cta_stops.csv if not found
 */

#include <flipper_application.h>
#include "../../metroflip_i.h"
#include "../../metroflip_plugins.h"
#include "../../api/metroflip/metroflip_api.h"

#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#include <dolphin/dolphin.h>
#include <datetime.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TAG "Metroflip:Scene:Ventra"

// Paths to station database files on SD card
// Primary paths - separate files for bus and train (Metroflip app)
#define VENTRA_BUS_STATIONS_PATH   "/ext/apps_assets/metroflip/ventra/stations/bus/stations.txt"
#define VENTRA_TRAIN_STATIONS_PATH "/ext/apps_assets/metroflip/ventra/stations/train/stations.txt"
// Fallback path - unified CSV (for users without Metroflip)
#define VENTRA_STOP_DB_PATH        "/ext/apps_data/ventra/cta_stops.csv"

static DateTime ventra_exp_date = {0};
static DateTime ventra_validity_date = {0};
static uint8_t ventra_high_seq = 0;
static uint8_t ventra_cur_blk = 0;
static uint8_t ventra_mins_active = 0;

static uint32_t ventra_time_now(void) {
    return furi_hal_rtc_get_timestamp();
}

static DateTime ventra_dt_delta(DateTime dt, uint8_t delta_days) {
    // returns shifted DateTime, from initial DateTime and time offset in days
    DateTime dt_shifted = {0};
    datetime_timestamp_to_datetime(
        datetime_datetime_to_timestamp(&dt) - (uint64_t)delta_days * 86400, &dt_shifted);
    return dt_shifted;
}

// Card is expired if:
// - Hard expiration date passed (90 days from purchase, encoded in product record)
// - Soft expiration date passed:
//   - For passes, n days after first use
//   - For tickets, 2 hours after first use
static bool ventra_is_expired(void) {
    uint32_t ts_hard_exp = datetime_datetime_to_timestamp(&ventra_exp_date);
    uint32_t ts_soft_exp = datetime_datetime_to_timestamp(&ventra_validity_date);
    uint32_t ts_now = ventra_time_now();
    return (ts_now >= ts_hard_exp || ts_now > ts_soft_exp);
}

/********************************************************************
 * Stop database: CSV-on-demand lookup
 ********************************************************************/

// Simple line reader for Storage File API.
// Reads until newline or EOF, null-terminates buffer.
// Returns true if a line was read, false on EOF/no data.
static bool ventra_read_line(File* file, char* buf, size_t buf_size) {
    if(buf_size == 0) return false;

    size_t pos = 0;
    char ch;
    size_t read_len = 0;

    while(true) {
        read_len = storage_file_read(file, &ch, 1);
        if(read_len == 0) {
            // EOF
            break;
        }

        if(ch == '\r') {
            // ignore CR, but keep going (handle CRLF)
            continue;
        }

        if(ch == '\n') {
            // end of line
            break;
        }

        if(pos < buf_size - 1) {
            buf[pos++] = ch;
        } else {
            // line too long, keep consuming but don't overflow
        }
    }

    if(pos == 0 && read_len == 0) {
        // nothing read and EOF
        return false;
    }

    buf[pos] = '\0';
    return true;
}

/* Helper function to search a single file for an ID.
 * File format (IDs stored as exact strings):
 *   Bus:   16959,Harlem & Addison
 *   Train: 003B,Jefferson Park
 */
static bool ventra_search_file(const char* file_path, const char* id_str, char* out_name, size_t out_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char line[256];
    bool found = false;

    while(ventra_read_line(file, line, sizeof(line))) {
        // Skip empty or whitespace-only lines
        char* trimmed = line;
        while(*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }
        if(*trimmed == '\0') continue;

        // Skip comment lines
        if(line[0] == '#') continue;

        char* comma = strchr(line, ',');
        if(!comma) continue;

        *comma = '\0';
        const char* csv_id = line;
        const char* csv_name = comma + 1;

        while(*csv_name == ' ' || *csv_name == '\t')
            csv_name++;

        if(strcmp(csv_id, id_str) == 0) {
            strncpy(out_name, csv_name, out_size);
            out_name[out_size - 1] = '\0';
            found = true;
            break;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return found;
}

/* Two-tiered lookup for bus (decimal) and train (hex) IDs.
 *
 * Lookup order:
 *   1. Type-specific file (bus/train stations.txt from Metroflip)
 *   2. Fallback to unified CSV (cta_stops.csv)
 *
 * line parameter: 1 = Train (T), 2 = Bus (B)
 */
static bool ventra_lookup_stop_name_str(const char* id_str, uint8_t line, char* out_name, size_t out_size) {
    // Validation for out_size parameter
    if(out_size == 0) return false;

    // Determine primary lookup path based on transport type
    const char* primary_path = NULL;
    if(line == 2) {
        // Bus
        primary_path = VENTRA_BUS_STATIONS_PATH;
    } else if(line == 1) {
        // Train
        primary_path = VENTRA_TRAIN_STATIONS_PATH;
    }

    // Try primary lookup (type-specific Metroflip file)
    if(primary_path != NULL) {
        if(ventra_search_file(primary_path, id_str, out_name, out_size)) {
            return true;
        }
    }

    // Fallback to unified CSV
    return ventra_search_file(VENTRA_STOP_DB_PATH, id_str, out_name, out_size);
}

/********************************************************************
 * Original Ventra transaction parsing, with stop-name injection
 ********************************************************************/

/* Format Cubic stop IDs into lookup keys:
 *   Bus   → decimal string
 *   Train → 4‑digit uppercase hex
 */
static void ventra_format_id(uint16_t locus, uint8_t line, char* out, size_t out_size) {
    if(line == 2) {
        // Bus → decimal
        snprintf(out, out_size, "%u", (unsigned int)locus);
    } else if(line == 1) {
        // Train → 4‑digit uppercase hex
        snprintf(out, out_size, "%04X", (unsigned int)locus);
    } else {
        // Purchases or unknown → fallback decimal
        snprintf(out, out_size, "%u", (unsigned int)locus);
    }
}

static FuriString* ventra_parse_xact(const MfUltralightData* data, uint8_t blk, bool is_pass) {
    FuriString* ventra_xact_str = furi_string_alloc();
    uint16_t ts = data->page[blk].data[0] | data->page[blk].data[1] << 8;
    uint8_t tran_type = ts & 0x1F;
    ts >>= 5;
    uint8_t day = data->page[blk].data[2];
    uint32_t work = data->page[blk + 1].data[0] | data->page[blk + 1].data[1] << 8 |
                    data->page[blk + 1].data[2] << 16;
    uint8_t seq = work & 0x7F;
    uint16_t exp = (work >> 7) & 0x7FF;
    uint8_t exp_day = data->page[blk + 2].data[0];
    uint16_t locus = data->page[blk + 2].data[1] | data->page[blk + 2].data[2] << 8;
    uint8_t line = data->page[blk + 2].data[3];

    // This computes the block timestamp, based on the card expiration date and delta from it
    DateTime dt = ventra_dt_delta(ventra_exp_date, day);
    dt.hour = (ts & 0x7FF) / 60;
    dt.minute = (ts & 0x7FF) % 60;

    // If sequence is 0, block isn't used yet
    if(seq == 0) {
        furi_string_printf(ventra_xact_str, "-- EMPTY --");
        return (ventra_xact_str);
    }
    if(seq > ventra_high_seq) {
        ventra_high_seq = seq;
        ventra_cur_blk = blk;
        ventra_mins_active = data->page[blk + 1].data[3];
        if(tran_type == 6) {
            if(is_pass) {
                ventra_validity_date = ventra_dt_delta(ventra_exp_date, exp_day);
                ventra_validity_date.hour = (exp & 0x7FF) / 60;
                ventra_validity_date.minute = (exp & 0x7FF) % 60;
            } else {
                uint32_t validity_ts = datetime_datetime_to_timestamp(&dt);
                validity_ts += (120 - ventra_mins_active) * 60;
                datetime_timestamp_to_datetime(validity_ts, &ventra_validity_date);
            }
        }
    }

    // Type 0 = Purchase, 1 = Train ride, 2 = Bus ride
    char linemap[3] = "PTB";

    // Unified bus/train stop lookup
    char stop_name[128];
    bool have_name = false;
    char id_key[16];

    // Convert locus → lookup key (decimal for bus, hex for train)
    ventra_format_id(locus, line, id_key, sizeof(id_key));

    // Look up the formatted key (tries type-specific file first, then CSV fallback)
    have_name = ventra_lookup_stop_name_str(id_key, line, stop_name, sizeof(stop_name));

    if(have_name) {
        furi_string_printf(
            ventra_xact_str,
            "%c %s (V %s) %04d-%02d-%02d %02d:%02d",
            (line < 3) ? linemap[line] : '?',
            stop_name,
            id_key,
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute);
    } else {
        // Fallback to original formatting
        if(line == 2) {
            furi_string_printf(
                ventra_xact_str,
                "%c %u %04d-%02d-%02d %02d:%02d",
                (line < 3) ? linemap[line] : '?',
                (unsigned int)locus,
                dt.year,
                dt.month,
                dt.day,
                dt.hour,
                dt.minute);
        } else {
            furi_string_printf(
                ventra_xact_str,
                "%c %04X %04d-%02d-%02d %02d:%02d",
                (line < 3) ? linemap[line] : '?',
                (unsigned int)locus,
                dt.year,
                dt.month,
                dt.day,
                dt.hour,
                dt.minute);
        }
    }

    return (ventra_xact_str);
}

static bool ventra_parse(FuriString* parsed_data, const MfUltralightData* data) {
    furi_assert(data);
    furi_assert(parsed_data);

    bool parsed = false;

    // Reset static state for each parse
    ventra_exp_date = (DateTime){0};
    ventra_validity_date = (DateTime){0};
    ventra_high_seq = 0;
    ventra_cur_blk = 0;
    ventra_mins_active = 0;

    do {
        // This test can probably be improved -- it matches every Ventra I've seen
        if(data->page[4].data[0] != 0x0A || data->page[4].data[1] != 4 ||
           data->page[4].data[2] != 0 || data->page[6].data[0] != 0 ||
           data->page[6].data[1] != 0 || data->page[6].data[2] != 0) {
            FURI_LOG_D(TAG, "Not Ventra Ultralight");
            break;
        }

        // Parse the product record
        FuriString* ventra_prod_str = furi_string_alloc();
        uint8_t otp = data->page[3].data[0];
        uint8_t prod_code = data->page[5].data[2];
        bool is_pass = false;
        switch(prod_code) {
        case 2:
        case 0x1F:
            furi_string_cat_printf(ventra_prod_str, "Single");
            break;
        case 0x01:
            furi_string_cat_printf(ventra_prod_str, "Single");
            break;
        case 3:
        case 0x3F:
            is_pass = true;
            furi_string_cat_printf(ventra_prod_str, "1-Day");
            break;
        case 4:
            is_pass = true;
            furi_string_cat_printf(ventra_prod_str, "3-Day");
            break;
        default:
            is_pass = true;
            furi_string_cat_printf(ventra_prod_str, "0x%02X", data->page[5].data[2]);
            break;
        }

        uint16_t date_y = data->page[4].data[3] | (data->page[5].data[0] << 8);
        uint8_t date_d = date_y & 0x1F;
        uint8_t date_m = (date_y >> 5) & 0x0F;
        date_y >>= 9;
        date_y += 2000;

        // Month validation - if invalid, card data may be corrupted
        if(date_m >= 1 && date_m <= 12) {
            ventra_exp_date.day = date_d;
            ventra_exp_date.month = date_m;
            ventra_exp_date.year = date_y;
            ventra_validity_date = ventra_exp_date;
        } else {
            FURI_LOG_W(TAG, "Invalid month value: %d", date_m);
            furi_string_free(ventra_prod_str);
            break;
        }

        // Parse the transaction blocks
        FuriString* ventra_xact_str1 = ventra_parse_xact(data, 8, is_pass);
        FuriString* ventra_xact_str2 = ventra_parse_xact(data, 12, is_pass);

        uint8_t card_state = 1;
        uint8_t rides_left = 0;

        char* card_states[5] = {"???", "NEW", "ACT", "USED", "EXP"};

        if(ventra_high_seq > 1) card_state = 2;
        if(!is_pass) {
            switch(otp) {
            case 0:
                rides_left = 3;
                break;
            case 2:
                card_state = 2;
                rides_left = 2;
                break;
            case 6:
                card_state = 2;
                rides_left = 1;
                break;
            case 0x0E:
            case 0x7E:
                card_state = 3;
                rides_left = 0;
                break;
            default:
                card_state = 0;
                rides_left = 0;
                break;
            }
        }
        if(ventra_is_expired()) {
            card_state = 4;
            rides_left = 0;
        }

        furi_string_printf(
            parsed_data,
            "\e#Ventra %s (%s)\n",
            furi_string_get_cstr(ventra_prod_str),
            card_states[card_state]);

        furi_string_cat_printf(
            parsed_data,
            "Exp: %04d-%02d-%02d %02d:%02d\n",
            ventra_validity_date.year,
            ventra_validity_date.month,
            ventra_validity_date.day,
            ventra_validity_date.hour,
            ventra_validity_date.minute);

        if(rides_left) {
            furi_string_cat_printf(parsed_data, "Rides left: %d\n", rides_left);
        }

        furi_string_cat_printf(
            parsed_data,
            "%s\n",
            furi_string_get_cstr(ventra_cur_blk == 8 ? ventra_xact_str1 : ventra_xact_str2));

        furi_string_cat_printf(
            parsed_data,
            "%s\n",
            furi_string_get_cstr(ventra_cur_blk == 8 ? ventra_xact_str2 : ventra_xact_str1));

        furi_string_cat_printf(
            parsed_data, "TVM ID: %02X%02X\n", data->page[7].data[1], data->page[7].data[0]);
        furi_string_cat_printf(parsed_data, "Tx count: %d\n", ventra_high_seq);
        furi_string_cat_printf(
            parsed_data,
            "Hard Expiry: %04d-%02d-%02d\n",
            ventra_exp_date.year,
            ventra_exp_date.month,
            ventra_exp_date.day);

        furi_string_free(ventra_prod_str);
        furi_string_free(ventra_xact_str1);
        furi_string_free(ventra_xact_str2);

        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand ventra_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfUltralight);

    Metroflip* app = context;
    const MfUltralightPollerEvent* mf_ultralight_event = event.event_data;

    if(mf_ultralight_event->type == MfUltralightPollerEventTypeReadSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfUltralight, nfc_poller_get_data(app->poller));

        const MfUltralightData* data =
            nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
        uint32_t custom_event = (data->pages_read == data->pages_total) ?
                                    MetroflipCustomEventPollerSuccess :
                                    MetroflipCustomEventPollerFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
        return NfcCommandStop;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthRequest) {
        // Don't treat auth request as failure - let the read continue
        // The read may still succeed even if auth fails
        return NfcCommandContinue;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthSuccess) {
        // Auth success is informational, continue with read
        return NfcCommandContinue;
    }

    return NfcCommandContinue;
}

static void ventra_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        // Data loaded from file
        FURI_LOG_I(TAG, "Ventra data loaded from file");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfUltralightData* ultralight_data = mf_ultralight_alloc();
            mf_ultralight_load(ultralight_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!ventra_parse(parsed_data, ultralight_data)) {
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
            mf_ultralight_free(ultralight_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        } else {
            // Proper cleanup when file open fails (Copilot review feedback)
            FURI_LOG_E(TAG, "Failed to open file: %s", app->file_path);
        }
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
    } else if(app->auto_mode) {
        // Coming from Auto scene - data already read into app->nfc_device
        FURI_LOG_I(TAG, "Ventra using data from auto-detect scan");
        const MfUltralightData* ultralight_data =
            nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
        
        // Safety check for null data
        if(!ultralight_data) {
            FURI_LOG_E(TAG, "Failed to get ultralight data from nfc_device");
            view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
            return;
        }
        
        FuriString* parsed_data = furi_string_alloc();
        Widget* widget = app->widget;

        furi_string_reset(app->text_box_store);
        if(!ventra_parse(parsed_data, ultralight_data)) {
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

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    } else {
        // Manual scan - need to read card
        FURI_LOG_I(TAG, "Ventra scanning for card");
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
        nfc_poller_start(app->poller, ventra_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool ventra_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            const MfUltralightData* ultralight_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!ventra_parse(parsed_data, ultralight_data)) {
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
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");

            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            metroflip_app_blink_stop(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void ventra_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    // Only stop poller if we started one (manual scan, not auto_mode or file load)
    if(app->poller && !app->data_loaded && !app->auto_mode) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin ventra_plugin = {
    .card_name = "Ventra",
    .plugin_on_enter = ventra_on_enter,
    .plugin_on_event = ventra_on_event,
    .plugin_on_exit = ventra_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor ventra_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &ventra_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* ventra_plugin_ep(void) {
    return &ventra_plugin_descriptor;
}
