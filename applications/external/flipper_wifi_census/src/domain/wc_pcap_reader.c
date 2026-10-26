#include "include/domain/wc_pcap_reader.h"

#define WC_PCAP_GLOBAL_HDR 24
#define WC_PCAP_REC_HDR 16
#define WC_LINKTYPE_IEEE802_11 105
#define WC_PCAP_MAX_FRAME 4096 // sane upper bound (typical snaplen) to reject garbage lengths

static uint32_t rd_u32(const uint8_t *p, bool le) {
    if (le) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
    }
    return (uint32_t)p[3] | ((uint32_t)p[2] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[0] << 24);
}

bool wc_pcap_read(const uint8_t *data, size_t len, WcPcapFrameFn cb, void *ctx) {
    if (len < WC_PCAP_GLOBAL_HDR) {
        return false;
    }
    bool le;
    if (data[0] == 0xD4 && data[1] == 0xC3 && data[2] == 0xB2 && data[3] == 0xA1) {
        le = true; // file written little-endian
    } else if (data[0] == 0xA1 && data[1] == 0xB2 && data[2] == 0xC3 && data[3] == 0xD4) {
        le = false; // big-endian
    } else {
        return false; // not a classic pcap (nanosecond/pcapng not supported here)
    }
    if (rd_u32(data + 20, le) != WC_LINKTYPE_IEEE802_11) {
        return false;
    }

    size_t off = WC_PCAP_GLOBAL_HDR;
    while (off + WC_PCAP_REC_HDR <= len) {
        uint32_t incl = rd_u32(data + off + 8, le);
        off += WC_PCAP_REC_HDR;
        if (incl > WC_PCAP_MAX_FRAME || off + incl > len) {
            break; // truncated or implausible record — stop cleanly
        }
        cb(ctx, data + off, incl);
        off += incl;
    }
    return true;
}

// Window big enough for the global header and any plausible frame, so a record is always
// contiguous when it reaches the callback.
#define WC_PCAP_WINDOW (WC_PCAP_MAX_FRAME + WC_PCAP_REC_HDR + WC_PCAP_GLOBAL_HDR)

typedef struct {
    uint8_t buf[WC_PCAP_WINDOW];
    size_t base;  // file offset of buf[0]
    size_t valid; // bytes currently in buf
} PcapWindow;

// Make sure `need` bytes from file offset `at` are in the window, refilling from `at`.
// Returns false when the file cannot supply them.
static bool window_hold(PcapWindow *w, WcPcapPullFn pull, const void *pull_ctx, size_t at,
                        size_t need) {
    if (need > sizeof(w->buf)) {
        return false;
    }
    if (at >= w->base && at + need <= w->base + w->valid) {
        return true;
    }
    w->base = at;
    w->valid = pull(pull_ctx, at, w->buf, sizeof(w->buf));
    return w->valid >= need;
}

bool wc_pcap_stream(WcPcapPullFn pull, const void *pull_ctx, WcPcapFrameFn cb, void *ctx) {
    static PcapWindow w; // one window, reused: a FAP has 4 KB of stack and this is 4 KB+
    w.base = 0;
    w.valid = 0;
    if (!window_hold(&w, pull, pull_ctx, 0, WC_PCAP_GLOBAL_HDR)) {
        return false;
    }
    const uint8_t *h = w.buf;
    bool le;
    if (h[0] == 0xD4 && h[1] == 0xC3 && h[2] == 0xB2 && h[3] == 0xA1) {
        le = true;
    } else if (h[0] == 0xA1 && h[1] == 0xB2 && h[2] == 0xC3 && h[3] == 0xD4) {
        le = false;
    } else {
        return false;
    }
    if (rd_u32(h + 20, le) != WC_LINKTYPE_IEEE802_11) {
        return false;
    }

    size_t off = WC_PCAP_GLOBAL_HDR;
    while (window_hold(&w, pull, pull_ctx, off, WC_PCAP_REC_HDR)) {
        uint32_t incl = rd_u32(w.buf + (off - w.base) + 8, le);
        off += WC_PCAP_REC_HDR;
        if (incl > WC_PCAP_MAX_FRAME || !window_hold(&w, pull, pull_ctx, off, incl)) {
            break; // truncated or implausible record - stop cleanly
        }
        cb(ctx, w.buf + (off - w.base), incl);
        off += incl;
    }
    return true;
}
