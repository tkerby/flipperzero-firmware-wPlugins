#include "include/application/wc_settings_service.h"

#include "include/application/wc_files.h"

#include <string.h>

// "WCS1" + baud (u32 LE) + autosave (u8).
#define WC_SETTINGS_BYTES 9
static const uint8_t k_magic[4] = {'W', 'C', 'S', '1'};

static void settings_defaults(WcSettings *s) {
    s->baud = WC_SETTINGS_BAUD_DEFAULT;
    s->autosave = false;
}

bool wc_settings_service_load(const WcStorePort *store, WcSettings *s) {
    settings_defaults(s);
    // Size first: read_file reports 0 both for "no file" and for "file too big for the buffer",
    // so reading alone would file a 20-byte settings.db under "never saved" and overwrite it.
    size_t size = store->file_size(store->self, WC_SETTINGS_FILE);
    if (size == 0) {
        return true; // never saved yet
    }
    uint8_t buf[WC_SETTINGS_BYTES];
    size_t n = store->read_file(store->self, WC_SETTINGS_FILE, buf, sizeof(buf));
    if (size != WC_SETTINGS_BYTES || n != WC_SETTINGS_BYTES ||
        memcmp(buf, k_magic, sizeof(k_magic)) != 0) {
        return false;
    }
    s->baud = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) | ((uint32_t)buf[6] << 16) |
              ((uint32_t)buf[7] << 24);
    s->autosave = (buf[8] != 0);
    // A stored baud the serial layer cannot use would open the port at nonsense; only the two
    // the Settings screen offers are accepted.
    if (s->baud != 115200 && s->baud != 230400) {
        s->baud = WC_SETTINGS_BAUD_DEFAULT;
        return false;
    }
    return true;
}

bool wc_settings_service_save(const WcStorePort *store, const WcSettings *s) {
    uint8_t buf[WC_SETTINGS_BYTES];
    memcpy(buf, k_magic, sizeof(k_magic));
    buf[4] = (uint8_t)(s->baud & 0xFF);
    buf[5] = (uint8_t)((s->baud >> 8) & 0xFF);
    buf[6] = (uint8_t)((s->baud >> 16) & 0xFF);
    buf[7] = (uint8_t)((s->baud >> 24) & 0xFF);
    buf[8] = s->autosave ? 1 : 0;
    return store->write_file(store->self, WC_SETTINGS_FILE, buf, sizeof(buf));
}
