// Pwnpal — pwngrid advertisement broadcaster for ESP32 Marauder.
//
// Broadcasts a Pwnagotchi-compatible beacon (MAC de:ad:be:ef:de:ad, JSON persona
// in vendor IE 222) so a nearby Pwnagotchi detects, greets, and befriends it.
// Self-contained: only reaches into Marauder for the raw 802.11 TX primitive.
// See PATCH.md for insertion points; wire format: ../doc/PwnpalProtocol.md

#pragma once

// serial-protocol version, stamped on PWNPAL_ADV (ver=N) so the Flipper warns on
// stale firmware. bump on any protocol change. v2=dwell recon+unicast/repeat deauth+
// PMKID auth+ESSID embed+flags; v3=live RSSI (PWNPAL_RSSI); v4=epoch telemetry+
// provenance (via=)+floor-free PMKID; v5=PWNPAL_GPS fix-status telemetry.
#define PWNPAL_PROTO 6

// min interval between PWNPAL_RSSI updates per AP, so re-heard beacons don't flood serial.
#define PWNPAL_RSSI_EMIT_MS 3000

// min interval between PWNPAL_GPS status lines (emitted even with no fix) so the Flipper
// can watch fix acquisition / log time-to-first-fix without flooding serial.
#define PWNPAL_GPS_EMIT_MS 3000

// no-GPS movement sensing: an AP still in range whose RSSI fell at least this many dB since the
// last epoch counts as "receding". a high fraction receding across the board = we're moving.
#define PWNPAL_RECEDE_DB 6

// crackability (✓): the strict pair — M1+M2 (same replay) or M2+M3 (replay+1) — must land within
// this window to be a genuinely crackable capture. Generous vs a single-channel camper (we hop and
// a deauthed client can reconnect a second later); wider risks a rare cross-association false ✓.
#define HS_CRACK_WINDOW_MS 2000
#define MAX_HS_HALFS 32 // (AP, client) 4-way accumulators tracked at once (~40 B each)

#include <Arduino.h>
#include <esp_wifi.h>
#include <LinkedList.h>
#include "pwnpal_frames.h" // pure 802.11 parsers + PwnpalHs (used by the class members below)

// provided by Marauder (WiFiScan.h); redeclared so we compile if included first.
extern "C" esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void* buffer,
                                       int len, bool en_sys_seq);

class Pwnpal {
  public:
    Pwnpal();

    // parse a tokenised `pwnpal ...` CLI line into the live persona; missing
    // args keep their value. false only on a malformed identity.
    bool configureFromArgs(LinkedList<String>* args);

    // rebuild the beacon frame from the persona; call after any change (also called by broadcast()).
    void rebuild();

    // hop/pin channel and transmit the persona beacon a few times; call on a timer from WiFiScan::main().
    void broadcast();

    // emit one PWNPAL_PEER line for a sniffed Pwnagotchi beacon. has_fix/lat/lon
    // geotag the sighting (RSSI + position -> triangulate offline).
    void reportPeer(const uint8_t* payload, int length, int rssi, int channel,
                    bool has_fix, double lat, double lon);

    // periodic GPS fix status (throttled; emitted even with no fix) so the Flipper can
    // watch acquisition and log time-to-first-fix. lat/lon are the module's raw strings.
    void reportGps(bool fix, int sats, float acc_m, const char* lat, const char* lon);

    // resolve the position to geotag with: the live fix (cached) if present, else a recent
    // last-known fix. rewrites *lat/*lon; returns whether a usable position was produced.
    bool geoResolve(bool has_fix, double* lat, double* lon);

    // capture path (rx callback, DATA frames). detects EAPOL M2 / RSN PMKID (M1) and
    // emits PWNPAL_PWND once per BSSID; streams every EAPOL frame as a self-describing
    // PWNPAL_HS line. returns true if EAPOL (caller appends to pcap).
    bool reportHandshake(const uint8_t* payload, int length, int rssi, int channel,
                         bool has_fix, double lat, double lon);

