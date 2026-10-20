/*
 * can_driver.cpp
 *
 * CAN driver abstraction: compile-time selection between
 *   CAN_DRIVER_TWAI        — ESP32 built-in TWAI peripheral
 *   CAN_DRIVER_MCP2515     — SPI-attached MCP2515
 *   CAN_DRIVER_T2CAN_DUAL  — LilyGO T-2CAN: TWAI can0 + MCP2515 can1
 *                            (+ BOARD_LILYGO_T2CAN_FD: MCP2518FD can1)
 *
 * All drivers implement the CanDriver interface from can_driver.h.
 */

#include "can_driver.h"
#include "can_capture_filter.h"
#include "config.h"
#include "../../fsd_logic/fsd_can_ops.h"  // TESLA_CAN_MAX_DLC / tesla_can_tx_valid
#include <Arduino.h>
#include <string.h>

// DLC>8 frames one receive() call may drop before it reports "queue empty".
#define CAN_RX_REJECT_BURST_MAX 8u

// ── TWAI driver ───────────────────────────────────────────────────────────────
#if defined(CAN_DRIVER_TWAI) || defined(CAN_DRIVER_T2CAN_DUAL)

#include "driver/twai.h"

class TwaiDriver : public CanDriver {
    const char *label_;
    int      tx_pin_;
    int      rx_pin_;
    bool     listen_only_ = false;
    bool     installed_   = false;
    uint32_t tx_count_    = 0;
    uint32_t rx_count_    = 0;
    uint8_t  filter_count_ = 0;
    uint32_t filter_ids_[CAN_CAPTURE_MAX_IDS] = {};
    bool     recovering_    = false;  // true while a bus-off recovery is in flight
    bool     busoff_event_  = false;  // consume-on-read edge: recovery just started
    uint32_t rx_rejected_   = 0;      // DLC>8 frames dropped

    bool install_and_start(bool listen_only) {
#ifdef SNIFFER_ONLY
        // Research sniffer build: hardware-forced Listen-Only. TWAI_MODE_LISTEN_ONLY
        // never emits dominant bits (not even ACKs), so the node is physically
        // incapable of transmitting — zero ban risk from injected frames.
        listen_only = true;
#endif
        twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
            (gpio_num_t)tx_pin_,
            (gpio_num_t)rx_pin_,
            listen_only ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
        // Queue depths: 64 RX (busy Vehicle CAN can deliver thousands of
        // frames/s; a deeper queue cuts controller-level drops), 5 TX.
        g.rx_queue_len = 64;
        g.tx_queue_len = 5;

        twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f;
        if (filter_count_ > 0) {
            // Standard-frame hardware prefilter. TWAI has one acceptance
            // code/mask, so an exact whitelist is only possible for one ID.
            // For multiple IDs, match the common ID bits and leave differing
            // bits as don't-care; software filtering still enforces the final
            // list, while hardware drops part of unrelated traffic.
            CanTwaiPrefilter prefilter = can_capture_twai_prefilter(filter_ids_, filter_count_);
            f.acceptance_code = prefilter.code;
            f.acceptance_mask = prefilter.mask;
            f.single_filter   = true;
        } else {
            f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        }

        if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
        if (twai_start() != ESP_OK) {
            twai_driver_uninstall();
            return false;
        }
        installed_    = true;
        listen_only_  = listen_only;
        recovering_   = false;  // fresh controller, no recovery pending
        return true;
    }

    void stop_and_uninstall() {
        if (!installed_) return;
        twai_stop();
        twai_driver_uninstall();
        installed_ = false;
        recovering_ = false;
    }

public:
    TwaiDriver(const char *label, int tx_pin, int rx_pin)
        : label_(label), tx_pin_(tx_pin), rx_pin_(rx_pin) {}

    bool begin(bool listen_only) override {
        bool ok = install_and_start(listen_only);
        Serial.printf("[CAN] %s TWAI %s @ 500 kbps (TX=%d RX=%d)\n",
                      label_, ok ? (listen_only ? "Listen-Only" : "Normal") : "FAILED",
                      tx_pin_, rx_pin_);
        return ok;
    }

    bool send(const CanFrame &frame) override {
        if (listen_only_) return false;
        if (!tesla_can_tx_valid(frame.id, frame.dlc)) return false;
        twai_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.identifier       = frame.id;
        msg.data_length_code = frame.dlc;
        memcpy(msg.data, frame.data, frame.dlc);
        // 5 ms TX timeout — short enough to not stall the main loop
        if (twai_transmit(&msg, pdMS_TO_TICKS(5)) != ESP_OK) return false;
        tx_count_++;
        return true;
    }

    bool receive(CanFrame &frame) override {
        for (uint8_t i = 0; i < CAN_RX_REJECT_BURST_MAX; i++) {
            twai_message_t msg;
            // Non-blocking receive (timeout = 0)
            if (twai_receive(&msg, 0) != ESP_OK) return false;
            // TWAI hands DLC 9..15 up as-is while msg.data stays 8 bytes; a
            // plain memcpy of dlc bytes overran both buffers. Drop those.
            if (msg.data_length_code > TESLA_CAN_MAX_DLC) {
                rx_rejected_++;
                continue;
            }
            memset(&frame, 0, sizeof(frame));
            frame.id  = msg.identifier;
            frame.ext = msg.extd ? 1u : 0u;   // process_frame() records these
            frame.req = msg.rtr  ? 1u : 0u;   // but never hands them to a handler
            frame.dlc = msg.data_length_code;
            memcpy(frame.data, msg.data, frame.dlc);
            rx_count_++;
            return true;
        }
        return false;
    }

