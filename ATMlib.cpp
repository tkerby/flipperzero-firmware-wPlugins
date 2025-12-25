#include "lib/ATMlib.h"

#include <string.h>

#include <furi.h>
#include <furi_hal.h>

#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_dma.h>

// ---------------------------
// Original engine state
// ---------------------------

byte trackCount = 0;
byte tickRate = 25;
const word* trackList = NULL;
const byte* trackBase = NULL;

uint8_t pcm = 128;

uint16_t cia = 1;
uint16_t cia_count = 1;

osc_t osc[4];

// Channel active/mute bitmap
static byte ChannelActiveMute = 0b11110000;

// note table
static const word noteTable[64] PROGMEM = {
    0,
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,
    523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,
    1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,
    2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951,
    4186,4435,4699,4978,5274,5588,5920,6272,6645,7040,7459,7902,
    8372,8870,9397,
};

// ---------------------------
// Track interpreter state
// ---------------------------

struct ch_t {
    const byte* ptr;
    byte note;

    // Nesting
    word stackPointer[7];
    byte stackCounter[7];
    byte stackTrack[7];
    byte stackIndex;
    byte repeatPoint;

    // Looping
    word delay;
    byte counter;
    byte track;

    // External FX
    word freq;
    byte vol;

    // Volume & Frequency slide FX
    char volFreSlide;
    byte volFreConfig;
    byte volFreCount;

    // Arpeggio / Note Cut
    byte arpNotes;
    byte arpTiming;
    byte arpCount;

    // Retrig FX
    byte reConfig;
    byte reCount;

    // Transposition
    char transConfig;

    // Tremolo / Vibrato
    byte treviDepth;
    byte treviConfig;
    byte treviCount;

    // Glissando
    char glisConfig;
    byte glisCount;
};

static ch_t channel_state[4];

// ---------------------------
// Helpers
// ---------------------------

static uint16_t read_vle(const byte** pp) {
    word q = 0;
    byte d;
    do {
        q <<= 7;
        d = pgm_read_byte((*pp)++);
        q |= (d & 0x7F);
    } while(d & 0x80);
    return q;
}

static inline const byte* getTrackPointer(byte track) {
    return trackBase + pgm_read_word(&trackList[track]);
}

// ---------------------------
// DMA PWM audio backend (TIM16 CH1 -> CCR1)
// ---------------------------

// PWM carrier 62.5kHz: 64MHz/(PSC+1)/(ARR+1)
// ARR=255, PSC=3 => 64e6/4/256 = 62500
static constexpr uint32_t ATM_PWM_ARR = 255;
static constexpr uint32_t ATM_PWM_PSC = 3;

// Logical rate 31250 (как оригинал с half)
static constexpr uint32_t ATM_LOGICAL_HZ = 31250;

// Buffer sizes
static constexpr size_t ATM_LOGICAL_SAMPLES_PER_HALF = 128;
static constexpr size_t ATM_DMA_SAMPLES_PER_HALF = ATM_LOGICAL_SAMPLES_PER_HALF * 2;
static constexpr size_t ATM_DMA_TOTAL = ATM_DMA_SAMPLES_PER_HALF * 2;

// DMA buffer (words -> CCR1)
static uint32_t dma_buf[ATM_DMA_TOTAL];

// flags
static bool atm_running = false;
static bool atm_paused = false;
static float atm_master_volume = 1.0f;

// tick scheduling
static uint32_t atm_tick_div = (ATM_LOGICAL_HZ / 25);
static uint32_t atm_tick_acc = 0;

// atomic pending tick counter
static uint32_t atm_tick_pending = 0;

// thread infra
static FuriThread* atm_thread = NULL;
static FuriMessageQueue* atm_cmd_q = NULL;

static void dma_isr(void* ctx);

// Command protocol
enum AtmCmdType : uint8_t {
    AtmCmdPlay,
    AtmCmdStop,
    AtmCmdTogglePause,
    AtmCmdMute,
    AtmCmdUnmute,
    AtmCmdSetVolume,
};

