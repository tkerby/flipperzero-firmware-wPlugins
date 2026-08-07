#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Encode byte array to hexadecimal string (big-endian).
 *
 * @param data Input byte array
 * @param data_len Length of input array
 * @param out Output buffer
 * @param out_size Size of output buffer
 */
void wmbus_hex_encode(const uint8_t* data, size_t data_len, char* out, size_t out_size);
