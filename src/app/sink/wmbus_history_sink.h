#pragma once

#include "wmbus_packet_sink.h"

#include "../../ui/views/wmbus_rx_view.h"
#include <notification/notification.h>

typedef struct {
    WmBusRxView* rx_view;
    NotificationApp* notification;
    WmBusPacketSink sink;
} WmBusHistorySink;

void wmbus_history_sink_init(
    WmBusHistorySink* history_sink,
    WmBusRxView* rx_view,
    NotificationApp* notification);
