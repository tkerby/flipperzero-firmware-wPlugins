#include "link_glue.h"

#include <furi.h>
#include <gblink.h>

/* gblink callback, ISR context: one byte just finished shifting in; the
 * byte we hand back is shifted out during the next transfer. */
static void link_rx_callback(void* context, uint8_t in) {
    PkbLink* link = context;
    uint8_t tx = pbl_on_byte(&link->session, in, furi_get_tick());
    gblink_transfer(link->gblink, tx);
}

static void link_notify(void* context, uint32_t events) {
    PkbLink* link = context;
    atomic_fetch_or(&link->events, events);
}

void pkb_link_start(PkbLink* link, void* gblink) {
    furi_assert(!link->running);
    link->gblink = gblink;
    atomic_store(&link->events, 0);
    link->session.notify = link_notify;
    link->session.notify_ctx = link;
    pbl_start(&link->session);

    gblink_clk_source_set(gblink, GBLINK_CLK_EXT);
    gblink_callback_set(gblink, link_rx_callback, link);
    /* Only sent if we ever fail to answer in time: what an idle GB sends */
    gblink_nobyte_set(gblink, PKB_SERIAL_NO_DATA_BYTE);
    gblink_start(gblink);
    /* Preload the handshake reply for the master's first $01 */
    gblink_transfer(gblink, 0x02);
    link->running = true;
}

void pkb_link_stop(PkbLink* link) {
    if(!link->running) return;
    gblink_stop(link->gblink);
    link->running = false;
}

uint32_t pkb_link_take_events(PkbLink* link) {
    return atomic_exchange(&link->events, 0);
}