struct AtmCmd {
    AtmCmdType type;
    union {
        struct { const byte* song; } play;
        struct { byte ch; } ch;
        struct { float v; } vol;
    } u;
};

// ---------------------------
// Render one logical sample (0..255)
// ---------------------------

static inline uint8_t atm_render_logical_sample_u8() {
    // triangle
    osc[2].phase = (uint16_t)(osc[2].phase + osc[2].freq);
    int8_t phase2 = (int8_t)(osc[2].phase >> 8);
    if(phase2 < 0) phase2 = (int8_t)(~phase2);
    phase2 = (int8_t)(phase2 << 1);
    phase2 = (int8_t)(phase2 - 128);
    int8_t vol = (int8_t)((((int16_t)phase2 * (int8_t)osc[2].vol) << 1) >> 8);

    // pulse
    osc[0].phase = (uint16_t)(osc[0].phase + osc[0].freq);
    int8_t vol0 = (int8_t)osc[0].vol;
    if(osc[0].phase >= 0xC000) vol0 = (int8_t)(-vol0);
    vol = (int8_t)(vol + vol0);

    // square
    osc[1].phase = (uint16_t)(osc[1].phase + osc[1].freq);
    int8_t vol1 = (int8_t)osc[1].vol;
    if(osc[1].phase & 0x8000) vol1 = (int8_t)(-vol1);
    vol = (int8_t)(vol + vol1);

    // noise LFSR
    uint16_t freq = osc[3].freq;
    freq <<= 1;
    if(freq & 0x8000) freq ^= 1;
    if(freq & 0x4000) freq ^= 1;
    osc[3].freq = freq;
    int8_t vol3 = (int8_t)osc[3].vol;
    if(freq & 0x8000) vol3 = (int8_t)(-vol3);
    vol = (int8_t)(vol + vol3);

    int16_t out = (int16_t)vol + (int16_t)pcm;

    // master volume around 128
    float centered = (float)(out - 128);
    centered *= atm_master_volume;
    int16_t outv = (int16_t)(128.0f + centered);

    if(outv < 0) outv = 0;
    if(outv > 255) outv = 255;

    // tick accumulate
    atm_tick_acc++;
    if(atm_tick_acc >= atm_tick_div) {
        atm_tick_acc = 0;
        __atomic_fetch_add(&atm_tick_pending, 1, __ATOMIC_RELAXED);
    }

    return (uint8_t)outv;
}

static inline void atm_fill_half(size_t half_index) {
    uint32_t* dst = dma_buf + (half_index * ATM_DMA_SAMPLES_PER_HALF);

    for(size_t i = 0; i < ATM_LOGICAL_SAMPLES_PER_HALF; i++) {
        uint8_t s = atm_paused ? 128 : atm_render_logical_sample_u8();

        uint32_t duty = (uint32_t)s;
        if(duty > ATM_PWM_ARR) duty = ATM_PWM_ARR;

        dst[i * 2 + 0] = duty;
        dst[i * 2 + 1] = duty;
    }
}

// ---------------------------
// TIM16 + DMA start/stop (SAFE via speaker ownership)
// ---------------------------

