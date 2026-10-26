// Motion Lab — measurement core.
//
// Deliberately free of any furi/Flipper dependency so the whole measurement
// path can be compiled and tested on a host machine. Everything here is plain
// C and floats; the app layer feeds it samples and reads results back out.

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Samples arrive at 1 kHz. A capture keeps 400 ms of pre-trigger history plus
// up to 1.1 s after, which comfortably spans a golf swing, a bat swing or a
// drop. Magnitudes are stored decimated to CAPTURE_TRACE_LEN points for the
// graph; the peak statistics are computed at full rate, never from the trace.
#define CAPTURE_RATE_HZ       1000u
#define CAPTURE_PRE_MS        400u
#define CAPTURE_POST_MS       1100u
#define CAPTURE_PRE_SAMPLES   ((CAPTURE_RATE_HZ * CAPTURE_PRE_MS) / 1000u)
#define CAPTURE_POST_SAMPLES  ((CAPTURE_RATE_HZ * CAPTURE_POST_MS) / 1000u)
#define CAPTURE_TOTAL_SAMPLES (CAPTURE_PRE_SAMPLES + CAPTURE_POST_SAMPLES)

// 128 = the Flipper screen width, so one trace point per pixel column.
#define CAPTURE_TRACE_LEN 128u

// Below these the device is treated as at rest. Gravity alone reads ~1.0 g, so
// the resting magnitude is subtracted before comparing against thresholds.
//
// Rotation has its own threshold because acceleration alone does not detect
// motion: a backswing is slow and smooth, carrying hundreds of deg/s while
// barely registering above gravity. Judging onset on acceleration only would
// throw the whole windup away and leave nothing to measure tempo against.
#define CAPTURE_REST_G   0.25f
#define CAPTURE_REST_DPS 40.0f

typedef struct {
    float ax, ay, az; // g
    float gx, gy, gz; // deg/s
} CaptureSample;

typedef enum {
    CaptureIdle = 0, // not armed, ignoring samples
    CaptureArmed = 1, // watching for the trigger
    CaptureRecording = 2, // trigger seen, filling the post-trigger window
    CaptureDone = 3, // a result is ready to read
} CaptureState;

typedef struct {
    float peak_accel_g; // highest total acceleration, gravity removed
    float peak_gyro_dps; // highest total rotation rate
    uint32_t duration_ms; // time spent above the trigger threshold
    uint32_t time_to_peak_ms; // from motion onset to peak acceleration

    // Backswing:downswing ratio, the standard way swing tempo is quoted (a
    // 3:1 golf tempo gives 3.0). Zero when no direction reversal was found,
    // which is the normal case for an impact rather than a swing.
    float tempo_ratio;
    bool tempo_valid;

    // Decimated magnitude trace for drawing, newest-last. Accel in g.
    uint8_t trace_len;
    float trace_accel_g[CAPTURE_TRACE_LEN];
    float trace_gyro_dps[CAPTURE_TRACE_LEN];
} CaptureResult;

typedef struct Capture Capture;

// trigger_g is the acceleration above rest that starts a recording.
Capture* capture_alloc(float trigger_g);
void capture_free(Capture* c);

// Discard any in-progress capture and start watching for a new trigger.
void capture_arm(Capture* c, float trigger_g);

// Stop watching. Samples fed while idle are still used to track the resting
// orientation, so arming after a period of stillness is immediately accurate.
void capture_disarm(Capture* c);

CaptureState capture_state(const Capture* c);

// Feed one sample. Returns true when this sample completed a capture, i.e.
// the state just became CaptureDone and a result can be read.
bool capture_feed(Capture* c, const CaptureSample* s);

// Valid only in CaptureDone. Returns false otherwise and leaves out untouched.
bool capture_result(const Capture* c, CaptureResult* out);

// Live magnitudes for the idle/armed readout, gravity removed from accel.
float capture_live_accel_g(const Capture* c);
float capture_live_gyro_dps(const Capture* c);

// Total acceleration magnitude in g, including gravity.
float capture_accel_magnitude(const CaptureSample* s);
// Total rotation rate magnitude in deg/s.
float capture_gyro_magnitude(const CaptureSample* s);
