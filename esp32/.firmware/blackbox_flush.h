#pragma once
/*
 * blackbox_flush.h — ring bookkeeping for saving a black-box event a slice at
 * a time (blackbox.cpp). Header-only and Arduino-free, so
 * test/test_esp32_core.cpp runs it on the host.
 *
 * The ring holds frames from the oldest (tail) to the newest (head), in time
 * order. A save writes the frames of its window a slice per loop pass, so the
 * loop, CAN included, runs between slices, and the ring keeps recording
 * meanwhile. Only the frames the save has not written yet are protected: with
 * the ring full and the oldest frame being the next one to write, a new frame
 * is dropped instead of evicting it.
 *
 * A trigger while an event is armed: up to the end of that event's post-roll
 * it is inside the window being recorded, and part of that event. After that
 * the event is being saved; the first trigger then waits, and is armed with
 * its own window when the save is over. Its pre-roll is whatever the ring
 * still holds of it by then. A further trigger during the same save is part
 * of the waiting event when it falls inside that event's post-roll, and is
 * missed otherwise.
 */

#include <stdint.h>

// `cap` slots hold at most cap - 1 frames: head == tail is an empty ring.
static inline uint32_t bb_ring_next(uint32_t cap, uint32_t i) { return (i + 1u) % cap; }

static inline uint32_t bb_ring_count(uint32_t cap, uint32_t head, uint32_t tail) {
    return (head + cap - tail) % cap;
}

// Slot for a new frame, evicting the oldest one when the ring is full.
// `keep` is the slot of the next frame a running save still has to write (cap
// when no save runs): it is never evicted, and the new frame is dropped
// instead (returns false).
static inline bool bb_ring_push(uint32_t cap, uint32_t &head, uint32_t &tail,
                                uint32_t keep, uint32_t *slot) {
    if (bb_ring_next(cap, head) == tail) {
        if (tail == keep) return false;
        tail = bb_ring_next(cap, tail);
    }
    *slot = head;
    head = bb_ring_next(cap, head);
    return true;
}

// The frames with lo_ms <= ts <= hi_ms, as `count` frames from logical position
// `first` (0 = the oldest frame in the ring).
struct BBWindow {
    uint32_t first;
    uint32_t count;
};

// The frames are in time order, so two binary searches find the window: the
// cost grows with log(frames in the ring), not with the ring or the window.
// ts_at(k) is the timestamp of the frame at logical position k, k < n.
template <typename TsAt>
static inline BBWindow bb_find_window(uint32_t n, uint32_t lo_ms, uint32_t hi_ms, TsAt ts_at) {
    uint32_t a = 0, b = n;  // first frame at or after lo_ms
    while (a < b) {
        uint32_t m = a + (b - a) / 2u;
        if (ts_at(m) < lo_ms) a = m + 1u; else b = m;
    }
    uint32_t c = a, d = n;  // first frame after hi_ms
    while (c < d) {
        uint32_t m = c + (d - c) / 2u;
        if (ts_at(m) <= hi_ms) c = m + 1u; else d = m;
    }
    BBWindow w = {a, c - a};
    return w;
}

// Frames written in this pass.
static inline uint32_t bb_slice(uint32_t left, uint32_t per_pass) {
    return left < per_pass ? left : per_pass;
}

// What a new trigger does (see the top of this file).
enum BBArmAction : uint8_t {
    BB_ARM_NOW = 0,     // nothing armed: arm it
    BB_ARM_IN_WINDOW,   // inside the armed or the waiting event's window
    BB_ARM_QUEUE,       // the armed event is being saved: wait for it
    BB_ARM_MISSED,      // one is already waiting, and this is outside its window
};

// True while `now_ms` is at most `post_ms` after `trig_ms` (millis() wrap-safe).
static inline bool bb_in_post_roll(uint32_t trig_ms, uint32_t post_ms, uint32_t now_ms) {
    return (int32_t)(now_ms - trig_ms - post_ms) <= 0;
}

static inline BBArmAction bb_arm_action(bool armed, uint32_t trig_ms, bool queued,
                                        uint32_t queued_ms, uint32_t post_ms,
                                        uint32_t now_ms) {
    if (!armed) return BB_ARM_NOW;
    if (bb_in_post_roll(trig_ms, post_ms, now_ms)) return BB_ARM_IN_WINDOW;
    if (!queued) return BB_ARM_QUEUE;
    return bb_in_post_roll(queued_ms, post_ms, now_ms) ? BB_ARM_IN_WINDOW : BB_ARM_MISSED;
}