static void tim16_dma_start() {
    furi_check(furi_hal_speaker_is_mine());

    memset(dma_buf, 128, sizeof(dma_buf));
    atm_tick_acc = 0;
    __atomic_store_n(&atm_tick_pending, 0, __ATOMIC_RELAXED);

    atm_fill_half(0);
    atm_fill_half(1);

    // TIM16 PWM setup
    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_DisableCounter(TIM16);

    LL_TIM_SetPrescaler(TIM16, ATM_PWM_PSC);
    LL_TIM_SetAutoReload(TIM16, ATM_PWM_ARR);
    LL_TIM_EnableARRPreload(TIM16);

    LL_TIM_OC_SetMode(TIM16, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_EnablePreload(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_SetCompareCH1(TIM16, dma_buf[0]);

    // FIX #1: polarity helper that exists in your SDK
    LL_TIM_OC_SetPolarity(TIM16, LL_TIM_CHANNEL_CH1, LL_TIM_OCPOLARITY_HIGH);

    LL_TIM_CC_EnableChannel(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_EnableAllOutputs(TIM16);

    // DMA config
    LL_DMA_InitTypeDef dma_init;
    memset(&dma_init, 0, sizeof(dma_init));

    dma_init.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_init.PeriphOrM2MSrcAddress = (uint32_t)&TIM16->CCR1;
    dma_init.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_init.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_init.MemoryOrM2MDstAddress = (uint32_t)dma_buf;
    dma_init.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_init.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_init.Mode = LL_DMA_MODE_CIRCULAR;
    dma_init.NbData = ATM_DMA_TOTAL;
    dma_init.Priority = LL_DMA_PRIORITY_HIGH;
    dma_init.PeriphRequest = LL_DMAMUX_REQ_TIM16_UP;

    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_Init(DMA1, LL_DMA_CHANNEL_1, &dma_init);

    LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);

    // FIX #2: proper enum priority
    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdDma1Ch1,
        FuriHalInterruptPriorityNormal,
        dma_isr,
        NULL);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    LL_TIM_EnableUpdateEvent(TIM16);
    LL_TIM_EnableDMAReq_UPDATE(TIM16);

    LL_TIM_EnableCounter(TIM16);
    LL_TIM_GenerateEvent_UPDATE(TIM16);
}

static void tim16_dma_stop() {
    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdDma1Ch1,
        FuriHalInterruptPriorityNormal,
        NULL,
        NULL);

    LL_TIM_DisableDMAReq_UPDATE(TIM16);

    LL_DMA_DisableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);

    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_DisableCounter(TIM16);
}

static void dma_isr(void* /*ctx*/) {
    if(LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        atm_fill_half(0);
    }
    if(LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        atm_fill_half(1);
    }
}

// ---------------------------
// ATM playroutine (track interpreter)
// ---------------------------

