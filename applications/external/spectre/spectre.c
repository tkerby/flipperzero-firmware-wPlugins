// Spectre: a ghost hunting kit for Flipper Zero.
//
// Four tabs share one "spirit energy" level. The level is fed by real inputs
// where the hardware has them (NFC and 125 kHz reader fields, Sub-GHz noise,
// battery temperature) and by the hardware random number generator for the
// rest. It is entertainment, not an instrument, and the README says so.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <math.h>

#define TICK_MS 50
#define W       128
#define H       64
#define BODY_Y  12 // first row under the tab strip

#define BOOT_TICKS 50

#define BOX_POINTS     64
#define BOX_CHUNK      8
#define BOX_LOCK_DBM   (-76) // a signal must be at least this strong...
#define BOX_LOCK_RISE  10 // ...and this far above that frequency's usual level
#define BOX_LOCK_TICKS 24
#define BOX_COOLDOWN   100 // ticks between catches

#define WORD_MAX   12
#define TRANSCRIPT 3

#define RADAR_MAX 6
#define RADAR_CX  98
#define RADAR_CY  38
#define RADAR_R   24

typedef enum {
    ModeSense,
    ModeBox,
    ModeVoice,
    ModeRadar,
    ModeCount,
} Mode;

static const char* const mode_names[ModeCount] = {"SENSE", "BOX", "VOICE", "RADAR"};

typedef struct {
    uint32_t lo;
    uint32_t hi;
    const char* name;
} Band;

static const Band bands[] = {
    {300000000, 348000000, "300 MHz"},
    {387000000, 464000000, "433 MHz"},
    {779000000, 928000000, "868 MHz"},
};
#define BAND_COUNT COUNT_OF(bands)

typedef enum {
    AnswerNone,
    AnswerYes,
    AnswerNo,
    AnswerUnclear,
} Answer;

typedef struct {
    float angle; // radians, 0 = east, counter-clockwise
    float dist; // metres, 0.5..9.9
    uint8_t ttl; // ticks left
    bool alive;
} Blip;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    NotificationApp* notifications;
    bool running;
    Mode mode;
    uint32_t frame;
    uint8_t boot;

    // sound
    bool sound;
    bool speaker_held;
    bool tone_on;

    // spirit energy, 0..100
    uint8_t energy;
    int16_t noise; // slow random walk
    int16_t surge; // decaying bursts
    uint8_t peak;

    // SENSE
    bool nfc_held;
    bool nfc_field;
    bool lf_field;
    float temp0;
    float temp;
    uint32_t temp_at;
    bool cold;
    uint8_t hist[W]; // energy history, newest last

    // BOX
    uint8_t band;
    uint8_t box_i;
    int8_t box_rssi[BOX_POINTS];
    int8_t box_avg[BOX_POINTS]; // slow average per point, so a new signal stands out
    bool box_seeded; // the averages hold a full first sweep
    uint8_t box_cooldown;
    uint32_t box_freq;
    int8_t box_last;
    bool box_lock;
    uint8_t box_lock_left;
    bool box_hold;

    // VOICE
    char word[WORD_MAX + 1];
    uint8_t word_len;
    uint8_t word_shown;
    uint8_t word_timer;
    uint8_t word_hold; // ticks the finished word stays up
    char transcript[TRANSCRIPT][WORD_MAX + 1];
    bool asked;
    uint8_t ask_left;

    // RADAR
    Blip blips[RADAR_MAX];
    float sweep;
    Answer answer;
    uint8_t answer_left;
    bool radar_asked;
    uint8_t radar_ask_left;
    uint8_t ping;

    // LED: the notification service reads these later on its own thread,
    // so they live here for the whole run rather than on a stack frame
    NotificationMessage led_msg[3];
    const NotificationMessage* led_seq[5];
    uint32_t led_last;
    uint8_t flash;

    char toast[24];
    uint8_t toast_left;
} App;

// ---------------------------------------------------------------- word bank

