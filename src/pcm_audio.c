#include "pcm_audio.h"
#include "wav_reader.h"

#include <furi_hal.h>
#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_tim.h>
#include <string.h>

#define HALF_SAMPLES   4096U
#define FLAG_STOP      (1U << 0)
#define FLAG_HALF      (1U << 1)
#define FLAG_FULL      (1U << 2)
#define FLAG_DMA_ERROR (1U << 3)

struct PcmAudio {
    Storage* storage;
    char* path;
    FuriThread* thread;
    FuriMutex* mutex;
    PcmAudioState state;
    const char* error;
    uint32_t started_tick;
    uint32_t position_ms;
    uint32_t duration_ms;
    uint8_t samples[HALF_SAMPLES * 2];
};

static void set_state(PcmAudio* audio, PcmAudioState state, const char* error) {
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    audio->state = state;
    audio->error = error;
    furi_mutex_release(audio->mutex);
}

static bool read_at(void* context, uint32_t offset, void* data, size_t size) {
    File* file = context;
    return storage_file_seek(file, offset, true) && storage_file_read(file, data, size) == size;
}

static void dma_isr(void* context) {
    PcmAudio* audio = context;
    uint32_t flags = 0;
    if(LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        flags |= FLAG_HALF;
    }
    if(LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        flags |= FLAG_FULL;
    }
    if(LL_DMA_IsActiveFlag_TE1(DMA1)) {
        LL_DMA_ClearFlag_TE1(DMA1);
        flags |= FLAG_DMA_ERROR;
    }
    if(flags) furi_thread_flags_set(furi_thread_get_id(audio->thread), flags);
}

static void hardware_start(PcmAudio* audio, uint32_t rate) {
    furi_hal_bus_enable(FuriHalBusTIM2);
    /* TIM16 drives the internal speaker, already connected by speaker_acquire.
     * 125 kHz PWM carrier; TIM2 requests one DMA transfer per PCM sample. */
    LL_TIM_SetPrescaler(TIM16, 1);
    LL_TIM_SetAutoReload(TIM16, 255);
    LL_TIM_SetCounter(TIM16, 0);
    LL_TIM_OC_SetMode(TIM16, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_SetCompareCH1(TIM16, 128);
    LL_TIM_OC_DisablePreload(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_CC_EnableChannel(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_GenerateEvent_UPDATE(TIM16);

    LL_TIM_SetPrescaler(TIM2, 0);
    LL_TIM_SetAutoReload(TIM2, (SystemCoreClock + rate / 2) / rate - 1);
    LL_TIM_SetCounter(TIM2, 0);
    LL_TIM_GenerateEvent_UPDATE(TIM2);
    LL_TIM_ClearFlag_UPDATE(TIM2);

    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ClearFlag_GI1(DMA1);
    LL_DMA_ConfigTransfer(
        DMA1,
        LL_DMA_CHANNEL_1,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_HALFWORD | LL_DMA_MDATAALIGN_BYTE |
            LL_DMA_PRIORITY_HIGH);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_TIM2_UP);
    LL_DMA_ConfigAddresses(
        DMA1,
        LL_DMA_CHANNEL_1,
        (uint32_t)audio->samples,
        (uint32_t)&TIM16->CCR1,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, sizeof(audio->samples));
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch1, dma_isr, audio);
    LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_TIM_EnableDMAReq_UPDATE(TIM2);
    LL_TIM_EnableAllOutputs(TIM16);
    LL_TIM_EnableCounter(TIM16);
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    audio->started_tick = furi_get_tick();
    audio->state = PcmAudioPlaying;
    LL_TIM_EnableCounter(TIM2);
    furi_mutex_release(audio->mutex);
}

static void hardware_stop(void) {
    LL_TIM_DisableDMAReq_UPDATE(TIM2);
    LL_TIM_DisableCounter(TIM2);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ClearFlag_GI1(DMA1);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch1, NULL, NULL);
    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_DisableCounter(TIM16);
    furi_hal_bus_disable(FuriHalBusTIM2);
}

static bool fill_half(File* file, uint8_t* samples, uint32_t* remaining) {
    size_t count = MIN(*remaining, HALF_SAMPLES);
    memset(samples, 128, HALF_SAMPLES); /* unsigned PCM silence */
    if(count && storage_file_read(file, samples, count) != count) return false;
    *remaining -= count;
    return true;
}