void ATM_playroutine(void) {
    ch_t* ch;

    for(byte n = 0; n < 4; n++) {
        ch = &channel_state[n];

        // Noise retriggering
        if(ch->reConfig) {
            if(ch->reCount >= (ch->reConfig & 0x03)) {
                osc[n].freq = pgm_read_word(&noteTable[ch->reConfig >> 2]);
                ch->reCount = 0;
            } else ch->reCount++;
        }

        // Glissando
        if(ch->glisConfig) {
            if(ch->glisCount >= (ch->glisConfig & 0x7F)) {
                if(ch->glisConfig & 0x80) ch->note -= 1;
                else ch->note += 1;
                if(ch->note < 1) ch->note = 1;
                else if(ch->note > 63) ch->note = 63;
                ch->freq = pgm_read_word(&noteTable[ch->note]);
                ch->glisCount = 0;
            } else ch->glisCount++;
        }

        // Slide
        if(ch->volFreSlide) {
            if(!ch->volFreCount) {
                int16_t vf = ((ch->volFreConfig & 0x40) ? ch->freq : ch->vol);
                vf += (ch->volFreSlide);
                if(!(ch->volFreConfig & 0x80)) {
                    if(vf < 0) vf = 0;
                    else if(ch->volFreConfig & 0x40) {
                        if(vf > 9397) vf = 9397;
                    } else {
                        if(vf > 63) vf = 63;
                    }
                }
                if(ch->volFreConfig & 0x40) ch->freq = (word)vf;
                else ch->vol = (byte)vf;
            }
            if(ch->volFreCount++ >= (ch->volFreConfig & 0x3F)) ch->volFreCount = 0;
        }

        // Arpeggio / Note Cut
        if(ch->arpNotes && ch->note) {
            if((ch->arpCount & 0x1F) < (ch->arpTiming & 0x1F)) ch->arpCount++;
            else {
                if((ch->arpCount & 0xE0) == 0x00) ch->arpCount = 0x20;
                else if((ch->arpCount & 0xE0) == 0x20 && !(ch->arpTiming & 0x40) &&
                        (ch->arpNotes != 0xFF))
                    ch->arpCount = 0x40;
                else
                    ch->arpCount = 0x00;

                byte arpNote = ch->note;
                if((ch->arpCount & 0xE0) != 0x00) {
                    if(ch->arpNotes == 0xFF) arpNote = 0;
                    else arpNote += (ch->arpNotes >> 4);
                }
                if((ch->arpCount & 0xE0) == 0x40) arpNote += (ch->arpNotes & 0x0F);
                ch->freq = pgm_read_word(&noteTable[arpNote + ch->transConfig]);
            }
        }

        // Tre/Vib
        if(ch->treviDepth) {
            int16_t vt = ((ch->treviConfig & 0x40) ? ch->freq : ch->vol);
            vt = (ch->treviCount & 0x80) ? (vt + ch->treviDepth) : (vt - ch->treviDepth);
            if(vt < 0) vt = 0;
            else if(ch->treviConfig & 0x40) {
                if(vt > 9397) vt = 9397;
            } else {
                if(vt > 63) vt = 63;
            }
            if(ch->treviConfig & 0x40) ch->freq = (word)vt;
            else ch->vol = (byte)vt;

            if((ch->treviCount & 0x1F) < (ch->treviConfig & 0x1F)) ch->treviCount++;
            else ch->treviCount = (ch->treviCount & 0x80) ? 0 : 0x80;
        }

        if(ch->delay) {
            if(ch->delay != 0xFFFF) ch->delay--;
        } else {
            do {
                byte cmd = pgm_read_byte(ch->ptr++);

                if(cmd < 64) {
                    if((ch->note = cmd)) ch->note += ch->transConfig;
                    ch->freq = pgm_read_word(&noteTable[ch->note]);
                    if(!ch->volFreConfig) ch->vol = ch->reCount;
                    if(ch->arpTiming & 0x20) ch->arpCount = 0;
                } else if(cmd < 160) {
                    switch(cmd - 64) {
                    case 0:
                        ch->vol = pgm_read_byte(ch->ptr++);
                        ch->reCount = ch->vol;
                        break;
                    case 1:
                    case 4:
                        ch->volFreSlide = (char)pgm_read_byte(ch->ptr++);
                        ch->volFreConfig = (cmd - 64) == 1 ? 0x00 : 0x40;
                        break;
                    case 2:
                    case 5:
                        ch->volFreSlide = (char)pgm_read_byte(ch->ptr++);
                        ch->volFreConfig = pgm_read_byte(ch->ptr++);
                        break;
                    case 3:
                    case 6:
                        ch->volFreSlide = 0;
                        break;
                    case 7:
                        ch->arpNotes = pgm_read_byte(ch->ptr++);
                        ch->arpTiming = pgm_read_byte(ch->ptr++);
                        break;
                    case 8:
                        ch->arpNotes = 0;
                        break;
                    case 9:
                        ch->reConfig = pgm_read_byte(ch->ptr++);
                        break;
                    case 10:
                        ch->reConfig = 0;
                        break;
                    case 11:
                        ch->transConfig += (char)pgm_read_byte(ch->ptr++);
                        break;
                    case 12:
                        ch->transConfig = (char)pgm_read_byte(ch->ptr++);
                        break;
                    case 13:
                        ch->transConfig = 0;
                        break;
                    case 14:
                    case 16:
                        ch->treviDepth = (byte)pgm_read_word(ch->ptr++);
                        ch->treviConfig =
                            (byte)pgm_read_word(ch->ptr++) + ((cmd - 64) == 14 ? 0x00 : 0x40);
                        break;
                    case 15:
                    case 17:
                        ch->treviDepth = 0;
                        break;
                    case 18:
                        ch->glisConfig = (char)pgm_read_byte(ch->ptr++);
                        break;
                    case 19:
                        ch->glisConfig = 0;
                        break;
                    case 20:
                        ch->arpNotes = 0xFF;
                        ch->arpTiming = pgm_read_byte(ch->ptr++);
                        break;
                    case 21:
                        ch->arpNotes = 0;
                        break;
                    case 92:
                        tickRate += pgm_read_byte(ch->ptr++);
                        if(tickRate < 1) tickRate = 1;
                        atm_tick_div = (ATM_LOGICAL_HZ / tickRate);
                        break;
                    case 93:
                        tickRate = pgm_read_byte(ch->ptr++);
                        if(tickRate < 1) tickRate = 1;
                        atm_tick_div = (ATM_LOGICAL_HZ / tickRate);
                        break;
                    case 94:
                        for(byte i = 0; i < 4; i++)
                            channel_state[i].repeatPoint = pgm_read_byte(ch->ptr++);
                        break;
                    case 95:
                        ChannelActiveMute = (byte)(ChannelActiveMute ^ (1 << (n + 4)));
                        ch->vol = 0;
                        ch->delay = 0xFFFF;
                        break;
                    default:
                        break;
                    }
                } else if(cmd < 224) {
                    ch->delay = (word)(cmd - 159);
                } else if(cmd == 224) {
                    ch->delay = (word)(read_vle(&ch->ptr) + 65);
                } else if(cmd == 252 || cmd == 253) {
                    byte new_counter = (cmd == 252) ? 0 : pgm_read_byte(ch->ptr++);
                    byte new_track = pgm_read_byte(ch->ptr++);

                    if(new_track != ch->track) {
                        ch->stackCounter[ch->stackIndex] = ch->counter;
                        ch->stackTrack[ch->stackIndex] = ch->track;
                        ch->stackPointer[ch->stackIndex] = (word)(ch->ptr - trackBase);
                        ch->stackIndex++;
                        ch->track = new_track;
                    }

                    ch->counter = new_counter;
                    ch->ptr = getTrackPointer(ch->track);
                } else if(cmd == 254) {
                    if(ch->counter > 0 || ch->stackIndex == 0) {
                        if(ch->counter) ch->counter--;
                        ch->ptr = getTrackPointer(ch->track);
                    } else {
                        if(ch->stackIndex == 0) {
                            ch->delay = 0xFFFF;
                        } else {
                            ch->stackIndex--;
                            ch->ptr = ch->stackPointer[ch->stackIndex] + trackBase;
                            ch->counter = ch->stackCounter[ch->stackIndex];
                            ch->track = ch->stackTrack[ch->stackIndex];
                        }
                    }
                } else if(cmd == 255) {
                    ch->ptr += read_vle(&ch->ptr);
                }
            } while(ch->delay == 0);

            if(ch->delay != 0xFFFF) ch->delay--;
        }

        if(!(ChannelActiveMute & (1 << n))) {
            if(n == 3) {
                osc[n].vol = (uint8_t)(ch->vol >> 1);
            } else {
                osc[n].freq = ch->freq;
                osc[n].vol = ch->vol;
            }
        }

        if(!(ChannelActiveMute & 0xF0)) {
            byte repeatSong = 0;
            for(byte j = 0; j < 4; j++) repeatSong = (byte)(repeatSong + channel_state[j].repeatPoint);
            if(repeatSong) {
                for(byte k = 0; k < 4; k++) {
                    channel_state[k].ptr = getTrackPointer(channel_state[k].repeatPoint);
                    channel_state[k].delay = 0;
                }
                ChannelActiveMute = 0b11110000;
            } else {
                ATMsynth::stop();
            }
        }
    }
}

