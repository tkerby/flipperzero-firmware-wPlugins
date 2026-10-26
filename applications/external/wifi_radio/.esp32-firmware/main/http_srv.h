#pragma once
// HTTP server: "/" player page, "/stream" live MP3 fan-out, "/status" JSON,
// plus the OS captive-check URLs (redirect to "/"). Phase 1 (stream from an
// embedded loop), Phase 2 (stream from the UART ring buffer).
void http_srv_start(void);
