#include "settings.h"
#include "assets.h"
#include "text.h"
#include "gen/langs_gen.h"
#include <string.h>

Settings g_settings = {SETTINGS_LANG_UNSET, TEXT_SPEED_NORMAL, SOUND_HIGH, 0};

__attribute__((noinline)) void settings_defaults(Settings* s) {
    s->lang_id = SETTINGS_LANG_UNSET;
    s->text_speed = TEXT_SPEED_NORMAL;
    s->sound = SOUND_HIGH;
    s->battle = 0;
}

uint32_t settings_crc32(const uint8_t* data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for(uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) - (int32_t)(crc & 1));
    }
    return ~crc;
}

bool settings_decode(const uint8_t in[SETTINGS_FILE_SIZE], Settings* s) {
    settings_defaults(s);
    if(memcmp(in, "NSET", 4) != 0 || in[4] != 1) return false;
    uint32_t crc = (uint32_t)in[8] | ((uint32_t)in[9] << 8) | ((uint32_t)in[10] << 16) |
                   ((uint32_t)in[11] << 24);
    if(crc != settings_crc32(in, 8)) return false;
    if(in[6] & ~(SETTINGS_SPEED_BITS | SET_BATTLE_ANIM_OFF << SETTINGS_BATTLE_SHIFT |
                 SET_BATTLE_SET << SETTINGS_BATTLE_SHIFT))
        return false;
    s->lang_id = in[5];
    s->text_speed = in[6] & SETTINGS_SPEED_BITS;
    s->battle = in[6] >> SETTINGS_BATTLE_SHIFT;
    s->sound = in[7] <= SOUND_OFF ? in[7] : SOUND_HIGH; // (0 in a file from before the sound)
    return true;
}

bool settings_load_from(const char* path, Settings* s) {
    uint8_t b[SETTINGS_FILE_SIZE];
    if(file_size(path) != SETTINGS_FILE_SIZE || !read_at(path, 0, b, sizeof(b))) {
        settings_defaults(s);
        return false;
    }
    return settings_decode(b, s);
}

bool settings_load(Settings* s) {
    return settings_load_from(SETTINGS_PATH, s);
}

int settings_lang_index(const Settings* s) {
    for(int i = 0; i < LANG_COUNT; i++)
        if(LANG_ID[i] == s->lang_id) return i;
    return -1;
}

int settings_current_lang_index(void) {
    return settings_lang_index(&g_settings);
}

bool settings_boot(void) {
    bool have = settings_load(&g_settings);
    int li = settings_lang_index(&g_settings);
    text_set_speed(g_settings.text_speed);
    int idx = li >= 0 ? li :
                        LANG_IDX_EN; // no valid choice yet: English until the picker stores one
    if(!text_set_language((uint8_t)idx) && idx != 0) text_set_language(0);
    return have && li >= 0;
}