    // recon: dedup a non-pwngrid beacon into one PWNPAL_AP line per BSSID.
    // has_fix/lat/lon geotag the AP line. returns true the first time a BSSID is
    // stored (append beacon to pcap then).
    bool reportAP(const uint8_t* payload, int length, int rssi, int channel,
                  bool has_fix, double lat, double lon);

    // recon: harvest a client STA (non-BSSID address of a known AP) for unicast deauth.
    // rx callback, every DATA frame, allocation-free.
    void reportClient(const uint8_t* payload, int length);

    // de-cloak: a client's (re)association request to a HIDDEN AP carries the real ESSID.
    // adopt it (emits PWNPAL_AP -> Flipper splices the ESSID beacon into an existing capture,
    // making the hidden-AP handshake crackable). assoc-req subtype 0x00 / reassoc-req 0x20.
    void reportDecloak(const uint8_t* payload, int length, int rssi, int channel,
                       bool has_fix, double lat, double lon);

    // true once a persona is loaded.
    bool ready() const { return _ready; }

    // clear per-session dedup tables. at scan start only, NOT on the 15s persona
    // refresh (else re-counts pwnd APs, inflates pwnd_tot).
    void beginSession() {
        _n_recon = 0;
        _n_pwnd_seen = 0;
        _n_sta = 0;
        for(int i = 0; i < MAX_HS_HALFS; i++) _hs[i].used = false;
        _inactive_epochs = 0;  // a fresh scan starts at full recon speed
        _epoch_pwnd = false;
        _epoch_seq = 0;
        _ep_assoc = _ep_deauth = _ep_unicast = _ep_hs = _ep_pmkid = _ep_miss = 0;
        _ep_dpmf = _ep_dnocli = _ep_dcloak = _ep_adds = 0;
        _epoch_start_ms = 0;
        _saver_idle = false;       // duty-cycle state; _saver level itself is kept (set by args)
        _saver_phase_ms = 0;
        _saver_hb_ms = 0;
        resetPhase();
    }

    // battery-saver level: 0 off, 1 light (lower TX power + slower advert), 2 deep (+ radio
    // duty-cycle). set from the -saver arg.
    uint8_t saverLevel() const { return _saver; }
    // advance the deep-saver duty cycle; returns 0 keep scanning, 1 doze now (stop radio),
    // 2 wake now (restart radio), 3 stay dozing (skip broadcast). also emits a doze heartbeat.
    int saverTick(uint32_t now);

    void reset();

  private:
    void buildJson(char* out, size_t out_len);

    // Persona
    char     _name[33];
    char     _identity[65];   // 64 hex + NUL
    const char* _face;        // UTF-8 glyph
    uint32_t _pwnd_run;
    uint32_t _pwnd_tot;
    uint32_t _uptime;
    uint32_t _epoch;
    bool     _deauth_policy;
    bool     _assoc_policy;   // associate (solicit PMKID) without deauth — "PMKID-only"
    bool     _wardrive;       // recon-only fast sweep for moving capture (no attack dwell)
    uint8_t  _session_id[6];  // Addr3, stable per persona

    // Channel hopping
    int      _pinned_channel;  // -1 => hop
    uint8_t  _hop_idx;
    uint32_t _cmd_count = 0;   // DEBUG: total pwnpal commands parsed (ncmd= on ADV line)

    // Prebuilt frame
    uint8_t  _frame[300];
    int      _frame_len;
    bool     _ready;

    uint32_t _sent;
    uint32_t _last_active_ms;  // throttle for the pinned-channel active burst

