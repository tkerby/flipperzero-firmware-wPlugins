// Host unit tests for the EAPOL / 4-way parsers in pwnpal-marauder/pwnpal_frames.h.
// No hardware needed: compile with g++ and run (see tests/run.sh). Frames are built to the real
// 802.11-data + LLC/SNAP + 802.1X-Key layout so the offset math is exercised end to end.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../pwnpal-marauder/pwnpal_frames.h"

static int failures = 0;
#define CHECK(cond, name)                \
    do {                                 \
        if(cond) {                       \
            printf("  ok   %s\n", name); \
        } else {                         \
            printf("  FAIL %s\n", name); \
            failures++;                  \
        }                                \
    } while(0)

// key_info bit values (16-bit field): ack=b7, mic=b8, secure=b9, install=b6.
enum { KI_ACK = 0x0080, KI_MIC = 0x0100, KI_SECURE = 0x0200, KI_INSTALL = 0x0040 };

// Build an 802.11 DATA frame carrying an EAPOL-Key. No QoS -> LLC/SNAP at 24, etherType at 30,
// so the 802.1X header lands at eo=32. Returns the total length.
// pmkid: 0 = no key data, 1 = RSN PMKID KDE (nonzero), 2 = RSN PMKID KDE (all zero),
//        3 = WPA1 vendor KDE (OUI 00:50:F2 — must NOT match).
static int mk_eapol(uint8_t* b, uint8_t ds, uint16_t ki, bool nonce_nz, int pmkid) {
    memset(b, 0, 256);
    b[0] = 0x08; // data frame
    b[1] = ds;
    // Addr1 @4, Addr2 @10, Addr3 @16 — give them distinct markers for ds_bssid tests.
    for(int i = 0; i < 6; i++) {
        b[4 + i] = 0xA0 + i;  // Addr1
        b[10 + i] = 0xB0 + i; // Addr2
        b[16 + i] = 0xC0 + i; // Addr3
    }
    // LLC/SNAP + EAPOL etherType
    b[24] = 0xAA; b[25] = 0xAA; b[26] = 0x03; b[27] = 0x00; b[28] = 0x00; b[29] = 0x00;
    b[30] = 0x88; b[31] = 0x8e;
    int eo = 32;
    b[eo + 0] = 0x01;        // 802.1X version
    b[eo + 1] = 0x03;        // type = EAPOL-Key
    b[eo + 4] = 0x02;        // descriptor type = RSN
    b[eo + 5] = (uint8_t)(ki >> 8);
    b[eo + 6] = (uint8_t)(ki & 0xff);
    b[eo + 9] = 0x01;        // replay counter (low byte); rest zero
    if(nonce_nz) b[eo + 17] = 0xAB; // one nonzero nonce byte
    int len = eo + 97 + 2;   // through key-data-length
    if(pmkid) {
        int kd = eo + 99;
        b[eo + 97] = 0x00; b[eo + 98] = 0x16; // KDL = 22
        b[kd + 0] = 0xDD; b[kd + 1] = 0x14;   // vendor-specific KDE, len 20
        if(pmkid == 3) {                       // WPA1 OUI 00:50:F2 type 02
            b[kd + 2] = 0x00; b[kd + 3] = 0x50; b[kd + 4] = 0xF2; b[kd + 5] = 0x02;
        } else {                               // RSN OUI 00:0F:AC type 04 (PMKID)
            b[kd + 2] = 0x00; b[kd + 3] = 0x0F; b[kd + 4] = 0xAC; b[kd + 5] = 0x04;
        }
        for(int i = 0; i < 16; i++) b[kd + 6 + i] = (pmkid == 1) ? 0x11 : 0x00;
        len = kd + 22;
    }
    return len;
}

