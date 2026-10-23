// Flipper Radio — UART wire protocol (pure C, no furi deps so it host-tests).
//
// Frame: [0xAA][type:1][len:2 LE][payload:len][crc8]
// crc8 = CRC-8 poly 0x07, init 0x00, computed over type | len | payload.
// See protocol.md for the message table.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RADIO_FRAME_START 0xAAu
#define RADIO_MAX_PAYLOAD 512u

// Longest TRACK_META payload. The ESP stores this string verbatim and serves it
// from /status, so the cap has to be agreed by both halves rather than picked
// independently on each side.
#define RADIO_META_MAX       128u
#define RADIO_FRAME_OVERHEAD 5u // start + type + len(2) + crc
#define RADIO_MAX_FRAME      (RADIO_MAX_PAYLOAD + RADIO_FRAME_OVERHEAD)

// Flipper -> ESP
#define RADIO_MSG_AUDIO_DATA 0x01u
#define RADIO_MSG_TRACK_META 0x02u
#define RADIO_MSG_FLUSH      0x03u
#define RADIO_MSG_SET_SSID   0x04u
#define RADIO_MSG_PING       0x05u

// ESP -> Flipper
#define RADIO_MSG_CREDIT 0x81u
#define RADIO_MSG_STATS  0x82u
#define RADIO_MSG_PONG   0x83u

uint8_t radio_crc8(const uint8_t* data, size_t len);

// Serialize a frame into `out` (must hold RADIO_MAX_FRAME).
// Returns total bytes written, or 0 if the payload is too long.
size_t radio_frame_encode(
    uint8_t type,
    const uint8_t* payload,
    uint16_t payload_len,
    uint8_t* out,
    size_t out_cap);

// Incremental decoder: feed bytes as they arrive off the UART.
typedef enum {
    RadioDecodeStart = 0,
    RadioDecodeType,
    RadioDecodeLenLo,
    RadioDecodeLenHi,
    RadioDecodePayload,
    RadioDecodeCrc,
} RadioDecodeState;

typedef struct {
    RadioDecodeState state;
    uint8_t type;
    uint16_t len;
    uint16_t got;
    uint8_t payload[RADIO_MAX_PAYLOAD];
    // Diagnostics — a noisy link should be visible on the station console.
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t oversize_errors;
    uint32_t resyncs;
    uint32_t truncated;
} RadioDecoder;

void radio_decoder_init(RadioDecoder* d);

// Abandon any half-received frame, keeping the diagnostic counters.
//
// A byte-oriented framer cannot resync mid-payload: 0xAA occurs naturally in
// MP3 data, so a truncated frame otherwise eats the next frame's start byte and
// loses that frame too. Real truncation always comes with a timing gap, so the
// UART glue calls this when the link goes idle mid-frame (see
// RADIO_FRAME_IDLE_RESET_MS). Both halves of the link must do this.
void radio_decoder_reset(RadioDecoder* d);

// Idle time that means "the sender stopped mid-frame". At 921600 baud a 512-byte
// frame takes ~5.6 ms, so 50 ms is far longer than any legitimate inter-byte gap.
#define RADIO_FRAME_IDLE_RESET_MS 50u

// Feed one byte. Returns true when a frame just completed and passed CRC;
// `d->type`, `d->payload` and `d->len` then hold it until the next feed.
bool radio_decoder_feed(RadioDecoder* d, uint8_t byte);

// MP3 helpers — the CBR bitrate from the first frame header is the send clock.
// Returns bitrate in kbps, or 0 if `hdr` is not a valid MPEG1 Layer III header.
uint16_t radio_mp3_bitrate_kbps(const uint8_t hdr[4]);

// Sample rate declared by an MPEG1 Layer III header, or 0 if it is not one.
uint32_t radio_mp3_samplerate(const uint8_t hdr[4]);

// Total length of the frame this header introduces, including the header
// itself, or 0 if the header is not valid. This is what makes it possible to
// tell a real header from a chance byte pattern: the next frame must begin
// exactly this far along.
uint16_t radio_mp3_frame_len(const uint8_t hdr[4]);

// Scan `buf` for the first valid MP3 frame header; returns its offset or -1.
//
// A lone sync pattern is NOT accepted as proof. 0xFF followed by a byte with
// its top bits set occurs by chance about once per 10 kB of ordinary audio, so
// a single match picked at a random offset is as likely to be noise as a real
// frame -- and starting a stream on one gives a decoder plausible metadata
// followed by garbage for every frame after it. A match is confirmed by
// checking for a second header exactly one frame length later. When `buf` is
// too short to contain that second header the lone match is accepted, since
// there is nothing further to test it against.
int32_t radio_mp3_find_header(const uint8_t* buf, size_t len);

// Bounded string length — strnlen is NOT in the FAP API.
size_t radio_strnlen(const char* s, size_t max);
