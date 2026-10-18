#pragma once
// handheld style sound through the Flipper's piezo speaker (contract section 28). Resident (CORE_SRC): 1,018 B (contract 28.6).
//
// One square-wave voice plays at a time: the MUSIC voice (a looping track, or a one-shot "fanfare"), pre-empted by an EFFECT (jingle / SFX),
// after which the music goes on. A track (at most 400 bytes, 200 for an effect: tools/musc.py) is read from files/music.bin by read_at() into a small heap block
// when it is asked for (no file stays open); timing is a 10 ms FuriTimer that also owns the speaker (the firmware only lets the thread that acquired
// it drive it) and never touches the card, so a slow card or a busy game thread cannot make the music stutter.
// Everything is silent, without an error, when music.bin is missing, the speaker is taken, or Options says "No".
#include <stdint.h>
#include <stdbool.h>
#include "gen/music_gen.h" // SND_* track ids (tools/musc.py): music first, then SND_FX0.. the effects; SND_TAB_*: the map / trainer tables

#define SND_CRY 0x400  // sound_play(SND_CRY + species id), species 0..255: that species' cry (contract 75, tools/cryc.py, files/cry.bin; silent for an id with no record). An effect: it pre-empts the music for ~0.2 s
#define SND_PICK 0x100 // sound_play(SND_PICK + slot), slot 0..511 (format 3 of music.bin: 256 map slots + 256 trainer slots): see below
#if defined(NUCLEO_DEVICE) || defined(SOUND_IMPL)
// A music track (id < SND_FX0) replaces the music; an effect (id >= SND_FX0) pre-empts it and the music goes on with its next note when the effect
// is over; the same effect is not restarted while it plays. SND_PICK + slot = the track a table slot of music.bin names: SND_PICK + SND_TAB_MAP + map id
// (interiors share the town's track; any map id 0..255) or SND_PICK + SND_TAB_TRAINER + trainer id (0 = a wild battle; 0..255); a slot with no entry keeps what plays.
// The table is read from the card one byte at a time (nothing resident). The argument is 16 bits: a slot does not fit in a byte any more.
void sound_play(uint16_t id);
// App exit: silences, releases the speaker and frees the timer.
void sound_shutdown(void);
#else
// Host builds that do not link sound.c get no-op copies (weak: the real functions of sound.c win when it is linked).
__attribute__((weak)) void sound_play(uint16_t id) { (void)id; }
__attribute__((weak)) void sound_shutdown(void) {}
#endif

#ifndef NUCLEO_DEVICE
// ---- host mock of the speaker + timer (tests: tools/host_world/sound_check.c). Time is virtual: the test calls snd_mock_tick() every 10 ms.
typedef struct {
    uint32_t t;    // ms of the mock clock
    uint16_t freq; // Hz, 0 = the speaker went silent
    uint8_t vol;   // 0..255 (after the Options volume)
} SndMockEv;
#define SND_MOCK_EVS 16384 // (C5-B: 4096 was too few for three passes of the arpeggio and chug tracks, mar and autopista)
extern SndMockEv snd_mock_ev[SND_MOCK_EVS];
extern int snd_mock_n;            // events recorded
extern uint32_t snd_mock_now;     // the mock clock (ms): the test advances it
extern bool snd_mock_busy;        // another "app" owns the speaker: acquire fails
extern bool snd_mock_held;        // the speaker is acquired (by sound.c)
extern bool snd_mock_timer_on;    // the timer is running
extern int snd_mock_bad;          // speaker calls made outside the timer callback (the firmware would furi_check them)
extern int snd_mock_acquires;     // successful acquires
extern int snd_mock_prio_elev;    // 1 while the timer thread priority is raised
extern int snd_mock_allocs;       // timers alive
extern int snd_mock_log[256];     // every sound_play() id since the last reset (a SND_PICK slot is logged as it was called), snd_mock_nlog of them
extern int snd_mock_nlog;
void snd_mock_tick(void);         // one timer period (10 ms): runs the callback when the timer is on
void snd_mock_reset(void);        // forget the recorded events (the sound.c state stays)
#endif
