#include "mp3_pacer.h"

#include <furi.h>
#include <storage/storage.h>

#define TAG "Mp3Pacer"

// One AUDIO_DATA payload. 512 bytes is ~32 ms of 128 kbps audio, so the pacer
// wakes ~31 times a second — frequent enough to stay smooth, rare enough that
// the scheduler is never the bottleneck.
#define CHUNK RADIO_MAX_PAYLOAD

// How far into the file to look for the first frame header. ID3v2 tags are
// usually a few hundred bytes but album art can push them much further.
//
// This buffer is heap-allocated, never a local: 8 KB on a worker stack is four
// times the whole stack and the MPU kills the process instantly.
#define HEADER_SCAN_BYTES 8192

#define FLAG_STOP   (1u << 0)
#define FLAG_CREDIT (1u << 1)
#define FLAG_ALL    (FLAG_STOP | FLAG_CREDIT)

struct Mp3Pacer {
    RadioUart* uart;
    Storage* storage;
    FuriThread* worker;
    FuriEventFlag* flags;
    FuriMutex* lock;

    char path[256];
    bool running;

    // --- guarded by lock ---
    Mp3PacerState state;
    uint16_t bitrate_kbps;
    uint64_t sent;
    uint64_t total;
    uint32_t credit;
    bool starved;
    // Audio bytes already sent for this track. A pause parks the position here
    // and a resume seeks to it, so the two share one field rather than the
    // worker having to be told which kind of start it is.
    uint64_t resume_at;
};

static void pacer_set_state(Mp3Pacer* p, Mp3PacerState s) {
    furi_mutex_acquire(p->lock, FuriWaitForever);
    p->state = s;
    furi_mutex_release(p->lock);
}

// Copy the track name out of a path and into a JSON string body, escaping what
// JSON requires. Filenames routinely contain quotes and backslashes, and an
// unescaped one would produce a broken "now playing" payload.
static void json_escape_name(const char* path, char* out, size_t out_cap) {
    const char* base = path;
    for(const char* c = path; *c; c++) {
        if(*c == '/') base = c + 1;
    }

    size_t n = radio_strnlen(base, 255);
    // Drop a trailing ".mp3" so the display shows the track, not the file.
    if(n > 4 && (base[n - 4] == '.') && (base[n - 3] == 'm' || base[n - 3] == 'M')) n -= 4;

    size_t o = 0;
    for(size_t i = 0; i < n && o + 2 < out_cap; i++) {
        const unsigned char ch = (unsigned char)base[i];
        if(ch == '"' || ch == '\\') {
            out[o++] = '\\';
            out[o++] = (char)ch;
        } else if(ch < 0x20) {
            out[o++] = ' '; // control characters are not legal raw in JSON
        } else {
            out[o++] = (char)ch;
        }
    }
    out[o] = '\0';
}

// Find the first MP3 frame header and return its bitrate, seeking the file to
// that header so streaming starts on a frame boundary rather than mid-tag.
static uint16_t find_stream_start(File* file, uint64_t* start_offset) {
    uint8_t* scan = malloc(HEADER_SCAN_BYTES);
    if(!scan) return 0;

    const uint16_t got = storage_file_read(file, scan, HEADER_SCAN_BYTES);
    uint16_t kbps = 0;

    if(got >= 4) {
        const int32_t at = radio_mp3_find_header(scan, got);
        if(at >= 0) {
            *start_offset = (uint64_t)at;
            storage_file_seek(file, (uint32_t)at, true);
            kbps = radio_mp3_bitrate_kbps(&scan[at]);
        }
    }

    free(scan);
    return kbps;
}

