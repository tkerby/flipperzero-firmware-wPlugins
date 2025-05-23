/**
 * @file furi_hal_version.h
 * Version HAL API
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <lib/toolbox/version.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FURI_HAL_VERSION_NAME_LENGTH        (8)
#define FURI_HAL_VERSION_ARRAY_NAME_LENGTH  (FURI_HAL_VERSION_NAME_LENGTH + 1)
/** 31b BLE Adv - 3b flags - 2b name prefix - 4b service uuid - 3b tx power = 19, + 1b null terminator (not present in packet) */
#define FURI_HAL_BT_ADV_NAME_LENGTH         (20)
/** BLE symbol + name */
#define FURI_HAL_VERSION_DEVICE_NAME_LENGTH (1 + FURI_HAL_BT_ADV_NAME_LENGTH)

/** OTP Versions enum */
typedef enum {
    FuriHalVersionOtpVersion0 = 0x00,
    FuriHalVersionOtpVersion1 = 0x01,
    FuriHalVersionOtpVersion2 = 0x02,
    FuriHalVersionOtpVersionEmpty = 0xFFFFFFFE,
    FuriHalVersionOtpVersionUnknown = 0xFFFFFFFF,
} FuriHalVersionOtpVersion;

/** Device Colors */
typedef enum {
    FuriHalVersionColorUnknown = 0x00,
    FuriHalVersionColorBlack = 0x01,
    FuriHalVersionColorWhite = 0x02,
    FuriHalVersionColorTransparent = 0x03,
    FuriHalVersionColorCount,
} FuriHalVersionColor;

/** Device Regions */
typedef enum {
    FuriHalVersionRegionUnknown = 0x00,
    FuriHalVersionRegionEuRu = 0x01,
    FuriHalVersionRegionUsCaAu = 0x02,
    FuriHalVersionRegionJp = 0x03,
    FuriHalVersionRegionWorld = 0x04,
} FuriHalVersionRegion;

/** Device Display */
typedef enum {
    FuriHalVersionDisplayUnknown = 0x00,
    FuriHalVersionDisplayErc = 0x01,
    FuriHalVersionDisplayMgg = 0x02,
} FuriHalVersionDisplay;

/** Init flipper version
 */
void furi_hal_version_init(void);

/** Check target firmware version
 *
 * @return     true if target and real matches
 */
bool furi_hal_version_do_i_belong_here(void);

/** Get model name
 *
 * @return     model name C-string
 */
const char* furi_hal_version_get_model_name(void);

/** Get model name
 *
 * @return     model code C-string
 */
const char* furi_hal_version_get_model_code(void);

/** Get FCC ID
 *
 * @return     FCC id as C-string
 */
const char* furi_hal_version_get_fcc_id(void);

/** Get IC id
 *
 * @return     IC id as C-string
 */
const char* furi_hal_version_get_ic_id(void);

/** Get MIC id
 *
 * @return     MIC id as C-string
 */
const char* furi_hal_version_get_mic_id(void);

/** Get SRRC id
 *
 * @return     SRRC id as C-string
 */
const char* furi_hal_version_get_srrc_id(void);

/** Get NCC id
 *
 * @return     NCC id as C-string
 */
const char* furi_hal_version_get_ncc_id(void);

/** Get OTP version
 *
 * @return     OTP Version
 */
FuriHalVersionOtpVersion furi_hal_version_get_otp_version(void);

/** Get hardware version
 *
 * @return     Hardware Version
 */
uint8_t furi_hal_version_get_hw_version(void);

/** Get hardware target
 *
 * @return     Hardware Target
 */
uint8_t furi_hal_version_get_hw_target(void);

/** Get hardware body
 *
 * @return     Hardware Body
 */
uint8_t furi_hal_version_get_hw_body(void);

/** Get hardware body color
 *
 * @return     Hardware Color
 */
FuriHalVersionColor furi_hal_version_get_hw_color(void);

/** Get hardware connect
 *
 * @return     Hardware Interconnect
 */
uint8_t furi_hal_version_get_hw_connect(void);

/** Get hardware region (fake) = 0
 *
 * @return     Hardware Region (fake)
 */
FuriHalVersionRegion furi_hal_version_get_hw_region(void);

/** Get hardware region name (fake) = R00
 *
 * @return     Hardware Region name (fake)
 */
const char* furi_hal_version_get_hw_region_name(void);

/** Get hardware region (OTP)
 *
 * @return     Hardware Region
 */
FuriHalVersionRegion furi_hal_version_get_hw_region_otp(void);

/** Get hardware display id
 *
 * @return     Display id
 */
FuriHalVersionDisplay furi_hal_version_get_hw_display(void);

/** Get hardware timestamp
 *
 * @return     Hardware Manufacture timestamp
 */
uint32_t furi_hal_version_get_hw_timestamp(void);

/** Get pointer to target name
 *
 * @return     Hardware Name C-string
 */
const char* furi_hal_version_get_name_ptr(void);

/** Get pointer to target device name
 *
 * @return     Hardware Device Name C-string
 */
const char* furi_hal_version_get_device_name_ptr(void);

/** Get pointer to target ble local device name
 *
 * @return     Ble Device Name C-string
 */
const char* furi_hal_version_get_ble_local_device_name_ptr(void);

/** Set flipper name
 */
void furi_hal_version_set_name(const char* name);

/** Get BLE MAC address
 *
 * @return     pointer to BLE MAC address
 */
const uint8_t* furi_hal_version_get_ble_mac(void);

/** Get address of version structure of firmware.
 *
 * @return     Address of firmware version structure.
 */
const struct Version* furi_hal_version_get_firmware_version(void);

/** Get platform UID size in bytes
 *
 * @return     UID size in bytes
 */
size_t furi_hal_version_uid_size(void);

/** Get const pointer to UID
 *
 * @return     pointer to UID
 */
const uint8_t* furi_hal_version_uid(void);

const uint8_t* furi_hal_version_uid_default(void);

#ifdef __cplusplus
}
#endif
