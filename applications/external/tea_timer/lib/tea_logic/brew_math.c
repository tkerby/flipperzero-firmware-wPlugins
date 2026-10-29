#include "brew_math.h"

#include <stdio.h>
#include <string.h>

void brew_session_reset_from_preset(const TeaPreset* preset, BrewSession* session) {
    memset(session, 0, sizeof(BrewSession));
    if(preset != NULL) {
        session->preset = *preset;
        session->has_preset = true;
        session->next_pour_sec = preset->first_pour_sec;
    }
    session->completed_pours = 0;
    session->phase = BrewPhaseIDLE;
    session->deadline_tick = 0;
}

uint16_t brew_math_pour_sec(uint16_t prev, uint16_t increment) {
    uint32_t next = (uint32_t)prev + (uint32_t)increment;
    if(next > BREW_NEXT_POUR_MAX) {
        next = BREW_NEXT_POUR_MAX;
    }
    return (uint16_t)next;
}

void brew_session_start(uint32_t now_tick, BrewSession* session) {
    if(session->phase == BrewPhaseRUNNING) {
        return;
    }
    session->deadline_tick = now_tick + (uint32_t)session->next_pour_sec * 1000U;
    session->phase = BrewPhaseRUNNING;
}

uint16_t brew_session_remaining_sec(uint32_t now_tick, uint32_t deadline_tick) {
    if(deadline_tick > now_tick) {
        return (uint16_t)((deadline_tick - now_tick + 999U) / 1000U);
    }
    return 0;
}

bool brew_session_complete(BrewSession* session) {
    if(session->phase != BrewPhaseRUNNING) {
        return false;
    }
    session->completed_pours++;
    session->next_pour_sec =
        brew_math_pour_sec(session->next_pour_sec, session->preset.increment_sec);
    session->phase = BrewPhaseDONE;
    return true;
}

void brew_session_abort(BrewSession* session) {
    if(session->phase == BrewPhaseRUNNING) {
        session->phase = BrewPhaseIDLE;
    }
}

void brew_session_clear(BrewSession* session) {
    memset(session, 0, sizeof(BrewSession));
    session->has_preset = false;
    session->completed_pours = 0;
    session->next_pour_sec = 0;
    session->phase = BrewPhaseIDLE;
    session->deadline_tick = 0;
}

bool brew_session_over_limit(const BrewSession* session) {
    return session->completed_pours > session->preset.max_pours;
}

void brew_math_format_time(uint16_t sec, char* buf, size_t size) {
    if(size == 0) {
        return;
    }
    if(sec < 3600U) {
        snprintf(buf, size, "%02u:%02u", (unsigned)(sec / 60U), (unsigned)(sec % 60U));
    } else {
        snprintf(
            buf,
            size,
            "%02u:%02u:%02u",
            (unsigned)(sec / 3600U),
            (unsigned)((sec % 3600U) / 60U),
            (unsigned)(sec % 60U));
    }
}
