#include "taiko_sfx.h"
#include "wav_reader.h"

#include <furi.h>
#include <stdlib.h>

#define SFX_PATH        EXT_PATH("apps_data/taiko_player/Sfx")
#define SFX_MAX_SAMPLES 32768U

static bool read_at(void* context, uint32_t offset, void* data, size_t size) {
    File* file = context;
    return storage_file_seek(file, offset, true) && storage_file_read(file, data, size) == size;
}

static TaikoPcmClip load_clip(Storage* storage, const char* path) {
    TaikoPcmClip clip = {0};
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        WavInfo info;
        if(size <= UINT32_MAX && wav_read_header(read_at, file, size, &info) &&
           info.sample_count <= SFX_MAX_SAMPLES) {
            uint8_t* samples = malloc(info.sample_count);
            if(samples && read_at(file, info.data_offset, samples, info.sample_count)) {
                clip = (TaikoPcmClip){samples, info.sample_count, info.sample_rate};
            } else {
                free(samples);
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return clip;
}

TaikoSfx taiko_sfx_load(Storage* storage) {
    return (TaikoSfx){
        .don = load_clip(storage, SFX_PATH "/dong.wav"),
        .ka = load_clip(storage, SFX_PATH "/ka.wav"),
    };
}

void taiko_sfx_free(TaikoSfx* sfx) {
    free((void*)sfx->don.samples);
    free((void*)sfx->ka.samples);
    *sfx = (TaikoSfx){0};
}
