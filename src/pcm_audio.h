#pragma once

#include <furi.h>
#include <storage/storage.h>

typedef enum {
    PcmAudioLoading,
    PcmAudioPlaying,
    PcmAudioFinished,
    PcmAudioError,
} PcmAudioState;

typedef struct PcmAudio PcmAudio;

/* Worker owns speaker, file, timers and DMA; join before freeing. */
PcmAudio* pcm_audio_start(Storage* storage, const char* path);
PcmAudioState pcm_audio_state(PcmAudio* audio);
uint32_t pcm_audio_position_ms(PcmAudio* audio);
const char* pcm_audio_error(PcmAudio* audio);
void pcm_audio_free(PcmAudio* audio);
