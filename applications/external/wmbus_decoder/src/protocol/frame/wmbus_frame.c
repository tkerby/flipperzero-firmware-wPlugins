#include "wmbus_frame.h"

#include <stdio.h>
#include <string.h>

void wmbus_frame_decode_mfg(uint16_t man, char out[WMBUS_MFG_STR_LEN]) {
    out[0] = (char)(((man >> 10) & 0x1F) + ('A' - 1));
    out[1] = (char)(((man >> 5) & 0x1F) + ('A' - 1));
    out[2] = (char)((man & 0x1F) + ('A' - 1));
    out[3] = '\0';
    for(size_t i = 0; i < 3; i++) {
        if(out[i] < 'A' || out[i] > 'Z') {
            out[i] = '?';
        }
    }
}

void wmbus_frame_format_id(const uint8_t id[4], char out[WMBUS_ID_STR_LEN]) {
    snprintf(out, WMBUS_ID_STR_LEN, "%02X%02X%02X%02X", id[3], id[2], id[1], id[0]);
}

static uint16_t wmbus_crc16_en13757(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for(size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            if((((crc & 0x8000) >> 8) ^ (b & 0x80)) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x3D65);
            } else {
                crc = (uint16_t)(crc << 1);
            }
            b <<= 1;
        }
    }
    return (uint16_t)(~crc);
}

typedef struct {
    size_t first_crc_pos;
    size_t second_crc_pos;
} WmBusFrameBLayout;

static bool
    wmbus_frame_b_layout(const uint8_t* data, size_t available_len, WmBusFrameBLayout* layout) {
    if(!data || available_len == 0U || !layout ||
       !wmbus_frame_l_field_valid_for_format(data[0], WmBusFrameFormatB)) {
        return false;
    }

    const size_t frame_len = wmbus_frame_expected_len(data[0], WmBusFrameFormatB);
    if(frame_len == 0U || available_len < frame_len) return false;

    *layout = (WmBusFrameBLayout){
        .first_crc_pos = frame_len <= 128U ? frame_len - 2U : 126U,
        .second_crc_pos = frame_len <= 128U ? 0U : frame_len - 2U,
    };
    return true;
}

bool wmbus_frame_l_field_valid(uint8_t l_field) {
    return l_field >= WMBUS_FRAME_L_FIELD_MIN && l_field <= WMBUS_FRAME_L_FIELD_MAX;
}

bool wmbus_frame_l_field_valid_for_format(uint8_t l_field, WmBusFrameFormat format) {
    if(!wmbus_frame_l_field_valid(l_field)) return false;

    if(format == WmBusFrameFormatA) return true;
    if(format == WmBusFrameFormatB) {
        return l_field >= WMBUS_FRAME_B_L_FIELD_MIN &&
               (l_field <= WMBUS_FRAME_B_SINGLE_CRC_L_FIELD_MAX ||
                l_field >= WMBUS_FRAME_B_DOUBLE_CRC_L_FIELD_MIN);
    }

    return false;
}

size_t wmbus_frame_len_format_a(uint8_t l_field) {
    size_t n = l_field;
    size_t len = 1U + n + 2U; // L + data + first CRC
    if(n > 9U) {
        size_t rem = n - 9U;
        size_t blocks = (rem + 15U) / 16U;
        len += 2U * blocks;
    }
    return len;
}

size_t wmbus_frame_expected_len(uint8_t l_field, WmBusFrameFormat format) {
    if(!wmbus_frame_l_field_valid_for_format(l_field, format)) return 0U;

    if(format == WmBusFrameFormatA) {
        return wmbus_frame_len_format_a(l_field);
    } else if(format == WmBusFrameFormatB) {
        return 1U + (size_t)l_field;
    }
    return 0U;
}

static bool wmbus_frame_crc_matches(const uint8_t* data, size_t data_len, const uint8_t crc[2]) {
    const uint16_t expected = ((uint16_t)crc[0] << 8U) | crc[1];
    return wmbus_crc16_en13757(data, data_len) == expected;
}

