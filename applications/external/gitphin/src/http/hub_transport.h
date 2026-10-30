#pragma once

/*
 * Byte-stream transport for the HTTP server.
 *
 * The HTTP layer only sees "connections" that it can read from and write to.
 * Today one connection is carried in frames over the second USB CDC channel,
 * and a tiny bridge on the PC maps each TCP connection onto it.
 * A USB network transport (CDC-ECM + TCP/IP) can replace this file later
 * without changes to hub_http.c.
 *
 * Frame format (both directions):
 *   0xA5 | type | len_lo | len_hi | payload[len]
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HUB_FRAME_SYNC        0xA5
#define HUB_FRAME_OPEN        0x01 /* PC -> Flipper: new connection */
#define HUB_FRAME_DATA        0x02 /* both: payload bytes */
#define HUB_FRAME_CLOSE       0x03 /* both: end of connection */
#define HUB_FRAME_PING        0x10 /* PC -> Flipper */
#define HUB_FRAME_PONG        0x11 /* Flipper -> PC */
#define HUB_FRAME_MAX_PAYLOAD 1024

typedef struct HubTransport HubTransport;

HubTransport* hub_transport_alloc(void);
void hub_transport_free(HubTransport* t);

/* Wait for the PC to open a connection. Answers pings while it waits. */
bool hub_transport_accept(HubTransport* t, uint32_t timeout_ms);

/* Read up to len bytes. Returns 0 when the peer closed or on timeout. */
size_t hub_conn_read(HubTransport* t, uint8_t* buf, size_t len, uint32_t timeout_ms);

/* Write all bytes. Returns false when the PC stopped reading. */
bool hub_conn_write(HubTransport* t, const void* data, size_t len);

/* Tell the PC that the response is complete. */
void hub_conn_close(HubTransport* t);

/* Answer pending PING frames without waiting (call it while a request is busy
 * and its body is fully read). */
void hub_transport_service(HubTransport* t);