    uint32_t errorCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.rx_missed_count + info.bus_error_count + info.tx_failed_count;
    }

    // Bus-off auto-recovery. Without this, once TX errors push the TWAI TEC past
    // the limit the controller goes BUS_OFF and twai_receive() stops returning
    // frames forever (RX frames/s drops to 0) until a manual mode toggle reinstalls
    // the driver (#108). Acts only in the BUS_OFF/RECOVERING states; a healthy bus
    // hits the first early-return and this is a no-op.
    void serviceHealth() override {
        if (!installed_) return;
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return;
        if (info.state == TWAI_STATE_BUS_OFF && !recovering_) {
            // BUS_OFF -> RECOVERING; controller auto-advances to STOPPED after
            // observing 128 occurrences of 11 consecutive recessive bits.
            if (twai_initiate_recovery() == ESP_OK) {
                recovering_ = true;
                busoff_event_ = true;  // edge for the black-box EVT_BUSOFF trigger
                Serial.printf("[CAN] %s bus-off detected — initiating recovery\n", label_);
            }
        } else if (recovering_ && info.state == TWAI_STATE_STOPPED) {
            // Recovery complete — restart so RX/TX resume with the existing
            // filter and listen-only configuration.
            if (twai_start() == ESP_OK) {
                recovering_ = false;
                Serial.printf("[CAN] %s bus recovered — restarted\n", label_);
            }
        }
    }

    bool busOffEvent() override {
        bool e = busoff_event_;
        busoff_event_ = false;
        return e;
    }

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

    uint32_t rxRejectedCount() override { return rx_rejected_; }

    uint32_t rxMissedCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.rx_missed_count;
    }

    uint32_t busErrorCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.bus_error_count;
    }

    uint32_t txFailedCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.tx_failed_count;
    }

    // rx_overrun (the controller's own FIFO overflowing while the ISR is held
    // off, e.g. during a flash write) is not part of errorCount(). tx_queued
    // counts frames not yet sent; on a bus where nothing ACKs they stay there.
    void describeState(char *out, size_t n) override {
        static const char *const kState[] = {"stopped", "running", "bus-off", "recovering"};
        twai_status_info_t info;
        if (!installed_ || twai_get_status_info(&info) != ESP_OK) {
            snprintf(out, n, "TWAI not installed");
            return;
        }
        snprintf(out, n,
                 "TWAI %s %s TEC=%lu REC=%lu rx_missed=%lu rx_overrun=%lu bus_err=%lu "
                 "tx_fail=%lu tx_queued=%lu",
                 listen_only_ ? "Listen-Only" : "Normal",
                 info.state <= TWAI_STATE_RECOVERING ? kState[info.state] : "?",
                 (unsigned long)info.tx_error_counter, (unsigned long)info.rx_error_counter,
                 (unsigned long)info.rx_missed_count, (unsigned long)info.rx_overrun_count,
                 (unsigned long)info.bus_error_count, (unsigned long)info.tx_failed_count,
                 (unsigned long)info.msgs_to_tx);
    }

    void setListenOnly(bool enable) override {
#ifdef SNIFFER_ONLY
        (void)enable;   // sniffer build is permanently Listen-Only; ignore mode switches
        return;
#endif
        if (listen_only_ == enable) return;
        stop_and_uninstall();
        if (!install_and_start(enable)) {
            Serial.printf("[CAN] %s TWAI mode switch FAILED\n", label_);
        }
    }

    const char *acceptanceFilterMode() override {
        if (!installed_) return "down";
        return filter_count_ == 0 ? "accept-all" : filter_count_ == 1 ? "exact" : "prefilter";
    }

    void setAcceptanceFilter(bool single, uint32_t id) override {
        if (single) setAcceptanceFilters(&id, 1);
        else setAcceptanceFilters(nullptr, 0);
    }

    void setAcceptanceFilters(const uint32_t *ids, uint8_t count) override {
        if (ids == nullptr) count = 0;
        if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;

        bool same = filter_count_ == count;
        for (uint8_t i = 0; same && i < count; i++) {
            same = ((filter_ids_[i] & 0x7FFu) == (ids[i] & 0x7FFu));
        }
        if (same) return;

        filter_count_ = count;
        for (uint8_t i = 0; i < count; i++) {
            filter_ids_[i] = ids[i] & 0x7FFu;
        }
        if (!installed_) return;  // begin() will pick up the new filter
        bool lo = listen_only_;
        stop_and_uninstall();
        if (!install_and_start(lo)) {
            Serial.printf("[CAN] %s TWAI filter switch FAILED\n", label_);
        } else if (count > 0) {
            Serial.printf("[CAN] %s TWAI hardware filter -> %u id prefilter\n",
                          label_, (unsigned)count);
        } else {
            Serial.printf("[CAN] %s TWAI hardware filter -> accept all\n", label_);
        }
    }

    // Pre-reboot quiesce: stop the controller (no TX, no ACK), then take the TX
    // pad back from the TWAI peripheral and drive it recessive until the chip
    // resets. The level is latched high before the pad becomes an output (and
    // set again after) so the handover itself can't blip dominant on the bus.
    void shutdown() override {
        stop_and_uninstall();
        if (tx_pin_ >= 0) {
            digitalWrite((uint8_t)tx_pin_, HIGH);
            pinMode((uint8_t)tx_pin_, OUTPUT);
            digitalWrite((uint8_t)tx_pin_, HIGH);
        }
    }
};
#endif

// ── MCP2515 driver ────────────────────────────────────────────────────────────
#if defined(CAN_DRIVER_MCP2515) || (defined(CAN_DRIVER_T2CAN_DUAL) && !defined(BOARD_LILYGO_T2CAN_FD))

#include <SPI.h>
#include <mcp2515.h>   // autowp/autowp-mcp2515

static const char *mcp_clock_name(CAN_CLOCK clock) {
    switch (clock) {
        case MCP_20MHZ: return "20 MHz";
        case MCP_16MHZ: return "16 MHz";
        case MCP_8MHZ:  return "8 MHz";
        default:        return "?";
    }
}