int main(void) {
    uint8_t b[256];
    PwnpalEapolKey k;
    int len, eo;

    printf("pwnpal_eapol_locate:\n");
    len = mk_eapol(b, 0x02, KI_ACK, true, 1);
    CHECK(pwnpal_eapol_locate(b, len) == 32, "finds 888e -> eo=32");
    b[30] = 0x08; b[31] = 0x00; // rewrite etherType to IPv4
    CHECK(pwnpal_eapol_locate(b, len) < 0, "non-EAPOL etherType -> -1");
    CHECK(pwnpal_eapol_locate(b, 31) < 0, "too short -> -1 (no overread)");

    printf("pwnpal_eapol_key M1/PMKID:\n");
    len = mk_eapol(b, 0x02, KI_ACK, true, 1);
    eo = pwnpal_eapol_locate(b, len);
    CHECK(pwnpal_eapol_key(b, len, eo, &k), "M1+PMKID parses");
    CHECK(k.key_ack && !k.key_mic, "M1 bits: ack, no mic");
    CHECK(k.pmkid_nz, "PMKID present + nonzero");
    CHECK(k.nonce_nz, "M1 ANonce banked");

    len = mk_eapol(b, 0x02, KI_ACK, true, 2); // zero PMKID
    eo = pwnpal_eapol_locate(b, len);
    pwnpal_eapol_key(b, len, eo, &k);
    CHECK(!k.pmkid_nz, "all-zero PMKID -> not a pmkid");

    len = mk_eapol(b, 0x02, KI_ACK, true, 3); // WPA1 vendor KDE
    eo = pwnpal_eapol_locate(b, len);
    pwnpal_eapol_key(b, len, eo, &k);
    CHECK(!k.pmkid_nz, "WPA1 vendor OUI KDE -> not a pmkid");

    printf("pwnpal_eapol_key M2/M3/M4:\n");
    len = mk_eapol(b, 0x01, KI_MIC, true, 0); // M2: mic, no ack, no secure, SNonce
    eo = pwnpal_eapol_locate(b, len);
    pwnpal_eapol_key(b, len, eo, &k);
    CHECK(!k.key_ack && k.key_mic && !k.secure, "M2 bits: mic only");
    CHECK(k.nonce_nz, "M2 SNonce present");

    len = mk_eapol(b, 0x02, KI_ACK | KI_MIC | KI_SECURE | KI_INSTALL, true, 0); // M3
    eo = pwnpal_eapol_locate(b, len);
    pwnpal_eapol_key(b, len, eo, &k);
    CHECK(k.key_ack && k.key_mic && k.secure && k.install, "M3 bits: ack+mic+secure+install");

    len = mk_eapol(b, 0x01, KI_MIC | KI_SECURE, false, 0); // M4
    eo = pwnpal_eapol_locate(b, len);
    pwnpal_eapol_key(b, len, eo, &k);
    CHECK(!k.key_ack && k.key_mic && k.secure, "M4 bits: mic+secure, no ack");

    printf("pwnpal_eapol_key robustness:\n");
    len = mk_eapol(b, 0x02, KI_ACK, true, 1);
    CHECK(!pwnpal_eapol_key(b, 35, 32, &k), "truncated before key_info -> false");
    b[33] = 0x01; // 802.1X type != Key
    CHECK(!pwnpal_eapol_key(b, len, 32, &k), "non-key 802.1X type -> false");

    printf("pwnpal_ds_bssid:\n");
    const uint8_t *bssid, *client;
    len = mk_eapol(b, 0x02, KI_ACK, true, 1); // From-DS (AP->STA)
    bssid = pwnpal_ds_bssid(b, len, &client);
    CHECK(bssid == b + 10 && client == b + 4, "From-DS: bssid=Addr2, client=Addr1");
    len = mk_eapol(b, 0x01, KI_MIC, true, 0); // To-DS (STA->AP)
    bssid = pwnpal_ds_bssid(b, len, &client);
    CHECK(bssid == b + 4 && client == b + 10, "To-DS: bssid=Addr1, client=Addr2");

    printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
