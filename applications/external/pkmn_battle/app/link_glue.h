#pragma once

#include <stdatomic.h>
#include <pkb/session.h>

/* Connects the gblink byte interrupt to the portable session state machine. */
typedef struct {
    PblSession session;
    void* gblink;
    atomic_uint events; /* PBL_EV_* accumulated from the ISR */
    bool running;
} PkbLink;

/* Starts listening as the external-clock slave (TX buffers must be set). */
void pkb_link_start(PkbLink* link, void* gblink);
void pkb_link_stop(PkbLink* link);
/* Returns and clears the events raised since the last call. */
uint32_t pkb_link_take_events(PkbLink* link);
