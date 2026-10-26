#include "wmbus_log.h"
#include "wmbus_paths.h"

#include "../protocol/format/wmbus_packet_summary.h"
#include "../protocol/format/wmbus_record_formatter.h"
#include "../protocol/format/wmbus_hex_utils.h"
#include "../protocol/model/wmbus_application_record.h"
#include "../protocol/parser/wmbus_parser.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdarg.h>
#include <stdio.h>

static const char* wmbus_log_header(WmBusCsvLogging logging) {
    return (logging == WmBusCsvLoggingFull) ?
               "tick,mode,frame_type,rssi,quality,mfg,id,version,device_type,ci,parser,security_mode,key_index,main_field,fields,hex\n" :
               "tick,mode,frame_type,rssi,quality,mfg,id,version,device_type,ci,parser,main_field\n";
}

static void wmbus_log_format_path(WmBusCsvLogging logging, char* path, size_t path_size) {
    if(!path || path_size == 0U) return;

    DateTime now = {0};
    furi_hal_rtc_get_datetime(&now);
    const char* kind = (logging == WmBusCsvLoggingFull) ? "full" : "basic";

    if(now.year >= 2020U && now.month >= 1U && now.month <= 12U && now.day >= 1U &&
       now.day <= 31U) {
        snprintf(
            path,
            path_size,
            "%s/packets_%04u%02u%02u_%s.csv",
            WMBUS_APP_FOLDER,
            (unsigned int)now.year,
            (unsigned int)now.month,
            (unsigned int)now.day,
            kind);
    } else {
        snprintf(path, path_size, "%s/packets_undated_%s.csv", WMBUS_APP_FOLDER, kind);
    }
}

static bool wmbus_log_write_line(File* file, const char* format, ...) {
    if(!file || !format) return false;

    FuriString* line = furi_string_alloc();
    va_list args;
    va_start(args, format);
    furi_string_vprintf(line, format, args);
    va_end(args);
    size_t len = furi_string_size(line);
    bool written = storage_file_write(file, furi_string_get_cstr(line), len) == len;
    furi_string_free(line);
    return written;
}

bool wmbus_log_append(Storage* storage, WmBusCsvLogging logging, const WmBusPacketRecord* record) {
    if(!storage || !record || logging == WmBusCsvLoggingNone) return false;

    if(!wmbus_storage_ensure_app_folder(storage)) {
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool written = false;

    do {
        char path[WMBUS_PACKET_LOG_PATH_MAX] = {0};
        wmbus_log_format_path(logging, path, sizeof(path));
        if(!storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_APPEND)) {
            break;
        }

        if(storage_file_size(file) == 0U) {
            if(!wmbus_log_write_line(file, "%s", wmbus_log_header(logging))) {
                break;
            }
        }

        char total_m3[24] = {0};
        wmbus_application_format_total_volume_m3(
            &record->application, total_m3, sizeof(total_m3), false);

        unsigned long tick = (unsigned long)record->rx_tick;
        char mode = record->mode == WmBusRxModeT ? 'T' : 'C';
        const char* frame_type = wmbus_packet_summary_frame_type(record->format);
        const char* quality = wmbus_packet_quality_str(record->quality);
        bool has_frame =
            wmbus_packet_quality_meets(record->quality, WmBusPacketQualityFrameComplete);
        const char* mfg = has_frame ? record->identity.manufacturer : "";
        const char* id = has_frame ? record->identity.meter_id : "";
        uint8_t version = has_frame ? record->identity.version : 0U;
        uint8_t dev_type = has_frame ? record->identity.dev_type : 0U;
        uint8_t ci = has_frame ? record->tpl.ci_field : 0U;
        const char* parser = wmbus_parser_id_name(record->application.parser_id);

        if(logging == WmBusCsvLoggingFull) {
            char hex[WMBUS_PHY_FRAME_MAX_BYTES * 2U + 1U];
            FuriString* fields = furi_string_alloc();
            wmbus_record_formatter_format_joined(
                record->application.records, record->application.record_count, ';', fields);
            wmbus_hex_encode(record->wire_bytes, record->wire_len, hex, sizeof(hex));
            written = wmbus_log_write_line(
                file,
                "%lu,%c,%s,%d,%s,%s,%s,%02X,%02X,%02X,%s,%02X,%u,%s,%s,%s\n",
                tick,
                mode,
                frame_type,
                record->rssi,
                quality,
                mfg,
                id,
                version,
                dev_type,
                ci,
                parser,
                wmbus_packet_summary_security_mode(&record->ell, &record->tpl),
                wmbus_packet_summary_key_index(&record->ell, &record->tpl),
                total_m3,
                furi_string_get_cstr(fields),
                hex);
            furi_string_free(fields);
        } else {
            written = wmbus_log_write_line(
                file,
                "%lu,%c,%s,%d,%s,%s,%s,%02X,%02X,%02X,%s,%s\n",
                tick,
                mode,
                frame_type,
                record->rssi,
                quality,
                mfg,
                id,
                version,
                dev_type,
                ci,
                parser,
                total_m3);
        }

        written = storage_file_sync(file) && written;
    } while(false);

    if(storage_file_is_open(file)) {
        storage_file_close(file);
    }
    storage_file_free(file);
    return written;
}
