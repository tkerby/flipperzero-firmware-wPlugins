#pragma once

#include <stddef.h>
#include "../model/wmbus_application_types.h"

bool wmbus_decode_bcd(const uint8_t* data, uint8_t data_len, uint64_t* value);
bool wmbus_decode_date(const uint8_t* data, WmBusApplicationDateTime* out);
