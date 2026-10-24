#include "stream_hub.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* TAG = "stream_hub";

struct StreamListener {
    bool in_use;
    uint64_t pos; // absolute read position
    uint64_t dropped; // bytes skipped because this listener fell behind
    bool synced; // an MP3 frame header has been handed over as the first byte
    bool lapped; // overrun since its last read -- excluded from the credit math
    uint32_t sync_scanned; // bytes examined while still hunting for a header
    bool evicted; // hung up on to free the slot; the owning task must exit
    int64_t last_read_us; // when its task last took bytes -- see the stall rule
    bool stall_logged; // one line per stall episode, not one per credit tick
};

static struct {
    uint8_t* ring;
    uint64_t written; // absolute write position, only ever increases
    uint64_t floor; // no listener may start before this (set by a track flush)
    StreamListener listeners[STREAM_MAX_LISTENERS];
    char meta[STREAM_META_MAX];
    SemaphoreHandle_t lock;
} hub;

// The hub is touched from the UART task and from one task per listener, so
// every public call takes this lock. Critical sections are memcpy-short.
static inline void hub_lock(void) {
    xSemaphoreTake(hub.lock, portMAX_DELAY);
}
static inline void hub_unlock(void) {
    xSemaphoreGive(hub.lock);
}

void stream_hub_init(void) {
    memset(&hub, 0, sizeof(hub));
    // PSRAM first, internal RAM only as a fallback. MALLOC_CAP_8BIT is
    // satisfied from internal memory first, so asking for it put this 64 KB in
    // the one place it must not be: a listener's 6 KB task stack is forced into
    // internal RAM and cannot live out here, so every kilobyte the ring takes
    // internally is a kilobyte no second listener can have. With the ring
    // internal the board hit 259 bytes free on the third listener and lwip
    // could not allocate a pbuf, which dropped every stream at once. The ring
    // is only ever touched by memcpy -- never DMA -- so external RAM suits it.
    hub.ring = heap_caps_malloc(STREAM_RING_SIZE, MALLOC_CAP_SPIRAM);
    const bool ring_external = (hub.ring != NULL);
    if(!hub.ring) {
        hub.ring = heap_caps_malloc(STREAM_RING_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    assert(hub.ring);
    hub.lock = xSemaphoreCreateMutex();
    assert(hub.lock);
    snprintf(hub.meta, sizeof(hub.meta), "{\"title\":\"\",\"artist\":\"\"}");
    ESP_LOGI(
        TAG,
        "ring %d bytes in %s RAM, max %d listeners (%u internal free)",
        STREAM_RING_SIZE,
        ring_external ? "external" : "internal",
        STREAM_MAX_LISTENERS,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

bool stream_hub_ready(void) {
    return hub.lock != NULL;
}

void stream_hub_write(const uint8_t* data, size_t len) {
    if(!data || !len) return;

    // A write larger than the ring can only leave the final STREAM_RING_SIZE
    // bytes; skip ahead rather than wrapping over ourselves repeatedly.
    if(len > STREAM_RING_SIZE) {
        data += len - STREAM_RING_SIZE;
        len = STREAM_RING_SIZE;
    }

    hub_lock();
    size_t off = (size_t)(hub.written % STREAM_RING_SIZE);
    size_t first = STREAM_RING_SIZE - off;
    if(first > len) first = len;
    memcpy(hub.ring + off, data, first);
    if(len > first) memcpy(hub.ring, data + first, len - first);
    hub.written += len;

    // Fast-forward anyone who just got lapped, so they resume at the live edge
    // instead of reading bytes that have already been overwritten.
    uint64_t oldest = (hub.written > STREAM_RING_SIZE) ? hub.written - STREAM_RING_SIZE : 0;
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        StreamListener* l = &hub.listeners[i];
        if(l->in_use && l->pos < oldest) {
            l->dropped += oldest - l->pos;
            l->pos = oldest;
            // It has already lost audio, so it no longer constrains the writer;
            // see hub_max_backlog_locked() for why that matters.
            l->lapped = true;
            l->synced = false;
            l->sync_scanned = 0;
        }
    }
    hub_unlock();
}

void stream_hub_flush(void) {
    hub_lock();
    // Jump every listener to the live edge; the buffered bytes belong to a
    // track that is no longer playing. The floor stops a listener that joins
    // afterwards from priming itself out of the tail of that old track.
    hub.floor = hub.written;
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        if(!hub.listeners[i].in_use) continue;
        hub.listeners[i].pos = hub.written;
        hub.listeners[i].synced = false;
        hub.listeners[i].sync_scanned = 0;
    }
    hub_unlock();
}

StreamListener* stream_hub_listener_add(void) {
    StreamListener* out = NULL;
    hub_lock();
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        if(!hub.listeners[i].in_use) {
            hub.listeners[i].in_use = true;
            hub.listeners[i].dropped = 0;
            hub.listeners[i].lapped = false;
            hub.listeners[i].synced = false;
            hub.listeners[i].sync_scanned = 0;
            hub.listeners[i].evicted = false;
            hub.listeners[i].last_read_us = esp_timer_get_time();
            hub.listeners[i].stall_logged = false;
            // Not the bare live edge: iOS will not commit to decoding until it
            // has several frames in hand, and at the live edge they arrive one
            // 512-byte chunk per 32 ms. Hand over a short head start instead,
            // clamped to what the ring still holds and to the current track.
            uint64_t start =
                (hub.written > STREAM_PRIME_BYTES) ? hub.written - STREAM_PRIME_BYTES : 0;
            const uint64_t oldest =
                (hub.written > STREAM_RING_SIZE) ? hub.written - STREAM_RING_SIZE : 0;
            if(start < oldest) start = oldest;
            if(start < hub.floor) start = hub.floor;
            hub.listeners[i].pos = start;
            out = &hub.listeners[i];
            break;
        }
    }
    hub_unlock();
    if(!out) ESP_LOGW(TAG, "listener rejected: %d already connected", STREAM_MAX_LISTENERS);
    return out;
}

void stream_hub_listener_remove(StreamListener* l) {
    if(!l) return;
    hub_lock();
    if(l->dropped) ESP_LOGW(TAG, "listener dropped %llu bytes while behind", l->dropped);
    l->in_use = false;
    hub_unlock();
}

size_t stream_hub_listener_read(StreamListener* l, uint8_t* out, size_t max) {
    if(!l || !out || !max) return 0;
    hub_lock();
    if(l->evicted) {
        hub_unlock();
        return 0;
    }
    size_t avail = (size_t)(hub.written - l->pos);
    if(avail > max) avail = max;

    // A cursor lands wherever the writer happened to be, which is almost never
    // a frame boundary. A decoder mid-stream resyncs by itself, but the FIRST
    // bytes of an audio/mpeg response are how iOS decides what the format is --
    // give it the tail of a frame and it never starts at all. So hold the first
    // handover back until a header is in view, and begin the stream there.
    if(!l->synced && avail < 4) {
        hub_unlock();
        return 0;
    }

    if(avail) {
        size_t off = (size_t)(l->pos % STREAM_RING_SIZE);
        size_t first = STREAM_RING_SIZE - off;
        if(first > avail) first = avail;
        memcpy(out, hub.ring + off, first);
        if(avail > first) memcpy(out + first, hub.ring, avail - first);
        l->pos += avail;
        l->lapped = false; // it is draining, so let it count again
        l->last_read_us = esp_timer_get_time();
        l->stall_logged = false;

        if(!l->synced) {
            const int32_t at = radio_mp3_find_header(out, avail);
            if(at < 0 && l->sync_scanned + avail >= STREAM_SYNC_SCAN_MAX) {
                // Whatever is on the wire is not parsing as MP3. Stream it
                // anyway rather than stay silent: being wrong about the frame
                // boundary costs one glitch, staying silent costs the track.
                ESP_LOGW(
                    TAG,
                    "no frame header in %u bytes, streaming unsynced",
                    (unsigned)(l->sync_scanned + avail));
                l->synced = true;
            } else if(at < 0) {
                // Rewind the last 3 bytes: a header can straddle two reads.
                const size_t keep = (avail >= 3) ? 3 : avail;
                l->sync_scanned += (uint32_t)(avail - keep);
                l->pos -= keep;
                avail = 0;
            } else {
                l->synced = true;
                l->sync_scanned = 0;
                avail -= (size_t)at;
                if(at > 0) memmove(out, out + at, avail);
            }
        }
    }
    hub_unlock();
    return avail;
}

bool stream_hub_listener_evicted(StreamListener* l) {
    if(!l) return true;
    hub_lock();
    const bool e = l->evicted;
    hub_unlock();
    return e;
}

bool stream_hub_evict_stalest(void) {
    StreamListener* victim = NULL;
    hub_lock();
    size_t worst = 0;
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        StreamListener* l = &hub.listeners[i];
        if(!l->in_use || l->evicted) continue;
        const size_t behind = (size_t)(hub.written - l->pos);
        if(!victim || behind >= worst) {
            worst = behind;
            victim = l;
        }
    }
    if(victim) victim->evicted = true;
    hub_unlock();
    if(victim) ESP_LOGW(TAG, "station full: evicted a listener %u bytes behind", (unsigned)worst);
    return victim != NULL;
}