static const char* const words[] = {
    "COLD",    "LEAVE",    "HELP",     "BEHIND", "MOTHER",  "FATHER",  "DOOR",     "SEVEN",
    "RUN",     "WATER",    "LIGHT",    "SORRY",  "STAY",    "NAME",    "WHO",      "YOU",
    "HERE",    "BELOW",    "ATTIC",    "CELLAR", "CHILD",   "OLD",     "FIRE",     "NIGHT",
    "SLEEP",   "WAKE",     "HIDE",     "SEEK",   "FOLLOW",  "MINE",    "OURS",     "LOST",
    "FOUND",   "KEY",      "LOCK",     "WINDOW", "STAIRS",  "BED",     "CANDLE",   "MIRROR",
    "GARDEN",  "WELL",     "RIVER",    "BRIDGE", "TRAIN",   "CLOCK",   "MIDNIGHT", "DAWN",
    "WINTER",  "SNOW",     "RAIN",     "WIND",   "SHADOW",  "WALL",    "FLOOR",    "CORNER",
    "HAND",    "EYES",     "VOICE",    "SONG",   "DANCE",   "HOME",    "AWAY",     "CLOSE",
    "FAR",     "SOON",     "NEVER",    "ALWAYS", "WAIT",    "LISTEN",  "LOOK",     "TOUCH",
    "BREATHE", "CROSS",    "CHURCH",   "BELL",   "GRAVE",   "STONE",   "ROSE",     "THORN",
    "APPLE",   "BREAD",    "SALT",     "SILVER", "GOLD",    "IRON",    "ROPE",     "BOAT",
    "SEA",     "SHORE",    "ISLAND",   "FOREST", "PATH",    "GATE",    "FENCE",    "FIELD",
    "HORSE",   "DOG",      "CAT",      "BIRD",   "CROW",    "OWL",     "MOTH",     "SPIDER",
    "FRIEND",  "BROTHER",  "SISTER",   "BABY",   "DOCTOR",  "SOLDIER", "TEACHER",  "NURSE",
    "MARY",    "JOHN",     "ANNA",     "THOMAS", "ELIZA",   "HENRY",   "YES",      "NO",
    "MAYBE",   "WHY",      "WHEN",     "WHERE",  "DOWN",    "UP",      "OUT",      "IN",
    "GO",      "COME",     "BACK",     "TURN",   "OPEN",    "SHUT",    "QUIET",    "LOUD",
    "WARM",    "DARK",     "BRIGHT",   "HUNGRY", "TIRED",   "AFRAID",  "HAPPY",    "ANGRY",
    "ALONE",   "TOGETHER", "REMEMBER", "FORGET", "FORGIVE", "PROMISE", "SECRET",   "TRUTH",
    "DREAM",   "GAME",     "PLAY",     "WORK",   "REST",    "PRAY",    "SING",     "CRY",
    "LAUGH",   "KNOCK",    "THREE",    "TWELVE", "HUNDRED", "FIRST",   "LAST",     "US",
};
#define WORD_COUNT COUNT_OF(words)

// ---------------------------------------------------------------- small helpers

static uint32_t rnd(uint32_t n) {
    return furi_hal_random_get() % n;
}

static bool tick_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static void toast(App* app, const char* text) {
    strlcpy(app->toast, text, sizeof(app->toast));
    app->toast_left = 30;
}

// ---------------------------------------------------------------- sound