class Mcp2515Driver : public CanDriver {
#if defined(BOARD_TTGO_DISPLAY)
    SPIClass spi_;
    bool     spi_begun_    = false;
#endif
    MCP2515  mcp_;
    const char *label_;
    int      cs_pin_;
    int      sck_pin_;
    int      miso_pin_;
    int      mosi_pin_;
    int      rst_pin_;
    bool     listen_only_  = false;
    bool     installed_    = false;
    bool     chip_detected_ = false;
    const char *filter_mode_ = "unknown";
    uint32_t err_count_    = 0;
    uint32_t tx_count_     = 0;
    uint32_t rx_count_     = 0;
    uint32_t rx_rejected_  = 0;   // DLC>8 frames dropped (readMessage FAIL)

public:
#if defined(BOARD_TTGO_DISPLAY)
    Mcp2515Driver(const char *label, int cs_pin, int sck_pin, int miso_pin, int mosi_pin, int rst_pin)
        : spi_(HSPI),
          mcp_(cs_pin, 10000000, &spi_),
          label_(label),
          cs_pin_(cs_pin),
          sck_pin_(sck_pin),
          miso_pin_(miso_pin),
          mosi_pin_(mosi_pin),
          rst_pin_(rst_pin) {}
#else
    Mcp2515Driver(const char *label, int cs_pin, int sck_pin, int miso_pin, int mosi_pin, int rst_pin)
        : mcp_(cs_pin),
          label_(label),
          cs_pin_(cs_pin),
          sck_pin_(sck_pin),
          miso_pin_(miso_pin),
          mosi_pin_(mosi_pin),
          rst_pin_(rst_pin) {}
#endif

    bool begin(bool listen_only) override {
        if (rst_pin_ >= 0) {
            pinMode((uint8_t)rst_pin_, OUTPUT);
            digitalWrite((uint8_t)rst_pin_, LOW);
            delay(2);
            digitalWrite((uint8_t)rst_pin_, HIGH);
            delay(10);
        }

#if defined(BOARD_TTGO_DISPLAY)
        // Keep MCP2515 on HSPI so TFT_eSPI can own the T-Display LCD SPI bus.
        if (!spi_begun_) {
            spi_.begin(sck_pin_, miso_pin_, mosi_pin_, cs_pin_);
            spi_.setFrequency(8000000);
            spi_begun_ = true;
        }
#else
        static bool s_spi_begun = false;
        if (!s_spi_begun) {
            SPI.begin(sck_pin_, miso_pin_, mosi_pin_, cs_pin_);
            SPI.setFrequency(8000000);
            s_spi_begun = true;
        }
#endif

        filter_mode_ = mcp_.reset() == MCP2515::ERROR_OK ? "accept-all" : "unknown";

        // setBitrate() internally enters CONFIG mode and verifies the mode
        // change via an SPI register read-back. If the MCP2515 isn't wired
        // up / powered / responding on SPI, this is the call that fails first.
        // Failure here means SPI/chip presence problem (entering CONFIG mode
        // does not require any CAN bus traffic).
        if (mcp_.setBitrate(CAN_500KBPS, MCP_CRYSTAL_MHZ) != MCP2515::ERROR_OK) {
            chip_detected_ = false;
            installed_     = false;
            Serial.printf("[CAN] %s MCP2515 NOT detected on SPI "
                          "(CS=%d SCK=%d MISO=%d MOSI=%d CLK=%s) — "
                          "check wiring, 5V power, and crystal\n",
                          label_, cs_pin_, sck_pin_, miso_pin_, mosi_pin_,
                          mcp_clock_name(MCP_CRYSTAL_MHZ));
            return false;
        }

        // Chip responded over SPI: we know the MCP2515 is physically there.
        chip_detected_ = true;

        // Switching to listen-only / normal mode is a chip-internal CANCTRL
        // change — does not require CAN bus traffic either, but if it fails
        // after a successful setBitrate it is still a chip / SPI issue.
        MCP2515::ERROR err = listen_only
            ? mcp_.setListenOnlyMode()
            : mcp_.setNormalMode();
        listen_only_ = listen_only;
        installed_ = (err == MCP2515::ERROR_OK);
        if (installed_) {
            Serial.printf("[CAN] %s MCP2515 detected on SPI — %s mode @ 500 kbps, clock=%s\n",
                          label_,
                          listen_only ? "Listen-Only" : "Normal",
                          mcp_clock_name(MCP_CRYSTAL_MHZ));
        } else {
            Serial.printf("[CAN] %s MCP2515 detected but mode change FAILED (err=%d)\n",
                          label_, (int)err);
        }
        return installed_;
    }

    bool hardwarePresent() override { return chip_detected_; }

    bool send(const CanFrame &frame) override {
        if (!installed_ || listen_only_) return false;
        if (!tesla_can_tx_valid(frame.id, frame.dlc)) return false;
        struct can_frame f;
        f.can_id  = frame.id;
        f.can_dlc = frame.dlc;
        memcpy(f.data, frame.data, frame.dlc);
        if (mcp_.sendMessage(&f) != MCP2515::ERROR_OK) {
            err_count_++;
            return false;
        }
        tx_count_++;
        return true;
    }

