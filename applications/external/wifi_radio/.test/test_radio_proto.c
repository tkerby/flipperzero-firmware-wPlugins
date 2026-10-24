// Host tests for the UART wire codec. This file is shared by both halves of the
// link — the FAP and the ESP32 firmware compile the same radio_proto.c — so
// these checks cover the Flipper side and the ESP side at once.
//
//   cc -I../flipper-app -O1 -Wall -Wextra -o /tmp/test_radio_proto \
//      test_radio_proto.c ../flipper-app/radio_proto.c && /tmp/test_radio_proto

#include "radio_proto.h"

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

static void check_eq(unsigned long got, unsigned long want, const char* what) {
    checks++;
    if(got != want) {
        failures++;
        printf("  FAIL: %s (got %lu, want %lu)\n", what, got, want);
    }
}

// Feed a whole buffer; returns how many frames completed.
static int feed_all(RadioDecoder* d, const uint8_t* buf, size_t len) {
    int frames = 0;
    for(size_t i = 0; i < len; i++) {
        if(radio_decoder_feed(d, buf[i])) frames++;
    }
    return frames;
}

int main(void) {
    printf("radio protocol codec tests\n");

    // --- CRC-8 poly 0x07, the canonical check vector ---
    {
        const uint8_t v[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        check_eq(radio_crc8(v, sizeof(v)), 0xF4, "CRC-8/ATM check vector");
        check_eq(radio_crc8(NULL, 0), 0x00, "CRC of nothing is 0");
    }

    // --- round trip, every message type ---
    {
        const uint8_t audio[] = {0xFF, 0xFB, 0x90, 0x44, 0x00, 0xAA, 0xAA, 0x13};
        uint8_t frame[RADIO_MAX_FRAME];
        size_t n =
            radio_frame_encode(RADIO_MSG_AUDIO_DATA, audio, sizeof(audio), frame, sizeof(frame));
        check_eq(n, sizeof(audio) + RADIO_FRAME_OVERHEAD, "encoded length");
        check_eq(frame[0], RADIO_FRAME_START, "starts with 0xAA");
        check_eq(frame[1], RADIO_MSG_AUDIO_DATA, "type byte");
        check_eq(frame[2] | (frame[3] << 8), sizeof(audio), "length is LE");

        RadioDecoder d;
        radio_decoder_init(&d);
        check_eq(feed_all(&d, frame, n), 1, "one frame decoded");
        check_eq(d.type, RADIO_MSG_AUDIO_DATA, "type survives");
        check_eq(d.len, sizeof(audio), "length survives");
        check(memcmp(d.payload, audio, sizeof(audio)) == 0, "payload survives (0xAA inside)");
        check_eq(d.frames_ok, 1, "frames_ok counted");
        check_eq(d.crc_errors, 0, "no spurious crc errors");
    }

    // --- empty payload (FLUSH, PING, PONG carry none) ---
    {
        uint8_t frame[RADIO_MAX_FRAME];
        size_t n = radio_frame_encode(RADIO_MSG_FLUSH, NULL, 0, frame, sizeof(frame));
        check_eq(n, RADIO_FRAME_OVERHEAD, "empty frame is 5 bytes");
        RadioDecoder d;
        radio_decoder_init(&d);
        check_eq(feed_all(&d, frame, n), 1, "empty frame decodes");
        check_eq(d.type, RADIO_MSG_FLUSH, "empty frame type");
        check_eq(d.len, 0, "empty frame length");
    }

    // --- maximum payload ---
    {
        uint8_t big[RADIO_MAX_PAYLOAD];
        for(size_t i = 0; i < sizeof(big); i++)
            big[i] = (uint8_t)(i * 7u);
        uint8_t frame[RADIO_MAX_FRAME];
        size_t n =
            radio_frame_encode(RADIO_MSG_AUDIO_DATA, big, sizeof(big), frame, sizeof(frame));
        check_eq(n, RADIO_MAX_FRAME, "max frame fills the buffer");
        RadioDecoder d;
        radio_decoder_init(&d);
        check_eq(feed_all(&d, frame, n), 1, "max frame decodes");
        check(memcmp(d.payload, big, sizeof(big)) == 0, "max payload survives");

        // One byte over the limit must be refused outright.
        check_eq(
            radio_frame_encode(
                RADIO_MSG_AUDIO_DATA, big, RADIO_MAX_PAYLOAD + 1, frame, sizeof(frame)),
            0,
            "oversize payload refused");
        // So must a buffer that cannot hold the frame.
        check_eq(
            radio_frame_encode(RADIO_MSG_AUDIO_DATA, big, sizeof(big), frame, 8),
            0,
            "short output buffer refused");
    }

    // --- a corrupted payload byte is caught by the CRC ---
    {
        const uint8_t pay[] = {1, 2, 3, 4};
        uint8_t frame[RADIO_MAX_FRAME];
        size_t n =
            radio_frame_encode(RADIO_MSG_TRACK_META, pay, sizeof(pay), frame, sizeof(frame));
        frame[5] ^= 0x01; // flip a bit in the payload
        RadioDecoder d;
        radio_decoder_init(&d);
        check_eq(feed_all(&d, frame, n), 0, "corrupt frame rejected");
        check_eq(d.crc_errors, 1, "crc error counted");
        check_eq(d.frames_ok, 0, "corrupt frame not counted ok");
    }

    // --- a bogus length drops back to hunting instead of eating the stream ---
    {
        RadioDecoder d;
        radio_decoder_init(&d);
        const uint8_t bogus[] = {RADIO_FRAME_START, RADIO_MSG_AUDIO_DATA, 0xFF, 0xFF};
        feed_all(&d, bogus, sizeof(bogus));
        check_eq(d.oversize_errors, 1, "oversize length counted");

        // The very next well-formed frame must still decode.
        const uint8_t pay[] = {9, 9};
        uint8_t frame[RADIO_MAX_FRAME];
        size_t n = radio_frame_encode(RADIO_MSG_PING, pay, sizeof(pay), frame, sizeof(frame));
        check_eq(feed_all(&d, frame, n), 1, "recovers after a bogus length");
    }

    // --- garbage between frames is skipped, not fatal ---
    {
        RadioDecoder d;
        radio_decoder_init(&d);
        const uint8_t noise[] = {0x00, 0x7F, 0x41, 0x20}; // e.g. ESP log text
        feed_all(&d, noise, sizeof(noise));
        check_eq(d.resyncs, 4, "non-start bytes counted as resyncs");

        uint8_t frame[RADIO_MAX_FRAME];
        size_t n = radio_frame_encode(RADIO_MSG_PONG, NULL, 0, frame, sizeof(frame));
        check_eq(feed_all(&d, frame, n), 1, "frame after noise decodes");
    }

    // --- the truncation trap protocol.md describes ---
    // A frame cut short mid-payload cannot be recovered by hunting for 0xAA.
    // Without the idle-gap reset, the stale decoder eats the NEXT frame's start
    // byte and loses that frame too. This is the whole reason the reset exists.
    {
        const uint8_t pay[] = {1, 2, 3, 4, 5, 6, 7, 8};
        uint8_t whole[RADIO_MAX_FRAME];
        size_t n =
            radio_frame_encode(RADIO_MSG_AUDIO_DATA, pay, sizeof(pay), whole, sizeof(whole));

        // Without the reset: feed a truncated frame, then a good one.
        RadioDecoder bad;
        radio_decoder_init(&bad);
        feed_all(&bad, whole, n - 4); // cut mid-payload
        check_eq(feed_all(&bad, whole, n), 0, "truncation also destroys the next frame");

        // With the reset, the next frame is fine.
        RadioDecoder good;
        radio_decoder_init(&good);
        feed_all(&good, whole, n - 4);
        radio_decoder_reset(&good); // what the idle gap triggers
        check_eq(good.truncated, 1, "truncated frame counted");
        check_eq(feed_all(&good, whole, n), 1, "reset rescues the next frame");
        check_eq(good.frames_ok, 1, "exactly one good frame");
    }

    // --- reset on an idle decoder is a no-op, not a false truncation ---
    {
        RadioDecoder d;
        radio_decoder_init(&d);
        radio_decoder_reset(&d);
        radio_decoder_reset(&d);
        check_eq(d.truncated, 0, "reset while idle counts nothing");
    }

    // --- back-to-back frames in one read, as the UART actually delivers them ---
    {
        uint8_t stream[RADIO_MAX_FRAME * 3];
        size_t off = 0;
        const uint8_t a[] = {0xAA, 0xAA, 0xAA}; // start bytes as payload
        off += radio_frame_encode(
            RADIO_MSG_AUDIO_DATA, a, sizeof(a), stream + off, sizeof(stream) - off);
        off += radio_frame_encode(RADIO_MSG_FLUSH, NULL, 0, stream + off, sizeof(stream) - off);
        off += radio_frame_encode(RADIO_MSG_PING, NULL, 0, stream + off, sizeof(stream) - off);

        RadioDecoder d;
        radio_decoder_init(&d);
        check_eq(feed_all(&d, stream, off), 3, "three frames in one buffer");
        check_eq(d.resyncs, 0, "no resyncs on a clean stream");
    }

    // --- MP3 header parsing: this is the send clock ---
    {
        // MPEG1 Layer III, 128 kbps, 44.1 kHz.
        const uint8_t h128[4] = {0xFF, 0xFB, 0x90, 0x00};
        check_eq(radio_mp3_bitrate_kbps(h128), 128, "128 kbps header");
        const uint8_t h320[4] = {0xFF, 0xFB, 0xE0, 0x00};
        check_eq(radio_mp3_bitrate_kbps(h320), 320, "320 kbps header");

        const uint8_t no_sync[4] = {0xFE, 0xFB, 0x90, 0x00};
        check_eq(radio_mp3_bitrate_kbps(no_sync), 0, "bad sync rejected");
        const uint8_t layer2[4] = {0xFF, 0xFD, 0x90, 0x00}; // layer II
        check_eq(radio_mp3_bitrate_kbps(layer2), 0, "layer II rejected");
        const uint8_t free_br[4] = {0xFF, 0xFB, 0x00, 0x00};
        check_eq(radio_mp3_bitrate_kbps(free_br), 0, "free bitrate rejected");
        const uint8_t bad_br[4] = {0xFF, 0xFB, 0xF0, 0x00};
        check_eq(radio_mp3_bitrate_kbps(bad_br), 0, "bad bitrate index rejected");
        const uint8_t bad_sr[4] = {0xFF, 0xFB, 0x9C, 0x00};
        check_eq(radio_mp3_bitrate_kbps(bad_sr), 0, "reserved sample rate rejected");
    }

    // --- finding the first frame header past an ID3 tag ---
    {
        uint8_t buf[64];
        memset(buf, 0, sizeof(buf));
        memcpy(buf, "ID3\x04\x00\x00", 6);
        buf[20] = 0xFF;
        buf[21] = 0xFB;
        buf[22] = 0x90;
        buf[23] = 0x00;
        check_eq(
            (unsigned long)radio_mp3_find_header(buf, sizeof(buf)), 20, "header found past ID3");

        uint8_t none[16];
        memset(none, 0, sizeof(none));
        check(radio_mp3_find_header(none, sizeof(none)) == -1, "no header reported as -1");
        check(radio_mp3_find_header(buf, 2) == -1, "too-short buffer reported as -1");
    }

    // --- frame geometry, and rejecting a chance byte pattern -----------------
    //
    // This is what used to break playback: the scan locked onto a stray
    // 0xFF 0xEx pair, the decoder read its metadata and then failed on every
    // real frame after it -- MEDIA_ERR_DECODE with HAVE_METADATA already set.
    {
        // 0xFF 0xFB 0x90 0x00 -> MPEG1 Layer III, 128 kbps, 44100 Hz, no pad.
        const uint8_t h128[4] = {0xFF, 0xFB, 0x90, 0x00};
        check_eq(radio_mp3_bitrate_kbps(h128), 128, "128 kbps header");
        check_eq(radio_mp3_samplerate(h128), 44100, "44100 Hz header");
        check_eq(radio_mp3_frame_len(h128), 417, "128 kbps frame is 417 bytes");

        // Same header with the padding bit set is one byte longer.
        const uint8_t hpad[4] = {0xFF, 0xFB, 0x92, 0x00};
        check_eq(radio_mp3_frame_len(hpad), 418, "padded frame is 418 bytes");

        const uint8_t bad[4] = {0xFF, 0xFB, 0x00, 0x00}; // bitrate index 0
        check_eq(radio_mp3_frame_len(bad), 0, "invalid header has no frame length");

        // A lone header with nothing one frame later is noise, not a frame.
        uint8_t fake[1024];
        memset(fake, 0x5A, sizeof(fake));
        memcpy(&fake[10], h128, 4);
        check(
            radio_mp3_find_header(fake, sizeof(fake)) == -1,
            "lone header with no following frame is rejected");

        // Two headers exactly one frame apart are accepted.
        uint8_t real[1024];
        memset(real, 0x5A, sizeof(real));
        memcpy(&real[10], h128, 4);
        memcpy(&real[10 + 417], h128, 4);
        check_eq(
            (unsigned long)radio_mp3_find_header(real, sizeof(real)),
            10,
            "confirmed header pair is accepted");

        // A stray pattern before a real pair must be skipped, not taken.
        uint8_t mixed[2048];
        memset(mixed, 0x5A, sizeof(mixed));
        memcpy(&mixed[5], h128, 4); // decoy: nothing 417 bytes later
        memcpy(&mixed[600], h128, 4);
        memcpy(&mixed[600 + 417], h128, 4);
        check_eq(
            (unsigned long)radio_mp3_find_header(mixed, sizeof(mixed)),
            600,
            "decoy skipped in favour of the confirmed pair");

        // With no room to verify, a lone header is still accepted -- the ESP
        // syncs inside a single read and must not refuse everything.
        uint8_t tight[32];
        memset(tight, 0x5A, sizeof(tight));
        memcpy(&tight[4], h128, 4);
        check_eq(
            (unsigned long)radio_mp3_find_header(tight, sizeof(tight)),
            4,
            "lone header accepted when the buffer cannot verify it");
    }

    // --- bounded strlen (strnlen is not in the FAP API) ---
    {
        check_eq(radio_strnlen("abc", 10), 3, "strnlen short string");
        check_eq(radio_strnlen("abcdef", 3), 3, "strnlen clamps to max");
        check_eq(radio_strnlen("", 4), 0, "strnlen empty");
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