static void sound_tone(App* app, float freq, float volume) {
    if(!app->sound || furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;
    if(!app->speaker_held) {
        if(!furi_hal_speaker_acquire(10)) return;
        app->speaker_held = true;
    }
    furi_hal_speaker_start(freq, volume);
    app->tone_on = true;
}

static void sound_quiet(App* app) {
    if(app->tone_on) {
        furi_hal_speaker_stop();
        app->tone_on = false;
    }
}

static void sound_release(App* app) {
    sound_quiet(app);
    if(app->speaker_held) {
        furi_hal_speaker_release();
        app->speaker_held = false;
    }
}

// A Geiger-style tick: a couple of milliseconds of a high tone
static void sound_click(App* app) {
    if(!app->sound || furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;
    if(!app->speaker_held) {
        if(!furi_hal_speaker_acquire(10)) return;
        app->speaker_held = true;
    }
    furi_hal_speaker_start(3400.0f, 0.4f);
    furi_delay_ms(2);
    furi_hal_speaker_stop();
    app->tone_on = false;
}

// ---------------------------------------------------------------- LED

static void led_init(App* app) {
    app->led_msg[0].type = NotificationMessageTypeLedRed;
    app->led_msg[1].type = NotificationMessageTypeLedGreen;
    app->led_msg[2].type = NotificationMessageTypeLedBlue;
    app->led_seq[0] = &app->led_msg[0];
    app->led_seq[1] = &app->led_msg[1];
    app->led_seq[2] = &app->led_msg[2];
    app->led_seq[3] = &message_do_not_reset;
    app->led_seq[4] = NULL;
    app->led_last = 0xFFFFFFFF;
}

static void led_update(App* app) {
    uint8_t r = 0, g = 0, b = 0;
    if(app->boot) {
        // a slow violet glow while the veil opens
        uint32_t t = app->frame % 40;
        uint8_t v = (t < 20 ? t : 40 - t) * 3;
        r = v;
        b = v;
    } else if(app->flash) {
        r = g = b = 255;
    } else {
        // breathing: ten steps up and ten down, a step every three frames
        uint32_t t = app->frame / 3 % 20;
        uint32_t breath = (t < 10 ? t : 20 - t); // 0..10
        uint32_t level = 8 + breath * (4 + app->energy / 4); // brighter when excited
        if(level > 255) level = 255;
        switch(app->mode) {
        case ModeSense:
            // green when quiet, through amber to red as the energy climbs
            r = app->energy * 255 / 100;
            g = 255 - r;
            break;
        case ModeBox:
            r = 255;
            g = 90;
            break;
        case ModeVoice:
            r = 160;
            b = 255;
            break;
        default:
            g = 200;
            b = 255;
            break;
        }
        r = r * level / 255;
        g = g * level / 255;
        b = b * level / 255;
    }
    uint32_t packed = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if(packed == app->led_last) return;
    app->led_last = packed;
    app->led_msg[0].data.led.value = r;
    app->led_msg[1].data.led.value = g;
    app->led_msg[2].data.led.value = b;
    notification_message(app->notifications, (const NotificationSequence*)&app->led_seq);
}

// Something happened: flash the LED, buzz
static void event_announce(App* app) {
    app->flash = 3;
    notification_message(app->notifications, &sequence_single_vibro);
}

// ---------------------------------------------------------------- spirit energy

// boost: steady contribution from real sensors. chance: surge odds per 1000 ticks.
static void energy_step(App* app, int boost, uint32_t chance) {
    int d = (int)rnd(7) - 3;
    app->noise += d;
    if(app->noise < 3) app->noise = 3; // never quite still
    if(app->noise > 22) app->noise = 22;
    if(app->surge > 0) app->surge -= MAX(1, app->surge / 12);
    if(rnd(1000) < chance) app->surge = 35 + rnd(50);
    int e = app->noise + app->surge + boost;
    e = CLAMP(e, 100, 0);
    app->energy = (app->energy * 3 + e) / 4;
    if(app->energy > app->peak) app->peak = app->energy;
    memmove(app->hist, app->hist + 1, W - 1);
    app->hist[W - 1] = app->energy;
}

static void energy_surge(App* app, int amount) {
    app->surge = MIN(100, app->surge + amount);
}

// ---------------------------------------------------------------- VOICE

static void voice_say(App* app, const char* word) {
    for(size_t i = TRANSCRIPT - 1; i > 0; i--) {
        strlcpy(app->transcript[i], app->transcript[i - 1], sizeof(app->transcript[i]));
    }
    strlcpy(app->transcript[0], word, sizeof(app->transcript[0]));
    strlcpy(app->word, word, sizeof(app->word));
    app->word_len = strlen(app->word);
    app->word_shown = 0;
    app->word_timer = 0;
    app->word_hold = 80;
    energy_surge(app, 50);
    event_announce(app);
}

static void voice_say_random(App* app) {
    voice_say(app, words[rnd(WORD_COUNT)]);
}

// Runs every tick in every mode so a word caught by the BOX plays out too.
// Returns true while letters are still arriving.
static bool voice_tick(App* app, bool audible) {
    if(app->word_len == 0) return false;
    if(app->word_shown < app->word_len) {
        if(++app->word_timer >= 3) {
            app->word_timer = 0;
            app->word_shown++;
        }
        // a garbled syllable per letter
        if(audible) sound_tone(app, 150.0f + rnd(230), 0.35f);
        return true;
    }
    if(app->word_hold) {
        if(--app->word_hold == 0) {
            app->word[0] = 0;
            app->word_len = 0;
        }
    }
    return false;
}

static void voice_step(App* app) {
    uint32_t chance = 2 + (app->energy > 60 ? 10 : 0);
    energy_step(app, 0, chance);
    bool talking = voice_tick(app, true);
    if(!talking) sound_quiet(app);
    if(app->asked) {
        if(--app->ask_left == 0) {
            app->asked = false;
            voice_say_random(app);
        }
    } else if(app->word_len == 0 && app->surge > 45 && rnd(10) < 3) {
        voice_say_random(app);
    }
}

static void voice_ask(App* app) {
    if(app->asked) return;
    app->asked = true;
    app->ask_left = 20 + rnd(60);
    app->word[0] = 0;
    app->word_len = 0;
}

// ---------------------------------------------------------------- SENSE

static void sense_start(App* app) {
    app->nfc_held = furi_hal_nfc_acquire() == FuriHalNfcErrorNone;
    if(app->nfc_held) furi_hal_nfc_field_detect_start();
    furi_hal_rfid_field_detect_start();
    app->temp0 = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    app->temp = app->temp0;
    app->temp_at = furi_get_tick();
    app->cold = false;
    app->peak = app->energy;
}

static void sense_stop(App* app) {
    furi_hal_rfid_field_detect_stop();
    if(app->nfc_held) {
        furi_hal_nfc_field_detect_stop();
        furi_hal_nfc_release();
        app->nfc_held = false;
    }
    app->nfc_field = false;
    app->lf_field = false;
    sound_quiet(app);
}

static void sense_step(App* app) {
    uint32_t now = furi_get_tick();
    uint32_t lf_freq = 0;
    bool lf = furi_hal_rfid_field_is_present(&lf_freq);
    bool nfc = app->nfc_held && furi_hal_nfc_field_is_present();
    if(tick_reached(now, app->temp_at + 1000)) {
        app->temp_at = now;
        app->temp = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
        app->cold = app->temp < app->temp0 - 0.4f;
    }
    bool was = app->nfc_field || app->lf_field;
    app->lf_field = lf;
    app->nfc_field = nfc;
    int boost = (lf ? 45 : 0) + (nfc ? 70 : 0) + (app->cold ? 20 : 0);
    energy_step(app, boost, (lf || nfc) ? 25 : 2);
    if((lf || nfc) && !was) event_announce(app);
    voice_tick(app, false);

    // Geiger crackle, faster as the needle climbs; a warble at the top
    if(app->energy >= 85) {
        float f = 600.0f + (app->frame & 1 ? 200.0f : 0.0f) + rnd(60);
        sound_tone(app, f, 0.3f);
    } else {
        sound_quiet(app);
        uint32_t chance = 3 + (uint32_t)app->energy * app->energy / 120; // 3..86 %
        if(rnd(100) < chance) sound_click(app);
    }
}

// ---------------------------------------------------------------- BOX

static uint32_t box_point_freq(uint8_t band, uint8_t i) {
    const Band* b = &bands[band];
    return b->lo + (uint32_t)(((uint64_t)(b->hi - b->lo) * i) / (BOX_POINTS - 1));
}

static void box_clear(App* app) {
    memset(app->box_rssi, -100, sizeof(app->box_rssi));
    memset(app->box_avg, -100, sizeof(app->box_avg));
    app->box_seeded = false;
    app->box_cooldown = 40;
    app->box_i = 0;
    app->box_lock = false;
    app->box_lock_left = 0;
    app->box_last = -100;
}

static void box_start(App* app) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    box_clear(app);
    app->box_hold = false;
}

static void box_stop(App* app) {
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    sound_quiet(app);
}

static int8_t box_read(void) {
    float rssi = furi_hal_subghz_get_rssi();
    return (int8_t)CLAMP(rssi, 0.0f, -127.0f);
}

static void box_step(App* app) {
    int8_t best = -127;
    if(app->box_lock) {
        // stay on the station for a moment
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(app->box_freq);
        furi_hal_subghz_rx();
        furi_delay_ms(4);
        app->box_last = box_read();
        furi_hal_subghz_idle();
        best = app->box_last;
        if(--app->box_lock_left == 0) app->box_lock = false;
    } else if(!app->box_hold) {
        bool fresh = false; // a signal this frequency does not usually carry
        uint32_t fresh_freq = 0;
        for(uint8_t k = 0; k < BOX_CHUNK; k++) {
            uint32_t freq = box_point_freq(app->band, app->box_i);
            int8_t v = -100;
            if(furi_hal_subghz_is_frequency_valid(freq)) {
                furi_hal_subghz_idle();
                furi_hal_subghz_set_frequency_and_path(freq);
                furi_hal_subghz_rx();
                furi_delay_ms(2);
                v = box_read();
            }
            int8_t* avg = &app->box_avg[app->box_i];
            if(app->box_seeded) {
                if(v >= BOX_LOCK_DBM && v >= *avg + BOX_LOCK_RISE) {
                    fresh = true;
                    fresh_freq = freq;
                }
                *avg = (*avg * 7 + v) / 8;
            } else {
                *avg = v;
            }
            app->box_rssi[app->box_i] = v;
            app->box_freq = freq;
            app->box_last = v;
            if(v > best) best = v;
            app->box_i = (app->box_i + 1) % BOX_POINTS;
            if(app->box_i == 0) app->box_seeded = true;
        }
        furi_hal_subghz_idle();
        if(app->box_cooldown) {
            app->box_cooldown--;
        } else if(fresh || rnd(400) == 0) {
            // something new is on the air (or the box just feels like it):
            // hold the station and let it speak
            if(fresh) app->box_freq = fresh_freq;
            app->box_lock = true;
            app->box_lock_left = BOX_LOCK_TICKS;
            app->box_cooldown = BOX_COOLDOWN;
            voice_say_random(app);
        }
    }

    int boost = CLAMP(((int)best + 95) * 2, 40, 0);
    energy_step(app, boost, app->box_lock ? 30 : 4);
    bool talking = voice_tick(app, false);

    if(app->box_hold) {
        sound_quiet(app);
    } else if(app->box_lock) {
        // a voice fighting through the static: a wavering tone that sinks
        float f = 260.0f + app->box_lock_left * 14.0f + rnd(talking ? 160 : 40);
        sound_tone(app, f, 0.35f);
    } else {
        // channel-hopping static: bursts of low noise with dropouts
        uint32_t r = rnd(10);
        if(r < 3) {
            sound_quiet(app);
        } else {
            float f = 130.0f + rnd(110) + ((int)app->box_last + 100) * 5.0f;
            sound_tone(app, f, 0.18f);
        }
    }
}

// ---------------------------------------------------------------- RADAR

static void radar_spawn(App* app) {
    for(size_t i = 0; i < RADAR_MAX; i++) {
        Blip* b = &app->blips[i];
        if(b->alive) continue;
        b->angle = rnd(628) / 100.0f;
        b->dist = 0.8f + rnd(85) / 10.0f;
        b->ttl = 60 + rnd(120);
        b->alive = true;
        return;
    }
}

static void radar_step(App* app) {
    energy_step(app, 0, 4);
    voice_tick(app, false);
    app->sweep += 0.11f;
    if(app->sweep > 6.2832f) app->sweep -= 6.2832f;

    size_t alive = 0;
    bool ping = false;
    for(size_t i = 0; i < RADAR_MAX; i++) {
        Blip* b = &app->blips[i];
        if(!b->alive) continue;
        if(--b->ttl == 0) {
            b->alive = false;
            continue;
        }
        alive++;
        // the sweep just passed this echo
        float d = app->sweep - b->angle;
        if(d < 0) d += 6.2832f;
        if(d < 0.11f) ping = true;
        // echoes drift
        b->angle += (rnd(5) - 2) * 0.01f;
        b->dist += (rnd(5) - 2) * 0.03f;
        b->dist = CLAMP(b->dist, 9.9f, 0.5f);
    }
    if(app->surge > 40 && alive < RADAR_MAX && rnd(8) == 0) radar_spawn(app);
    if(alive == 0 && rnd(60) == 0) radar_spawn(app);

    if(app->radar_asked) {
        if(--app->radar_ask_left == 0) {
            app->radar_asked = false;
            uint32_t r = rnd(100);
            app->answer = r < 45 ? AnswerYes : r < 85 ? AnswerNo : AnswerUnclear;
            app->answer_left = 70;
            energy_surge(app, 40);
            event_announce(app);
            radar_spawn(app);
        }
    }
    if(app->answer_left) app->answer_left--;

    // sound: a ping as the sweep crosses an echo, a two-note reply for answers
    if(app->answer_left > 62) {
        uint8_t n = 70 - app->answer_left; // 0..7
        float f = app->answer == AnswerYes ? 440.0f + n * 40.0f :
                  app->answer == AnswerNo  ? 440.0f - n * 25.0f :
                                             (n & 1 ? 500.0f : 380.0f);
        sound_tone(app, f, 0.35f);
    } else if(ping) {
        sound_tone(app, 1400.0f, 0.3f);
        app->ping = 2;
    } else if(app->ping) {
        app->ping--;
    } else {
        sound_quiet(app);
    }
}

static void radar_ask(App* app) {
    if(app->radar_asked) return;
    app->radar_asked = true;
    app->radar_ask_left = 30 + rnd(50);
    app->answer_left = 0;
}

// ---------------------------------------------------------------- modes

static void mode_start(App* app) {
    switch(app->mode) {
    case ModeSense:
        sense_start(app);
        break;
    case ModeBox:
        box_start(app);
        break;
    default:
        break;
    }
}

static void mode_stop(App* app) {
    switch(app->mode) {
    case ModeSense:
        sense_stop(app);
        break;
    case ModeBox:
        box_stop(app);
        break;
    default:
        sound_quiet(app);
        break;
    }
}

static void mode_step(App* app) {
    switch(app->mode) {
    case ModeSense:
        sense_step(app);
        break;
    case ModeBox:
        box_step(app);
        break;
    case ModeVoice:
        voice_step(app);
        break;
    case ModeRadar:
        radar_step(app);
        break;
    default:
        break;
    }
}

static void mode_switch(App* app, int dir) {
    mode_stop(app);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->mode = (Mode)((app->mode + ModeCount + dir) % ModeCount);
    furi_mutex_release(app->mutex);
    mode_start(app);
}

// ---------------------------------------------------------------- drawing

static const uint8_t ghost_bits[] = {
    // 20 x 20, generated from ascii art in tools/assets.py
    0xc0, 0x3f, 0x00, 0xf0, 0xff, 0x00, 0xf8, 0xff, 0x01, 0xfc, 0xff, 0x03, 0xfc, 0xff, 0x03,
    0x9e, 0x9f, 0x07, 0x0e, 0x0f, 0x07, 0x0e, 0x0f, 0x07, 0x9e, 0x9f, 0x07, 0xfe, 0xff, 0x07,
    0xfe, 0xff, 0x07, 0xfe, 0xff, 0x07, 0xfe, 0xff, 0x07, 0xfe, 0xff, 0x07, 0xfe, 0xff, 0x07,
    0xfe, 0xff, 0x07, 0xde, 0x7b, 0x07, 0x8e, 0x31, 0x06, 0x06, 0x41, 0x04, 0x00, 0x00, 0x00,
};

static void draw_tabs(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < ModeCount; i++) {
        int x = i * 32;
        if(i == (int)app->mode) {
            canvas_draw_rbox(canvas, x, 0, 32, 11, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, x + 16, 6, AlignCenter, AlignCenter, mode_names[i]);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 11, W - 1, 11);
}

static void draw_footer(Canvas* canvas, const char* text) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, W / 2, H - 1, AlignCenter, AlignBottom, text);
}

