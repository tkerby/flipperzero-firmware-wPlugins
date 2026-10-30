#include "cli_control.h"
#include <cli/cli_gui_bridge.h>

static const CliGuiBridge* bridge;

bool clicontrol_hijack(size_t tx_size, size_t rx_size) {
    if(bridge) return true;
    if(!furi_record_exists(RECORD_CLI_GUI_BRIDGE)) return false;

    bridge = furi_record_open(RECORD_CLI_GUI_BRIDGE);
    if(bridge->version != CLI_GUI_BRIDGE_API_VERSION || !bridge->open(tx_size, rx_size)) {
        furi_record_close(RECORD_CLI_GUI_BRIDGE);
        bridge = NULL;
        return false;
    }
    return true;
}

void clicontrol_unhijack(bool persist) {
    if(!bridge) return;
    bridge->close(persist);
    bridge = NULL;
    furi_record_close(RECORD_CLI_GUI_BRIDGE);
}

size_t clicontrol_bytes_available(void) {
    return bridge ? bridge->available() : 0;
}

size_t clicontrol_send(const void* data, size_t size, uint32_t timeout) {
    if(!bridge) return 0;
    const uint8_t* bytes = data;
    const uint32_t start = furi_get_tick();
    size_t sent = 0;
    while(sent < size) {
        size_t count = bridge->send(bytes + sent, size - sent);
        sent += count;
        if(sent == size || (timeout != FuriWaitForever && furi_get_tick() - start >= timeout))
            break;
        if(!count) furi_delay_tick(1);
    }
    return sent;
}

size_t clicontrol_receive(void* data, size_t size, uint32_t timeout) {
    if(!bridge) return 0;
    uint8_t* bytes = data;
    const uint32_t start = furi_get_tick();
    size_t received = 0;
    while(received < size) {
        size_t count = bridge->receive(bytes + received, size - received);
        received += count;
        if(received == size || (timeout != FuriWaitForever && furi_get_tick() - start >= timeout))
            break;
        if(!count) furi_delay_tick(1);
    }
    return received;
}
