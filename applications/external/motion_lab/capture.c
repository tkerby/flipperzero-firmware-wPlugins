#include "capture.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// One trace point per this many samples. Fixed rather than derived from the
// actual capture length so a short capture simply ends the trace early instead
// of changing the time-per-pixel scale between captures.
#define TRACE_DECIM ((CAPTURE_TOTAL_SAMPLES + CAPTURE_TRACE_LEN - 1u) / CAPTURE_TRACE_LEN)

// Rotation rate above which the swing's main axis is considered established.
// Below this the gyro is mostly noise and picking an axis from it is arbitrary.
#define AXIS_LOCK_DPS 100.0f

// Weight of each new sample in the resting-magnitude average. At 1 kHz this
// settles in well under a second but ignores brief motion.
#define REST_ALPHA 0.002f

// A sample after the derived values have been computed. This is what the
// pre-trigger ring holds, so a capture can replay its own history through
// exactly the same accumulator the live samples go through.
typedef struct {
    float accel_net_g; // |a| - rest, absolute value
    float gyro_dps; // |g|
    float gx, gy, gz; // signed, for finding the direction reversal
} Derived;

struct Capture {
    CaptureState state;
    float trigger_g;

    // Resting acceleration magnitude, tracked whenever the device is still.
    float rest_g;
    bool rest_valid;

    float live_accel_g;
    float live_gyro_dps;

    // Pre-trigger history, newest at (head - 1).
    Derived ring[CAPTURE_PRE_SAMPLES];
    uint32_t head;
    uint32_t filled;

    // --- accumulator, valid from the trigger onwards ---
    uint32_t n; // samples pushed into the accumulator
    uint32_t post_remaining; // live samples still owed after the replay

    float peak_accel_g;
    uint32_t peak_at; // sample index of peak_accel_g
    float peak_gyro_dps;
    uint32_t above_count; // samples above the trigger threshold

    int axis; // 0=x 1=y 2=z, -1 until locked
    float axis_sign; // sign of the locked axis at lock time
    bool reversal_seen;
    uint32_t last_reversal_at; // sample index of the most recent sign change
    uint32_t reversal_at_peak; // last_reversal_at as of the current peak
    bool reversal_at_peak_valid;

    // Trace decimation: max within each bucket.
    uint32_t bucket_n;
    float bucket_accel_max;
    float bucket_gyro_max;
    uint8_t trace_len;
    float trace_accel_g[CAPTURE_TRACE_LEN];
    float trace_gyro_dps[CAPTURE_TRACE_LEN];
};

float capture_accel_magnitude(const CaptureSample* s) {
    return sqrtf(s->ax * s->ax + s->ay * s->ay + s->az * s->az);
}

float capture_gyro_magnitude(const CaptureSample* s) {
    return sqrtf(s->gx * s->gx + s->gy * s->gy + s->gz * s->gz);
}

Capture* capture_alloc(float trigger_g) {
    Capture* c = malloc(sizeof(Capture));
    memset(c, 0, sizeof(Capture));
    c->state = CaptureIdle;
    c->trigger_g = trigger_g;
    c->rest_g = 1.0f; // gravity, until the real value is measured
    c->axis = -1;
    return c;
}

void capture_free(Capture* c) {
    free(c);
}

CaptureState capture_state(const Capture* c) {
    return c->state;
}

float capture_live_accel_g(const Capture* c) {
    return c->live_accel_g;
}

float capture_live_gyro_dps(const Capture* c) {
    return c->live_gyro_dps;
}

void capture_arm(Capture* c, float trigger_g) {
    c->trigger_g = trigger_g;
    c->state = CaptureArmed;
    // The pre-trigger ring is intentionally kept: it is the device's recent
    // history and is exactly what a trigger a moment from now will want.
}

void capture_disarm(Capture* c) {
    c->state = CaptureIdle;
}

static void accumulator_reset(Capture* c) {
    c->n = 0;
    c->peak_accel_g = 0.0f;
    c->peak_at = 0;
    c->peak_gyro_dps = 0.0f;
    c->above_count = 0;
    c->axis = -1;
    c->axis_sign = 0.0f;
    c->reversal_seen = false;
    c->last_reversal_at = 0;
    c->reversal_at_peak = 0;
    c->reversal_at_peak_valid = false;
    c->bucket_n = 0;
    c->bucket_accel_max = 0.0f;
    c->bucket_gyro_max = 0.0f;
    c->trace_len = 0;
}