static const char* sense_word(uint8_t e) {
    if(e >= 85) return "CONTACT";
    if(e >= 65) return "STRONG";
    if(e >= 40) return "PRESENCE";
    if(e >= 20) return "FAINT";
    return "QUIET";
}

static void draw_sense(Canvas* canvas, App* app) {
    // the needle gauge: a half circle sitting on the bottom edge
    const int cx = 64, cy = 60, r = 36;
    for(int a = 0; a <= 180; a += 2) {
        float rad = a * 0.0174533f;
        int x = cx + (int)(cosf(rad) * r);
        int y = cy - (int)(sinf(rad) * r);
        canvas_draw_dot(canvas, x, y);
        // the hot end is drawn thick
        if(a <= 30)
            canvas_draw_dot(
                canvas, cx + (int)(cosf(rad) * (r - 2)), cy - (int)(sinf(rad) * (r - 2)));
    }
    for(int a = 0; a <= 180; a += 45) {
        float rad = a * 0.0174533f;
        canvas_draw_line(
            canvas,
            cx + (int)(cosf(rad) * (r - 5)),
            cy - (int)(sinf(rad) * (r - 5)),
            cx + (int)(cosf(rad) * r),
            cy - (int)(sinf(rad) * r));
    }
    float needle = (180.0f - app->energy * 1.8f) * 0.0174533f;
    int nx = cx + (int)(cosf(needle) * (r - 8));
    int ny = cy - (int)(sinf(needle) * (r - 8));
    canvas_draw_line(canvas, cx, cy, nx, ny);
    canvas_draw_line(canvas, cx - 1, cy, nx, ny);
    canvas_draw_disc(canvas, cx, cy, 2);

    canvas_set_font(canvas, FontPrimary);
    char num[8];
    snprintf(num, sizeof(num), "%u", app->energy);
    canvas_draw_str_aligned(canvas, cx, cy - 12, AlignCenter, AlignBottom, num);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, BODY_Y + 9, sense_word(app->energy));
    char temp[16];
    snprintf(temp, sizeof(temp), "%.1fC", (double)app->temp);
    canvas_draw_str_aligned(canvas, W - 2, BODY_Y + 9, AlignRight, AlignBottom, temp);
    const char* note = app->nfc_field ? "NFC FIELD" :
                       app->lf_field  ? "LF FIELD" :
                       app->cold      ? "COLD SPOT" :
                                        NULL;
    if(note && (app->frame / 5) % 2 == 0) {
        canvas_draw_str_aligned(canvas, W - 2, BODY_Y + 19, AlignRight, AlignBottom, note);
    }
    if(!note) {
        char pk[12];
        snprintf(pk, sizeof(pk), "peak %u", app->peak);
        canvas_draw_str(canvas, 2, BODY_Y + 19, pk);
    }
}

