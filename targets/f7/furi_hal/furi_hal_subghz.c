#include <furi_hal_subghz.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <furi_hal_region_i.h>
#include <furi_hal_region.h>
#include <furi_hal_version.h>
#include <furi_hal_rtc.h>
#include <furi_hal_spi.h>
#include <furi_hal_cortex.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_resources.h>
#include <furi_hal_bus.h>

#include <stm32wbxx_ll_dma.h>

#include <furi.h>
#include <cc1101.h>
#include <stdio.h>

#define TAG "FuriHalSubGhz"

static uint32_t furi_hal_subghz_debug_gpio_buff[2] = {0};

/* DMA Channels definition */
#define SUBGHZ_DMA             (DMA2)
#define SUBGHZ_DMA_CH1_CHANNEL (LL_DMA_CHANNEL_1)
#define SUBGHZ_DMA_CH2_CHANNEL (LL_DMA_CHANNEL_2)
#define SUBGHZ_DMA_CH1_IRQ     (FuriHalInterruptIdDma2Ch1)
#define SUBGHZ_DMA_CH1_DEF     SUBGHZ_DMA, SUBGHZ_DMA_CH1_CHANNEL
#define SUBGHZ_DMA_CH2_DEF     SUBGHZ_DMA, SUBGHZ_DMA_CH2_CHANNEL

/** SubGhz state */
typedef enum {
    SubGhzStateInit, /**< Init pending */
    SubGhzStateBroken, /**< Chip power-on self test failed */
    SubGhzStateIdle, /**< Idle, energy save mode */

    SubGhzStateAsyncRx, /**< Async RX started */

    SubGhzStateAsyncTx, /**< Async TX started, DMA and timer is on */

} SubGhzState;

/** SubGhz regulation, receive transmission on the current frequency for the
 * region */
typedef enum {
    SubGhzRegulationOnlyRx, /**only Rx*/
    SubGhzRegulationTxRx, /**TxRx*/
} SubGhzRegulation;

typedef struct {
    volatile SubGhzState state;
    volatile SubGhzRegulation regulation;
    const GpioPin* async_mirror_pin;

    int32_t rolling_counter_mult;
    bool extended_range   : 1;
    bool bypass_region    : 1;
    bool ext_leds_and_amp : 1;
} FuriHalSubGhz;

volatile FuriHalSubGhz furi_hal_subghz = {
    .state = SubGhzStateInit,
    .regulation = SubGhzRegulationTxRx,
    .async_mirror_pin = NULL,
    .rolling_counter_mult = 1,
    .extended_range = false,
    .bypass_region = false,
    .ext_leds_and_amp = true,
};

int32_t furi_hal_subghz_get_rolling_counter_mult(void) {
    return furi_hal_subghz.rolling_counter_mult;
}

void furi_hal_subghz_set_rolling_counter_mult(int32_t mult) {
    furi_hal_subghz.rolling_counter_mult = mult;
}

void furi_hal_subghz_set_extended_range(bool enabled) {
    furi_hal_subghz.extended_range = enabled;
}

bool furi_hal_subghz_get_extended_range(void) {
    return furi_hal_subghz.extended_range;
}

void furi_hal_subghz_set_bypass_region(bool enabled) {
    furi_hal_subghz.bypass_region = enabled;
}

bool furi_hal_subghz_get_bypass_region(void) {
    return furi_hal_subghz.bypass_region;
}

void furi_hal_subghz_set_async_mirror_pin(const GpioPin* pin) {
    furi_hal_subghz.async_mirror_pin = pin;
}

void furi_hal_subghz_set_ext_leds_and_amp(bool enabled) {
    furi_hal_subghz.ext_leds_and_amp = enabled;
}

bool furi_hal_subghz_get_ext_leds_and_amp(void) {
    return furi_hal_subghz.ext_leds_and_amp;
}

const GpioPin* furi_hal_subghz_get_data_gpio(void) {
    return &gpio_cc1101_g0;
}