// Feed one derived sample into the statistics and the trace.
static void accumulator_push(Capture* c, const Derived* d) {
    if(d->accel_net_g > c->peak_accel_g) {
        c->peak_accel_g = d->accel_net_g;
        c->peak_at = c->n;
        // The last reversal before the peak is simply the most recent reversal
        // at the moment the peak occurs, so no history scan is needed.
        c->reversal_at_peak = c->last_reversal_at;
        c->reversal_at_peak_valid = c->reversal_seen;
    }
    if(d->gyro_dps > c->peak_gyro_dps) c->peak_gyro_dps = d->gyro_dps;
    if(d->accel_net_g > c->trigger_g) c->above_count++;

    // Lock onto the axis the rotation is mostly about, then watch that one
    // axis for the sign flip that separates backswing from downswing.
    if(c->axis < 0) {
        if(d->gyro_dps > AXIS_LOCK_DPS) {
            const float ax = fabsf(d->gx), ay = fabsf(d->gy), az = fabsf(d->gz);
            if(ax >= ay && ax >= az) {
                c->axis = 0;
                c->axis_sign = (d->gx < 0.0f) ? -1.0f : 1.0f;
            } else if(ay >= az) {
                c->axis = 1;
                c->axis_sign = (d->gy < 0.0f) ? -1.0f : 1.0f;
            } else {
                c->axis = 2;
                c->axis_sign = (d->gz < 0.0f) ? -1.0f : 1.0f;
            }
        }
    } else {
        const float v = (c->axis == 0) ? d->gx : (c->axis == 1) ? d->gy : d->gz;
        // Hysteresis: only a clear swing past zero counts, so noise around
        // zero cannot manufacture a reversal.
        if(fabsf(v) > AXIS_LOCK_DPS) {
            const float sign = (v < 0.0f) ? -1.0f : 1.0f;
            if(sign != c->axis_sign) {
                c->axis_sign = sign;
                c->last_reversal_at = c->n;
                c->reversal_seen = true;
            }
        }
    }

    // Trace: keep the largest value in each bucket, since this graph exists to
    // show peaks and averaging would flatten exactly what matters.
    if(d->accel_net_g > c->bucket_accel_max) c->bucket_accel_max = d->accel_net_g;
    if(d->gyro_dps > c->bucket_gyro_max) c->bucket_gyro_max = d->gyro_dps;
    c->bucket_n++;
    if(c->bucket_n >= TRACE_DECIM) {
        if(c->trace_len < CAPTURE_TRACE_LEN) {
            c->trace_accel_g[c->trace_len] = c->bucket_accel_max;
            c->trace_gyro_dps[c->trace_len] = c->bucket_gyro_max;
            c->trace_len++;
        }
        c->bucket_n = 0;
        c->bucket_accel_max = 0.0f;
        c->bucket_gyro_max = 0.0f;
    }

    c->n++;
}

// Walk back through the ring to find where motion actually began, so tempo and
// time-to-peak are measured from the start of the movement rather than from
// the moment it crossed the trigger threshold. Returns a count of samples.
static uint32_t find_onset_depth(const Capture* c) {
    uint32_t depth = 0;
    while(depth < c->filled) {
        const uint32_t idx = (c->head + CAPTURE_PRE_SAMPLES - 1u - depth) % CAPTURE_PRE_SAMPLES;
        if(c->ring[idx].accel_net_g <= CAPTURE_REST_G && c->ring[idx].gyro_dps <= CAPTURE_REST_DPS)
            break;
        depth++;
    }
    return depth;
}

static void replay_from_onset(Capture* c, uint32_t depth) {
    for(uint32_t k = depth; k > 0; k--) {
        const uint32_t idx = (c->head + CAPTURE_PRE_SAMPLES - k) % CAPTURE_PRE_SAMPLES;
        accumulator_push(c, &c->ring[idx]);
    }
}

static void finalize(Capture* c, CaptureResult* unused) {
    (void)unused;
    c->state = CaptureDone;
}

bool capture_feed(Capture* c, const CaptureSample* s) {
    const float mag = capture_accel_magnitude(s);
    const float gyro = capture_gyro_magnitude(s);

    // Track the resting magnitude only while genuinely still, so gravity is
    // measured rather than assumed. Taking the absolute difference means free
    // fall reads as ~1 g of event too, which is what a drop test wants.
    if(c->state != CaptureRecording && fabsf(mag - c->rest_g) <= CAPTURE_REST_G &&
       gyro <= CAPTURE_REST_DPS) {
        c->rest_g += REST_ALPHA * (mag - c->rest_g);
        c->rest_valid = true;
    }

    Derived d = {
        .accel_net_g = fabsf(mag - c->rest_g),
        .gyro_dps = gyro,
        .gx = s->gx,
        .gy = s->gy,
        .gz = s->gz,
    };

    c->live_accel_g = d.accel_net_g;
    c->live_gyro_dps = d.gyro_dps;

    if(c->state == CaptureRecording) {
        accumulator_push(c, &d);
        if(c->post_remaining > 0) c->post_remaining--;
        if(c->post_remaining == 0) {
            finalize(c, NULL);
            return true;
        }
        return false;
    }

    // Idle and armed both keep the ring current; only armed can trigger.
    c->ring[c->head] = d;
    c->head = (c->head + 1u) % CAPTURE_PRE_SAMPLES;
    if(c->filled < CAPTURE_PRE_SAMPLES) c->filled++;

    if(c->state == CaptureArmed && d.accel_net_g > c->trigger_g) {
        accumulator_reset(c);
        const uint32_t depth = find_onset_depth(c);
        replay_from_onset(c, depth);
        c->post_remaining = CAPTURE_POST_SAMPLES;
        c->state = CaptureRecording;
    }

    return false;
}

bool capture_result(const Capture* c, CaptureResult* out) {
    if(c->state != CaptureDone) return false;

    memset(out, 0, sizeof(CaptureResult));
    out->peak_accel_g = c->peak_accel_g;
    out->peak_gyro_dps = c->peak_gyro_dps;
    out->duration_ms = (c->above_count * 1000u) / CAPTURE_RATE_HZ;
    out->time_to_peak_ms = (c->peak_at * 1000u) / CAPTURE_RATE_HZ;

    // Tempo needs a reversal that sits strictly between the start of the
    // movement and the peak; an impact has no such reversal and correctly
    // reports no tempo rather than a meaningless ratio.
    if(c->reversal_at_peak_valid && c->reversal_at_peak > 0 && c->peak_at > c->reversal_at_peak) {
        const float back = (float)c->reversal_at_peak;
        const float down = (float)(c->peak_at - c->reversal_at_peak);
        if(down > 0.0f) {
            out->tempo_ratio = back / down;
            out->tempo_valid = true;
        }
    }

    out->trace_len = c->trace_len;
    memcpy(out->trace_accel_g, c->trace_accel_g, sizeof(out->trace_accel_g));
    memcpy(out->trace_gyro_dps, c->trace_gyro_dps, sizeof(out->trace_gyro_dps));
    return true;
}
