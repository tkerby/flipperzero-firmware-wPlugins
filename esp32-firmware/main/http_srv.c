#include "http_srv.h"

#include <string.h>

#include <errno.h>
#include <stdlib.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stream_hub.h"
#include "uart_link.h"
#include "wifi_ap.h"

static const char* TAG = "http_srv";

// web/index.html, linked in by EMBED_FILES.
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

// 96 ms of 128 kbps audio. Sized to hold a whole frame plus the start of the
// next one (417 + 4 bytes at 128 kbps), because this is also the window the
// hub gets to confirm a frame header in -- at 512 bytes it could only ever
// verify a match found in the first 91, so it effectively never verified.
#define STREAM_CHUNK 1536
#define STREAM_IDLE_MS 10 // nothing buffered yet -> wait this long and retry
#define STREAM_PEER_CHECK_MS 500 // how often to test a socket we are not writing to
#define STREAM_IDLE_GIVEUP_MS 30000 // no audio at all for this long -> hang up

// Every listener costs one of these, and a FreeRTOS stack is forced into
// internal RAM -- it cannot be moved to PSRAM like the audio ring can. At 6144
// the third listener was enough to run the board down to 259 bytes free. The
// task's own frame is the STREAM_CHUNK buffer plus a handful of locals, so this
// is still roughly double what it needs; the high-water mark is logged on exit
// so the real figure is measured rather than guessed at.
#define STREAM_TASK_STACK 4096

static httpd_handle_t server = NULL;

// ---------------------------------------------------------------- player page

static esp_err_t root_get(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(
        req, (const char*)index_html_start, index_html_end - index_html_start - 1);
}

// --------------------------------------------------------------------- status

static esp_err_t status_get(httpd_req_t* req) {
    char ssid[WIFI_AP_SSID_MAX + 1];
    char meta[STREAM_META_MAX];
    wifi_ap_get_ssid(ssid, sizeof(ssid));
    stream_hub_get_meta(meta, sizeof(meta));

    // protocol.md asks that a flaky link be visible here rather than only
    // sounding bad, so the decoder's counters ride along with the audio stats.
    UartLinkStats link;
    uart_link_get_stats(&link);

    char body[STREAM_META_MAX + 384];
    int n = snprintf(
        body,
        sizeof(body),
        "{\"station\":\"%s\",\"listeners\":%u,\"max\":%u,\"clients\":%u,\"buf\":%u,"
        "\"bytes\":%llu,"
        "\"link\":{\"up\":%s,\"frames\":%lu,\"crc\":%lu,\"oversize\":%lu,"
        "\"resyncs\":%lu,\"truncated\":%lu,\"audio\":%llu},"
        "\"meta\":%s}",
        ssid,
        stream_hub_listener_count(),
        // Sent rather than hardcoded in the page: the cap is a firmware
        // constant, and a copy of it in the HTML would drift the first time
        // it changed.
        STREAM_MAX_LISTENERS,
        wifi_ap_station_count(),
        stream_hub_fill_percent(),
        stream_hub_total_written(),
        link.ever_received ? "true" : "false",
        (unsigned long)link.frames_ok,
        (unsigned long)link.crc_errors,
        (unsigned long)link.oversize_errors,
        (unsigned long)link.resyncs,
        (unsigned long)link.truncated,
        (unsigned long long)link.audio_bytes,
        meta);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, n);
}

// --------------------------------------------------------------------- stream

// Written by hand instead of httpd_resp_set_* because this response has no
// Content-Length and must not be chunk-encoded: iOS Safari is unreliable
// decoding chunked audio/mpeg. This is the Icecast convention.
static const char STREAM_HEADERS[] = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: audio/mpeg\r\n"
                                     "Cache-Control: no-store, no-cache\r\n"
                                     "Pragma: no-cache\r\n"
                                     "Connection: close\r\n"
                                     "icy-name: Flipper Radio\r\n"
                                     "\r\n";

// Has the phone gone? A stream that is not being written to never finds out
// any other way. MSG_PEEK leaves the (empty) receive side untouched, so this
// costs nothing and cannot disturb the audio going the other direction.
static bool peer_still_there(int fd) {
    if(fd < 0) return false;
    char c;
    const int n = recv(fd, &c, 1, MSG_PEEK | MSG_DONTWAIT);
    // ONLY a zero-length read is proof the far end closed. Treating an
    // unexpected errno as "gone" risks hanging up on a listener that is
    // perfectly fine, which is far worse than holding a slot a while longer --
    // the idle giveup below reclaims a genuinely dead one anyway.
    if(n == 0) return false;
    (void)errno;
    return true;
}

