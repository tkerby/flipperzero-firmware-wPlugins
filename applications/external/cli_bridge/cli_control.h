#pragma once

#include <furi.h>

bool clicontrol_hijack(size_t tx_size, size_t rx_size);
void clicontrol_unhijack(bool persist);
size_t clicontrol_bytes_available(void);
size_t clicontrol_send(const void* data, size_t size, uint32_t timeout);
size_t clicontrol_receive(void* data, size_t size, uint32_t timeout);
