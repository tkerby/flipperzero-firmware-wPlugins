// Writing settings.bin (UI plugin; the resident settings.c only reads it at boot). The writers are only used by the language picker and the options
// screen, so they moved out of the app with write_file (M5 memory recovery, contract 25.6). Linked into the plugin on the device, into every host
// executable that links settings.c.
#include "settings.h"
#include "assets.h"
#include "gen/langs_gen.h"
#include <string.h>

void settings_encode(const Settings* s, uint8_t out[SETTINGS_FILE_SIZE]) {
    memcpy(out, "NSET", 4);
    out[4] = 1;
    out[5] = s->lang_id;
    out[6] = (uint8_t)(s->text_speed | s->battle << SETTINGS_BATTLE_SHIFT);
    out[7] = s->sound;
    uint32_t crc = settings_crc32(out, 8);
    for(int i = 0; i < 4; i++)
        out[8 + i] = (uint8_t)(crc >> (8 * i));
}

bool settings_save_to(const char* path, const Settings* s) {
    uint8_t b[SETTINGS_FILE_SIZE];
    settings_encode(s, b);
    return write_file(path, b, sizeof(b));
}

bool settings_save(const Settings* s) {
    return settings_save_to(SETTINGS_PATH, s);
}

void settings_store_language(uint8_t idx) {
    if(idx >= LANG_COUNT) return;
    g_settings.lang_id = LANG_ID[idx];
    settings_save(&g_settings);
}
