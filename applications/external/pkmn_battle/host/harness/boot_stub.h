#pragma once

#include <stdint.h>

#define BOOT_STUB_SIZE 0x900

void boot_stub_build(uint8_t rom[BOOT_STUB_SIZE]);