static void draw_box(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, BODY_Y + 9, bands[app->band].name);
    const char* status = app->box_hold ? "HOLD" : app->box_lock ? "SIGNAL" : "SWEEP";
    canvas_draw_str_aligned(canvas, W - 2, BODY_Y + 9, AlignRight, AlignBottom, status);
    if(app->box_lock && (app->frame / 3) % 2 == 0) {
        canvas_draw_rframe(canvas, W - 36, BODY_Y + 1, 35, 10, 2);
    }

    char freq[16];
    snprintf(
        freq, sizeof(freq), "%lu.%03lu", app->box_freq / 1000000, (app->box_freq / 1000) % 1000);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, BODY_Y + 30, AlignCenter, AlignBottom, freq);

    // the spectrum ribbon, two pixels per point
    const int base = H - 1, hmax = 14;
    for(int i = 0; i < BOX_POINTS; i++) {
        int v = app->box_rssi[i] + 100; // -100..-60 -> 0..40
        int h = CLAMP(v * hmax / 40, hmax, 0);
        if(h) canvas_draw_line(canvas, i * 2, base, i * 2, base - h);
    }
    // the cursor
    int cx = app->box_i * 2;
    canvas_draw_line(canvas, cx, base - hmax - 2, cx, base - hmax);

    if(app->word_len) {
        char buf[WORD_MAX + 1];
        memcpy(buf, app->word, app->word_shown);
        buf[app->word_shown] = 0;
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, BODY_Y + 36, AlignCenter, AlignTop, buf);
    }
}

