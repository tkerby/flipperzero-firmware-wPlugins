// handheld style sound (contract section 28): see sound.h. Threads: the 10 ms FuriTimer callback `tick` is the ONLY code that touches the speaker
// (acquire / start / stop / release: the firmware checks that the calling thread owns it) and the only writer of the playback position; the game
// thread (`sound_play`) loads a track from music.bin into a small heap block and publishes it with one pointer store (`blk[0]` music, `blk[1]` effect).
// A replaced block is freed one load later (`blk[2..3]`), so a callback that is still reading it is never left with freed memory. Nothing in the timer
// thread touches the card or the heap. A track that plays to its end gets its size set to 0 ("nothing to play"): an effect then gives way to the music,
// which goes on with the note after the one the effect cut (each voice keeps its own position); a fanfare is silence afterwards.
#define SOUND_IMPL
#include "sound.h"
#include "assets.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>

#ifdef NUCLEO_DEVICE
#include <furi.h>
#include <furi_hal.h>
#define SPK_ACQ() furi_hal_speaker_acquire(0)
#define SPK_REL() furi_hal_speaker_release()
#define SPK_STOP() furi_hal_speaker_stop()
#define SPK_START(f, v) furi_hal_speaker_start((float)(f), (float)(v) * (1.0f / 255.0f))
#define TM_NEW() tm_new()
typedef FuriTimer* SndTimer;
static void tick(void* ctx);
static bool tm_new(void);
#else
typedef int SndTimer;
SndMockEv snd_mock_ev[SND_MOCK_EVS];
int snd_mock_n, snd_mock_bad, snd_mock_acquires, snd_mock_prio_elev, snd_mock_allocs;
uint32_t snd_mock_now;
bool snd_mock_busy, snd_mock_held, snd_mock_timer_on;
int snd_mock_log[256], snd_mock_nlog;
static bool in_tick;
static void mock_ev(uint16_t f, uint8_t v) {
    if(!in_tick) snd_mock_bad++;
    if(snd_mock_n < SND_MOCK_EVS) snd_mock_ev[snd_mock_n++] = (SndMockEv){snd_mock_now, f, v};
}
static bool SPK_ACQ(void) {
    if(!in_tick) snd_mock_bad++;
    if(snd_mock_busy || snd_mock_held) return false;
    snd_mock_acquires++;
    return snd_mock_held = true;
}
static void SPK_REL(void) {
    if(!in_tick) snd_mock_bad++;
    snd_mock_held = false;
}
static void SPK_STOP(void) {
    mock_ev(0, 0);
}
#define SPK_START(f, v) mock_ev((uint16_t)(f), (uint8_t)(v))
#define TM_NEW() (snd_mock_allocs++, snd_mock_prio_elev = snd_mock_timer_on = 1, S.tm = 1)
static void tick(void* ctx);
#endif

#define NONE 0xFF
#define F_MUSIC "music.bin"
#define F_CRY "cry.bin" // F3 (contract 75): the species' cries: "NCRY" + 4 bytes, then {u16 offset, u16 size} per species id (from byte 8), then the records (the format of an effect)

// C7 .. B7 in Hz. A pitch byte is (octave - 2) << 4 | semitone (C3 = 0x10, B8 = 0x6B; 0 = rest): it sounds (NOTE7[semitone] << (pitch >> 4)) >> 5 Hz.
static const uint16_t NOTE7[12] = {2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951};

