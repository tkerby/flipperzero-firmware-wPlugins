/**
 * @file hvac_hitachi.h
 * @brief Hitachi remote code generator.
 * @version 0.1
 * @date 2025-05-23
 */

#pragma once

#include <stdint.h>

#include <infrared_transmit.h>
#include <infrared_worker.h>

#define HVAC_HITACHI_TEMPERATURE_MIN 17
#define HVAC_HITACHI_TEMPERATURE_MAX 30

#define HVAC_HITACHI_ADDRESS_MASK (0xf0u)
#define HVAC_HITACHI_KEYCODE_MASK (0x0fu)

typedef enum {
    HvacHitachiKeycodePower = 0x01,
    HvacHitachiKeycodeMode = 0x04,
    HvacHitachiKeycodeFanSpeed,
    HvacHitachiKeycodeTempDown = 0x08,
    HvacHitachiKeycodeTempUp,
    HvacHitachiKeycodeTimerSet = 0x0b,
    HvacHitachiKeycodeTimerReset = 0x0d,
    HvacHitachiKeycodeVane,
    HvacHitachiKeycodeResetFilter,
} HvacHitachiKeycode;

typedef enum {
    HvacHitachiFanSpeedLow = 0x10,
    HvacHitachiFanSpeedMedium = 0x20,
    HvacHitachiFanSpeedHigh = 0x40,
} HvacHitachiFanSpeed;

typedef enum {
    HvacHitachiModeFan = 0x2,
    HvacHitachiModeDehumidifying = 0x4,
    HvacHitachiModeCooling = 0x6,
    HvacHitachiModeHeating = 0x8,
} HvacHitachiMode;

typedef enum {
    HvacHitachiVanePos0,
    HvacHitachiVaneAuto,
    HvacHitachiVanePos1,
    HvacHitachiVanePos2 = 0x4,
    HvacHitachiVanePos3 = 0x6,
    HvacHitachiVanePos4 = 0x8,
    HvacHitachiVanePos5 = 0xa,
    HvacHitachiVanePos6 = 0xc,
} HvacHitachiVane;

typedef enum {
    HvacHitachiControlPowerOn = 0x90,
    HvacHitachiControlPowerOff = 0xa0,
} HvacHitachiControl;

typedef struct FURI_PACKED HvacHitachiPayloadSet1_s {
    union {
        uint8_t temperature;
        uint8_t fan_speed_mode;
        uint8_t vane;
    };
} HvacHitachiPayloadSet1;

typedef struct FURI_PACKED HvacHitachiPayloadModePower_s {
    uint8_t temperature;
    uint8_t fan_speed_mode;
    uint8_t vane;
    uint8_t control;
} HvacHitachiPayloadModePower;

typedef struct FURI_PACKED HvacHitachiPayloadTimer_s {
    uint8_t temperature;
    uint16_t timer_lo;
    uint8_t timer_hi;
    uint8_t fan_speed_mode;
    uint8_t vane;
} HvacHitachiPayloadModeTimer;

typedef struct FURI_PACKED HvacHitachiMessageHeader_s {
    uint8_t type;
    uint8_t sbff;
    uint8_t size;
} HvacHitachiMessageHeader;

typedef struct FURI_PACKED HvacHitachiMessageHeader2_s {
    uint8_t sb89;
    uint8_t keycode_address;
    uint8_t sb3f;
} HvacHitachiMessageHeader2;

typedef struct FURI_PACKED HvacHitachiMessage_s {
    HvacHitachiMessageHeader header;
    HvacHitachiMessageHeader2 header2;
    union {
        HvacHitachiPayloadSet1 set1;
        HvacHitachiPayloadModePower set_mode_power;
        HvacHitachiPayloadModeTimer set_timer;
    };
} HvacHitachiMessage;
#define HVAC_HITACHI_SIZE_SET0 (0xe0 - 1 + sizeof(HvacHitachiMessageHeader2))
#define HVAC_HITACHI_SIZE_SET1 \
    (0xe0 - 1 + sizeof(HvacHitachiMessageHeader2) + sizeof(HvacHitachiPayloadSet1))