static void draw_voice(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    if(app->asked) {
        canvas_draw_str(canvas, 2, BODY_Y + 9, "asking");
    } else if(app->word_len) {
        canvas_draw_str(canvas, 2, BODY_Y + 9, "they speak");
    } else {
        canvas_draw_str(canvas, 2, BODY_Y + 9, "listening");
    }
    int dots = (app->frame / 6) % 4;
    for(int i = 0; i < dots; i++)
        canvas_draw_box(canvas, 52 + i * 4, BODY_Y + 7, 2, 2);
    char e[12];
    snprintf(e, sizeof(e), "energy %u", app->energy);
    canvas_draw_str_aligned(canvas, W - 2, BODY_Y + 9, AlignRight, AlignBottom, e);

    if(app->word_len) {
        char buf[WORD_MAX + 1];
        memcpy(buf, app->word, app->word_shown);
        buf[app->word_shown] = 0;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, buf);
        int w = canvas_string_width(canvas, buf);
        if(app->word_shown < app->word_len && (app->frame & 2)) {
            canvas_draw_box(canvas, 64 + w / 2 + 2, 31, 5, 9);
        }
    } else if(app->asked) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, 36, AlignCenter, AlignCenter, (app->frame / 4) % 2 ? "?" : "");
    } else {
        // a faint waveform while nothing comes through
        int prev = 36;
        for(int x = 24; x < 104; x++) {
            int y = 36 + (int)(sinf((x + app->frame * 2) * 0.25f) * (2 + app->energy / 20));
            canvas_draw_line(canvas, x - 1, prev, x, y);
            prev = y;
        }
    }

    canvas_set_font(canvas, FontSecondary);
    char line[48] = "";
    for(size_t i = 0; i < TRANSCRIPT; i++) {
        if(!app->transcript[i][0]) break;
        if(i) strlcat(line, "  ", sizeof(line));
        strlcat(line, app->transcript[i], sizeof(line));
    }
    draw_footer(canvas, line[0] ? line : "OK: ask a question");
}