static void stream_task(void* arg) {
    httpd_req_t* req = arg;
    StreamListener* listener = stream_hub_listener_add();

    if(!listener) {
        // Every slot taken. Do not simply refuse: a 503 reaches the page as a
        // bare "unsupported source", and the slots are most likely held by
        // phones that have already gone. Hang up on the stalest and take it.
        if(stream_hub_evict_stalest()) {
            for(int i = 0; i < 25 && !listener; i++) {
                vTaskDelay(pdMS_TO_TICKS(20));
                listener = stream_hub_listener_add();
            }
        }
    }

    if(!listener) {
        const char* busy = "HTTP/1.1 503 Service Unavailable\r\n"
                           "Content-Type: text/plain\r\n"
                           "Connection: close\r\n\r\n"
                           "Station full\r\n";
        httpd_send(req, busy, strlen(busy));
        httpd_req_async_handler_complete(req);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(
        TAG,
        "listener connected (%u total), %llu bytes written so far",
        stream_hub_listener_count(),
        (unsigned long long)stream_hub_total_written());

    bool alive = httpd_send(req, STREAM_HEADERS, sizeof(STREAM_HEADERS) - 1) > 0;
    uint8_t buf[STREAM_CHUNK];
    const int fd = httpd_req_to_sockfd(req);
    uint64_t delivered = 0;
    int64_t last_data_us = esp_timer_get_time();
    int64_t last_peek_us = last_data_us;

    while(alive) {
        if(stream_hub_listener_evicted(listener)) {
            ESP_LOGI(TAG, "listener evicted, closing");
            break;
        }

        size_t n = stream_hub_listener_read(listener, buf, sizeof(buf));
        if(n == 0) {
            // No new audio yet. The browser holds its own buffer, so a short
            // sleep here is inaudible and keeps this task off the CPU.
            //
            // But a quiet stream touches the socket NOWHERE ELSE, so without
            // the checks below this task never learns the phone has gone and
            // holds its listener slot and socket for good. Eight of those and
            // /stream answers 503, which the page can only report as an
            // unsupported source; eleven and the portal stops loading at all.
            const int64_t now = esp_timer_get_time();
            if(now - last_peek_us >= (int64_t)STREAM_PEER_CHECK_MS * 1000) {
                last_peek_us = now;
                if(!peer_still_there(fd)) {
                    ESP_LOGI(TAG, "peer closed while idle");
                    break;
                }
            }
            if(now - last_data_us >= (int64_t)STREAM_IDLE_GIVEUP_MS * 1000) {
                ESP_LOGW(TAG, "no audio for %ds, hanging up", STREAM_IDLE_GIVEUP_MS / 1000);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(STREAM_IDLE_MS));
            continue;
        }

        if(delivered == 0) ESP_LOGI(TAG, "first %u bytes away to the listener", (unsigned)n);
        delivered += n;
        last_data_us = esp_timer_get_time();
        size_t sent = 0;
        while(sent < n) {
            int w = httpd_send(req, (const char*)buf + sent, n - sent);
            if(w <= 0) { // phone closed the tab, locked, or walked out of range
                alive = false;
                break;
            }
            sent += (size_t)w;
        }
    }

    stream_hub_listener_remove(listener);
    httpd_req_async_handler_complete(req);
    ESP_LOGI(
        TAG,
        "listener gone after %llu bytes (%u left, %u stack spare)",
        (unsigned long long)delivered,
        stream_hub_listener_count(),
        (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    vTaskDelete(NULL);
}

static esp_err_t stream_get(httpd_req_t* req) {
    // esp_http_server runs handlers in a shared worker task, so a handler that
    // blocks for the life of a stream would freeze the portal and /status too.
    // Hand the request to its own task and return immediately.

    // WebKit (Safari, and every browser on iOS -- Apple forces them all onto it)
    // takes two slots per press where desktop Chrome takes one, so the split is
    // by engine, not by page. That makes the second request's shape the whole
    // question: WebKit hands progressive audio to AVFoundation, whose loader
    // probes for seek support with a Range header, so a range on the second
    // socket with the SAME cache-busting token means the media stack opened it
    // by itself. Two different tokens and no range would mean the page really
    // did run its start path twice, which is a bug we would own.
    char range[64];
    char agent[64];
    esp_err_t rr = httpd_req_get_hdr_value_str(req, "Range", range, sizeof(range));
    if(rr != ESP_OK && rr != ESP_ERR_HTTPD_RESULT_TRUNC) strlcpy(range, "-", sizeof(range));
    esp_err_t ar = httpd_req_get_hdr_value_str(req, "User-Agent", agent, sizeof(agent));
    if(ar != ESP_OK && ar != ESP_ERR_HTTPD_RESULT_TRUNC) strlcpy(agent, "-", sizeof(agent));
    ESP_LOGI(TAG, "stream request: %s range=%s ua=%s", req->uri, range, agent);

    // A ranged request is AVFoundation asking whether it can seek, not somebody
    // wanting to listen: WebKit sends it 107 ms before the real request and with
    // the same cache-busting token. Ignoring the Range and streaming at it anyway
    // -- what we used to do -- cost a listener slot, a task stack and a full
    // parallel copy of the song for every Apple device. Answering the two bytes
    // it asked for and hanging up settles the question for free. Advertising
    // "Accept-Ranges: none" cannot help, because the probe is already sent before
    // any reply of ours is read.
    if(rr == ESP_OK || rr == ESP_ERR_HTTPD_RESULT_TRUNC) {
        // Two bytes of nothing: the probe only cares that a range request is
        // answered at all, and it discards the body before opening the real
        // stream.
        const uint8_t two[2] = {0, 0};
        httpd_resp_set_status(req, "206 Partial Content");
        httpd_resp_set_type(req, "audio/mpeg");
        // The total length is genuinely unknown on a live stream, hence "*".
        httpd_resp_set_hdr(req, "Content-Range", "bytes 0-1/*");
        httpd_resp_send(req, (const char*)two, sizeof(two));
        ESP_LOGI(TAG, "range probe answered, no listener taken");
        return ESP_OK;
    }

    httpd_req_t* detached = NULL;
    if(httpd_req_async_handler_begin(req, &detached) != ESP_OK) {
        ESP_LOGE(TAG, "could not detach stream request");
        return ESP_FAIL;
    }
    if(xTaskCreate(stream_task, "radio_stream", STREAM_TASK_STACK, detached, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "could not start stream task");
        httpd_req_async_handler_complete(detached);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ------------------------------------------------------------- captive portal
//
// A phone decides whether a network has internet by fetching a known URL and
// comparing the body against an exact string. Until the user has pressed
// Connect we deliberately FAIL that check, which is what makes the sign-in
// sheet open by itself. That sheet is the only thing that reaches the user
// without being asked, so it is where we tell them to open Safari -- the sheet
// itself is a cut-down WebView that cannot play audio at all.
//
// Pressing Connect makes us start PASSING the check for that client, and that
// matters for more than tidiness: iOS reads a dismissed sheet as "I declined to
// sign in" and drops the Wi-Fi association, which takes the stream down with
// it. Passing the probe turns the sheet's Cancel button into Done and keeps the
// phone on the network.

#define PORTAL_MAX_CLIENTS 8
// MACs, packed into the low 48 bits. A failed MAC lookup falls back to the IP
// with bit 48 set, so the two can never be confused for one another.
#define PORTAL_IP_FALLBACK (1ULL << 48)
static uint64_t portal_authed[PORTAL_MAX_CLIENTS];

static const char APPLE_SUCCESS[] =
    "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";

static uint32_t req_peer_ip(httpd_req_t* req) {
    const int fd = httpd_req_to_sockfd(req);
    if(fd < 0) return 0;
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if(getpeername(fd, (struct sockaddr*)&ss, &len) != 0) return 0;
    if(ss.ss_family == AF_INET) return ((struct sockaddr_in*)&ss)->sin_addr.s_addr;
    if(ss.ss_family == AF_INET6) {
        // The AP only hands out IPv4; with CONFIG_LWIP_IPV6 the socket layer
        // reports those addresses v4-mapped, in the low four bytes.
        uint32_t ip;
        memcpy(&ip, (const uint8_t*)&((struct sockaddr_in6*)&ss)->sin6_addr + 12, sizeof(ip));
        return ip;
    }
    return 0;
}

// Identify the client by MAC so that signing in SURVIVES a rejoin. Keying this
// on the IP is what made the network flap back to "no internet" every time the
// phone dropped and reconnected on a fresh DHCP lease.
static uint64_t req_client_id(httpd_req_t* req) {
    const uint32_t ip = req_peer_ip(req);
    if(!ip) return 0;
    uint8_t mac[6];
    if(!wifi_ap_mac_for_ip(ip, mac)) return PORTAL_IP_FALLBACK | (uint64_t)ip;
    uint64_t id = 0;
    for(int i = 0; i < 6; i++) {
        id = (id << 8) | mac[i];
    }
    return id;
}

static bool portal_is_authed(uint64_t id) {
    if(!id) return false;
    for(int i = 0; i < PORTAL_MAX_CLIENTS; i++) {
        if(portal_authed[i] == id) return true;
    }
    return false;
}

static void portal_auth(uint64_t id) {
    if(!id || portal_is_authed(id)) return;
    for(int i = 0; i < PORTAL_MAX_CLIENTS; i++) {
        if(portal_authed[i] == 0) {
            portal_authed[i] = id;
            return;
        }
    }
    // Full: drop the oldest rather than leave a new phone stuck unauthorised
    // and unable to get the sheet off its screen.
    memmove(portal_authed, portal_authed + 1, sizeof(portal_authed) - sizeof(portal_authed[0]));
    portal_authed[PORTAL_MAX_CLIENTS - 1] = id;
}

// The sheet polls its probe URL, so this is also what flips Cancel to Done.
static esp_err_t probe_get(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    if(!portal_is_authed(req_client_id(req))) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/?portal=1");
        return httpd_resp_send(req, NULL, 0);
    }

    // Each vendor checks for its own exact body, so give each the one it wants.
    const char* uri = req->uri;
    if(strstr(uri, "204")) {
        httpd_resp_set_status(req, "204 No Content");
        return httpd_resp_send(req, NULL, 0);
    }
    if(strstr(uri, "connecttest")) {
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "Microsoft Connect Test");
    }
    if(strstr(uri, "ncsi")) {
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "Microsoft NCSI");
    }
    if(strstr(uri, "success.txt")) {
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "success\n");
    }
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, APPLE_SUCCESS, sizeof(APPLE_SUCCESS) - 1);
}

