#include "dns_captive.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

static const char* TAG = "dns_captive";

#define DNS_PORT 53
#define DNS_MAX_MSG 512 // classic UDP DNS limit; anything larger we ignore
#define AP_IP_BYTES {192, 168, 4, 1}

// Minimal DNS header, packed as it appears on the wire.
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

#define DNS_QTYPE_A 1
#define DNS_QCLASS_IN 1

// Walk a (possibly compressed) QNAME and return the offset just past it.
// Returns 0 if the name is malformed or runs off the end of the message.
static size_t dns_skip_qname(const uint8_t* msg, size_t len, size_t off) {
    while(off < len) {
        uint8_t label = msg[off];
        if(label == 0) return off + 1;
        if((label & 0xC0) == 0xC0) return (off + 2 <= len) ? off + 2 : 0; // pointer
        if(label > 63) return 0; // invalid label length
        off += 1 + label;
    }
    return 0;
}

// Build a reply that points every A question at our own IP. Returns the reply
// length, or 0 if the query is not something we should answer.
static size_t dns_build_reply(const uint8_t* q, size_t qlen, uint8_t* out, size_t out_cap) {
    if(qlen < sizeof(dns_header_t) || qlen > DNS_MAX_MSG) return 0;

    dns_header_t hdr;
    memcpy(&hdr, q, sizeof(hdr));
    // Only plain standard queries: QR=0, OPCODE=0.
    if(ntohs(hdr.flags) & 0x8000) return 0;
    if((ntohs(hdr.flags) >> 11) & 0x0F) return 0;
    uint16_t qdcount = ntohs(hdr.qdcount);
    if(qdcount == 0) return 0;

    size_t off = sizeof(dns_header_t);
    size_t qstart = off;

    // Validate every question, and remember whether any was an A record.
    bool has_a = false;
    for(uint16_t i = 0; i < qdcount; i++) {
        size_t after = dns_skip_qname(q, qlen, off);
        if(!after || after + 4 > qlen) return 0;
        uint16_t qtype = (uint16_t)((q[after] << 8) | q[after + 1]);
        uint16_t qclass = (uint16_t)((q[after + 2] << 8) | q[after + 3]);
        if(qtype == DNS_QTYPE_A && qclass == DNS_QCLASS_IN) has_a = true;
        off = after + 4;
    }
    // Let AAAA and friends go unanswered; phones fall back to IPv4 quickly,
    // and a bogus A answer for an AAAA query confuses some clients.
    if(!has_a) return 0;

    size_t qsection = off - qstart;
    // header + echoed questions + one answer record (name ptr, type, class,
    // ttl, rdlength, 4-byte address)
    size_t reply_len = sizeof(dns_header_t) + qsection + 16;
    if(reply_len > out_cap) return 0;

    dns_header_t* rh = (dns_header_t*)out;
    rh->id = hdr.id;
    rh->flags = htons(0x8180); // QR=1, RD copied, RA=1, NOERROR
    rh->qdcount = htons(qdcount);
    rh->ancount = htons(1);
    rh->nscount = 0;
    rh->arcount = 0;

    memcpy(out + sizeof(dns_header_t), q + qstart, qsection);

    uint8_t* a = out + sizeof(dns_header_t) + qsection;
    a[0] = 0xC0; // compression pointer to the first question's name
    a[1] = (uint8_t)sizeof(dns_header_t);
    a[2] = 0x00;
    a[3] = DNS_QTYPE_A;
    a[4] = 0x00;
    a[5] = DNS_QCLASS_IN;
    // TTL 0: never let a phone cache this hijacked answer beyond the session.
    a[6] = a[7] = a[8] = a[9] = 0x00;
    a[10] = 0x00;
    a[11] = 0x04; // rdlength
    const uint8_t ip[4] = AP_IP_BYTES;
    memcpy(&a[12], ip, 4);

    return reply_len;
}

static void dns_task(void* arg) {
    (void)arg;
    uint8_t rx[DNS_MAX_MSG];
    uint8_t tx[DNS_MAX_MSG];

    while(1) {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if(sock < 0) {
            ESP_LOGE(TAG, "socket failed (%d), retrying", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(DNS_PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY),
        };
        if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "bind :53 failed (%d), retrying", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "answering all A queries with 192.168.4.1");

        while(1) {
            struct sockaddr_in src;
            socklen_t srclen = sizeof(src);
            int n = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr*)&src, &srclen);
            if(n < 0) {
                ESP_LOGW(TAG, "recvfrom failed (%d), resetting socket", errno);
                break;
            }
            size_t reply = dns_build_reply(rx, (size_t)n, tx, sizeof(tx));
            if(reply) {
                sendto(sock, tx, reply, 0, (struct sockaddr*)&src, srclen);
            }
        }
        close(sock);
    }
}

void dns_captive_start(void) {
    xTaskCreate(dns_task, "dns_captive", 3072, NULL, 4, NULL);
}