static const char* bearing_name(float angle) {
    static const char* const names[] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
    int i = (int)((angle + 0.3927f) / 0.7854f) & 7;
    return names[i];
}

static void draw_radar(Canvas* canvas, App* app) {
    canvas_draw_circle(canvas, RADAR_CX, RADAR_CY, RADAR_R);
    canvas_draw_circle(canvas, RADAR_CX, RADAR_CY, RADAR_R * 2 / 3);
    canvas_draw_circle(canvas, RADAR_CX, RADAR_CY, RADAR_R / 3);
    for(int a = 0; a < 360; a += 45) {
        float rad = a * 0.0174533f;
        canvas_draw_dot(
            canvas,
            RADAR_CX + (int)(cosf(rad) * (RADAR_R - 2)),
            RADAR_CY - (int)(sinf(rad) * (RADAR_R - 2)));
    }
    // the sweep and a fading tail
    for(int k = 0; k < 3; k++) {
        float a = app->sweep - k * 0.12f;
        int len = RADAR_R - k * 6;
        canvas_draw_line(
            canvas,
            RADAR_CX,
            RADAR_CY,
            RADAR_CX + (int)(cosf(a) * len),
            RADAR_CY - (int)(sinf(a) * len));
    }
    size_t alive = 0;
    const Blip* nearest = NULL;
    for(size_t i = 0; i < RADAR_MAX; i++) {
        const Blip* b = &app->blips[i];
        if(!b->alive) continue;
        alive++;
        if(!nearest || b->dist < nearest->dist) nearest = b;
        float d = app->sweep - b->angle;
        if(d < 0) d += 6.2832f;
        if(d > 2.4f) continue; // the glow has faded until the next pass
        int rr = (int)(b->dist / 10.0f * (RADAR_R - 3)) + 2;
        int x = RADAR_CX + (int)(cosf(b->angle) * rr);
        int y = RADAR_CY - (int)(sinf(b->angle) * rr);
        if(d < 0.8f) {
            canvas_draw_disc(canvas, x, y, 2);
        } else if(d < 1.6f) {
            canvas_draw_circle(canvas, x, y, 1);
        } else {
            canvas_draw_dot(canvas, x, y);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    char buf[24];
    if(app->answer_left) {
        canvas_set_font(canvas, FontPrimary);
        const char* t = app->answer == AnswerYes ? "YES" :
                        app->answer == AnswerNo  ? "NO" :
                                                   "UNCLEAR";
        canvas_draw_rbox(canvas, 2, 26, 66, 18, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 35, 35, AlignCenter, AlignCenter, t);
        canvas_set_color(canvas, ColorBlack);
    } else if(app->radar_asked) {
        canvas_draw_str(canvas, 2, 36, (app->frame / 4) % 2 ? "asking . . ." : "asking");
    } else {
        snprintf(buf, sizeof(buf), "echoes %u", (unsigned)alive);
        canvas_draw_str(canvas, 2, BODY_Y + 9, buf);
        if(nearest) {
            snprintf(
                buf, sizeof(buf), "%.1f m %s", (double)nearest->dist, bearing_name(nearest->angle));
            canvas_draw_str(canvas, 2, BODY_Y + 21, buf);
        } else {
            canvas_draw_str(canvas, 2, BODY_Y + 21, "nothing near");
        }
        canvas_draw_str(canvas, 2, H - 2, "OK: yes / no");
    }
}

static void draw_boot(Canvas* canvas, App* app) {
    int bob = (app->frame / 5) % 4;
    bob = bob == 3 ? 1 : bob;
    canvas_draw_xbm(canvas, 54, 4 + bob, 20, 20, ghost_bits);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "S P E C T R E");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "opening the veil");
    int w = (BOOT_TICKS - app->boot) * 100 / BOOT_TICKS;
    canvas_draw_frame(canvas, 14, 56, 100, 5);
    canvas_draw_box(canvas, 14, 56, w, 5);
}