// A loaded track (heap block): [u8 events, u8 loop event (NONE: plays once), u8 volume, then {u8 a, u8 b} per event, then the instruments].
// Event: a = pitch (0 = rest, else (octave - 2) << 4 | semitone) and b = length in ticks, or (a & 15 >= 12) a command with x = a >> 4:
//   12 DRUM  instrument x plays its own pitch for b ticks (a hit inside the melody)        13 INS  the notes that follow use instrument x
//   15 REP   x plays in all of the last b events (one level; musc.py writes it for a repeated `[ ... ]`)       (14: free)
// Instruments (musc.py writes the shapes): at the end of the block, u8 offset per instrument (from that byte on), then each: u8 n steps (bits 0..4) |
// slow (bit 7: a step is 2 ticks), u8 loop (the step the table returns to after its last: n - 1 holds it), u8 pitch (a drum's note, 0 = a melodic
// instrument), then n steps {u8 volume of 255, i8 pitch change in 1/128 of the note's frequency}: the envelope, vibrato, arpeggio or drum sweep is the table.
// A track that has played to its end (no loop) gets its event count set to 0: it is "nothing to play" from then on.
static struct {
    uint8_t* volatile blk[4]; // 0 = the music, 1 = the effect (written by the game thread), 2 / 3 = the blocks they replaced: freed one load later
    struct {
        uint8_t* mine;        // timer: the block this voice plays
        uint8_t i, k, len;    // next event, tick and length of the current note
        uint8_t p, cur, ins;  // its pitch, its instrument, the instrument of the next notes
        uint8_t rep;          // REP plays left
    } v[2];                   // timer: the music's position survives an effect
    uint16_t sf;              // timer: the frequency on the speaker
    uint8_t sv, last, acq;    // timer: its volume, which voice played at the last tick, the speaker is ours
    uint8_t has, fxid;        // the looping music loaded (NONE: none / a fanfare), the effect loaded
    SndTimer tm;
} S = {.has = NONE};

static void tick_body(void) { // timer thread, every 10 ms
    if(g_settings.sound >= SOUND_OFF) { // off: the speaker is let go (and taken again when the sound is back)
        if(S.acq) {
            SPK_STOP();
            SPK_REL();
            S.acq = S.sf = 0;
        }
        return;
    }
    const uint8_t c = S.blk[1] && S.blk[1][0]; // an effect (until it has played to its end) pre-empts the music
    uint8_t* b = S.blk[c];
    if(!b || !b[0] || (!S.acq && !(S.acq = SPK_ACQ()))) return; // nothing to play / the speaker is taken (it plays on when it is free)
    if(c != S.last) { // the effect has begun or ended: the music goes on with its next note
        S.last = c;
        S.v[0].len = 0;
    }
    typeof(S.v[0])* V = &S.v[c];
    if(b != V->mine) { // a new track: from its first note
        V->i = V->ins = V->rep = V->len = 0; // (the note state is set by the first fetch)
        V->mine = b;
    }
    const uint8_t* ia = b + 3 + 2 * b[0]; // the instruments
    int32_t f = 0, e = 0;
    if(++V->k >= V->len) { // the next note
        for(uint8_t g = 255; g--;) { // (a damaged record that never reaches a note is given up: this thread must not hang)
            if(V->i >= b[0]) { // end of the track: back to the loop point, or over
                if(b[1] == NONE) {
                    b[0] = 0; // (it cannot start again)
                    goto out;
                }
                V->i = b[1];
            }
            const uint8_t* ev = b + 3 + 2 * V->i++;
            const uint8_t a = ev[0], x = a >> 4, n = a & 15;
            if(n <= 12) { // a note, a rest or a drum
                V->p = n == 12 ? ia[ia[x] + 2] : a;
                V->cur = n == 12 ? x : V->ins;
                V->len = ev[1];
                V->k = 0;
                break;
            }
            if(n == 13) V->ins = x;
            else {
                if(!V->rep) V->rep = x; // REP: x plays in all
                if(--V->rep) V->i -= ev[1] + 1;
            }
        }
    }
    const uint8_t* in = ia + ia[V->cur];
    const uint8_t k = V->k, p = V->p, n = in[0] & 31, lp = in[1];
    uint8_t j = k >> (in[0] >> 7); // the step
    if(p && (in[2] || k + 1 < V->len || V->len == 1)) { // (the last tick of every note but a drum is a gap, so repeated notes are heard apart)
        if(j >= n) j = lp + (j - lp) % (n - lp);
        in += 3 + 2 * j;
        f = (NOTE7[p & 15] << (p >> 4)) >> 5;
        f += f * (int8_t)in[1] >> 7;
        e = ((b[2] >> g_settings.sound) + 1) * in[0] >> 8; // ("Bajo" = 1: half)
    }
out:
    if(f < 100 || !e) f = 0;
    if(f && (f != S.sf || e != S.sv)) SPK_START(f, e);
    else if(!f && S.sf) SPK_STOP();
    S.sf = f;
    S.sv = e;
}

