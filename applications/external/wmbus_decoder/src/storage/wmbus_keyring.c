#include "wmbus_keyring.h"

#include <stdio.h>
#include <string.h>

#include <toolbox/args.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>

#include "../protocol/format/wmbus_hex_utils.h"

#define WMBUS_KEYRING_LINE_MAX (WMBUS_KEY_BYTES * 2U + 3U)

static void wmbus_keyring_set_status(WmBusKeyring* keyring, const char* format, ...) {
    if(!keyring || !format) return;

    va_list args;
    va_start(args, format);
    vsnprintf(keyring->status, sizeof(keyring->status), format, args);
    va_end(args);
}

static bool wmbus_keyring_parse_hex_key(const char* text, uint8_t out[WMBUS_KEY_BYTES]) {
    if(!text || !out) return false;
    if(strlen(text) != (WMBUS_KEY_BYTES * 2U)) return false;

    for(size_t i = 0; i < WMBUS_KEY_BYTES; i++) {
        if(!args_char_to_hex(text[i * 2U], text[i * 2U + 1U], &out[i])) {
            return false;
        }
    }

    return true;
}

static void wmbus_keyring_format_hex_key(
    const uint8_t key[WMBUS_KEY_BYTES],
    char out[WMBUS_KEYRING_LINE_MAX]) {
    if(!key || !out) return;

    /* Separate an existing final line even when it has no newline. */
    out[0] = '\n';
    wmbus_hex_encode(key, WMBUS_KEY_BYTES, &out[1], WMBUS_KEYRING_LINE_MAX - 1U);
    out[WMBUS_KEY_BYTES * 2U + 1U] = '\n';
    out[WMBUS_KEY_BYTES * 2U + 2U] = '\0';
}

bool wmbus_keyring_load(Storage* storage, WmBusKeyring* keyring) {
    if(!storage || !keyring) return false;

    WmBusKeyring loaded = {0};
    wmbus_keyring_set_status(&loaded, "missing");
    wmbus_storage_ensure_app_folder(storage);

    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    bool ok = true;

    if(!file_stream_open(stream, WMBUS_KEYRING_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        loaded.file_present = false;
        loaded.loaded = false;
        wmbus_keyring_set_status(&loaded, "missing");
        *keyring = loaded;
        furi_string_free(line);
        file_stream_close(stream);
        stream_free(stream);
        return false;
    }

    loaded.file_present = true;

    while(stream_read_line(stream, line)) {
        furi_string_trim(line);
        const char* text = furi_string_get_cstr(line);

        if((text[0] == '\0') || (text[0] == '#')) {
            continue;
        }

        if(loaded.key_store.count >= WMBUS_CRYPTO_KEY_STORE_MAX) {
            ok = false;
            wmbus_keyring_set_status(&loaded, "too many keys");
            break;
        }

        if(!wmbus_keyring_parse_hex_key(text, loaded.key_store.keys[loaded.key_store.count])) {
            ok = false;
            wmbus_keyring_set_status(&loaded, "bad key");
            break;
        }

        loaded.key_store.count++;
    }

    if(ok) {
        loaded.loaded = true;
        wmbus_keyring_set_status(&loaded, "%u keys", (unsigned int)loaded.key_store.count);
    } else {
        loaded.loaded = false;
        loaded.key_store.count = 0;
    }

    *keyring = loaded;

    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
    return ok;
}

bool wmbus_keyring_append(
    Storage* storage,
    WmBusKeyring* keyring,
    const uint8_t key[WMBUS_KEY_BYTES]) {
    if(!storage || !keyring || !key) return false;

    if(keyring->file_present && !keyring->loaded) {
        return false;
    }

    for(uint8_t i = 0U; i < keyring->key_store.count; i++) {
        if(memcmp(keyring->key_store.keys[i], key, WMBUS_KEY_BYTES) == 0) return true;
    }
    if(keyring->key_store.count >= WMBUS_CRYPTO_KEY_STORE_MAX) {
        wmbus_keyring_set_status(keyring, "too many keys");
        return false;
    }

    if(!wmbus_storage_ensure_app_folder(storage)) {
        wmbus_keyring_set_status(keyring, "storage err");
        return false;
    }

    char line[WMBUS_KEYRING_LINE_MAX];
    wmbus_keyring_format_hex_key(key, line);

    File* file = storage_file_alloc(storage);
    bool written = false;

    if(storage_file_open(file, WMBUS_KEYRING_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        size_t line_len = strlen(line);
        written = storage_file_write(file, line, line_len) == line_len;
        if(written) {
            written = storage_file_sync(file);
        }
    }

    if(storage_file_is_open(file)) {
        storage_file_close(file);
    }
    storage_file_free(file);

    if(!written) {
        wmbus_keyring_set_status(keyring, "write fail");
        return false;
    }

    return wmbus_keyring_load(storage, keyring);
}
