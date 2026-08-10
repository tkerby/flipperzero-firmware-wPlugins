#include "wmbus_history_sink.h"

#include <notification/notification_messages.h>

static const NotificationSequence wmbus_history_sink_stored_sequence = {
    &message_note_c7,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static void wmbus_history_sink_consume(
    void* context,
    const WmBusSettings* settings,
    const WmBusPacketRecord* record) {
    WmBusHistorySink* history_sink = context;

    if(!history_sink || !history_sink->rx_view || !settings || !record) {
        return;
    }

    bool store_in_history = wmbus_packet_record_passes_policy(
        record, settings->memory_quality, settings->min_rssi_dbm);
    bool rssi_gate_ok = (settings->min_rssi_dbm >= 0) || (record->rssi >= settings->min_rssi_dbm);
    wmbus_rx_view_push_packet(history_sink->rx_view, record, rssi_gate_ok, store_in_history);
    if(store_in_history && history_sink->notification) {
        notification_message(history_sink->notification, &wmbus_history_sink_stored_sequence);
    }
}

void wmbus_history_sink_init(
    WmBusHistorySink* history_sink,
    WmBusRxView* rx_view,
    NotificationApp* notification) {
    if(!history_sink) {
        return;
    }

    *history_sink = (WmBusHistorySink){
        .rx_view = rx_view,
        .notification = notification,
        .sink =
            {
                .context = history_sink,
                .consume = wmbus_history_sink_consume,
            },
    };
}