uint8_t stream_hub_listener_count(void) {
    uint8_t n = 0;
    hub_lock();
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        if(hub.listeners[i].in_use) n++;
    }
    hub_unlock();
    return n;
}

// Backlog of the listener that is furthest behind — that is what determines
// how full the buffer effectively is and how much credit is safe to grant.
static size_t hub_max_backlog_locked(void) {
    size_t worst = 0;
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        StreamListener* l = &hub.listeners[i];
        if(!l->in_use) continue;
        // A listener that has been lapped and has not read since is a socket
        // nobody is draining -- a phone that paused, locked, or walked off. Its
        // backlog pins at the full ring, which drives credit to zero and stops
        // the Flipper sending to EVERY listener. It has already lost its audio;
        // it does not also get to take the station down.
        if(l->lapped) continue;
        // Same reasoning, but for the case that actually happens: a browser
        // fills its own buffer and stops reading the socket, so this
        // listener's task is blocked in httpd_send and takes nothing. Being
        // lapped would excuse it, but it cannot BE lapped while the writer is
        // stopped -- and the writer is stopped because of this backlog. Time
        // is the only signal available that does not depend on the writer.
        if(l->last_read_us &&
           (esp_timer_get_time() - l->last_read_us) > (int64_t)STREAM_STALL_IGNORE_MS * 1000) {
            if(!l->stall_logged) {
                l->stall_logged = true;
                ESP_LOGW(
                    TAG,
                    "listener read nothing for %dms and is %u behind; not holding the "
                    "station back",
                    STREAM_STALL_IGNORE_MS,
                    (unsigned)(hub.written - l->pos));
            }
            continue;
        }
        size_t behind = (size_t)(hub.written - l->pos);
        if(behind > worst) worst = behind;
    }
    return worst;
}