static int32_t audio_worker(void* context) {
    PcmAudio* audio = context;
    File* file = storage_file_alloc(audio->storage);
    bool speaker_owned = false, hardware_running = false, stop_requested = false;
    WavInfo info;
    const char* error = NULL;
    if(!storage_file_open(file, audio->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        error = "WAV missing.\nPrepare songs on PC\nand copy to SD card.";
        goto cleanup;
    }
    uint64_t size = storage_file_size(file);
    if(size > UINT32_MAX || !wav_read_header(read_at, file, (uint32_t)size, &info)) {
        error = "Use 8-bit mono PCM WAV\n(8000 - 48000 Hz).\nPrepare songs on PC.";
        goto cleanup;
    }
    uint32_t duration = (uint64_t)info.sample_count * 1000 / info.sample_rate;
    uint32_t sample =
        MIN((uint64_t)audio->position_ms * info.sample_rate / 1000, info.sample_count);
    if(audio->position_ms && audio->position_ms >= duration) sample = info.sample_count;
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    audio->duration_ms = duration;
    audio->position_ms = (uint64_t)sample * 1000 / info.sample_rate;
    furi_mutex_release(audio->mutex);
    if(sample == info.sample_count) goto cleanup;
    if(!storage_file_seek(file, info.data_offset + sample, true)) {
        error = "Cannot seek WAV";
        goto cleanup;
    }
    uint32_t remaining = info.sample_count - sample;
    if(!fill_half(file, audio->samples, &remaining) ||
       !fill_half(file, audio->samples + HALF_SAMPLES, &remaining)) {
        error = "Cannot read WAV";
        goto cleanup;
    }
    if(!furi_hal_speaker_acquire(1000)) {
        error = "Speaker is busy";
        goto cleanup;
    }
    speaker_owned = true;
    if(furi_hal_bus_is_enabled(FuriHalBusTIM2)) {
        error = "Audio timer is busy";
        goto cleanup;
    }
    hardware_start(audio, info.sample_rate);
    hardware_running = true;
    uint32_t played = sample;
    uint32_t expected = FLAG_HALF;
    while(true) {
        uint32_t flags = furi_thread_flags_wait(
            FLAG_STOP | FLAG_HALF | FLAG_FULL | FLAG_DMA_ERROR,
            FuriFlagWaitAny,
            furi_ms_to_ticks(1000));
        if(flags & FuriFlagError) {
            error = "Audio DMA timeout";
            break;
        }
        if(flags & FLAG_STOP) {
            stop_requested = true;
            break;
        }
        if((flags & FLAG_DMA_ERROR) || (flags & (FLAG_HALF | FLAG_FULL)) != expected) {
            error = "Audio underrun / DMA error";
            break;
        }
        played += MIN((uint32_t)HALF_SAMPLES, info.sample_count - played);
        if(played == info.sample_count) break;
        size_t offset = (flags & FLAG_HALF) ? 0 : HALF_SAMPLES;
        if(!fill_half(file, audio->samples + offset, &remaining)) {
            error = "SD card audio read failed";
            break;
        }
        expected = (expected == FLAG_HALF) ? FLAG_FULL : FLAG_HALF;
    }
cleanup:
    if(hardware_running) hardware_stop();
    if(speaker_owned) furi_hal_speaker_release();
    storage_file_close(file);
    storage_file_free(file);
    set_state(audio, error ? PcmAudioError : PcmAudioFinished, error);
    /* Keep the thread ID valid until the UI explicitly releases this player. */
    if(!stop_requested) furi_thread_flags_wait(FLAG_STOP, FuriFlagWaitAny, FuriWaitForever);
    return 0;
}

PcmAudio* pcm_audio_start(Storage* storage, const char* path, uint32_t position_ms) {
    PcmAudio* audio = malloc(sizeof(PcmAudio));
    audio->storage = storage;
    audio->path = strdup(path);
    audio->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    audio->state = PcmAudioLoading;
    audio->error = NULL;
    audio->duration_ms = 0;
    audio->started_tick = 0;
    audio->position_ms = position_ms;
    audio->thread = furi_thread_alloc_ex("TaikoAudio", 2048, audio_worker, audio);
    furi_thread_start(audio->thread);
    return audio;
}

PcmAudioState pcm_audio_state(PcmAudio* audio) {
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    PcmAudioState state = audio->state;
    furi_mutex_release(audio->mutex);
    return state;
}

uint32_t pcm_audio_position_ms(PcmAudio* audio) {
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    uint32_t position = audio->position_ms;
    if(audio->state == PcmAudioPlaying) {
        position += (uint64_t)(furi_get_tick() - audio->started_tick) * 1000 /
                    furi_kernel_get_tick_frequency();
        position = MIN(position, audio->duration_ms);
    } else if(audio->state == PcmAudioFinished)
        position = audio->duration_ms;
    furi_mutex_release(audio->mutex);
    return position;
}

uint32_t pcm_audio_duration_ms(PcmAudio* audio) {
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    uint32_t duration = audio->duration_ms;
    furi_mutex_release(audio->mutex);
    return duration;
}

const char* pcm_audio_error(PcmAudio* audio) {
    furi_mutex_acquire(audio->mutex, FuriWaitForever);
    const char* error = audio->error;
    furi_mutex_release(audio->mutex);
    return error;
}

void pcm_audio_free(PcmAudio* audio) {
    furi_thread_flags_set(furi_thread_get_id(audio->thread), FLAG_STOP);
    furi_thread_join(audio->thread);
    furi_thread_free(audio->thread);
    furi_mutex_free(audio->mutex);
    free(audio->path);
    free(audio);
}