    bool receive(CanFrame &frame) override {
        if (!installed_) return false;
        struct can_frame f;
        MCP2515::ERROR err = mcp_.readMessage(&f);
        if (err == MCP2515::ERROR_FAIL) {
            // autowp returns FAIL for a DLC 9..15 frame WITHOUT clearing its
            // RXnIF flag, and readMessage() always re-reads RXB0 first, so that
            // one frame would wedge RX for good. Drop it (and whatever sits in
            // the other buffer) by clearing the RX interrupt flags.
            mcp_.clearInterrupts();
            rx_rejected_++;
            return false;
        }
        if (err != MCP2515::ERROR_OK) return false;
        // autowp flags extended / remote frames in can_id (a remote frame's
        // data bytes are whatever the previous frame left in the buffer).
        bool extended = (f.can_id & CAN_EFF_FLAG) != 0u;
        memset(&frame, 0, sizeof(frame));
        frame.id  = f.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);
        frame.ext = extended ? 1u : 0u;
        frame.req = (f.can_id & CAN_RTR_FLAG) != 0u ? 1u : 0u;
        frame.dlc = f.can_dlc;   // readMessage() already refused DLC > 8
        memcpy(frame.data, f.data, frame.dlc);
        rx_count_++;
        return true;
    }

    uint32_t errorCount() override {
        return err_count_;
    }

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

    uint32_t rxRejectedCount() override { return rx_rejected_; }

    // rxMissedCount(): use the CanDriver default (0). The MCP2515 overflow flag
    // would need an extra SPI read on the hot path, so it is not surfaced here.
    // busErrorCount(): default 0 too; the chip only exposes live TEC/REC levels,
    // not a cumulative bus-error count.

    // Every errorCount() event is a sendMessage() failure.
    uint32_t txFailedCount() override { return err_count_; }

    // The chip keeps no count of lost or errored frames, only flags: an RXnOVR
    // bit means a frame was lost at least once since the last reset, since
    // nothing here clears it. So say that, rather than print a 0 that reads
    // like a clean bus next to the TWAI's real counts.
    void describeState(char *out, size_t n) override {
        if (!installed_) {
            snprintf(out, n, "MCP2515 %s", chip_detected_ ? "mode change failed"
                                                          : "not detected on SPI");
            return;
        }
        uint8_t eflg = mcp_.getErrorFlags();
        snprintf(out, n,
                 "MCP2515 %s EFLG=0x%02X%s%s%s TEC=%u REC=%u tx_fail=%lu "
                 "(no rx_missed/bus_err counts on this chip)",
                 listen_only_ ? "Listen-Only" : "Normal", (unsigned)eflg,
                 (eflg & (MCP2515::EFLG_RX0OVR | MCP2515::EFLG_RX1OVR)) ? " rx-overflow-seen" : "",
                 (eflg & MCP2515::EFLG_TXBO) ? " bus-off" : "",
                 (eflg & (MCP2515::EFLG_TXEP | MCP2515::EFLG_RXEP)) ? " error-passive" : "",
                 (unsigned)mcp_.errorCountTX(), (unsigned)mcp_.errorCountRX(),
                 (unsigned long)err_count_);
    }

    void setListenOnly(bool enable) override {
        if (!installed_ || listen_only_ == enable) return;
        MCP2515::ERROR err = enable ? mcp_.setListenOnlyMode() : mcp_.setNormalMode();
        if (err == MCP2515::ERROR_OK) {
            listen_only_ = enable;
        } else {
            Serial.printf("[CAN] %s MCP2515 mode switch FAILED (err=%d)\n",
                          label_, (int)err);
        }
    }

    void setAcceptanceFilter(bool single, uint32_t id) override {
        setAcceptanceFilters(single ? &id : nullptr, single ? 1 : 0);
    }

    const char *acceptanceFilterMode() override {
        return installed_ ? filter_mode_ : "down";
    }

    // Pre-reboot quiesce: CONFIG mode takes the MCP2515 off the bus (no TX, no
    // ACK), and it stays there across the ESP32 reset until begin() runs again.
    // If the mode request doesn't take (e.g. a frame stuck pending TX), the SPI
    // RESET instruction forces the chip into CONFIG mode.
    void shutdown() override {
        if (!chip_detected_) return;  // nothing answering on SPI
        if (mcp_.setConfigMode() != MCP2515::ERROR_OK) mcp_.reset();
        installed_ = false;
    }

    void setAcceptanceFilters(const uint32_t *ids, uint8_t count) override {
        if (!installed_) return;
        if (!ids) count = 0;
        if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
        bool ok = can_capture_program_mcp_filters(mcp_, ids, count, listen_only_);
        filter_mode_ = ok ? (count ? "exact" : "accept-all") : "unknown";
        if (!ok) {
            Serial.printf("[CAN] %s MCP2515 filter switch FAILED\n", label_);
        } else if (count) {
            Serial.printf("[CAN] %s MCP2515 hardware filter -> %u exact ids\n",
                          label_, (unsigned)count);
        } else {
            Serial.printf("[CAN] %s MCP2515 hardware filter -> accept all\n", label_);
        }
    }
};
#endif

// ── MCP2518FD driver (LilyGO T-2CAN FD can1) ──────────────────────────────────
#if defined(CAN_DRIVER_T2CAN_DUAL) && defined(BOARD_LILYGO_T2CAN_FD)

#include <SPI.h>
#include <ACAN2517FD.h>   // pierremolinaro/ACAN2517FD
#include "can_mcp2518fd.h"

// The controller runs on a 20 MHz SYSCLK either way, and the library clocks SPI
// at 2/5 of SYSCLK: 8 MHz, below the 10 MHz the classic T-2CAN's MCP2515 uses
// on the same pins.
#if MCP2518FD_OSC_HZ == 40000000
#define MCP2518FD_OSC ACAN2517FDSettings::OSC_40MHz_DIVIDED_BY_2
#elif MCP2518FD_OSC_HZ == 20000000
#define MCP2518FD_OSC ACAN2517FDSettings::OSC_20MHz
#else
#error "MCP2518FD_OSC_HZ must be 20000000 or 40000000"
#endif

