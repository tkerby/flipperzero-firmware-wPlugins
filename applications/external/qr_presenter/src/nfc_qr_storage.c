#include "nfc_qr_presenter.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

static bool nfc_qr_is_alnum(char c) {
    return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'));
}

static char nfc_qr_to_lower(char c) {
    if((c >= 'A') && (c <= 'Z')) return c + ('a' - 'A');
    return c;
}

static bool nfc_qr_ends_with(const char* text, const char* suffix) {
    const size_t text_len = strlen(text);
    const size_t suffix_len = strlen(suffix);
    if(text_len < suffix_len) return false;
    return strcasecmp(&text[text_len - suffix_len], suffix) == 0;
}

static void nfc_qr_make_path(char* out, size_t out_size, const char* name, const char* ext) {
    snprintf(out, out_size, "%s/%s%s", NFC_QR_DATA_DIR, name, ext);
}

static bool
    nfc_qr_write_file(Storage* storage, const char* path, const void* data, size_t data_len) {
    if(!storage || !path || !data) return false;

    bool ok = false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, data, data_len) == data_len;
        ok = storage_file_sync(file) && ok;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool nfc_qr_read_file(
    Storage* storage,
    const char* path,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(!storage || !path || !out || !out_len || (out_size == 0)) return false;

    bool ok = false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        const uint64_t file_size = storage_file_size(file);
        if(file_size > out_size) break;

        const size_t read_len = storage_file_read(file, out, (size_t)file_size);
        if(read_len != (size_t)file_size) break;

        *out_len = read_len;
        ok = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static void nfc_qr_make_slug(const char* text, char* out, size_t out_size) {
    const char* src = text;
    bool last_sep = false;
    size_t pos = 0;

    if(strncasecmp(src, "https://", 8) == 0) src += 8;
    if(strncasecmp(src, "http://", 7) == 0) src += 7;
    if(strncasecmp(src, "www.", 4) == 0) src += 4;
    if(strncasecmp(text, "BEGIN:VCARD", 11) == 0) src = "contact";

    for(size_t i = 0; (src[i] != '\0') && (pos + 1 < out_size); i++) {
        const char c = src[i];
        if(nfc_qr_is_alnum(c)) {
            out[pos++] = nfc_qr_to_lower(c);
            last_sep = false;
        } else if(!last_sep && pos > 0) {
            out[pos++] = '_';
            last_sep = true;
        }
        if(pos >= 24) break;
    }

    while(pos > 0 && out[pos - 1] == '_')
        pos--;
    if(pos == 0) {
        strlcpy(out, "payload", out_size);
    } else {
        out[pos] = '\0';
    }
}

void nfc_qr_normalize_text(const char* text, char* out, size_t out_size) {
    size_t pos = 0;

    for(size_t i = 0; text[i] && (pos + 1 < out_size); i++) {
        if((text[i] == '\\') && (text[i + 1] == 'n')) {
            out[pos++] = '\n';
            i++;
        } else if((text[i] == '\\') && (text[i + 1] == 'r')) {
            out[pos++] = '\r';
            i++;
        } else {
            out[pos++] = text[i];
        }
    }

    out[pos] = '\0';
}

static size_t nfc_qr_write_record(
    uint8_t* out,
    size_t out_size,
    uint8_t tnf,
    const char* type,
    const uint8_t* payload,
    size_t payload_len) {
    const size_t type_len = strlen(type);
    const bool short_record = payload_len <= 255;
    const size_t header_len = short_record ? 3 : 6;
    const size_t total_len = header_len + type_len + payload_len;

    if((type_len > 255) || (total_len > out_size)) return 0;

    size_t pos = 0;
    out[pos++] = 0x80 | 0x40 | (short_record ? 0x10 : 0x00) | (tnf & 0x07);
    out[pos++] = (uint8_t)type_len;
    if(short_record) {
        out[pos++] = (uint8_t)payload_len;
    } else {
        out[pos++] = (uint8_t)((payload_len >> 24) & 0xFF);
        out[pos++] = (uint8_t)((payload_len >> 16) & 0xFF);
        out[pos++] = (uint8_t)((payload_len >> 8) & 0xFF);
        out[pos++] = (uint8_t)(payload_len & 0xFF);
    }

    memcpy(&out[pos], type, type_len);
    pos += type_len;
    memcpy(&out[pos], payload, payload_len);
    pos += payload_len;

    return pos;
}

size_t nfc_qr_build_ndef_message(const char* text, uint8_t* out, size_t out_size) {
    if(!text || !out || (out_size == 0)) return 0;

    const size_t text_len = strlen(text);
    uint8_t payload[NFC_QR_NDEF_MAX];

    if(text_len == 0) return 0;

    if((strncasecmp(text, "https://", 8) == 0) || (strncasecmp(text, "http://", 7) == 0)) {
        const char* url_body = text;
        uint8_t prefix_code = 0x00;

        if(strncasecmp(text, "https://www.", 12) == 0) {
            prefix_code = 0x02;
            url_body = text + 12;
        } else if(strncasecmp(text, "http://www.", 11) == 0) {
            prefix_code = 0x01;
            url_body = text + 11;
        } else if(strncasecmp(text, "https://", 8) == 0) {
            prefix_code = 0x04;
            url_body = text + 8;
        } else if(strncasecmp(text, "http://", 7) == 0) {
            prefix_code = 0x03;
            url_body = text + 7;
        }

        const size_t body_len = strlen(url_body);
        if(body_len + 1 > sizeof(payload)) return 0;
        payload[0] = prefix_code;
        memcpy(&payload[1], url_body, body_len);
        return nfc_qr_write_record(out, out_size, 0x01, "U", payload, body_len + 1);
    }

    if(strncasecmp(text, "BEGIN:VCARD", 11) == 0) {
        return nfc_qr_write_record(
            out, out_size, 0x02, "text/vcard", (const uint8_t*)text, text_len);
    }

    if(text_len + 3 > sizeof(payload)) return 0;
    payload[0] = 0x02;
    payload[1] = 'e';
    payload[2] = 'n';
    memcpy(&payload[3], text, text_len);
    return nfc_qr_write_record(out, out_size, 0x01, "T", payload, text_len + 3);
}

static bool nfc_qr_storage_write_named(Storage* storage, const char* name, const char* text) {
    char txt_path[NFC_QR_PATH_LEN];
    char ndef_path[NFC_QR_PATH_LEN];
    uint8_t ndef[NFC_QR_NDEF_MAX];

    const size_t ndef_len = nfc_qr_build_ndef_message(text, ndef, sizeof(ndef));
    if(ndef_len == 0) return false;

    nfc_qr_make_path(txt_path, sizeof(txt_path), name, ".txt");
    nfc_qr_make_path(ndef_path, sizeof(ndef_path), name, ".ndef");

    return nfc_qr_write_file(storage, txt_path, text, strlen(text)) &&
           nfc_qr_write_file(storage, ndef_path, ndef, ndef_len);
}

bool nfc_qr_storage_init(Storage* storage) {
    if(!storage) return false;
    if(!storage_simply_mkdir(storage, "/ext/apps_data")) return false;
    if(!storage_simply_mkdir(storage, NFC_QR_DATA_DIR)) return false;
    return true;
}

size_t nfc_qr_storage_scan(Storage* storage, NfcQrPayload* payloads, size_t payloads_capacity) {
    if(!storage || !payloads || (payloads_capacity == 0)) return 0;

    size_t count = 0;
    File* dir = storage_file_alloc(storage);
    if(!dir) return 0;

    if(storage_dir_open(dir, NFC_QR_DATA_DIR)) {
        char name[64];
        FileInfo info;
        while((count < payloads_capacity) && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!nfc_qr_ends_with(name, ".txt") && !nfc_qr_ends_with(name, ".vcf")) continue;

            const size_t base_len = strlen(name) - 4;
            if((base_len == 0) || (base_len >= NFC_QR_NAME_LEN) || (name[0] == '.')) continue;

            char base[NFC_QR_NAME_LEN];
            memcpy(base, name, base_len);
            base[base_len] = '\0';

            NfcQrPayload candidate;
            strlcpy(candidate.name, base, sizeof(candidate.name));
            snprintf(
                candidate.txt_path, sizeof(candidate.txt_path), "%s/%s", NFC_QR_DATA_DIR, name);
            nfc_qr_make_path(candidate.ndef_path, sizeof(candidate.ndef_path), base, ".ndef");

            /* Text is the source of truth; NFC is generated when sharing. */
            payloads[count++] = candidate;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    /* Keep selection predictable after an SD refresh or a new import. */
    for(size_t i = 1; i < count; i++) {
        NfcQrPayload item = payloads[i];
        size_t j = i;
        while(j > 0 && strcasecmp(payloads[j - 1].name, item.name) > 0) {
            payloads[j] = payloads[j - 1];
            j--;
        }
        payloads[j] = item;
    }
    return count;
}

bool nfc_qr_storage_read_text(
    Storage* storage,
    const NfcQrPayload* payload,
    char* out,
    size_t out_size) {
    size_t read_len = 0;
    if(!storage || !payload || !out || (out_size == 0)) return false;
    uint8_t raw[NFC_QR_TEXT_MAX + 5];
    if(!nfc_qr_read_file(storage, payload->txt_path, raw, sizeof(raw), &read_len)) return false;
    size_t start = 0;
    if(read_len >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) start = 3;
    while(read_len > start && (raw[read_len - 1] == '\r' || raw[read_len - 1] == '\n'))
        read_len--;
    const size_t text_len = read_len - start;
    if(text_len == 0 || text_len > NFC_QR_TEXT_MAX || text_len >= out_size) return false;
    for(size_t i = start; i < read_len; i++) {
        if(raw[i] == 0 || (raw[i] < 0x20 && raw[i] != '\n' && raw[i] != '\r' && raw[i] != '\t'))
            return false;
    }
    memcpy(out, raw + start, text_len);
    out[text_len] = '\0';
    /* Decoding only shortens input, so in-place normalization is safe. */
    nfc_qr_normalize_text(out, out, out_size);
    return true;
}

bool nfc_qr_storage_read_ndef(
    Storage* storage,
    const NfcQrPayload* payload,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(!payload) return false;
    return nfc_qr_read_file(storage, payload->ndef_path, out, out_size, out_len);
}

bool nfc_qr_storage_write_payload(
    Storage* storage,
    const char* fixed_name,
    const char* text,
    NfcQrPayload* out_payload) {
    if(!storage || !text || (text[0] == '\0') || strlen(text) > NFC_QR_TEXT_MAX) return false;

    char name[NFC_QR_NAME_LEN];
    char normalized_text[NFC_QR_TEXT_MAX + 1];
    nfc_qr_normalize_text(text, normalized_text, sizeof(normalized_text));

    if(fixed_name && fixed_name[0]) {
        strlcpy(name, fixed_name, sizeof(name));
    } else {
        char slug[NFC_QR_NAME_LEN];
        FuriString* next_name = furi_string_alloc();
        if(!next_name) return false;

        nfc_qr_make_slug(normalized_text, slug, sizeof(slug));
        storage_get_next_filename(storage, NFC_QR_DATA_DIR, slug, ".txt", next_name, 24);
        strlcpy(name, furi_string_get_cstr(next_name), sizeof(name));
        furi_string_free(next_name);
    }

    if(!nfc_qr_storage_write_named(storage, name, normalized_text)) return false;

    if(out_payload) {
        strlcpy(out_payload->name, name, sizeof(out_payload->name));
        nfc_qr_make_path(out_payload->txt_path, sizeof(out_payload->txt_path), name, ".txt");
        nfc_qr_make_path(out_payload->ndef_path, sizeof(out_payload->ndef_path), name, ".ndef");
    }

    return true;
}

bool nfc_qr_storage_delete_payload(Storage* storage, const NfcQrPayload* payload) {
    if(!storage || !payload) return false;

    return storage_simply_remove(storage, payload->txt_path) &&
           storage_simply_remove(storage, payload->ndef_path);
}

bool nfc_qr_storage_add_payload(
    Storage* storage,
    const char* name,
    const char* text,
    NfcQrPayload* out_payload) {
    if(!storage || !name || !name[0]) return false;
    char slug[NFC_QR_NAME_LEN];
    nfc_qr_make_slug(name, slug, sizeof(slug));
    for(unsigned i = 0; i < 100; i++) {
        char candidate[NFC_QR_NAME_LEN];
        if(i == 0) {
            strlcpy(candidate, slug, sizeof(candidate));
        } else {
            snprintf(candidate, sizeof(candidate), "%.24s_%u", slug, i);
        }
        char path[NFC_QR_PATH_LEN];
        nfc_qr_make_path(path, sizeof(path), candidate, ".txt");
        if(storage_common_exists(storage, path)) continue;
        nfc_qr_make_path(path, sizeof(path), candidate, ".vcf");
        if(storage_common_exists(storage, path)) continue;
        nfc_qr_make_path(path, sizeof(path), candidate, ".ndef");
        if(storage_common_exists(storage, path)) continue;
        return nfc_qr_storage_write_payload(storage, candidate, text, out_payload);
    }
    return false;
}

bool nfc_qr_storage_update_payload(Storage* storage, const NfcQrPayload* payload, const char* text) {
    if(!storage || !payload || !text || strlen(text) > NFC_QR_TEXT_MAX) return false;
    char normalized[NFC_QR_TEXT_MAX + 1];
    uint8_t ndef[NFC_QR_NDEF_MAX];
    nfc_qr_normalize_text(text, normalized, sizeof(normalized));
    const size_t length = nfc_qr_build_ndef_message(normalized, ndef, sizeof(ndef));
    if(!length) return false;
    return nfc_qr_write_file(storage, payload->txt_path, normalized, strlen(normalized)) &&
           nfc_qr_write_file(storage, payload->ndef_path, ndef, length);
}