// What the sheet's Connect button calls. Reachable two ways on purpose: the
// page fetches it and updates in place, but the button is also a plain link,
// so it still works if scripting in the sign-in sheet does not.
static esp_err_t connect_get(httpd_req_t* req) {
    const uint64_t id = req_client_id(req);
    portal_auth(id);
    ESP_LOGI(TAG, "portal: client %012llx signed in", (unsigned long long)(id & 0xFFFFFFFFFFFFULL));

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    char q[32] = {0};
    const bool want_html = httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
                           strstr(q, "html") != NULL;
    if(!want_html) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(
        req,
        "<!doctype html><meta name=viewport content=\"width=device-width,initial-scale=1\">"
        // Sends the sheet to its own probe URL, which now passes for this
        // client -- that, not reaching /connect, is what makes iOS close it.
        "<meta http-equiv=refresh "
        "content=\"3;url=http://captive.apple.com/hotspot-detect.html\">"
        "<style>body{margin:0;font-family:system-ui,sans-serif;background:#0b0b0f;color:#f0f0f5;"
        "display:flex;flex-direction:column;align-items:center;justify-content:center;"
        "min-height:100vh;gap:1rem;text-align:center;padding:1.5rem}"
        "b{color:#ff8c1a}.a{font-size:1.6rem;font-weight:700;color:#ff8c1a}</style>"
        "<h2>Connected &#10003;</h2><p>Open <b>Safari</b> and go to:</p>"
        "<div class=a>192.168.4.1</div><p>Then tap <b>Done</b> to close this window.</p>");
}

