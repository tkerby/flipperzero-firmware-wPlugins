// Host tests for the measurement core. Built and run on a dev machine, never
// compiled into the FAP (application.fam pins its sources explicitly).
//
//   cc -I.. -O1 -Wall -Wextra -o /tmp/test_capture test_capture.c ../capture.c -lm && /tmp/test_capture

#include "capture.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    checks++;
    if(!ok) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

static void check_near(float got, float want, float tol, const char* what) {
    checks++;
    if(fabsf(got - want) > tol) {
        failures++;
        printf("  FAIL: %s (got %.3f, want %.3f +/- %.3f)\n", what, got, want, tol);
    }
}

// Feed n samples of a device sitting still, screen up.
static void feed_rest(Capture* c, int n) {
    CaptureSample s = {.ax = 0.0f, .ay = 0.0f, .az = 1.0f, .gx = 0, .gy = 0, .gz = 0};
    for(int i = 0; i < n; i++)
        capture_feed(c, &s);
}

int main(void) {
    printf("capture core tests\n");

    // --- rest: converges on gravity and never triggers ---
    {
        Capture* c = capture_alloc(1.5f);
        capture_arm(c, 1.5f);
        feed_rest(c, 3000);
        check(capture_state(c) == CaptureArmed, "rest does not trigger");
        check_near(capture_live_accel_g(c), 0.0f, 0.05f, "rest reads ~0 g net");
        capture_free(c);
    }

    // --- a swing: wind up one way, accelerate the other, peak at impact ---
    // 300 ms backswing at -400 dps, then 100 ms downswing at +1500 dps with
    // acceleration ramping to 6 g. A 3:1 tempo.
    {
        Capture* c = capture_alloc(1.5f);
        capture_arm(c, 1.5f);
        feed_rest(c, 2000);

        CaptureSample s = {0};
        // Backswing: moving, but gently — above rest, below the trigger.
        for(int i = 0; i < 300; i++) {
            s.ax = 0.4f;
            s.ay = 0.0f;
            s.az = 1.0f;
            s.gx = 0.0f;
            s.gy = -400.0f;
            s.gz = 0.0f;
            capture_feed(c, &s);
        }
        // Downswing: gyro flips sign, acceleration ramps to the peak.
        bool done = false;
        for(int i = 0; i < 100; i++) {
            const float t = (float)i / 100.0f;
            s.ax = 1.0f + 5.0f * t;
            s.ay = 0.0f;
            s.az = 1.0f;
            s.gx = 0.0f;
            s.gy = 300.0f + 1200.0f * t;
            s.gz = 0.0f;
            done |= capture_feed(c, &s);
        }
        check(capture_state(c) == CaptureRecording, "still recording after the swing");

        // Let the post-trigger window run out.
        while(!done) {
            s.ax = 0.0f;
            s.ay = 0.0f;
            s.az = 1.0f;
            s.gx = s.gy = s.gz = 0.0f;
            done = capture_feed(c, &s);
        }
        check(capture_state(c) == CaptureDone, "capture completes");

        CaptureResult r;
        check(capture_result(c, &r), "result available");
        // Peak |a| at the end of the ramp is sqrt(6^2 + 1^2) = 6.08, less 1 g rest.
        check_near(r.peak_accel_g, 5.08f, 0.2f, "peak accel");
        check_near(r.peak_gyro_dps, 1488.0f, 5.0f, "peak gyro"); // ramp ends at t=0.99
        check(r.tempo_valid, "tempo found for a swing");
        check_near(r.tempo_ratio, 3.0f, 0.4f, "3:1 tempo");
        check(r.trace_len > 0, "trace populated");
        capture_free(c);
    }

    // --- an impact: no reversal, so no tempo ---
    {
        Capture* c = capture_alloc(1.5f);
        capture_arm(c, 1.5f);
        feed_rest(c, 2000);

        CaptureSample s = {0};
        bool done = false;
        for(int i = 0; i < 5; i++) {
            s.ax = 0.0f;
            s.ay = 0.0f;
            s.az = 12.0f;
            s.gx = s.gy = s.gz = 0.0f;
            done |= capture_feed(c, &s);
        }
        while(!done) {
            s.az = 1.0f;
            done = capture_feed(c, &s);
        }
        CaptureResult r;
        check(capture_result(c, &r), "impact result available");
        check_near(r.peak_accel_g, 11.0f, 0.2f, "impact peak accel");
        check(!r.tempo_valid, "no tempo for a pure impact");
        capture_free(c);
    }

    // --- free fall: |a| drops to zero, which is still a 1 g event ---
    {
        Capture* c = capture_alloc(0.5f);
        capture_arm(c, 0.5f);
        feed_rest(c, 2000);

        CaptureSample s = {0};
        bool done = false;
        for(int i = 0; i < 200; i++) {
            s.ax = s.ay = s.az = 0.0f;
            s.gx = s.gy = s.gz = 0.0f;
            done |= capture_feed(c, &s);
        }
        check(capture_state(c) != CaptureArmed, "free fall triggers");
        while(!done) {
            s.az = 1.0f;
            done = capture_feed(c, &s);
        }
        CaptureResult r;
        check(capture_result(c, &r), "free fall result available");
        check_near(r.peak_accel_g, 1.0f, 0.1f, "free fall reads ~1 g");
        capture_free(c);
    }

    // --- disarmed: nothing triggers ---
    {
        Capture* c = capture_alloc(1.5f);
        capture_disarm(c);
        CaptureSample s = {.ax = 0, .ay = 0, .az = 20.0f, .gx = 0, .gy = 0, .gz = 0};
        for(int i = 0; i < 100; i++)
            check(!capture_feed(c, &s), "idle never completes");
        check(capture_state(c) == CaptureIdle, "stays idle");
        capture_free(c);
    }

    // --- trigger on the very first sample, with an empty history ring ---
    {
        Capture* c = capture_alloc(1.5f);
        capture_arm(c, 1.5f);
        CaptureSample s = {.ax = 0, .ay = 0, .az = 15.0f, .gx = 0, .gy = 0, .gz = 0};
        bool done = capture_feed(c, &s);
        check(!done, "first sample does not immediately complete");
        check(capture_state(c) == CaptureRecording, "triggers with an empty ring");
        s.az = 1.0f;
        while(!done)
            done = capture_feed(c, &s);
        CaptureResult r;
        check(capture_result(c, &r), "empty-ring result available");
        capture_free(c);
    }

    // --- re-arming after a result clears the previous capture ---
    {
        Capture* c = capture_alloc(1.5f);
        capture_arm(c, 1.5f);
        feed_rest(c, 2000);
        CaptureSample s = {.ax = 0, .ay = 0, .az = 10.0f, .gx = 0, .gy = 0, .gz = 0};
        bool done = false;
        done |= capture_feed(c, &s);
        s.az = 1.0f;
        while(!done)
            done = capture_feed(c, &s);
        CaptureResult first;
        check(capture_result(c, &first), "first result");

        capture_arm(c, 1.5f);
        check(capture_state(c) == CaptureArmed, "re-armed");
        CaptureResult second;
        check(!capture_result(c, &second), "no stale result while armed");
        capture_free(c);
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
