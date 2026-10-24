#pragma once
#include "catdriver.h"

#define UART_CH_ESP (momentum_settings.uart_esp_channel)
#define UART_CH_GPS (momentum_settings.uart_nmea_channel)

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

void catdriver_uart_init(Context* ctx);
void catdriver_uart_deinit(Context* ctx);