static bool wmbus_frame_copy_block(
    uint8_t* out,
    size_t out_max,
    size_t* write,
    const uint8_t* data,
    size_t data_len) {
    if(!out) return true;
    if(!write || *write + data_len > out_max) return false;

    memcpy(&out[*write], data, data_len);
    *write += data_len;
    return true;
}

static bool wmbus_frame_unpack_a(
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    bool* crc_ok) {
    if(!data || len < 12U || !crc_ok || (out && !out_len)) return false;

    size_t write = 0U;
    if(!wmbus_frame_copy_block(out, out_max, &write, data, 10U)) return false;
    *crc_ok = wmbus_frame_crc_matches(data, 10U, &data[10]);

    for(size_t pos = 12U; pos < len;) {
        if(len - pos < 3U) return false;
        const size_t data_len = len - pos - 2U < 16U ? len - pos - 2U : 16U;
        if(!wmbus_frame_copy_block(out, out_max, &write, &data[pos], data_len)) return false;
        *crc_ok &= wmbus_frame_crc_matches(&data[pos], data_len, &data[pos + data_len]);
        pos += data_len + 2U;
    }

    if(out) {
        if(write == 0U || write > UINT8_MAX + 1U) return false;
        out[0] = (uint8_t)(write - 1U);
        *out_len = write;
    }
    return true;
}

static bool wmbus_frame_unpack_b(
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    bool* crc_ok) {
    if(!data || !crc_ok || (out && !out_len)) return false;

    WmBusFrameBLayout layout = {0};
    if(!wmbus_frame_b_layout(data, len, &layout)) return false;

    size_t write = 0U;
    if(!wmbus_frame_copy_block(out, out_max, &write, data, layout.first_crc_pos)) return false;
    *crc_ok = wmbus_frame_crc_matches(data, layout.first_crc_pos, &data[layout.first_crc_pos]);

    if(layout.second_crc_pos > 0U) {
        const size_t data_pos = layout.first_crc_pos + 2U;
        const size_t data_len = layout.second_crc_pos - data_pos;
        if(!wmbus_frame_copy_block(out, out_max, &write, &data[data_pos], data_len)) return false;
        *crc_ok &=
            wmbus_frame_crc_matches(&data[data_pos], data_len, &data[layout.second_crc_pos]);
    }

    if(out) {
        if(write == 0U || write > UINT8_MAX + 1U) return false;
        out[0] = (uint8_t)(write - 1U);
        *out_len = write;
    }
    return true;
}

static bool wmbus_frame_unpack(
    WmBusFrameFormat format,
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    bool* crc_ok) {
    if(format == WmBusFrameFormatA) {
        return wmbus_frame_unpack_a(data, len, out, out_max, out_len, crc_ok);
    } else if(format == WmBusFrameFormatB) {
        return wmbus_frame_unpack_b(data, len, out, out_max, out_len, crc_ok);
    }
    return false;
}

bool wmbus_frame_crc_check(WmBusFrameFormat format, const uint8_t* data, size_t len) {
    bool crc_ok = false;
    return wmbus_frame_unpack(format, data, len, NULL, 0U, NULL, &crc_ok) && crc_ok;
}

bool wmbus_frame_normalize(
    WmBusFrameFormat format,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* normalized,
    size_t normalized_max,
    WmBusFrameNormalizeResult* out) {
    if(!frame || frame_len == 0U || !normalized || !out) return false;

    memset(out, 0, sizeof(*out));

    uint8_t l_field = frame[0];
    if(!wmbus_frame_l_field_valid_for_format(l_field, format)) return false;

    size_t expected_len = wmbus_frame_expected_len(l_field, format);
    if(expected_len == 0U || frame_len < expected_len) return false;

    size_t normalized_len = 0U;
    bool crc_ok = false;
    if(!wmbus_frame_unpack(
           format, frame, expected_len, normalized, normalized_max, &normalized_len, &crc_ok)) {
        return false;
    }

    out->crc_ok = crc_ok;
    out->computed_len = expected_len;
    out->normalized_len = normalized_len;
    return true;
}