    // pwnagotchi-faithful recon/attack epoch loop. RECON sweeps all channels gathering
    // APs; ATTACK visits each AP-bearing channel, fires assoc+deauth once, and dwells so
    // the 4-way completes before hopping.
    enum Phase { PHASE_RECON, PHASE_ATTACK };
    Phase    _phase;
    uint32_t _phase_ms;        // millis() when the phase / channel dwell began
    uint8_t  _cur_channel;     // channel we're parked on right now
    uint32_t _last_hop_ms;     // recon-sweep hop cadence timer
    uint32_t _last_sweep_ms;   // all-channel advertise-sweep cadence timer
    uint32_t _last_gps_ms;     // PWNPAL_GPS status-line cadence timer
    uint8_t  _saver;           // battery-saver level (0/1/2), from -saver
    bool     _saver_idle;      // deep saver: radio currently dozed off
    uint32_t _saver_phase_ms;  // millis() the current doze/scan phase began
    uint32_t _saver_hb_ms;     // last PWNPAL_DOZE heartbeat while dozing

    // last known good fix — geotag APs/captures/peers with a rough position when the live fix
    // has dropped (a moving walk loses fix in gaps; better a stale point than none — later
    // samples + triangulation refine it).
    bool     _have_lastfix;
    double   _lastfix_lat, _lastfix_lon;
    uint32_t _lastfix_ms;
    uint8_t  _attack_list[14]; // AP-bearing channels to attack this epoch
    int      _n_attack;        // channels in _attack_list
    int      _attack_idx;      // current channel within _attack_list
    bool     _chan_attacked;   // fired assoc+deauth on _attack_list[_attack_idx]?
    bool     _epoch_pwnd;      // captured anything this epoch (activity signal)
    uint8_t  _inactive_epochs; // consecutive fruitless epochs (recon_time doubling)
    uint32_t _last_deauth_ms;  // re-deauth cadence within the current channel dwell
    uint32_t _cur_dwell_ms;    // this channel's dwell, scaled by its target count
    int8_t   _attack_min_rssi; // deauth floor: don't bother deauthing APs weaker than this
    uint32_t _recon_time_ms;   // recon_time override (-recon), default 30s

    // Per-epoch telemetry (emitted as PWNPAL_EPOCH at endEpoch, then reset).
    uint32_t _epoch_seq;       // running epoch index since beginSession
    uint16_t _ep_assoc, _ep_deauth, _ep_unicast; // frames fired this epoch
    uint16_t _ep_hs, _ep_pmkid, _ep_miss;        // outcomes this epoch
    uint16_t _ep_dpmf, _ep_dnocli;               // deauths skipped: PMF-protected / no client
    uint16_t _ep_dcloak;                          // hidden APs de-cloaked (ESSID recovered) this epoch
    uint16_t _ep_adds;                            // BSSIDs newly added to recon this epoch (leading-edge movement)
    uint32_t _epoch_start_ms;                     // millis() the current epoch's recon began (cohort gate)

    // targeting + whitelist. target set -> only that BSSID attacked; whitelisted BSSIDs
    // never attacked (still recon'd).
    bool     _target_set;
    uint8_t  _target[6];
    static const int MAX_WL = 16;
    uint8_t  _wl[MAX_WL][6];
    int      _n_wl;

    // Per-session capture bookkeeping (cleared in beginSession(), at scan start).
    struct ReconAP {
        uint8_t bssid[6];
        char ssid[33];
        uint8_t channel;
        int8_t  rssi;    // latest beacon RSSI, refreshed as we re-hear it (0 = unknown)
        uint8_t attacks; // active-mode assoc/deauth bursts aimed at this AP
        bool missed;     // already emitted a PWNPAL_MISS for it
        bool pmf;        // 802.11w PMF required (RSN MFPR) -> deauth is futile, PMKID only
        uint32_t last_rssi_ms; // millis() of the last PWNPAL_RSSI we streamed for it
        // movement sensing (no-GPS): compare RSSI epoch-over-epoch; if the APs we still hear are
        // fading across the board, we're receding from them -> moving.
        int8_t  rssi_ref;  // RSSI snapshot at the last epoch boundary
        bool    ref_valid; // rssi_ref carried over a full epoch (excludes just-appeared APs)
        uint32_t seen_ms;  // millis() this AP was last actually heard (unthrottled)
    };
    // dense areas top 80 APs; 64 dropped ~16. 128 covers a busy neighbourhood.
    static const int MAX_RECON = 128;
    static const int MAX_PWND  = 128;
    static const int MAX_STA   = 128;   // client stations tracked for unicast deauth
    // active-mode bursts with no capture before it's a "miss" (pwnagotchi on_miss).
    static const int MISS_ATTEMPTS = 4;
    ReconAP  _recon[MAX_RECON];
    int      _n_recon;
    uint8_t  _pwnd_seen[MAX_PWND][6];
    int      _n_pwnd_seen;
    PwnpalHs _hs[MAX_HS_HALFS]; // per (AP, client) 4-way accumulators (loose count; strict is Flipper-side)