#define HVAC_HITACHI_SIZE_POWER \
    (0xe0 - 1 + sizeof(HvacHitachiMessageHeader2) + sizeof(HvacHitachiPayloadModePower))
#define HVAC_HITACHI_SIZE_MODE (HVAC_HITACHI_SIZE_POWER - 1)
#define HVAC_HITACHI_SIZE_TIMER \
    (0xe0 - 1 + sizeof(HvacHitachiMessageHeader2) + sizeof(HvacHitachiPayloadModeTimer))
#define HVAC_HITACHI_SIZE_KEYCODE_ONLY (HVAC_HITACHI_SIZE_SET0 - 1)

typedef struct HvacHitachiContext_s {
    /**
   * @brief Raw message buffer.
   */
    HvacHitachiMessage msg;
    /**
   * @brief Number of samples actually in the sample buffer.
   */
    size_t num_samples;
    /**
   * @brief The sample buffer.
   * @details Preamble (2 samples) + sync code (3 bytes) + max message size (w/
   * parity) + epilogue (1-2 samples) = 436 samples (1744 bytes)
   */
    uint32_t samples[2 + 8 * (3 + sizeof(HvacHitachiMessage) * 2) * 2 + 2];
} HvacHitachiContext;

/**
 * @brief Allocate the context buffer.
 *
 * @return Allocated context buffer.
 */
extern HvacHitachiContext* hvac_hitachi_init();

/**
 * @brief Deallocate the context buffer.
 *
 * @param ctx Allocated context buffer.
 */
extern void hvac_hitachi_deinit(HvacHitachiContext* const ctx);

/**
 * @brief Reset the context buffer.
 *
 * @param ctx Allocated context buffer.
 */
extern void hvac_hitachi_reset(HvacHitachiContext* const ctx);

/**
 * @brief Mark the message as a message for AC units operating on the specified
 * address.
 *
 * @param ctx Context buffer.
 * @param address Address the targeted AC unit is operating on.
 */
extern void hvac_hitachi_switch_address(HvacHitachiContext* const ctx, uint8_t address);

/**
 * @brief Build set temperature message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 */
extern void hvac_hitachi_set_temperature(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    bool is_up);

/**
 * @brief Build set fan speed/mode message.
 *
 * @param ctx Context buffer.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 */
extern void hvac_hitachi_set_fan_speed_mode(
    HvacHitachiContext* const ctx,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode);

/**
 * @brief Build set angle of vane message.
 *
 * @param ctx Context buffer.
 * @param vane Vane Angle of vane.
 */
extern void hvac_hitachi_set_vane(HvacHitachiContext* const ctx, HvacHitachiVane vane);

/**
 * @brief Build set mode message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 * @param vane Angle of vane.
 */
extern void hvac_hitachi_set_mode(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane);

/**
 * @brief Build set power message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 * @param vane Angle of vane.
 * @param control Control flag.
 */
extern void hvac_hitachi_set_power(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane,
    HvacHitachiControl control);

/**
 * @brief Build set timer message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param off_timer Off timer in minutes.
 * @param on_timer On timer in minutes.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 * @param vane Angle of vane.
 */
extern void hvac_hitachi_set_timer(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    uint_fast16_t off_timer,
    uint_fast16_t on_timer,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane);

/**
 * @brief Build reset timer message.
 *
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_reset_timer(HvacHitachiContext* const ctx);

/**
 * @brief Build reset filter message.
 *
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_reset_filter(HvacHitachiContext* const ctx);

/**
 * @brief Build message before sending.
 * 
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_build_samples(HvacHitachiContext* const ctx);

/**
 * @brief Send a built message.
 *
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_send(HvacHitachiContext* const ctx);
