#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * PT2257 Electronic Volume Controller driver (Flipper Zero, external I2C).
 *
 * Attenuation is expressed as unsigned integer dB: 0..79 (0 => 0dB, 79 => -79dB).
 * This mirrors the API style in victornpb/evc_pt2257.
 */

bool pt2257_is_device_ready(void);

bool pt2257_set_attenuation_db(uint8_t attenuation_db);
bool pt2257_set_attenuation_left_db(uint8_t attenuation_db);
bool pt2257_set_attenuation_right_db(uint8_t attenuation_db);

bool pt2257_mute(bool enable);
bool pt2257_off(void);
