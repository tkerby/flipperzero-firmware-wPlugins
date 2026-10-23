#pragma once
// Reads an MP3 off the SD card and feeds it to the ESP at the file's own
// constant bitrate. That bitrate IS the clock: neither chip decodes audio, so
// the only thing keeping the stream in real time is sending bytes at exactly
// the rate a player consumes them.

#include <stdbool.h>
#include <stdint.h>

#include "radio_uart.h"

typedef enum {
    Mp3PacerIdle = 0,
    Mp3PacerPlaying,
    Mp3PacerPaused, // holding position, resumable from the same byte
    Mp3PacerFinished, // reached end of file cleanly
    Mp3PacerErrorOpen, // could not open the file
    Mp3PacerErrorFormat, // not a constant-bitrate MPEG1 Layer III file
} Mp3PacerState;

typedef struct {
    Mp3PacerState state;
    uint16_t bitrate_kbps;
    uint64_t sent; // audio bytes sent for this track
    uint64_t total; // track size in bytes
    bool starved; // blocked waiting for the ESP to grant credit
} Mp3PacerStatus;

typedef struct Mp3Pacer Mp3Pacer;

Mp3Pacer* mp3_pacer_alloc(RadioUart* uart);
void mp3_pacer_free(Mp3Pacer* p);

// Starts streaming `path`. Any track already playing is stopped first.
bool mp3_pacer_play(Mp3Pacer* p, const char* path);

// Stops the current track, forgets the position, and tells the ESP to drop
// what it has buffered.
void mp3_pacer_stop(Mp3Pacer* p);

// Stop sending but REMEMBER where we got to. The ESP's buffer is flushed either
// way -- a paused station goes quiet immediately rather than draining the four
// seconds it is holding, which is what a listener expects from a pause.
void mp3_pacer_pause(Mp3Pacer* p);

// Pick the paused track back up at the byte it stopped on. False if nothing is
// paused. The send clock is rebased so pacing stays correct across the gap.
bool mp3_pacer_resume(Mp3Pacer* p);

// Play the current track again from the top. False if no track was ever picked.
bool mp3_pacer_replay(Mp3Pacer* p);

// True once a track has been chosen, whatever state it is now in -- the
// difference between "press play to resume" and "there is nothing to play".
bool mp3_pacer_has_track(Mp3Pacer* p);

// Feed in a RADIO_MSG_CREDIT grant. This is the overflow backstop, not the
// clock — see protocol.md.
void mp3_pacer_credit(Mp3Pacer* p, uint16_t credit);

void mp3_pacer_status(Mp3Pacer* p, Mp3PacerStatus* out);