void furi_hal_subghz_init(void) {
    furi_check(furi_hal_subghz.state == SubGhzStateInit);
    furi_hal_subghz.state = SubGhzStateBroken;

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    do {
#ifdef FURI_HAL_SUBGHZ_TX_GPIO
        furi_hal_gpio_init(
            &FURI_HAL_SUBGHZ_TX_GPIO, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
#endif

#ifdef FURI_HAL_SUBGHZ_ASYNC_MIRROR_GPIO
        furi_hal_subghz_set_async_mirror_pin(&FURI_HAL_SUBGHZ_ASYNC_MIRROR_GPIO);
#endif

        // Reset
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        cc1101_reset(&furi_hal_spi_bus_handle_subghz);
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHighImpedance);

        // Prepare GD0 for power on self test
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);

        // GD0 low
        FuriHalCortexTimer timeout = furi_hal_cortex_timer_get(10000);
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHW);
        while(furi_hal_gpio_read(&gpio_cc1101_g0) != false &&
              !furi_hal_cortex_timer_is_expired(timeout))
            ;

        if(furi_hal_gpio_read(&gpio_cc1101_g0) != false) {
            break;
        }

        // GD0 high
        timeout = furi_hal_cortex_timer_get(10000);
        cc1101_write_reg(
            &furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHW | CC1101_IOCFG_INV);
        while(furi_hal_gpio_read(&gpio_cc1101_g0) != true &&
              !furi_hal_cortex_timer_is_expired(timeout))
            ;

        if(furi_hal_gpio_read(&gpio_cc1101_g0) != true) {
            break;
        }

        // Reset GD0 to floating state
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHighImpedance);
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        // RF switches
        furi_hal_gpio_init(&gpio_rf_sw_0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG2, CC1101IocfgHW);

        // Go to sleep
        cc1101_shutdown(&furi_hal_spi_bus_handle_subghz);

        furi_hal_subghz.state = SubGhzStateIdle;
    } while(false);

    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    if(furi_hal_subghz.state == SubGhzStateIdle) {
        FURI_LOG_I(TAG, "Init OK");
    } else {
        FURI_LOG_E(TAG, "Init Fail");
    }
}

void furi_hal_subghz_sleep(void) {
    furi_check(furi_hal_subghz.state == SubGhzStateIdle);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);

    cc1101_switch_to_idle(&furi_hal_spi_bus_handle_subghz);

    cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHighImpedance);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    cc1101_shutdown(&furi_hal_spi_bus_handle_subghz);

    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_dump_state(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    printf(
        "[furi_hal_subghz] cc1101 chip %d, version %d\r\n",
        cc1101_get_partnumber(&furi_hal_spi_bus_handle_subghz),
        cc1101_get_version(&furi_hal_spi_bus_handle_subghz));
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_load_custom_preset(const uint8_t* preset_data) {
    furi_check(preset_data);

    //load config
    furi_hal_subghz_reset();
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint32_t i = 0;
    uint8_t pa[8] = {0};
    while(preset_data[i]) {
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, preset_data[i], preset_data[i + 1]);
        i += 2;
    }
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    //load pa table
    memcpy(&pa[0], &preset_data[i + 2], 8);
    furi_hal_subghz_load_patable(pa);

    //show debug
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        i = 0;
        FURI_LOG_D(TAG, "Loading custom preset");
        while(preset_data[i]) {
            FURI_LOG_D(TAG, "Reg[%lu]: %02X=%02X", i, preset_data[i], preset_data[i + 1]);
            i += 2;
        }
        for(uint8_t y = i; y < i + 10; y++) {
            FURI_LOG_D(TAG, "PA[%u]:  %02X", y, preset_data[y]);
        }
    }
}