// Classic CAN 2.0 at 500 kbit/s on an MCP2518FD through ACAN2517FD. The library
// runs without its INT pin: receive() polls the controller over SPI from the
// loop task, so its own ESP32 task never wakes and nothing else touches the
// chip or the SPI bus. begin() first starts the controller in Listen-Only and
// measures the crystal; nothing else starts it until that matches the build.
class Mcp2518FdDriver : public CanDriver {
    // Outside the library's begin() error bits (0..20).
    static constexpr uint32_t kTimingRejected = 1u << 31;
    // Raw register access for the crystal check and the error counters; in
    // spec for SYSCLK down to 10 MHz.
    static constexpr uint32_t kRawSpiHz = 4000000;
    // Before the oscillator is known to run: the clock the library resets and
    // first checks the chip at.
    static constexpr uint32_t kSlowSpiHz = 800000;
    // Bounded waits: OSCRDY after the reset, the MCP2515's oscillator start-up
    // timer after its reset (the Linux mcp251x driver's 5 ms).
    static constexpr uint32_t kOscStartMs     = 10;
    static constexpr uint32_t kMcp2515ResetMs = 5;

    ACAN2517FD can_;
    const char *label_;
    int      cs_pin_;
    int      sck_pin_;
    int      miso_pin_;
    int      mosi_pin_;
    bool     spi_begun_     = false;
    bool     started_       = false;  // library begin() ran, end() still owed
    bool     begun_         = false;  // last begin() succeeded
    bool     installed_     = false;
    bool     listen_only_   = false;
    bool     chip_detected_ = false;
    uint32_t crystal_hz_    = 0;      // last measurement
    bool     crystal_wrong_ = false;  // measured, and not the build's crystal
    const char *filter_mode_ = "unknown";
    uint8_t  filter_count_  = 0;
    uint32_t filter_ids_[CAN_CAPTURE_MAX_IDS] = {};
    uint32_t tx_count_      = 0;
    uint32_t rx_count_      = 0;
    uint32_t rx_rejected_   = 0;   // DLC>8 and CAN FD frames dropped
    uint32_t rx_missed_     = 0;   // RX FIFO overflow events
    uint32_t tx_failed_     = 0;   // tryToSend() refused: TX FIFO full
    uint32_t bus_errors_    = 0;
    bool     diag_printed_  = false;  // last not-detected readback, for retries
    uint32_t diag_err_      = 0;
    Mcp2518fdReadback diag_ = {};

    static ACAN2517FDSettings settings(ACAN2517FDSettings::Oscillator osc, bool listen_only) {
        ACAN2517FDSettings s(osc, 500UL * 1000UL, DataBitRateFactor::x1);
        // Listen-Only sends no ACK and no error frames. Active is the CAN 2.0
        // mode: the controller transmits FDF and BRS as zero whatever a
        // message says.
        s.mRequestedMode = listen_only ? ACAN2517FDSettings::ListenOnly
                                       : ACAN2517FDSettings::Normal20B;
        // 8-byte message objects: 32 RX and 8 TX slots (the MCP2515 has 2 and
        // 3), and a longer payload does not fit, so it is never sent.
        s.mControllerReceiveFIFOPayload  = ACAN2517FDSettings::PAYLOAD_8;
        s.mControllerReceiveFIFOSize     = 32;
        s.mControllerTransmitFIFOPayload = ACAN2517FDSettings::PAYLOAD_8;
        s.mControllerTransmitFIFOSize    = 8;
        // No software TX queue behind the controller FIFO: when that is full,
        // send() fails and is counted instead of queuing frames that go stale.
        s.mDriverTransmitFIFOSize = 0;
        // The library masks interrupts while it reads the controller; a small
        // ring caps one poll at four frames, so can0's TWAI interrupt is
        // never held off for long.
        s.mDriverReceiveFIFOSize = 4;
        return s;
    }

    // Configure from scratch. The library loads bit timing, FIFOs and filters
    // only in Configuration mode, so every mode or filter change restarts the
    // controller, as the TWAI driver reinstalls; end() first aborts pending TX.
    // Returns the library's begin() error code, 0 when running.
    uint32_t start(ACAN2517FDSettings::Oscillator osc, bool listen_only) {
        stop();
        ACAN2517FDSettings s = settings(osc, listen_only);
        if (!mcp2518fd_timing_ok(s.sysClock(), s.mBitRatePrescaler, s.mArbitrationPhaseSegment1,
                                 s.mArbitrationPhaseSegment2, s.mArbitrationSJW)) {
            return kTimingRejected;
        }
        ACAN2517FDFilters filters;
        mcp2518fd_capture_filters(filters, kStandard, filter_ids_, filter_count_);
        started_ = true;
        return can_.begin(s, nullptr, filters);
    }

    // Configuration mode, then the SPI reset: off the bus, and it stays there
    // across an ESP32 reset until begin() runs again.
    void stop() {
        if (!started_) return;
        rxMissedCount();  // begin() clears the library's overflow count
        can_.end();
        started_   = false;
        installed_ = false;
    }

    // Start in the given mode with the current filters. On failure the
    // controller is left stopped. Returns the start() error code.
    uint32_t restart(bool listen_only) {
        uint32_t err = start(MCP2518FD_OSC, listen_only);
        listen_only_ = listen_only;
        installed_ = (err == 0);
        if (!installed_) stop();
        filter_mode_ = installed_ ? (filter_count_ ? "exact" : "accept-all") : "unknown";
        return err;
    }

    void transfer(uint8_t *buf, size_t len, uint32_t hz = kRawSpiHz) {
        SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
        taskDISABLE_INTERRUPTS();   // same CS handling as the library
        digitalWrite((uint8_t)cs_pin_, LOW);
        SPI.transfer(buf, len);
        digitalWrite((uint8_t)cs_pin_, HIGH);
        taskENABLE_INTERRUPTS();
        SPI.endTransaction();
    }

    uint32_t read_reg32(uint16_t addr, uint32_t hz = kRawSpiHz) {
        uint8_t b[6] = {(uint8_t)(0x30 | (addr >> 8)), (uint8_t)addr, 0, 0, 0, 0};
        transfer(b, sizeof(b), hz);
        return (uint32_t)b[2] | ((uint32_t)b[3] << 8) | ((uint32_t)b[4] << 16) |
               ((uint32_t)b[5] << 24);
    }