// ---------------------------
// Worker thread: owns speaker + runs playroutine
// ---------------------------

static int32_t atm_thread_fn(void* /*ctx*/) {
    AtmCmd cmd;
    bool speaker_owned = false;

    while(true) {
        if(furi_message_queue_get(atm_cmd_q, &cmd, 10) == FuriStatusOk) {
            if(cmd.type == AtmCmdStop) {
                atm_running = false;
                atm_paused = false;

                if(speaker_owned) {
                    tim16_dma_stop();
                    furi_hal_speaker_release();
                    speaker_owned = false;
                }

                memset(channel_state, 0, sizeof(channel_state));
                ChannelActiveMute = 0b11110000;
                continue;
            }

            if(cmd.type == AtmCmdTogglePause) {
                if(atm_running) atm_paused = !atm_paused;
                continue;
            }

            if(cmd.type == AtmCmdMute) {
                ChannelActiveMute = (byte)(ChannelActiveMute | (1 << cmd.u.ch.ch));
                continue;
            }

            if(cmd.type == AtmCmdUnmute) {
                ChannelActiveMute = (byte)(ChannelActiveMute & (byte)~(1 << cmd.u.ch.ch));
                continue;
            }

            if(cmd.type == AtmCmdSetVolume) {
                float v = cmd.u.vol.v;
                if(v < 0) v = 0;
                if(v > 1) v = 1;
                atm_master_volume = v;
                continue;
            }

            if(cmd.type == AtmCmdPlay) {
                const byte* song = cmd.u.play.song;

                memset(channel_state, 0, sizeof(channel_state));
                ChannelActiveMute = 0b11110000;

                tickRate = 25;
                if(tickRate < 1) tickRate = 1;
                atm_tick_div = (ATM_LOGICAL_HZ / tickRate);

                osc[3].freq = 0x0001;
                channel_state[3].freq = 0x0001;

                trackCount = pgm_read_byte(song++);
                trackList = (const word*)song;
                trackBase = (song += (trackCount << 1)) + 4;

                for(byte n = 0; n < 4; n++) {
                    channel_state[n].ptr = getTrackPointer(pgm_read_byte(song++));
                }

                atm_running = true;
                atm_paused = false;

                if(!speaker_owned) {
                    if(furi_hal_speaker_acquire(200)) {
                        speaker_owned = true;
                        tim16_dma_start();
                    } else {
                        atm_running = false;
                    }
                }
                continue;
            }
        }

        if(atm_running && !atm_paused) {
            uint32_t pending = __atomic_load_n(&atm_tick_pending, __ATOMIC_RELAXED);
            if(pending) {
                __atomic_fetch_sub(&atm_tick_pending, 1, __ATOMIC_RELAXED);
                ATM_playroutine();
            }
        }
    }
}

