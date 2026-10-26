#pragma once
// Player settings, stored in /ext/apps_data/elementis_zeta/settings.bin (separate from the future save).
// No furi.h (host-compilable); file access via assets.h (read_at / write_file).
//
// File (12 bytes, little-endian): "NSET", u8 version = 1, u8 lang (the langs.toml `id`; 0xFF =
// not chosen yet), u8 text speed byte (bits 0..1 TEXT_SPEED_*; F3, contract 75: bit 4 = battle animations OFF, bit 5 = battle style SET; the other bits are 0, so every
// older file reads as animations ON + style SHIFT, the defaults), u8 sound (SOUND_*; it was a reserved 0 before the sound existed, so an old file reads
// as "high" = the default: contract 28), u32 crc32 (IEEE, of bytes 0..7).
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define SETTINGS_FILE_SIZE  12
#define SETTINGS_LANG_UNSET 0xFF

enum {
    TEXT_SPEED_SLOW = 0,
    TEXT_SPEED_NORMAL = 1,
    TEXT_SPEED_FAST = 2,
    TEXT_SPEED_INSTANT = 3
};

enum {
    SOUND_HIGH = 0,
    SOUND_LOW = 1,
    SOUND_OFF = 2
}; // Options "Sonido": Alto / Bajo / No (0 must stay the default)

// Options "Battle animations" / "Battle style" (contract 75): Settings.battle bits (0 = the default of each: animations On, style Shift)
#define SET_BATTLE_ANIM_OFF \
    1 // the move / effect animations and the focus shots are skipped (the HP bar and the messages stay)
#define SET_BATTLE_SET \
    2 // style "Set": no "Will you change creatures?" before a trainer's next creature
#define SETTINGS_SPEED_BITS 0x03
#define SETTINGS_BATTLE_SHIFT \
    4 // the Settings.battle bits sit at bits 4.. of the file's text speed byte

typedef struct {
    uint8_t
        lang_id; // langs.toml id of the chosen language, SETTINGS_LANG_UNSET before the first choice
    uint8_t text_speed; // TEXT_SPEED_*
    uint8_t sound; // SOUND_*
    uint8_t battle; // SET_BATTLE_* (contract 75)
} Settings;

extern Settings g_settings; // the live settings (game thread)

void settings_defaults(Settings* s);
uint32_t settings_crc32(const uint8_t* data, uint32_t len);
void settings_encode(const Settings* s, uint8_t out[SETTINGS_FILE_SIZE]);
// False (and *s = defaults) for a wrong size / magic / version / crc or an out-of-range value.
bool settings_decode(const uint8_t in[SETTINGS_FILE_SIZE], Settings* s);
// Load / save the file at `path` (absolute). load: false when missing or invalid (defaults filled).
bool settings_load_from(const char* path, Settings* s);
bool settings_save_to(const char* path, const Settings* s);
bool settings_load(Settings* s); // SETTINGS_PATH
bool settings_save(const Settings* s); // SETTINGS_PATH
// Index of the live setting (g_settings), -1 when unset / unknown.
int settings_current_lang_index(void);
// Stores language `idx` (an index of the enabled-language table) in the live settings and writes settings.bin (a failed write
// only means the picker shows again next launch).
void settings_store_language(uint8_t idx);
// Index into the enabled-language table (gen/langs_gen.h) of s->lang_id, or -1 when unset / unknown.
int settings_lang_index(const Settings* s);
// Game start: loads the settings file, applies text speed and language (a language whose font cannot
// be loaded falls back to language 0, then to the system font; the stored choice is kept). Returns
// true when the file held a valid language (go to the title), false for the first-run picker.
bool settings_boot(void);