    void write_reg32(uint16_t addr, uint32_t value) {
        uint8_t b[6] = {(uint8_t)(0x20 | (addr >> 8)), (uint8_t)addr, (uint8_t)value,
                        (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
        transfer(b, sizeof(b));
    }

    void write_reg8(uint16_t addr, uint8_t value, uint32_t hz) {
        uint8_t b[3] = {(uint8_t)(0x20 | (addr >> 8)), (uint8_t)addr, value};
        transfer(b, sizeof(b), hz);
    }

    // One time-base-counter sample: two reads that agree, and micros() right
    // after; a sample that took long was preempted and is thrown away.
    bool sample_tbc(uint32_t &us, uint32_t &ticks) {
        uint32_t t0 = micros();
        uint32_t first = read_reg32(MCP2518FD_REG_TBC);
        ticks = read_reg32(MCP2518FD_REG_TBC);
        us = micros();
        return us - t0 < 200u && mcp2518fd_tbc_steady(first, ticks);
    }

    // SYSCLK from the time base counter against micros() over 10 ms. In the
    // undivided Listen-Only start, SYSCLK is the crystal. 0 when no clean pair
    // of samples was taken.
    uint32_t measure_sysclk_hz() {
        write_reg32(MCP2518FD_REG_TSCON, MCP2518FD_TSCON_TBCEN | (MCP2518FD_TBC_PRESCALE - 1));
        for (uint8_t attempt = 0; attempt < 3; attempt++) {
            uint32_t t0, c0, t1, c1;
            if (!sample_tbc(t0, c0)) continue;
            delay(10);
            if (!sample_tbc(t1, c1)) continue;
            return mcp2518fd_sysclk_hz(c1 - c0, t1 - t0);
        }
        return 0;
    }

    void spi_reset() {
        uint8_t b[2] = {0, 0};  // RESET: instruction 0, address 0
        transfer(b, sizeof(b), kSlowSpiHz);
    }

    // OSC once OSCRDY is set, or as last read when kOscStartMs runs out.
    uint32_t wait_osc_ready() {
        uint32_t t0 = millis();
        uint32_t osc = read_reg32(MCP2518FD_REG_OSC, kSlowSpiHz);
        while (!mcp2518fd_osc_ready(osc) && millis() - t0 < kOscStartMs) {
            delay(1);
            osc = read_reg32(MCP2518FD_REG_OSC, kSlowSpiHz);
        }
        return osc;
    }

    // Before the library's begin(): Configuration mode, the SPI reset and the
    // oscillator start-up, so a chip that is slow to come up is not reported
    // missing. Configuration mode first, as the library and the Linux driver
    // reset the chip: a controller still running from before an ESP32 reset
    // ends the frame it is on and aborts the rest. A chip that has just
    // powered up is in Configuration mode already; one that does not answer
    // costs both bounded waits, 12 ms. Returns what the chip reads back then,
    // the state the library starts from, for the not-detected line.
    Mcp2518fdReadback reset_chip() {
        write_reg8(MCP2518FD_REG_CON + 3, MCP2518FD_CON_REQ_CONFIG, kSlowSpiHz);
        uint32_t t0 = millis();
        while (mcp2518fd_opmod(read_reg32(MCP2518FD_REG_CON, kSlowSpiHz)) != MCP2518FD_OPMOD_CONFIG &&
               millis() - t0 <= 2) {
        }
        spi_reset();
        Mcp2518fdReadback rb = {};
        rb.osc   = wait_osc_ready();
        rb.con   = read_reg32(MCP2518FD_REG_CON, kSlowSpiHz);
        rb.devid = read_reg32(MCP2518FD_REG_DEVID, kSlowSpiHz);
        return rb;
    }

    // A classic T-2CAN has an MCP2515 on these pins. To an MCP2518FD its RESET
    // (0xC0) is the start of a WRITE_SAFE, which writes nothing without the
    // CRC that ends it, and its READ (0x03 0x0E) starts with the RESET
    // instruction, so at most it resets the chip. The MCP2518FD RESET at the
    // end leaves it in Configuration mode, as the failed start did; an MCP2515
    // ignores that and stays in Configuration mode after its own RESET.
    bool probe_mcp2515() {
        uint8_t reset = 0xC0;
        transfer(&reset, 1, kSlowSpiHz);
        delay(kMcp2515ResetMs);
        uint8_t rd[4] = {0x03, 0x0E, 0, 0};  // READ CANSTAT, then CANCTRL
        transfer(rd, sizeof(rd), kSlowSpiHz);
        spi_reset();
        return mcp2515_reset_signature(rd[2], rd[3]);
    }

    // One line per failed attempt with what reset_chip() read back and what
    // that says; a retry that reads back the same gets a short one.
    void report_not_detected(uint32_t err, Mcp2518fdReadback rb) {
        rb.mcp2515 = mcp2518fd_probe_mcp2515(mcp2518fd_spi_state(rb.con, rb.osc)) &&
                     probe_mcp2515();
        if (diag_printed_ && diag_err_ == err && mcp2518fd_readback_same(rb, diag_)) {
            Serial.printf("[CAN] %s MCP2518FD NOT detected on SPI (code 0x%lX), same readback "
                          "as before\n", label_, (unsigned long)err);
            return;
        }
        diag_printed_ = true;
        diag_err_ = err;
        diag_ = rb;
        Serial.printf("[CAN] %s MCP2518FD NOT detected on SPI (CS=%d SCK=%d MISO=%d MOSI=%d, "
                      "code 0x%lX): CiCON=0x%08lX OSC=0x%08lX DEVID=0x%08lX — %s\n",
                      label_, cs_pin_, sck_pin_, miso_pin_, mosi_pin_, (unsigned long)err,
                      (unsigned long)rb.con, (unsigned long)rb.osc, (unsigned long)rb.devid,
                      mcp2518fd_readback_text(rb));
    }

public:
    Mcp2518FdDriver(const char *label, int cs_pin, int sck_pin, int miso_pin, int mosi_pin)
        : can_((uint8_t)cs_pin, SPI, 255),  // 255: no INT pin, polled
          label_(label),
          cs_pin_(cs_pin),
          sck_pin_(sck_pin),
          miso_pin_(miso_pin),
          mosi_pin_(mosi_pin) {}

    bool begin(bool listen_only) override {
        begun_ = false;
        if (crystal_wrong_) {
            // A crystal does not change at run time: no restart per retry.
            Serial.printf("[CAN] %s MCP2518FD crystal measured %lu kHz, build expects %lu kHz — "
                          "left off the bus\n", label_, (unsigned long)(crystal_hz_ / 1000),
                          (unsigned long)(MCP2518FD_OSC_HZ / 1000));
            return false;
        }
        if (!spi_begun_) {
            digitalWrite((uint8_t)cs_pin_, HIGH);
            pinMode((uint8_t)cs_pin_, OUTPUT);
            SPI.begin(sck_pin_, miso_pin_, mosi_pin_, cs_pin_);
            spi_begun_ = true;
        }

        Mcp2518fdReadback readback = reset_chip();
        // Probe: Listen-Only on the undivided clock. Nothing reaches the bus
        // whatever the crystal is, and the library's SPI clock for a 20 MHz
        // SYSCLK is in spec with a 20 or a 40 MHz crystal.
        uint32_t err = start(ACAN2517FDSettings::OSC_20MHz, true);
        crystal_hz_ = err ? 0 : measure_sysclk_hz();
        stop();
        chip_detected_ = (err & (kTimingRejected | ACAN2517FD::kRequestedConfigurationModeTimeOut |
                                 ACAN2517FD::kReadBackErrorWith1MHzSPIClock)) == 0;
        if (err & kTimingRejected) {
            Serial.printf("[CAN] %s MCP2518FD bit timing rejected — not started\n", label_);
            return false;
        }
        if (!chip_detected_) {
            report_not_detected(err, readback);
            return false;
        }
        if (err != 0) {
            Serial.printf("[CAN] %s MCP2518FD detected but configuration FAILED (code 0x%lX)\n",
                          label_, (unsigned long)err);
            return false;
        }
        // A wrong crystal is a wrong bit rate, and a controller in normal mode
        // at the wrong bit rate puts error frames on the car's bus.
        if (crystal_hz_ == 0) {
            Serial.printf("[CAN] %s MCP2518FD crystal check FAILED (time base counter) — "
                          "left off the bus\n", label_);
            return false;
        }
        if (!mcp2518fd_osc_matches(crystal_hz_, MCP2518FD_OSC_HZ)) {
            crystal_wrong_ = true;
            Serial.printf("[CAN] %s MCP2518FD crystal measured %lu kHz, build expects %lu kHz — "
                          "left off the bus\n", label_, (unsigned long)(crystal_hz_ / 1000),
                          (unsigned long)(MCP2518FD_OSC_HZ / 1000));
            return false;
        }

        err = restart(listen_only);
        if (err != 0) {
            Serial.printf("[CAN] %s MCP2518FD start FAILED (code 0x%lX)\n", label_,
                          (unsigned long)err);
            return false;
        }
        begun_ = true;
        ACAN2517FDSettings s = settings(MCP2518FD_OSC, listen_only);
        Serial.printf("[CAN] %s MCP2518FD detected on SPI — %s mode @ 500 kbps, crystal %lu kHz "
                      "(SYSCLK %lu MHz, sample point %lu%%)\n",
                      label_, listen_only ? "Listen-Only" : "Normal CAN 2.0",
                      (unsigned long)(crystal_hz_ / 1000), (unsigned long)(s.sysClock() / 1000000),
                      (unsigned long)s.arbitrationSamplePointFromBitStart());
        return true;
    }

    bool hardwarePresent() override { return chip_detected_; }

    bool send(const CanFrame &frame) override {
        if (!installed_ || listen_only_) return false;
        CANFDMessage msg;
        if (!mcp2518fd_tx_message(frame, msg)) return false;
        // There is no software queue: a full controller FIFO fails the send,
        // as an MCP2515 with all TX buffers busy does.
        if (!can_.tryToSend(msg)) {
            tx_failed_++;
            return false;
        }
        tx_count_++;
        return true;
    }

    bool receive(CanFrame &frame) override {
        if (!installed_) return false;
        for (uint8_t i = 0; i < CAN_RX_REJECT_BURST_MAX; i++) {
            CANFDMessage msg;
            // The library returns what an earlier poll buffered and then polls
            // the controller; when nothing was buffered, take what that poll
            // just moved instead of waiting for the next loop.
            if (!can_.receive(msg) && !(can_.available() && can_.receive(msg))) return false;
            if (!mcp2518fd_rx_frame(msg, frame)) {
                rx_rejected_++;
                continue;
            }
            rx_count_++;
            return true;
        }
        return false;
    }

    uint32_t errorCount() override {
        return rxMissedCount() + busErrorCount() + txFailedCount();
    }

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

    uint32_t rxRejectedCount() override { return rx_rejected_; }

    // The controller flags an RX FIFO overflow, it doesn't count frames: one
    // event is one or more frames lost.
    uint32_t rxMissedCount() override {
        rx_missed_ += can_.hardwareReceiveBufferOverflowCount();
        can_.resetHardwareReceiveBufferOverflowCount();
        return rx_missed_;
    }

    // CiBDIAG0 holds four 8-bit error counters (nominal/data, RX/TX) that only
    // count up; fold them in and clear them so they never sit at 255.
    uint32_t busErrorCount() override {
        if (installed_) {
            uint32_t diag = read_reg32(MCP2518FD_REG_BDIAG0);
            if (diag != 0) {
                write_reg32(MCP2518FD_REG_BDIAG0, 0);
                bus_errors_ += mcp2518fd_bdiag0_errors(diag);
            }
        }
        return bus_errors_;
    }

    uint32_t txFailedCount() override { return tx_failed_; }

    // CiTREC: REC in bits 0-7, TEC in 8-15, bus-off in bit 21, error-passive
    // in bits 19-20. rx_missed counts FIFO overflow events, each one or more
    // frames.
    void describeState(char *out, size_t n) override {
        if (!installed_) {
            snprintf(out, n, "MCP2518FD %s",
                     crystal_wrong_ ? "crystal mismatch, kept off the bus"
                                    : chip_detected_ ? "stopped" : "not detected on SPI");
            return;
        }
        uint32_t trec = can_.errorCounters();
        snprintf(out, n,
                 "MCP2518FD %s TEC=%lu REC=%lu%s%s rx_missed=%lu bus_err=%lu tx_fail=%lu "
                 "crystal=%lukHz",
                 listen_only_ ? "Listen-Only" : "Normal",
                 (unsigned long)((trec >> 8) & 0xFFu), (unsigned long)(trec & 0xFFu),
                 (trec & (1u << 21)) ? " bus-off" : "",
                 (trec & ((1u << 19) | (1u << 20))) ? " error-passive" : "",
                 (unsigned long)rxMissedCount(), (unsigned long)busErrorCount(),
                 (unsigned long)tx_failed_, (unsigned long)(crystal_hz_ / 1000u));
    }

    void setListenOnly(bool enable) override {
        if (!begun_ || (installed_ && listen_only_ == enable)) return;
        uint32_t err = restart(enable);
        if (err != 0) {
            Serial.printf("[CAN] %s MCP2518FD mode switch FAILED (code 0x%lX) — controller stopped\n",
                          label_, (unsigned long)err);
        }
    }

    void setAcceptanceFilter(bool single, uint32_t id) override {
        setAcceptanceFilters(single ? &id : nullptr, single ? 1 : 0);
    }

    void setAcceptanceFilters(const uint32_t *ids, uint8_t count) override {
        if (!ids) count = 0;
        if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
        bool same = filter_count_ == count;
        for (uint8_t i = 0; same && i < count; i++) {
            same = filter_ids_[i] == (ids[i] & 0x7FFu);
        }
        filter_count_ = count;
        for (uint8_t i = 0; i < count; i++) filter_ids_[i] = ids[i] & 0x7FFu;
        if (!begun_ || (same && installed_)) return;  // begin() loads them
        uint32_t err = restart(listen_only_);
        if (err != 0) {
            Serial.printf("[CAN] %s MCP2518FD filter switch FAILED (code 0x%lX) — controller stopped\n",
                          label_, (unsigned long)err);
        } else if (count) {
            Serial.printf("[CAN] %s MCP2518FD hardware filter -> %u exact ids\n",
                          label_, (unsigned)count);
        } else {
            Serial.printf("[CAN] %s MCP2518FD hardware filter -> accept all\n", label_);
        }
    }

    const char *acceptanceFilterMode() override {
        return installed_ ? filter_mode_ : "down";
    }

    // Pre-reboot quiesce: Configuration mode and the SPI reset (see stop()).
    void shutdown() override {
        stop();
        begun_ = false;
    }
};
#endif

CanDriver *can_driver_create() {
#if defined(CAN_DRIVER_TWAI)
    return new TwaiDriver("can0", PIN_CAN_TX, PIN_CAN_RX);
#elif defined(CAN_DRIVER_MCP2515)
    return new Mcp2515Driver("can0", PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO,
                             PIN_MCP_MOSI, PIN_MCP_RST);
#elif defined(CAN_DRIVER_T2CAN_DUAL)
    return can_driver_create(CAN_BUS_PRIMARY);
#else
    return nullptr;
#endif
}

CanDriver *can_driver_create(CanBusId bus) {
#if defined(CAN_DRIVER_T2CAN_DUAL)
    if (bus == CAN_BUS_SECONDARY) {
#if defined(BOARD_LILYGO_T2CAN_FD)
        return new Mcp2518FdDriver("can1", PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO,
                                   PIN_MCP_MOSI);
#else
        return new Mcp2515Driver("can1", PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO,
                                 PIN_MCP_MOSI, PIN_MCP_RST);
#endif
    }
    return new TwaiDriver("can0", PIN_CAN_TX, PIN_CAN_RX);
#else
    (void)bus;
    return can_driver_create();
#endif
}

CanErrorSplit can_error_split(CanDriver **buses, uint8_t count) {
    CanErrorSplit s = {};
    for (uint8_t i = 0; i < count; i++) {
        if (!buses[i]) continue;
        s.rx_missed_count += buses[i]->rxMissedCount();
        s.bus_error_count += buses[i]->busErrorCount();
        s.tx_failed_count += buses[i]->txFailedCount();
    }
    return s;
}

void can_shutdown_all(CanDriver **buses, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (buses[i]) buses[i]->shutdown();
    }
    Serial.println("[CAN] Controllers stopped — bus released");
}

#if !defined(CAN_DRIVER_TWAI) && !defined(CAN_DRIVER_MCP2515) && !defined(CAN_DRIVER_T2CAN_DUAL)
#error "Define CAN_DRIVER_TWAI, CAN_DRIVER_MCP2515, or CAN_DRIVER_T2CAN_DUAL in platformio.ini build_flags"
#endif
// The FD board is a T-2CAN in every other respect (pins, bus count, black box),
// and those paths key off the two plain T-2CAN flags.
#if defined(BOARD_LILYGO_T2CAN_FD) && !(defined(BOARD_LILYGO_T2CAN) && defined(CAN_DRIVER_T2CAN_DUAL))
#error "BOARD_LILYGO_T2CAN_FD also needs BOARD_LILYGO_T2CAN and CAN_DRIVER_T2CAN_DUAL"
#endif