// ---------------------------
// System init/deinit + C wrappers
// ---------------------------

void ATMsynth::systemInit() {
    if(atm_cmd_q) return;

    atm_cmd_q = furi_message_queue_alloc(8, sizeof(AtmCmd));

    atm_thread = furi_thread_alloc();
    furi_thread_set_name(atm_thread, "ATMlib");
    furi_thread_set_stack_size(atm_thread, 2048);
    furi_thread_set_priority(atm_thread, FuriThreadPriorityHigh);
    furi_thread_set_callback(atm_thread, atm_thread_fn);
    furi_thread_start(atm_thread);
}

void ATMsynth::systemDeinit() {
    stop();
}

// C wrappers (to satisfy your main.cpp)
void atm_system_init(void) {
    ATMsynth::systemInit();
}

void atm_system_deinit(void) {
    ATMsynth::systemDeinit();
}

// ---------------------------
// Public API wrappers
// ---------------------------

ATMsynth ATM;

static inline void push_cmd(const AtmCmd& c) {
    if(!atm_cmd_q) ATMsynth::systemInit();
    furi_message_queue_put(atm_cmd_q, &c, FuriWaitForever);
}

void ATMsynth::play(const byte* song) {
    AtmCmd c{};
    c.type = AtmCmdPlay;
    c.u.play.song = song;
    push_cmd(c);
}

void ATMsynth::stop() {
    AtmCmd c{};
    c.type = AtmCmdStop;
    push_cmd(c);
}

void ATMsynth::playPause() {
    AtmCmd c{};
    c.type = AtmCmdTogglePause;
    push_cmd(c);
}

void ATMsynth::muteChannel(byte ch) {
    AtmCmd c{};
    c.type = AtmCmdMute;
    c.u.ch.ch = ch;
    push_cmd(c);
}

void ATMsynth::unMuteChannel(byte ch) {
    AtmCmd c{};
    c.type = AtmCmdUnmute;
    c.u.ch.ch = ch;
    push_cmd(c);
}

void ATMsynth::setVolume(float v) {
    AtmCmd c{};
    c.type = AtmCmdSetVolume;
    c.u.vol.v = v;
    push_cmd(c);
}