uint8_t stream_hub_fill_percent(void) {
    hub_lock();
    size_t worst = hub_max_backlog_locked();
    hub_unlock();
    if(worst > STREAM_RING_SIZE) worst = STREAM_RING_SIZE;
    return (uint8_t)((worst * 100) / STREAM_RING_SIZE);
}

uint16_t stream_hub_credit(void) {
    hub_lock();
    size_t worst = hub_max_backlog_locked();
    uint8_t listeners = 0;
    for(int i = 0; i < STREAM_MAX_LISTENERS; i++) {
        if(hub.listeners[i].in_use) listeners++;
    }
    hub_unlock();

    // With nobody listening there is no backlog to protect, so the only limit
    // is what a u16 can express. The Flipper still paces at the CBR rate.
    size_t headroom = STREAM_RING_SIZE;
    if(listeners > 0) headroom = (STREAM_RING_SIZE > worst) ? STREAM_RING_SIZE - worst : 0;
    // Never zero. A listener may slow the station down to protect its audio;
    // it may not stop it. At zero the Flipper stops writing, and every way out
    // of a stuck listener -- lapping it, timing it out, noticing its socket
    // died -- needs writes to keep happening.
    if(headroom < STREAM_MIN_CREDIT) headroom = STREAM_MIN_CREDIT;
    return (headroom > UINT16_MAX) ? UINT16_MAX : (uint16_t)headroom;
}

void stream_hub_set_meta(const char* json) {
    if(!json) return;
    hub_lock();
    snprintf(hub.meta, sizeof(hub.meta), "%s", json);
    hub_unlock();
}

void stream_hub_get_meta(char* out, size_t out_cap) {
    if(!out || !out_cap) return;
    hub_lock();
    snprintf(out, out_cap, "%s", hub.meta);
    hub_unlock();
}

uint64_t stream_hub_total_written(void) {
    hub_lock();
    uint64_t n = hub.written;
    hub_unlock();
    return n;
}