    // client stations sniffed from DATA frames for unicast deauth (both directions);
    // broadcast deauth is ignored by modern clients.
    struct ClientSta {
        uint8_t mac[6];
        uint8_t ap_idx;      // index into _recon of the AP this client belongs to
        uint32_t last_seen;  // millis(), to age out roamed/departed clients
    };
    ClientSta _sta[MAX_STA];
    int       _n_sta;

    // Recon/attack epoch machine helpers (see broadcast()).
    void resetPhase();                    // back to a fresh RECON sweep
    void endEpoch(uint32_t now);          // roll inactive streak, restart RECON
    void buildAttackList();               // AP-bearing channels, most-populated first
    void attackChannel(uint8_t channel);  // assoc + full deauth pass on entry
    void deauthChannelPass(uint8_t channel); // deauth-only re-kick during the dwell
    uint32_t channelDwellMs(uint8_t channel); // dwell scaled by eligible target count
    bool attackable(const ReconAP& ap) const; // eligible for assoc: not pwned/whitelisted/off-target
    bool deauthable(const ReconAP& ap) const;  // + strong enough to bother deauthing (RSSI floor)
    bool hasClient(int ap_idx) const;           // a fresh associated client -> deauth can work
    int  clientCount(int ap_idx) const;         // count of fresh clients tracked for this AP
    bool isWhitelisted(const uint8_t* bssid) const;
    // directed wildcard-SSID probe to make a nameless AP reveal its ESSID.
    void probeAP(const uint8_t* bssid);
    // clients older than this are treated as gone.
    static const uint32_t STA_TTL_MS = 180000;

    int  reconIndex(const uint8_t* bssid) const;   // -1 if unseen
    bool markPwnd(const uint8_t* bssid);           // true if newly counted
    bool isPwnd(const uint8_t* bssid) const;       // already captured this session?
    void emitPwnd(const uint8_t* bssid, const char* ssid,
                  const char* type, int channel, int rssi,
                  bool has_fix, double lat, double lon, bool active);
    void deauthAP(const uint8_t* bssid);
    // unicast deauth of one client, spoofed BOTH directions — the form modern clients honour.
    void deauthClient(const uint8_t* bssid, const uint8_t* client);
    // WPA2 assoc-request to solicit the RSN PMKID (EAPOL M1); no client needed. prefixed
    // by open-system auth so the AP processes it.
    void assocAP(const uint8_t* bssid, const char* ssid);
    // emit one self-describing "PWNPAL_HS <bssid> <hex>" line; Flipper files it under
    // <bssid>.pcap from this line alone. hex because raw binary trips the CLI's CR/XON handling.
    void streamFrameHex(const uint8_t* bssid, const uint8_t* frame, int length);
    // on capture, synthesize a minimal ESSID-bearing beacon into the same pcap: hashcat
    // needs the ESSID, and EAPOL-as-DATA caps otherwise lack it. no-op if SSID unknown.
    void streamSyntheticBeacon(const uint8_t* bssid, const char* ssid);
};

// map a face index (flipagotchi's PwnagotchiFace) to a glyph.
const char* pwnpal_face_glyph(int idx);