static void draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(app->boot) {
        draw_boot(canvas, app);
    } else {
        draw_tabs(canvas, app);
        switch(app->mode) {
        case ModeSense:
            draw_sense(canvas, app);
            break;
        case ModeBox:
            draw_box(canvas, app);
            break;
        case ModeVoice:
            draw_voice(canvas, app);
            break;
        case ModeRadar:
            draw_radar(canvas, app);
            break;
        default:
            break;
        }
        if(app->flash) canvas_invert_color(canvas);
    }
    if(app->toast_left) {
        canvas_set_font(canvas, FontSecondary);
        int w = canvas_string_width(canvas, app->toast) + 8;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 64 - w / 2, 22, w, 13);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 64 - w / 2, 22, w, 13, 2);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, app->toast);
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* queue = context;
    furi_message_queue_put(queue, event, 0);
}

static void handle_input(App* app, const InputEvent* event) {
    if(app->boot) {
        if(event->type == InputTypeShort) app->boot = 1;
        return;
    }
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        app->sound = !app->sound;
        if(!app->sound) sound_release(app);
        toast(app, app->sound ? "SOUND ON" : "SOUND OFF");
        return;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;
    bool repeat = event->type == InputTypeRepeat;
    switch(event->key) {
    case InputKeyBack:
        if(!repeat) app->running = false;
        break;
    case InputKeyLeft:
        if(!repeat) mode_switch(app, -1);
        break;
    case InputKeyRight:
        if(!repeat) mode_switch(app, 1);
        break;
    case InputKeyUp:
    case InputKeyDown:
        if(repeat) break;
        if(app->mode == ModeBox) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            int dir = event->key == InputKeyUp ? 1 : -1;
            app->band = (app->band + BAND_COUNT + dir) % BAND_COUNT;
            box_clear(app);
            furi_mutex_release(app->mutex);
        }
        break;
    case InputKeyOk:
        if(repeat) break;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        switch(app->mode) {
        case ModeSense:
            app->peak = app->energy;
            app->temp0 = app->temp;
            app->cold = false;
            toast(app, "BASELINE RESET");
            break;
        case ModeBox:
            app->box_hold = !app->box_hold;
            break;
        case ModeVoice:
            voice_ask(app);
            break;
        case ModeRadar:
            radar_ask(app);
            break;
        default:
            break;
        }
        furi_mutex_release(app->mutex);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- main

static void tick(App* app) {
    // hardware work happens here; the shared state is updated under the lock
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->boot) {
        if(--app->boot == 0) {
            furi_mutex_release(app->mutex);
            mode_start(app);
            furi_mutex_acquire(app->mutex, FuriWaitForever);
        }
    } else {
        mode_step(app);
    }
    app->frame++;
    if(app->flash) app->flash--;
    if(app->toast_left) app->toast_left--;
    furi_mutex_release(app->mutex);
    led_update(app);
    view_port_update(app->view_port);
}

int32_t spectre_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->running = true;
    app->sound = true;
    app->band = 1;
    app->boot = BOOT_TICKS;
    app->noise = 8;
    led_init(app);
    box_clear(app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    InputEvent event;
    uint32_t next = furi_get_tick();
    while(app->running) {
        uint32_t now = furi_get_tick();
        uint32_t wait = tick_reached(now, next) ? 0 : next - now;
        if(furi_message_queue_get(app->queue, &event, wait) == FuriStatusOk) {
            handle_input(app, &event);
            continue;
        }
        if(!app->running) break;
        next += TICK_MS;
        if(tick_reached(furi_get_tick(), next + 200)) next = furi_get_tick();
        tick(app);
    }

    if(!app->boot) mode_stop(app);
    sound_release(app);
    notification_message(app->notifications, &sequence_reset_rgb);
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
