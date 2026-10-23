#include "radio_proto.h"

// CRC-8, poly 0x07, init 0x00, no reflection, no final xor.
uint8_t radio_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

size_t radio_frame_encode(
    uint8_t type,
    const uint8_t* payload,
    uint16_t payload_len,
    uint8_t* out,
    size_t out_cap) {
    if(payload_len > RADIO_MAX_PAYLOAD) return 0;
    if(out_cap < (size_t)payload_len + RADIO_FRAME_OVERHEAD) return 0;
    if(payload_len > 0 && payload == NULL) return 0;

    out[0] = RADIO_FRAME_START;
    out[1] = type;
    out[2] = (uint8_t)(payload_len & 0xFF);
    out[3] = (uint8_t)(payload_len >> 8);
    for(uint16_t i = 0; i < payload_len; i++) {
        out[4 + i] = payload[i];
    }
    // CRC covers type | len | payload — i.e. everything but the start byte.
    out[4 + payload_len] = radio_crc8(&out[1], (size_t)payload_len + 3);
    return (size_t)payload_len + RADIO_FRAME_OVERHEAD;
}

void radio_decoder_init(RadioDecoder* d) {
    d->state = RadioDecodeStart;
    d->type = 0;
    d->len = 0;
    d->got = 0;
    d->frames_ok = 0;
    d->crc_errors = 0;
    d->oversize_errors = 0;
    d->resyncs = 0;
    d->truncated = 0;
}

void radio_decoder_reset(RadioDecoder* d) {
    if(d->state != RadioDecodeStart) d->truncated++;
    d->state = RadioDecodeStart;
    d->got = 0;
    d->len = 0;
}

bool radio_decoder_feed(RadioDecoder* d, uint8_t byte) {
    switch(d->state) {
    case RadioDecodeStart:
        if(byte == RADIO_FRAME_START) {
            d->state = RadioDecodeType;
        } else {
            // Garbage between frames; keep hunting for a start byte.
            d->resyncs++;
        }
        return false;

    case RadioDecodeType:
        d->type = byte;
        d->state = RadioDecodeLenLo;
        return false;

    case RadioDecodeLenLo:
        d->len = byte;
        d->state = RadioDecodeLenHi;
        return false;

    case RadioDecodeLenHi:
        d->len |= (uint16_t)byte << 8;
        if(d->len > RADIO_MAX_PAYLOAD) {
            // Can't trust the length; drop back to hunting rather than
            // blindly consuming a bogus count of bytes.
            d->oversize_errors++;
            d->state = RadioDecodeStart;
            return false;
        }
        d->got = 0;
        d->state = d->len ? RadioDecodePayload : RadioDecodeCrc;
        return false;

    case RadioDecodePayload:
        d->payload[d->got++] = byte;
        if(d->got >= d->len) d->state = RadioDecodeCrc;
        return false;

    case RadioDecodeCrc: {
        uint8_t hdr[3] = {d->type, (uint8_t)(d->len & 0xFF), (uint8_t)(d->len >> 8)};
        uint8_t crc = radio_crc8(hdr, 3);
        // Continue the same CRC across the payload.
        for(uint16_t i = 0; i < d->len; i++) {
            crc ^= d->payload[i];
            for(uint8_t bit = 0; bit < 8; bit++) {
                crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
            }
        }
        d->state = RadioDecodeStart;
        if(crc == byte) {
            d->frames_ok++;
            return true;
        }
        d->crc_errors++;
        return false;
    }

    default:
        d->state = RadioDecodeStart;
        return false;
    }
}

// MPEG1 Layer III bitrate table, index 1..14 (0 = free, 15 = bad).
static const uint16_t radio_mp3_bitrates[16] =
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};

// MPEG1 sample rates, indexed by the header's two-bit field. 0b11 is reserved.
static const uint32_t radio_mp3_samplerates[4] = {44100, 48000, 32000, 0};

uint16_t radio_mp3_bitrate_kbps(const uint8_t hdr[4]) {
    // Sync: 11 bits set. Then version 0b11 (MPEG1), layer 0b01 (Layer III).
    if(hdr[0] != 0xFF) return 0;
    if((hdr[1] & 0xE0) != 0xE0) return 0;
    uint8_t version = (hdr[1] >> 3) & 0x03;
    uint8_t layer = (hdr[1] >> 1) & 0x03;
    if(version != 0x03) return 0; // MPEG1 only
    if(layer != 0x01) return 0; // Layer III only
    uint8_t bitrate_index = (hdr[2] >> 4) & 0x0F;
    if(bitrate_index == 0 || bitrate_index == 0x0F) return 0;
    uint8_t sr_index = (hdr[2] >> 2) & 0x03;
    if(sr_index == 0x03) return 0; // reserved sample rate
    return radio_mp3_bitrates[bitrate_index];
}

uint32_t radio_mp3_samplerate(const uint8_t hdr[4]) {
    if(radio_mp3_bitrate_kbps(hdr) == 0) return 0;
    return radio_mp3_samplerates[(hdr[2] >> 2) & 0x03];
}

uint16_t radio_mp3_frame_len(const uint8_t hdr[4]) {
    const uint16_t kbps = radio_mp3_bitrate_kbps(hdr);
    const uint32_t sr = radio_mp3_samplerate(hdr);
    if(kbps == 0 || sr == 0) return 0;
    const uint8_t padding = (hdr[2] >> 1) & 0x01;
    // MPEG1 Layer III: 1152 samples per frame, so 1152/8 = 144 bytes per
    // (bitrate / samplerate), plus one optional padding byte.
    return (uint16_t)((144u * (uint32_t)kbps * 1000u) / sr + padding);
}

int32_t radio_mp3_find_header(const uint8_t* buf, size_t len) {
    if(len < 4) return -1;
    for(size_t i = 0; i + 4 <= len; i++) {
        if(radio_mp3_bitrate_kbps(&buf[i]) == 0) continue;

        // Confirm against the next frame when there is room to look. Without
        // this a chance 0xFF 0xEx pair is indistinguishable from a real frame.
        const uint16_t flen = radio_mp3_frame_len(&buf[i]);
        if(flen >= 4 && i + (size_t)flen + 4 <= len) {
            if(radio_mp3_bitrate_kbps(&buf[i + flen]) == 0) continue;
        }
        return (int32_t)i;
    }
    return -1;
}

size_t radio_strnlen(const char* s, size_t max) {
    size_t n = 0;
    while(n < max && s[n] != '\0')
        n++;
    return n;
}