void furi_hal_subghz_load_registers(const uint8_t* data) {
    furi_check(data);

    furi_hal_subghz_reset();
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint32_t i = 0;
    while(data[i]) {
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, data[i], data[i + 1]);
        i += 2;
    }
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_load_patable(const uint8_t data[8]) {
    furi_check(data);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_set_pa_table(&furi_hal_spi_bus_handle_subghz, data);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_write_packet(const uint8_t* data, uint8_t size) {
    furi_check(data);
    furi_check(size);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_flush_tx(&furi_hal_spi_bus_handle_subghz);
    cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_FIFO, size);
    cc1101_write_fifo(&furi_hal_spi_bus_handle_subghz, data, size);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_flush_rx(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_flush_rx(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_flush_tx(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_flush_tx(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

bool furi_hal_subghz_rx_pipe_not_empty(void) {
    CC1101RxBytes status[1];
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_read_reg(
        &furi_hal_spi_bus_handle_subghz, (CC1101_STATUS_RXBYTES) | CC1101_BURST, (uint8_t*)status);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    // TODO: Find reason why RXFIFO_OVERFLOW doesnt work correctly
    if(status->NUM_RXBYTES > 0) {
        return true;
    } else {
        return false;
    }
}

bool furi_hal_subghz_is_rx_data_crc_valid(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint8_t data[1];
    cc1101_read_reg(&furi_hal_spi_bus_handle_subghz, CC1101_STATUS_LQI | CC1101_BURST, data);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    if((data[0] >> 7) & 0x01) {
        return true;
    } else {
        return false;
    }
}

void furi_hal_subghz_read_packet(uint8_t* data, uint8_t* size) {
    furi_check(data);
    furi_check(size);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_read_fifo(&furi_hal_spi_bus_handle_subghz, data, size);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_shutdown(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    // Reset and shutdown
    cc1101_shutdown(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_reset(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    cc1101_switch_to_idle(&furi_hal_spi_bus_handle_subghz);
    cc1101_reset(&furi_hal_spi_bus_handle_subghz);
    // Warning: push pull cc1101 clock output on GD0
    cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHighImpedance);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_idle(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_switch_to_idle(&furi_hal_spi_bus_handle_subghz);
    //waiting for the chip to switch to IDLE mode
    furi_check(cc1101_wait_status_state(&furi_hal_spi_bus_handle_subghz, CC1101StateIDLE, 10000));
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

void furi_hal_subghz_rx(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_switch_to_rx(&furi_hal_spi_bus_handle_subghz);
    //waiting for the chip to switch to Rx mode
    furi_check(cc1101_wait_status_state(&furi_hal_spi_bus_handle_subghz, CC1101StateRX, 10000));
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

bool furi_hal_subghz_tx(void) {
    if(furi_hal_subghz.regulation != SubGhzRegulationTxRx) return false;
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_switch_to_tx(&furi_hal_spi_bus_handle_subghz);
    //waiting for the chip to switch to Tx mode
    furi_check(cc1101_wait_status_state(&furi_hal_spi_bus_handle_subghz, CC1101StateTX, 10000));
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    return true;
}

float furi_hal_subghz_get_rssi(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    int32_t rssi_dec = cc1101_get_rssi(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    float rssi = rssi_dec;
    if(rssi_dec >= 128) {
        rssi = ((rssi - 256.0f) / 2.0f) - 74.0f;
    } else {
        rssi = (rssi / 2.0f) - 74.0f;
    }

    return rssi;
}

uint8_t furi_hal_subghz_get_lqi(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint8_t data[1];
    cc1101_read_reg(&furi_hal_spi_bus_handle_subghz, CC1101_STATUS_LQI | CC1101_BURST, data);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    return data[0] & 0x7F;
}

/* 
 Modified by @tkerby & MX to the full YARD Stick One extended range of 281-361 MHz, 378-481 MHz, and 749-962 MHz. 
 These changes are at your own risk. The PLL may not lock and FZ devs have warned of possible damage!
 */

bool furi_hal_subghz_is_frequency_valid(uint32_t value) {
    if(!(value >= 281000000 && value <= 361000000) &&
       !(value >= 378000000 && value <= 481000000) &&
       !(value >= 749000000 && value <= 962000000)) {
        return false;
    }

    return true;
}

uint32_t furi_hal_subghz_set_frequency_and_path(uint32_t value) {
    // Set these values to the extended frequency range only. They dont define if you can transmit but do select the correct RF path
    value = furi_hal_subghz_set_frequency(value);
    if(value >= 280999633 && value <= 360999938) {
        furi_hal_subghz_set_path(FuriHalSubGhzPath315);
    } else if(value >= 377999755 && value <= 481000000) {
        furi_hal_subghz_set_path(FuriHalSubGhzPath433);
    } else if(value >= 748999633 && value <= 962000000) {
        furi_hal_subghz_set_path(FuriHalSubGhzPath868);
    } else {
        furi_crash("SubGhz: Incorrect frequency during set.");
    }
    return value;
}

SubGhzTx furi_hal_subghz_check_tx(uint32_t value) {
    // Check against extended range of YARD Stick One, no configuration would allow this frequency
    if(!furi_hal_subghz_is_frequency_valid(value)) {
        FURI_LOG_I(TAG, "Frequency blocked - outside supported range");
        return SubGhzTxUnsupported;
    }

    // Check against default range, regardless of region restrictions
    if(!furi_hal_subghz.extended_range &&
       !(value >= 299999755 && value <= 350000335) && // was increased from 348 to 350
       !(value >= 386999938 && value <= 467750000) && // was increased from 464 to 467.75
       !(value >= 778999847 && value <= 928000000)) {
        FURI_LOG_I(TAG, "Frequency blocked - outside default range");
        return SubGhzTxBlockedDefault;
    }

    // Check against region restrictions, tighter than extended and default
    if(!furi_hal_subghz.bypass_region) {
        if(!furi_hal_region_is_provisioned()) {
            FURI_LOG_I(TAG, "Frequency blocked - region not provisioned");
            return SubGhzTxBlockedRegionNotProvisioned;
        }
        if(!_furi_hal_region_is_frequency_allowed(value)) {
            FURI_LOG_I(TAG, "Frequency blocked - outside region range");
            return SubGhzTxBlockedRegion;
        }
    }

    // We already checked for extended range, default range, and region range
    return SubGhzTxAllowed;
}

bool furi_hal_subghz_is_tx_allowed(uint32_t value) {
    return furi_hal_subghz_check_tx(value) == SubGhzTxAllowed;
}

uint32_t furi_hal_subghz_set_frequency(uint32_t value) {
    if(furi_hal_subghz_is_tx_allowed(value)) {
        furi_hal_subghz.regulation = SubGhzRegulationTxRx;
    } else {
        furi_hal_subghz.regulation = SubGhzRegulationOnlyRx;
    }

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint32_t real_frequency = cc1101_set_frequency(&furi_hal_spi_bus_handle_subghz, value);
    cc1101_calibrate(&furi_hal_spi_bus_handle_subghz);

    furi_check(cc1101_wait_status_state(&furi_hal_spi_bus_handle_subghz, CC1101StateIDLE, 10000));

    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    return real_frequency;
}

void furi_hal_subghz_set_path(FuriHalSubGhzPath path) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    if(path == FuriHalSubGhzPath433) {
        furi_hal_gpio_write(&gpio_rf_sw_0, 0);
        cc1101_write_reg(
            &furi_hal_spi_bus_handle_subghz, CC1101_IOCFG2, CC1101IocfgHW | CC1101_IOCFG_INV);
    } else if(path == FuriHalSubGhzPath315) {
        furi_hal_gpio_write(&gpio_rf_sw_0, 1);
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG2, CC1101IocfgHW);
    } else if(path == FuriHalSubGhzPath868) {
        furi_hal_gpio_write(&gpio_rf_sw_0, 1);
        cc1101_write_reg(
            &furi_hal_spi_bus_handle_subghz, CC1101_IOCFG2, CC1101IocfgHW | CC1101_IOCFG_INV);
    } else if(path == FuriHalSubGhzPathIsolate) {
        furi_hal_gpio_write(&gpio_rf_sw_0, 0);
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG2, CC1101IocfgHW);
    } else {
        furi_crash("SubGhz: Incorrect path during set.");
    }
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

static bool furi_hal_subghz_start_debug(void) {
    bool ret = false;
    if(furi_hal_subghz.async_mirror_pin != NULL) {
        furi_hal_gpio_write(furi_hal_subghz.async_mirror_pin, false);
        furi_hal_gpio_init(
            furi_hal_subghz.async_mirror_pin,
            GpioModeOutputPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh);
        ret = true;
    }
    return ret;
}

static bool furi_hal_subghz_stop_debug(void) {
    bool ret = false;
    if(furi_hal_subghz.async_mirror_pin != NULL) {
        furi_hal_gpio_init(
            furi_hal_subghz.async_mirror_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        ret = true;
    }
    return ret;
}

volatile uint32_t furi_hal_subghz_capture_delta_duration = 0;
volatile FuriHalSubGhzCaptureCallback furi_hal_subghz_capture_callback = NULL;
volatile void* furi_hal_subghz_capture_callback_context = NULL;

static void furi_hal_subghz_capture_ISR(void* context) {
    UNUSED(context);
    // Channel 1
    if(LL_TIM_IsActiveFlag_CC1(TIM2)) {
        LL_TIM_ClearFlag_CC1(TIM2);
        furi_hal_subghz_capture_delta_duration = LL_TIM_IC_GetCaptureCH1(TIM2);
        if(furi_hal_subghz_capture_callback) {
            if(furi_hal_subghz.async_mirror_pin != NULL)
                furi_hal_gpio_write(furi_hal_subghz.async_mirror_pin, false);

            furi_hal_subghz_capture_callback(
                true,
                furi_hal_subghz_capture_delta_duration,
                (void*)furi_hal_subghz_capture_callback_context);
        }
    }
    // Channel 2
    if(LL_TIM_IsActiveFlag_CC2(TIM2)) {
        LL_TIM_ClearFlag_CC2(TIM2);
        if(furi_hal_subghz_capture_callback) {
            if(furi_hal_subghz.async_mirror_pin != NULL)
                furi_hal_gpio_write(furi_hal_subghz.async_mirror_pin, true);

            furi_hal_subghz_capture_callback(
                false,
                LL_TIM_IC_GetCaptureCH2(TIM2) - furi_hal_subghz_capture_delta_duration,
                (void*)furi_hal_subghz_capture_callback_context);
        }
    }
}

void furi_hal_subghz_start_async_rx(FuriHalSubGhzCaptureCallback callback, void* context) {
    furi_check(furi_hal_subghz.state == SubGhzStateIdle);
    furi_check(callback);

    furi_hal_subghz.state = SubGhzStateAsyncRx;

    furi_hal_subghz_capture_callback = callback;
    furi_hal_subghz_capture_callback_context = context;

    furi_hal_gpio_init_ex(
        &gpio_cc1101_g0, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn1TIM2);

    furi_hal_bus_enable(FuriHalBusTIM2);

    // Timer: base
    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Prescaler = 64 - 1;
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload = 0x7FFFFFFE;
    // Clock division for capture filter
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV4;
    LL_TIM_Init(TIM2, &TIM_InitStruct);

    // Timer: advanced
    LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM2);
    LL_TIM_SetTriggerInput(TIM2, LL_TIM_TS_TI2FP2);
    LL_TIM_SetSlaveMode(TIM2, LL_TIM_SLAVEMODE_RESET);
    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
    LL_TIM_EnableMasterSlaveMode(TIM2);
    LL_TIM_DisableDMAReq_TRIG(TIM2);
    LL_TIM_DisableIT_TRIG(TIM2);

    // Timer: channel 1 indirect
    LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_INDIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_FALLING);

    // Timer: channel 2 direct
    LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetFilter(
        TIM2,
        LL_TIM_CHANNEL_CH2,
        LL_TIM_IC_FILTER_FDIV32_N8); // Capture filter: 1/(64000000/64/4/32*8) = 16us

    // ISR setup
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, furi_hal_subghz_capture_ISR, NULL);

    // Interrupts and channels
    LL_TIM_EnableIT_CC1(TIM2);
    LL_TIM_EnableIT_CC2(TIM2);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);

    // Start timer
    LL_TIM_SetCounter(TIM2, 0);
    LL_TIM_EnableCounter(TIM2);

    // Start debug
    furi_hal_subghz_start_debug();

    // Switch to RX
    furi_hal_subghz_rx();

    // Clear the variable after the end of the session
    furi_hal_subghz_capture_delta_duration = 0;
}

void furi_hal_subghz_stop_async_rx(void) {
    furi_check(furi_hal_subghz.state == SubGhzStateAsyncRx);
    furi_hal_subghz.state = SubGhzStateIdle;

    // Shutdown radio
    furi_hal_subghz_idle();

    FURI_CRITICAL_ENTER();
    furi_hal_bus_disable(FuriHalBusTIM2);

    // Stop debug
    furi_hal_subghz_stop_debug();

    FURI_CRITICAL_EXIT();
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, NULL, NULL);

    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

typedef enum {
    FuriHalSubGhzAsyncTxMiddlewareStateIdle,
    FuriHalSubGhzAsyncTxMiddlewareStateReset,
    FuriHalSubGhzAsyncTxMiddlewareStateRun,
} FuriHalSubGhzAsyncTxMiddlewareState;

typedef struct {
    FuriHalSubGhzAsyncTxMiddlewareState state;
    bool is_odd_level;
    uint32_t adder_duration;
} FuriHalSubGhzAsyncTxMiddleware;

typedef struct {
    uint32_t* buffer;
    FuriHalSubGhzAsyncTxCallback callback;
    void* callback_context;
    uint64_t duty_high;
    uint64_t duty_low;
    FuriHalSubGhzAsyncTxMiddleware middleware;
} FuriHalSubGhzAsyncTx;

static FuriHalSubGhzAsyncTx furi_hal_subghz_async_tx = {0};

void furi_hal_subghz_async_tx_middleware_idle(FuriHalSubGhzAsyncTxMiddleware* middleware) {
    middleware->state = FuriHalSubGhzAsyncTxMiddlewareStateIdle;
    middleware->is_odd_level = false;
    middleware->adder_duration = 0;
}

static inline uint32_t furi_hal_subghz_async_tx_middleware_get_duration(
    FuriHalSubGhzAsyncTxMiddleware* middleware,
    FuriHalSubGhzAsyncTxCallback callback) {
    uint32_t ret = 0;
    bool is_level = false;

    if(middleware->state == FuriHalSubGhzAsyncTxMiddlewareStateReset) return 0;

    while(1) {
        LevelDuration ld = callback(furi_hal_subghz_async_tx.callback_context);
        if(level_duration_is_reset(ld)) {
            middleware->state = FuriHalSubGhzAsyncTxMiddlewareStateReset;
            if(!middleware->is_odd_level) {
                return 0;
            } else {
                return middleware->adder_duration;
            }
        } else if(level_duration_is_wait(ld)) {
            middleware->is_odd_level = !middleware->is_odd_level;
            ret = middleware->adder_duration + FURI_HAL_SUBGHZ_ASYNC_TX_GUARD_TIME;
            middleware->adder_duration = 0;
            return ret;
        }

        is_level = level_duration_get_level(ld);

        if(middleware->state == FuriHalSubGhzAsyncTxMiddlewareStateIdle) {
            if(is_level != middleware->is_odd_level) {
                middleware->state = FuriHalSubGhzAsyncTxMiddlewareStateRun;
                middleware->is_odd_level = is_level;
                middleware->adder_duration = 0;
            } else {
                continue;
            }
        }

        if(middleware->state == FuriHalSubGhzAsyncTxMiddlewareStateRun) {
            if(is_level == middleware->is_odd_level) {
                middleware->adder_duration += level_duration_get_duration(ld);
                continue;
            } else {
                middleware->is_odd_level = is_level;
                ret = middleware->adder_duration;
                middleware->adder_duration = level_duration_get_duration(ld);
                return ret;
            }
        }
    }
}

static void furi_hal_subghz_async_tx_refill(uint32_t* buffer, size_t samples) {
    furi_check(furi_hal_subghz.state == SubGhzStateAsyncTx);

    while(samples > 0) {
        volatile uint32_t duration = furi_hal_subghz_async_tx_middleware_get_duration(
            &furi_hal_subghz_async_tx.middleware, furi_hal_subghz_async_tx.callback);
        if(duration == 0) {
            *buffer = 0;
            buffer++;
            samples--;
            LL_DMA_DisableIT_HT(SUBGHZ_DMA_CH1_DEF);
            LL_DMA_DisableIT_TC(SUBGHZ_DMA_CH1_DEF);
            if(LL_DMA_IsActiveFlag_HT1(SUBGHZ_DMA)) {
                LL_DMA_ClearFlag_HT1(SUBGHZ_DMA);
            }
            if(LL_DMA_IsActiveFlag_TC1(SUBGHZ_DMA)) {
                LL_DMA_ClearFlag_TC1(SUBGHZ_DMA);
            }
            break;
        } else {
            // Lowest possible value is 2us
            if(duration > 2) {
                // Subtract 1 since we counting from 0
                *buffer = duration - 1;
            } else {
                *buffer = 1;
            }
            buffer++;
            samples--;
        }

        if(samples % 2) {
            furi_hal_subghz_async_tx.duty_high += duration;
        } else {
            furi_hal_subghz_async_tx.duty_low += duration;
        }
    }
}

static void furi_hal_subghz_async_tx_dma_isr(void* context) {
    UNUSED(context);
    furi_check(furi_hal_subghz.state == SubGhzStateAsyncTx);

#if SUBGHZ_DMA_CH1_CHANNEL == LL_DMA_CHANNEL_1
    if(LL_DMA_IsActiveFlag_HT1(SUBGHZ_DMA)) {
        LL_DMA_ClearFlag_HT1(SUBGHZ_DMA);
        furi_hal_subghz_async_tx_refill(
            furi_hal_subghz_async_tx.buffer, FURI_HAL_SUBGHZ_ASYNC_TX_BUFFER_HALF);
    }
    if(LL_DMA_IsActiveFlag_TC1(SUBGHZ_DMA)) {
        LL_DMA_ClearFlag_TC1(SUBGHZ_DMA);
        furi_hal_subghz_async_tx_refill(
            furi_hal_subghz_async_tx.buffer + FURI_HAL_SUBGHZ_ASYNC_TX_BUFFER_HALF,
            FURI_HAL_SUBGHZ_ASYNC_TX_BUFFER_HALF);
    }
#else
#error Update this code. Would you kindly?
#endif
}

bool furi_hal_subghz_start_async_tx(FuriHalSubGhzAsyncTxCallback callback, void* context) {
    furi_check(furi_hal_subghz.state == SubGhzStateIdle);
    furi_check(callback);

    //If transmission is prohibited by regional settings
    if(furi_hal_subghz.regulation != SubGhzRegulationTxRx) return false;

    furi_hal_subghz_async_tx.callback = callback;
    furi_hal_subghz_async_tx.callback_context = context;

    furi_hal_subghz.state = SubGhzStateAsyncTx;

    furi_hal_subghz_async_tx.duty_low = 0;
    furi_hal_subghz_async_tx.duty_high = 0;

    furi_hal_subghz_async_tx.buffer =
        malloc(FURI_HAL_SUBGHZ_ASYNC_TX_BUFFER_FULL * sizeof(uint32_t));

    // Connect CC1101_GD0 to TIM2 as output
    furi_hal_gpio_init_ex(
        &gpio_cc1101_g0, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn1TIM2);

    // Configure DMA
    LL_DMA_InitTypeDef dma_config = {0};
    dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (TIM2->ARR);
    dma_config.MemoryOrM2MDstAddress = (uint32_t)furi_hal_subghz_async_tx.buffer;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_CIRCULAR;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_config.NbData = FURI_HAL_SUBGHZ_ASYNC_TX_BUFFER_FULL;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM2_UP;
    dma_config.Priority =
        LL_DMA_PRIORITY_VERYHIGH; // Ensure that ARR is updated before anyone else try to check it
    LL_DMA_Init(SUBGHZ_DMA_CH1_DEF, &dma_config);
    furi_hal_interrupt_set_isr(SUBGHZ_DMA_CH1_IRQ, furi_hal_subghz_async_tx_dma_isr, NULL);
    LL_DMA_EnableIT_TC(SUBGHZ_DMA_CH1_DEF);
    LL_DMA_EnableIT_HT(SUBGHZ_DMA_CH1_DEF);
    LL_DMA_EnableChannel(SUBGHZ_DMA_CH1_DEF);

    furi_hal_bus_enable(FuriHalBusTIM2);

    // Configure TIM2
    LL_TIM_SetCounterMode(TIM2, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(TIM2, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetAutoReload(TIM2, 1000);
    LL_TIM_SetPrescaler(TIM2, 64 - 1);
    LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM2);

    // Configure TIM2 CH2
    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_TOGGLE;
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
    TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_DISABLE;
    TIM_OC_InitStruct.CompareValue = 0;
    TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
    LL_TIM_OC_Init(TIM2, LL_TIM_CHANNEL_CH2, &TIM_OC_InitStruct);
    LL_TIM_OC_DisableFast(TIM2, LL_TIM_CHANNEL_CH2);
    LL_TIM_DisableMasterSlaveMode(TIM2);

    furi_hal_subghz_async_tx_middleware_idle(&furi_hal_subghz_async_tx.middleware);
    furi_hal_subghz_async_tx_refill(
        furi_hal_subghz_async_tx.buffer, FURI_HAL_SUBGHZ_ASYNC_TX_BUFFER_FULL);

    LL_TIM_EnableDMAReq_UPDATE(TIM2);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);

    // Start debug
    if(furi_hal_subghz_start_debug()) {
        const GpioPin* gpio = furi_hal_subghz.async_mirror_pin;
        // //Preparing bit mask
        // //Debug pin is may be only PORTB! (PB0, PB1, .., PB15)
        // furi_hal_subghz_debug_gpio_buff[0] = 0;
        // furi_hal_subghz_debug_gpio_buff[1] = 0;

        furi_hal_subghz_debug_gpio_buff[0] = gpio->pin;
        furi_hal_subghz_debug_gpio_buff[1] = (uint32_t)gpio->pin << GPIO_NUMBER;

        dma_config.MemoryOrM2MDstAddress = (uint32_t)furi_hal_subghz_debug_gpio_buff;
        dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (gpio->port->BSRR);
        dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
        dma_config.Mode = LL_DMA_MODE_CIRCULAR;
        dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
        dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
        dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
        dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
        dma_config.NbData = 2;
        dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM2_UP;
        dma_config.Priority = LL_DMA_PRIORITY_HIGH; // Ensure that it's updated after ARR
        LL_DMA_Init(SUBGHZ_DMA_CH2_DEF, &dma_config);
        LL_DMA_SetDataLength(SUBGHZ_DMA_CH2_DEF, 2);
        LL_DMA_EnableChannel(SUBGHZ_DMA_CH2_DEF);
    }

    // Start counter
#ifdef FURI_HAL_SUBGHZ_TX_GPIO
    furi_hal_gpio_write(&FURI_HAL_SUBGHZ_TX_GPIO, true);
#endif
    furi_hal_subghz_tx();

    LL_TIM_SetCounter(TIM2, 0);
    LL_TIM_EnableCounter(TIM2);

    return true;
}

bool furi_hal_subghz_is_async_tx_complete(void) {
    return (furi_hal_subghz.state == SubGhzStateAsyncTx) && (LL_TIM_GetAutoReload(TIM2) == 0);
}

void furi_hal_subghz_stop_async_tx(void) {
    furi_check(furi_hal_subghz.state == SubGhzStateAsyncTx);

    // Shutdown radio
    furi_hal_subghz_idle();

    // Deinitialize GPIO
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
#ifdef FURI_HAL_SUBGHZ_TX_GPIO
    furi_hal_gpio_write(&FURI_HAL_SUBGHZ_TX_GPIO, false);
#endif

    // Deinitialize Timer
    furi_hal_bus_disable(FuriHalBusTIM2);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, NULL, NULL);

    // Deinitialize DMA
    LL_DMA_DeInit(SUBGHZ_DMA_CH1_DEF);

    furi_hal_interrupt_set_isr(SUBGHZ_DMA_CH1_IRQ, NULL, NULL);

    // Stop debug
    if(furi_hal_subghz_stop_debug()) {
        LL_DMA_DisableChannel(SUBGHZ_DMA_CH2_DEF);
    }

    free(furi_hal_subghz_async_tx.buffer);

    float duty_cycle =
        100.0f * (float)furi_hal_subghz_async_tx.duty_high /
        ((float)furi_hal_subghz_async_tx.duty_low + (float)furi_hal_subghz_async_tx.duty_high);
    FURI_LOG_D(
        TAG,
        "Async TX Radio stats: on %0.0fus, off %0.0fus, DutyCycle: %0.0f%%",
        (double)furi_hal_subghz_async_tx.duty_high,
        (double)furi_hal_subghz_async_tx.duty_low,
        (double)duty_cycle);

    furi_hal_subghz.state = SubGhzStateIdle;
}
