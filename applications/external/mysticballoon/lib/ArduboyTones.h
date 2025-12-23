#pragma once
#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>

#ifndef TONES_END
#define TONES_END 0x8000
#endif

#ifndef ARDUBOY_TONES_TICK_HZ
#define ARDUBOY_TONES_TICK_HZ 780u
#endif

// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ ЗВУКОВОЙ СИСТЕМЫ (КАК В PLATFORM)
typedef struct {
    const uint16_t* pattern;
} ArduboyToneSoundRequest;

static FuriMessageQueue* g_arduboy_sound_queue = NULL;
static FuriThread* g_arduboy_sound_thread = NULL;
static volatile bool g_arduboy_sound_thread_running = false;
static volatile bool g_arduboy_audio_enabled = false;
static const float kArduboyToneSoundVolume = 1.0f;
static const uint32_t kArduboyToneToneTickHz = ARDUBOY_TONES_TICK_HZ;

static inline uint32_t arduboy_tone_ticks_to_ms(uint16_t ticks) {
    return (uint32_t)((ticks * 1000u + (kArduboyToneToneTickHz / 2)) / kArduboyToneToneTickHz);
}

static int32_t arduboy_tone_sound_thread_fn(void* /*ctx*/) {
    ArduboyToneSoundRequest req;
    while(g_arduboy_sound_thread_running) {
        if(furi_message_queue_get(g_arduboy_sound_queue, &req, 50) != FuriStatusOk) {
            continue;
        }

        if(!g_arduboy_audio_enabled || !req.pattern) {
            continue;
        }

        if(!furi_hal_speaker_acquire(50)) {
            continue;
        }

        const uint16_t* p = req.pattern;

        while(g_arduboy_sound_thread_running && g_arduboy_audio_enabled) {
            ArduboyToneSoundRequest new_req;
            if(furi_message_queue_get(g_arduboy_sound_queue, &new_req, 0) == FuriStatusOk) {
                p = new_req.pattern ? new_req.pattern : p;
            }

            uint16_t freq = *p++;
            if(freq == TONES_END) {
                break;
            }

            uint16_t dur_ticks = *p++;
            uint32_t dur_ms = arduboy_tone_ticks_to_ms(dur_ticks);
            if(dur_ms == 0) dur_ms = 1;
            if(freq == 0) {
                furi_hal_speaker_stop();
                furi_delay_ms(dur_ms);
            } else {
                furi_hal_speaker_start((float)freq, kArduboyToneSoundVolume);
                furi_delay_ms(dur_ms);
                furi_hal_speaker_stop();
            }
        }

        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    return 0;
}

static void arduboy_tone_sound_system_init() {
    if(g_arduboy_sound_queue || g_arduboy_sound_thread) return;
    g_arduboy_sound_queue = furi_message_queue_alloc(4, sizeof(ArduboyToneSoundRequest));
    g_arduboy_sound_thread = furi_thread_alloc();
    furi_thread_set_name(g_arduboy_sound_thread, "ArduboySound");
    furi_thread_set_stack_size(g_arduboy_sound_thread, 1024);
    furi_thread_set_priority(g_arduboy_sound_thread, FuriThreadPriorityNormal);
    furi_thread_set_callback(g_arduboy_sound_thread, arduboy_tone_sound_thread_fn);
    g_arduboy_sound_thread_running = true;
    furi_thread_start(g_arduboy_sound_thread);
}

static void arduboy_tone_sound_system_deinit() {
    if(!g_arduboy_sound_thread) return;
    g_arduboy_sound_thread_running = false;
    furi_thread_join(g_arduboy_sound_thread);
    furi_thread_free(g_arduboy_sound_thread);
    g_arduboy_sound_thread = NULL;
    if(g_arduboy_sound_queue) {
        furi_message_queue_free(g_arduboy_sound_queue);
        g_arduboy_sound_queue = NULL;
    }
}

// API КЛАССОВ
class ArduboyAudio {
public:
    void begin() {
        if(!g_arduboy_audio_enabled) {
            g_arduboy_audio_enabled = true;
            arduboy_tone_sound_system_init();
        }
    }

    void on() {
        bool was_enabled = g_arduboy_audio_enabled;
        g_arduboy_audio_enabled = true;
        if(!was_enabled) {
            arduboy_tone_sound_system_init();
        }
    }

    void off() {
        bool was_enabled = g_arduboy_audio_enabled;
        g_arduboy_audio_enabled = false;
        if(was_enabled) {
            arduboy_tone_sound_system_deinit();
        }
    }

    bool enabled() const { 
        return g_arduboy_audio_enabled; 
    }
    
    void saveOnOff() {}
};

class ArduboyTones {
public:
    explicit ArduboyTones(bool /*enabled*/) {}
    ArduboyTones() {}

    void attachAudio(ArduboyAudio* /*audio*/) {}

    void begin() {
        g_arduboy_audio_enabled = true;
        arduboy_tone_sound_system_init();
    }

    void tones(const uint16_t* pattern) {
        if(!g_arduboy_audio_enabled) return;
        if(!pattern) return;
        if(!g_arduboy_sound_queue) return;

        ArduboyToneSoundRequest req = {.pattern = pattern};
        if(furi_message_queue_put(g_arduboy_sound_queue, &req, 0) != FuriStatusOk) {
            ArduboyToneSoundRequest dummy;
            (void)furi_message_queue_get(g_arduboy_sound_queue, &dummy, 0);
            (void)furi_message_queue_put(g_arduboy_sound_queue, &req, 0);
        }
    }

    void tone(uint16_t frequency, uint16_t duration_ms) {
        if(!g_arduboy_audio_enabled) return;

        uint32_t ticks = (duration_ms * kArduboyToneToneTickHz + 500u) / 1000u;
        if(ticks == 0) ticks = 1;
        if(ticks > 0xFFFFu) ticks = 0xFFFFu;

        uint8_t i = inline_idx;
        inline_idx = (inline_idx + 1) % 8;

        inline_patterns[i][0] = frequency;
        inline_patterns[i][1] = (uint16_t)ticks;
        inline_patterns[i][2] = TONES_END;

        tones(inline_patterns[i]);
    }

    void noTone() {
        tone(0, 1);
    }

private:
    uint16_t inline_patterns[8][3];
    uint8_t inline_idx = 0;
};