static void tick(void* ctx) {
    (void)ctx;
#ifndef NUCLEO_DEVICE
    in_tick = true;
#endif
    tick_body();
#ifndef NUCLEO_DEVICE
    in_tick = false;
#endif
}

#ifdef NUCLEO_DEVICE
static bool tm_new(void) { // the timer daemon runs below the game and flip threads by default, which are busy ~100 % of the time: raise it for our lifetime
    S.tm = furi_timer_alloc(tick, FuriTimerTypePeriodic, NULL);
    if(S.tm) {
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        furi_timer_start(S.tm, 10); // 1 tick = 1 ms
    }
    return S.tm != NULL;
}
#endif

// Loads a track into slot s (0 music, 1 effect): the 4-byte index entry {u16 offset, u16 size} at `io` of file `f`, then the record. False: nothing changed.
static bool load(uint8_t s, const char* f, uint32_t io) {
    uint16_t e[2];
    uint8_t* b;
    if(!read_at(f, io, e, 4) || e[1] < 8 || !(b = malloc(e[1]))) return false; // (a record is 8 bytes at least: a cry slot with no record, a damaged file)
    if(!read_at(f, e[0], b, e[1])) {
        free(b);
        return false;
    }
    free(S.blk[s + 2]);
    S.blk[s + 2] = S.blk[s];
    S.blk[s] = b;
    return true;
}

void sound_play(uint16_t id) {
    uint8_t t;
#ifndef NUCLEO_DEVICE
    if(snd_mock_nlog < 256) snd_mock_log[snd_mock_nlog++] = id;
#endif
    if(g_settings.sound >= SOUND_OFF || (!S.tm && !TM_NEW())) return; // (nothing is remembered while the sound is off: the caller asks again when it is back)
    if((uint16_t)(id - SND_CRY) < 256) { // a species' cry (contract 75): an effect from cry.bin; asked again at once it restarts (the Registro's CRY), so the effect de-duplication is off for it
        if(load(1, F_CRY, 8 + (id - SND_CRY) * 4)) S.fxid = NONE;
        return;
    }
    if(id >= SND_PICK) { // a table slot: the track it names (none: what plays goes on)
        if(id - SND_PICK >= SND_TAB_N || !read_at(F_MUSIC, SND_TAB_OFF + id - SND_PICK, &t, 1) || t >= SND_FX0) return;
        id = t;
    }
    if(id < SND_FX0) { // music: a looping track that is on goes on
        if(id != S.has && load(0, F_MUSIC, SND_IDX_OFF + id * 4)) S.has = S.blk[0][1] == NONE ? NONE : id;
    } else if(!(S.blk[1] && S.blk[1][0] && id == S.fxid) && load(1, F_MUSIC, SND_IDX_OFF + id * 4)) // (a playing effect is not restarted)
        S.fxid = id;
}

void sound_shutdown(void) {
    g_settings.sound = SOUND_OFF; // (the app is closing: the timer thread, which owns the speaker, lets it go at its next period)
#ifdef NUCLEO_DEVICE
    if(S.tm) {
        furi_delay_ms(30);
        furi_timer_free(S.tm);
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
    for(int i = 0; i < 4; i++) free(S.blk[i]);
#else
    snd_mock_tick();
    if(S.tm) snd_mock_allocs--, snd_mock_prio_elev = snd_mock_timer_on = 0;
    for(int i = 0; i < 4; i++) free(S.blk[i]), S.blk[i] = NULL;
    S.tm = 0;
    memset(S.v, 0, sizeof(S.v));
    S.last = S.sf = S.sv = 0;
    S.has = NONE;
#endif
}

#ifndef NUCLEO_DEVICE
void snd_mock_tick(void) {
    if(snd_mock_timer_on) tick(NULL);
}
void snd_mock_reset(void) {
    snd_mock_n = 0;
}
#endif
