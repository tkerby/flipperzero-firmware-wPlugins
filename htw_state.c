#include "htw_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <stdlib.h>
#include <string.h>

// Magic for saved struct validation
#define HTW_STATE_MAGIC 0x48545753 // "HTWS"

HtwState* htw_state_alloc(void) {
    HtwState* state = malloc(sizeof(HtwState));
    htw_state_reset(state);
    return state;
}

void htw_state_free(HtwState* state) {
    if(state) {
        free(state);
    }
}

void htw_state_reset(HtwState* state) {
    if(!state) return;

    state->mode = HTW_DEFAULT_MODE;
    state->fan = HTW_DEFAULT_FAN;
    state->temp = HTW_DEFAULT_TEMP;

    state->swing = false;
    state->turbo = false;
    state->fresh = false;
    state->led = true; // LED usually on by default
    state->clean = false;

    state->timer_on_step = 0;
    state->timer_off_step = 0;

    state->save_state = HTW_DEFAULT_SAVE_STATE;
}

bool htw_state_load(HtwState* state) {
    if(!state) return false;

    // First, try to load the file
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Check if file exists
    if(!storage_file_exists(storage, HTW_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        htw_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, HTW_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Read header
        uint32_t magic = 0;
        uint8_t version = 0;

        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version)) {
            if(magic == HTW_STATE_MAGIC && version == HTW_STATE_FILE_VERSION) {
                // Read state data
                HtwState temp_state;
                if(storage_file_read(file, &temp_state, sizeof(HtwState)) == sizeof(HtwState)) {
                    // Validate loaded data
                    if(temp_state.mode < HtwModeCount && temp_state.fan < HtwFanCount &&
                       temp_state.temp >= HTW_TEMP_MIN && temp_state.temp <= HTW_TEMP_MAX &&
                       temp_state.timer_on_step <= HTW_TIMER_STEPS_COUNT &&
                       temp_state.timer_off_step <= HTW_TIMER_STEPS_COUNT) {
                        // Check save_state setting
                        if(temp_state.save_state) {
                            // Copy all data
                            memcpy(state, &temp_state, sizeof(HtwState));
                            success = true;
                        } else {
                            // save_state was false - reset to defaults but keep save_state=false
                            htw_state_reset(state);
                            state->save_state = false;
                            success = true;
                        }
                    }
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) {
        htw_state_reset(state);
    }

    return success;
}

bool htw_state_save(HtwState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, HTW_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write header
        uint32_t magic = HTW_STATE_MAGIC;
        uint8_t version = HTW_STATE_FILE_VERSION;

        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            if(state->save_state) {
                // Save full state
                success = storage_file_write(file, state, sizeof(HtwState)) == sizeof(HtwState);
            } else {
                // Save only save_state=false, rest as defaults
                HtwState temp_state;
                htw_state_reset(&temp_state);
                temp_state.save_state = false;
                success = storage_file_write(file, &temp_state, sizeof(HtwState)) ==
                          sizeof(HtwState);
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

void htw_state_mode_next(HtwState* state) {
    if(!state) return;
    state->mode = (state->mode + 1) % HtwModeCount;
}

void htw_state_mode_prev(HtwState* state) {
    if(!state) return;
    if(state->mode == 0) {
        state->mode = HtwModeCount - 1;
    } else {
        state->mode--;
    }
}

void htw_state_fan_next(HtwState* state) {
    if(!state) return;
    state->fan = (state->fan + 1) % HtwFanCount;
}

void htw_state_fan_prev(HtwState* state) {
    if(!state) return;
    if(state->fan == 0) {
        state->fan = HtwFanCount - 1;
    } else {
        state->fan--;
    }
}

void htw_state_temp_up(HtwState* state) {
    if(!state) return;
    if(state->temp < HTW_TEMP_MAX) {
        state->temp++;
    }
}

void htw_state_temp_down(HtwState* state) {
    if(!state) return;
    if(state->temp > HTW_TEMP_MIN) {
        state->temp--;
    }
}

void htw_state_toggle(HtwState* state, HtwToggle toggle) {
    if(!state) return;

    switch(toggle) {
    case HtwToggleSwing:
        state->swing = !state->swing;
        break;
    case HtwToggleLed:
        state->led = !state->led;
        break;
    case HtwToggleTurbo:
        state->turbo = !state->turbo;
        break;
    case HtwToggleFresh:
        state->fresh = !state->fresh;
        break;
    case HtwToggleClean:
        state->clean = !state->clean;
        break;
    default:
        break;
    }
}

void htw_state_timer_on_cycle(HtwState* state, bool forward) {
    if(!state) return;

    if(forward) {
        if(state->timer_on_step < HTW_TIMER_STEPS_COUNT) {
            state->timer_on_step++;
        } else {
            state->timer_on_step = 0; // Wrap to off
        }
    } else {
        if(state->timer_on_step > 0) {
            state->timer_on_step--;
        } else {
            state->timer_on_step = HTW_TIMER_STEPS_COUNT; // Wrap to max
        }
    }
}

void htw_state_timer_off_cycle(HtwState* state, bool forward) {
    if(!state) return;

    if(forward) {
        if(state->timer_off_step < HTW_TIMER_STEPS_COUNT) {
            state->timer_off_step++;
        } else {
            state->timer_off_step = 0;
        }
    } else {
        if(state->timer_off_step > 0) {
            state->timer_off_step--;
        } else {
            state->timer_off_step = HTW_TIMER_STEPS_COUNT;
        }
    }
}

bool htw_state_can_change_fan(const HtwState* state) {
    if(!state) return false;
    // Auto and Dry modes have fixed fan speed
    return state->mode != HtwModeAuto && state->mode != HtwModeDry && state->mode != HtwModeOff;
}

bool htw_state_can_change_temp(const HtwState* state) {
    if(!state) return false;
    // Fan-only mode has no temperature, Off mode also
    return state->mode != HtwModeFan && state->mode != HtwModeOff;
}
