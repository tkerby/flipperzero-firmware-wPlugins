#pragma once

#include <furi.h>

typedef struct {
    uint8_t last_pet_day;
    uint8_t last_pet_month;
    uint16_t last_pet_year;

    uint32_t daily_pet_count;
} PetAppState;

bool pet_app_state_save(const PetAppState* state);
bool pet_app_state_load(PetAppState* state);