static int32_t pacer_worker(void* ctx) {
    Mp3Pacer* p = ctx;

    File* file = storage_file_alloc(p->storage);
    if(!storage_file_open(file, p->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "cannot open %s", p->path);
        pacer_set_state(p, Mp3PacerErrorOpen);
        storage_file_free(file);
        return 0;
    }

    uint64_t start_offset = 0;
    const uint16_t kbps = find_stream_start(file, &start_offset);
    if(kbps == 0) {
        FURI_LOG_E(TAG, "no MPEG1 Layer III header in %s", p->path);
        pacer_set_state(p, Mp3PacerErrorFormat);
        storage_file_close(file);
        storage_file_free(file);
        return 0;
    }

    // bytes/sec = kbps * 1000 / 8. This is the entire timing model.
    const uint32_t bytes_per_sec = (uint32_t)kbps * 125u;
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    const uint64_t size = storage_file_size(file);
    const uint64_t track_bytes = (size > start_offset) ? size - start_offset : 0;

    // Resuming: skip to the byte the pause stopped on. Clamped because the file
    // can have been replaced on the SD card while we were parked on it.
    furi_mutex_acquire(p->lock, FuriWaitForever);
    uint64_t resume_at = p->resume_at;
    furi_mutex_release(p->lock);
    if(resume_at > track_bytes) resume_at = 0;
    if(resume_at > 0) storage_file_seek(file, (uint32_t)(start_offset + resume_at), true);

    furi_mutex_acquire(p->lock, FuriWaitForever);
    p->bitrate_kbps = kbps;
    p->total = track_bytes;
    p->sent = resume_at;
    p->state = Mp3PacerPlaying;
    furi_mutex_release(p->lock);

    // A new track replaces whatever the station was buffering: on a radio you
    // hear the skip, you do not hear the old track drain out first.
    radio_uart_send(p->uart, RADIO_MSG_FLUSH, NULL, 0);

    char meta[RADIO_META_MAX];
    char name[160];
    json_escape_name(p->path, name, sizeof(name));
    int meta_len = snprintf(meta, sizeof(meta), "{\"title\":\"%s\",\"artist\":\"\"}", name);
    if(meta_len > 0 && (size_t)meta_len < sizeof(meta))
        radio_uart_send(p->uart, RADIO_MSG_TRACK_META, (const uint8_t*)meta, (uint16_t)meta_len);

    // start_tick is the virtual instant byte 0 went out, NOT when this thread
    // started: `due` below is computed from the cumulative byte count, so on a
    // resume the clock has to be rebased by the bytes already sent or every
    // chunk after the gap reads as overdue and the whole rest of the track is
    // fired off as fast as the link takes it.
    const uint32_t start_tick =
        furi_get_tick() - (uint32_t)((resume_at * tick_hz) / bytes_per_sec);
    uint64_t sent_total = resume_at;
    uint8_t buf[CHUNK];
    bool stopped = false;

    while(!stopped) {
        // --- flow control: never exceed what the ESP said it can hold ---
        furi_mutex_acquire(p->lock, FuriWaitForever);
        bool have_credit = p->credit >= CHUNK;
        p->starved = !have_credit;
        furi_mutex_release(p->lock);

        if(!have_credit) {
            // CREDIT arrives every 250 ms; wake early if one lands sooner.
            const uint32_t f = furi_event_flag_wait(
                p->flags, FLAG_ALL, FuriFlagWaitAny | FuriFlagNoClear, furi_ms_to_ticks(300));
            if(!(f & FuriFlagError) && (f & FLAG_STOP)) break;
            furi_event_flag_clear(p->flags, FLAG_CREDIT);
            continue;
        }

        const uint16_t n = storage_file_read(file, buf, sizeof(buf));
        if(n == 0) {
            pacer_set_state(p, Mp3PacerFinished);
            break;
        }

        if(!radio_uart_send(p->uart, RADIO_MSG_AUDIO_DATA, buf, n)) break;
        sent_total += n;

        furi_mutex_acquire(p->lock, FuriWaitForever);
        p->sent = sent_total;
        p->credit = (p->credit > n) ? p->credit - n : 0;
        furi_mutex_release(p->lock);

        // Pace against the start of the track, not against the previous chunk:
        // per-chunk sleeps accumulate every rounding error and the stream
        // drifts out of real time over a few minutes.
        const uint32_t due = start_tick + (uint32_t)((sent_total * tick_hz) / bytes_per_sec);
        const int32_t wait = (int32_t)(due - furi_get_tick());
        if(wait > 0) {
            const uint32_t f = furi_event_flag_wait(
                p->flags, FLAG_STOP, FuriFlagWaitAny | FuriFlagNoClear, (uint32_t)wait);
            if(!(f & FuriFlagError) && (f & FLAG_STOP)) stopped = true;
        }

        if(furi_event_flag_get(p->flags) & FLAG_STOP) stopped = true;
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_mutex_acquire(p->lock, FuriWaitForever);
    p->starved = false;
    if(p->state == Mp3PacerPlaying) p->state = Mp3PacerIdle;
    furi_mutex_release(p->lock);
    return 0;
}

Mp3Pacer* mp3_pacer_alloc(RadioUart* uart) {
    Mp3Pacer* p = malloc(sizeof(Mp3Pacer));
    memset(p, 0, sizeof(Mp3Pacer));
    p->uart = uart;
    p->storage = furi_record_open(RECORD_STORAGE);
    p->flags = furi_event_flag_alloc();
    p->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    p->state = Mp3PacerIdle;
    return p;
}

void mp3_pacer_free(Mp3Pacer* p) {
    if(!p) return;
    mp3_pacer_stop(p);
    furi_record_close(RECORD_STORAGE);
    furi_event_flag_free(p->flags);
    furi_mutex_free(p->lock);
    free(p);
}

// Spin up the worker. Caller has already decided `resume_at` and the state.
static void pacer_launch(Mp3Pacer* p) {
    furi_event_flag_clear(p->flags, FLAG_ALL);
    p->worker = furi_thread_alloc_ex("Mp3Pacer", 4096, pacer_worker, p);
    furi_thread_start(p->worker);
    p->running = true;
}

// Bring the worker down without touching position or state, so pause and stop
// can share it and differ only in what they do afterwards.
static void pacer_halt(Mp3Pacer* p) {
    if(!p->running) return;
    furi_event_flag_set(p->flags, FLAG_STOP);
    furi_thread_join(p->worker);
    furi_thread_free(p->worker);
    p->worker = NULL;
    p->running = false;
    furi_event_flag_clear(p->flags, FLAG_ALL);
}

bool mp3_pacer_play(Mp3Pacer* p, const char* path) {
    if(!p || !path) return false;
    mp3_pacer_stop(p);

    snprintf(p->path, sizeof(p->path), "%s", path);

    furi_mutex_acquire(p->lock, FuriWaitForever);
    p->sent = 0;
    p->total = 0;
    p->bitrate_kbps = 0;
    p->starved = false;
    p->resume_at = 0; // a new track always starts at the top
    p->state = Mp3PacerPlaying;
    furi_mutex_release(p->lock);

    pacer_launch(p);
    return true;
}

bool mp3_pacer_replay(Mp3Pacer* p) {
    if(!p || p->path[0] == '\0') return false;
    // Deliberately goes through play(), which resets resume_at: "play again"
    // after a track has finished has to start at the top, and the parked
    // position at that point is the end of the file.
    return mp3_pacer_play(p, p->path);
}

void mp3_pacer_stop(Mp3Pacer* p) {
    if(!p) return;

    furi_mutex_acquire(p->lock, FuriWaitForever);
    const bool was_active = p->running || p->state == Mp3PacerPlaying ||
                            p->state == Mp3PacerPaused;
    furi_mutex_release(p->lock);

    pacer_halt(p);

    furi_mutex_acquire(p->lock, FuriWaitForever);
    p->resume_at = 0;
    p->starved = false;
    if(p->state == Mp3PacerPlaying || p->state == Mp3PacerPaused) p->state = Mp3PacerIdle;
    furi_mutex_release(p->lock);

    // Only when something was actually on air. stop() is also the first thing
    // play() calls, and flushing a station that is already silent just costs a
    // frame on the wire.
    if(was_active) radio_uart_send(p->uart, RADIO_MSG_FLUSH, NULL, 0);
}

void mp3_pacer_pause(Mp3Pacer* p) {
    if(!p || !p->running) return;

    pacer_halt(p);

    furi_mutex_acquire(p->lock, FuriWaitForever);
    // The worker leaves p->sent at the last byte it handed to the UART, which
    // is exactly where the resume has to pick up.
    p->resume_at = p->sent;
    p->starved = false;
    // Finished/error states survive a pause press: there is nothing to park.
    if(p->state == Mp3PacerPlaying || p->state == Mp3PacerIdle) p->state = Mp3PacerPaused;
    furi_mutex_release(p->lock);

    radio_uart_send(p->uart, RADIO_MSG_FLUSH, NULL, 0);
}

bool mp3_pacer_resume(Mp3Pacer* p) {
    if(!p || p->running || p->path[0] == '\0') return false;

    furi_mutex_acquire(p->lock, FuriWaitForever);
    const bool paused = (p->state == Mp3PacerPaused);
    if(paused) {
        p->starved = false;
        p->state = Mp3PacerPlaying;
    }
    furi_mutex_release(p->lock);
    if(!paused) return false;

    pacer_launch(p);
    return true;
}

bool mp3_pacer_has_track(Mp3Pacer* p) {
    return p && p->path[0] != '\0';
}

void mp3_pacer_credit(Mp3Pacer* p, uint16_t credit) {
    if(!p) return;
    furi_mutex_acquire(p->lock, FuriWaitForever);
    // An absolute grant of remaining space, not an increment — see protocol.md.
    p->credit = credit;
    furi_mutex_release(p->lock);
    furi_event_flag_set(p->flags, FLAG_CREDIT);
}

void mp3_pacer_status(Mp3Pacer* p, Mp3PacerStatus* out) {
    if(!p || !out) return;
    furi_mutex_acquire(p->lock, FuriWaitForever);
    out->state = p->state;
    out->bitrate_kbps = p->bitrate_kbps;
    out->sent = p->sent;
    out->total = p->total;
    out->starved = p->starved;
    furi_mutex_release(p->lock);
}