// Anything else we do not serve. An unauthorised client is almost certainly a
// probe URL we have not listed, so send it to the portal view; an authorised
// one is a person who mistyped a path, so send them to the player.
static esp_err_t captive_redirect(httpd_req_t* req, httpd_err_code_t err) {
    (void)err;
    const bool authed = portal_is_authed(req_client_id(req));
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(
        req, "Location", authed ? "http://192.168.4.1/" : "http://192.168.4.1/?portal=1");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ----------------------------------------------------------------------- init

static const httpd_uri_t routes[] = {
    {.uri = "/", .method = HTTP_GET, .handler = root_get},
    {.uri = "/stream", .method = HTTP_GET, .handler = stream_get},
    {.uri = "/status", .method = HTTP_GET, .handler = status_get},
    {.uri = "/connect", .method = HTTP_GET, .handler = connect_get},
    // The connectivity-check URLs every platform uses.
    {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = probe_get},
    {.uri = "/library/test/success.html", .method = HTTP_GET, .handler = probe_get},
    {.uri = "/generate_204", .method = HTTP_GET, .handler = probe_get},
    {.uri = "/gen_204", .method = HTTP_GET, .handler = probe_get},
    {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = probe_get},
    {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = probe_get},
    {.uri = "/success.txt", .method = HTTP_GET, .handler = probe_get},
};
#define ROUTE_COUNT (sizeof(routes) / sizeof(routes[0]))

void http_srv_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Derived, never hand-counted: this defaults to 8, and quietly adding a
    // ninth route made httpd_register_uri_handler return HANDLERS_FULL, which
    // ESP_ERROR_CHECK turned into an abort() and a boot loop.
    config.max_uri_handlers = ROUTE_COUNT;
    // Each listener holds a socket for the whole stream, so the pool must cover
    // STREAM_MAX_LISTENERS plus headroom for portal and /status requests.
    config.max_open_sockets = STREAM_MAX_LISTENERS + 3;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 4096;

    if(httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }

    for(size_t i = 0; i < ROUTE_COUNT; i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &routes[i]));
    }
    ESP_ERROR_CHECK(httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_redirect));

    ESP_LOGI(TAG, "serving / , /stream , /status on 192.168.4.1");
}
