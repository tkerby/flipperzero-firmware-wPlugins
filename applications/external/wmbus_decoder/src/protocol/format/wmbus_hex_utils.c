#include "wmbus_hex_utils.h"

#include <stdio.h>

void wmbus_hex_encode(const uint8_t* data, size_t data_len, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(!data) return;

    size_t write = 0;
    for(size_t i = 0; i < data_len && (write + 2U) < out_size; i++) {
        snprintf(&out[write], out_size - write, "%02X", data[i]);
        write += 2U;
    }
    out[write] = '\0';
}
