#pragma once

#include "taiko_mixer.h"

#include <furi.h>
#include <storage/storage.h>

typedef enum {
    PcmAudioLoading,
    PcmAudioPlaying,
    PcmAudioPaused,
    PcmAudioFinished,
    PcmAudioError,
} PcmAudioState;

typedef struct PcmAudio PcmAudio;

/* Worker starts at the requested WAV position and owns speaker, file, timers
 * and DMA. Freeing joins the worker and releases all hardware. */
PcmAudio* pcm_audio_start(
    Storage* storage,
    const char* path,
    uint32_t position_ms,
    const TaikoMixer* mixer);
PcmAudioState pcm_audio_state(PcmAudio* audio);
uint32_t pcm_audio_position_ms(PcmAudio* audio);
uint32_t pcm_audio_duration_ms(PcmAudio* audio);
const char* pcm_audio_error(PcmAudio* audio);
void pcm_audio_free(PcmAudio* audio);
