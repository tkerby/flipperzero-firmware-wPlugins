#include "tt_timer.h"

#include <furi.h>

static uint32_t tt_timer_elapsed_ms(const TtTimer* timer) {
    uint32_t elapsed = timer->accumulated_ms;
    if(timer->state == TtTimerRunning) {
        /* Differenz ist auch über einen Tick-Überlauf hinweg korrekt (unsigned) */
        uint32_t ticks = furi_get_tick() - timer->run_start_tick;
        uint32_t freq = furi_kernel_get_tick_frequency();
        /* Getrennt rechnen: ticks * 1000 würde nach gut 71 Minuten überlaufen */
        elapsed += (ticks / freq) * 1000U + ((ticks % freq) * 1000U) / freq;
    }
    return elapsed;
}

static void tt_timer_clear(TtTimer* timer) {
    timer->state = TtTimerIdle;
    timer->accumulated_ms = 0;
    timer->run_start_tick = 0;
    timer->warn1_done = false;
    timer->warn2_done = false;
    timer->end_done = false;
    timer->overtime_done = 0;
}

void tt_timer_configure(
    TtTimer* timer,
    uint32_t total_seconds,
    uint32_t warn1_seconds,
    uint32_t warn2_seconds,
    uint32_t overtime_seconds,
    bool end_alert) {
    furi_assert(timer);
    timer->total_seconds = total_seconds;
    timer->warn1_seconds = warn1_seconds;
    timer->warn2_seconds = warn2_seconds;
    timer->overtime_seconds = overtime_seconds;
    timer->end_alert = end_alert;
    tt_timer_clear(timer);
}

void tt_timer_start(TtTimer* timer) {
    furi_assert(timer);
    if(timer->state == TtTimerRunning) return;
    timer->run_start_tick = furi_get_tick();
    timer->state = TtTimerRunning;
}

void tt_timer_pause(TtTimer* timer) {
    furi_assert(timer);
    if(timer->state != TtTimerRunning) return;
    timer->accumulated_ms = tt_timer_elapsed_ms(timer);
    timer->state = TtTimerPaused;
}

void tt_timer_reset(TtTimer* timer) {
    furi_assert(timer);
    tt_timer_clear(timer);
}

uint32_t tt_timer_elapsed_seconds(const TtTimer* timer) {
    furi_assert(timer);
    return tt_timer_elapsed_ms(timer) / 1000U;
}

bool tt_timer_is_started(const TtTimer* timer) {
    furi_assert(timer);
    return timer->state != TtTimerIdle;
}

/* Warnung ist nur sinnvoll, wenn sie innerhalb der Redezeit liegt */
static bool tt_timer_warn_due(const TtTimer* timer, uint32_t warn_seconds, uint32_t elapsed) {
    if(warn_seconds == 0 || warn_seconds >= timer->total_seconds) return false;
    return elapsed >= timer->total_seconds - warn_seconds;
}

TtTimerAlert tt_timer_poll(TtTimer* timer) {
    furi_assert(timer);
    if(timer->state != TtTimerRunning) return TtTimerAlertNone;

    uint32_t elapsed = tt_timer_elapsed_seconds(timer);

    /* Reihenfolge: das wichtigste Ereignis zuerst; es erledigt die früheren mit */
    if(elapsed >= timer->total_seconds) {
        if(!timer->end_done) {
            timer->end_done = true;
            timer->warn1_done = true;
            timer->warn2_done = true;
            if(timer->end_alert) return TtTimerAlertEnd;
        }
        if(timer->overtime_seconds > 0) {
            uint32_t due = (elapsed - timer->total_seconds) / timer->overtime_seconds;
            if(due > timer->overtime_done) {
                timer->overtime_done = due;
                return TtTimerAlertOvertime;
            }
        }
        return TtTimerAlertNone;
    }

    if(!timer->warn2_done && tt_timer_warn_due(timer, timer->warn2_seconds, elapsed)) {
        timer->warn2_done = true;
        /* Liegt Warnung 1 gleich oder später, ist sie damit ebenfalls erledigt */
        if(timer->warn1_seconds <= timer->warn2_seconds) timer->warn1_done = true;
        return TtTimerAlertWarn2;
    }
    if(!timer->warn1_done && tt_timer_warn_due(timer, timer->warn1_seconds, elapsed)) {
        timer->warn1_done = true;
        return TtTimerAlertWarn1;
    }
    return TtTimerAlertNone;
}
