#pragma once

//#include "nfc_eink_tag.h"
//#include "../nfc_eink_types.h"
#include "../nfc_eink_screen_i.h"
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/nfc_common.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

#define eink_goodisplay_on_done(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeFinish)

#define eink_goodisplay_on_config_received(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeConfigurationReceived)

#define eink_goodisplay_on_target_detected(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetDetected)

typedef enum {
    NfcEinkScreenTypeGoodisplayUnknown,
    NfcEinkScreenTypeGoodisplay2n13inch,
    NfcEinkScreenTypeGoodisplay2n9inch,
    //NfcEinkTypeGoodisplay4n2inch,

    NfcEinkScreenTypeGoodisplayNum
} NfcEinkScreenTypeGoodisplay;

#pragma pack(push, 1)
typedef struct {
    uint8_t CLA_byte;
    uint8_t CMD_code;
    uint8_t P1;
    uint8_t P2;
} APDU_Header;

typedef struct {
    APDU_Header header;
    uint8_t data_length;
    uint8_t data[];
} APDU_Command;

typedef APDU_Command APDU_Command_Select;

typedef struct {
    APDU_Header header;
    uint8_t expected_length;
} APDU_Command_Read;

typedef struct {
    uint8_t command_code;
    uint8_t command_data[];
    //APDU_Command* apdu_command;
} NfcEinkScreenCommand;

typedef struct {
    uint16_t status;
} APDU_Response;

typedef struct {
    uint16_t data_length; ///TODO: remove this to CC_File struct
    uint8_t data[];
} APDU_Response_Read;

typedef struct {
    uint16_t length;
    uint8_t ndef_message[];
} NDEF_File;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t file_id;
    uint16_t file_size;
    uint8_t read_access_flags;
    uint8_t write_access_flags;
} NDEF_File_Ctrl_TLV;

typedef struct {
    ///TODO: uint16_t data_length; //maybe this should be here instead of APDU_Response_Read
    uint8_t mapping_version;
    uint16_t r_apdu_max_size;
    uint16_t c_apdu_max_size;
    NDEF_File_Ctrl_TLV ctrl_file;
} CC_File;

typedef struct {
    uint8_t response_code;
    union {
        APDU_Response apdu_response;
        APDU_Response_Read apdu_response_read;
    } apdu_resp;
} NfcEinkScreenResponse;
#pragma pack(pop)

/// Some Poller shit
typedef enum {
    //Idle,
    SendC2Cmd,
    SelectNDEFTagApplication,
    SelectNDEFFile,
    ReadFIDFileData,
    Select0xE104File,
    Read0xE104FileData,
    SendConfigCmd,
    DuplicateC2Cmd,
    SendDataCmd,
    UpdateDisplay,
    SendDataDone,

    NfcEinkScreenGoodisplayPollerStateError,
    NfcEinkScreenGoodisplayPollerStateNum
} NfcEinkScreenGoodisplayPollerState;

/// -----------------------
typedef struct {
    NfcEinkScreenGoodisplayPollerState state;
} NfcEinkScreenSpecificGoodisplayContext;

void eink_goodisplay_parse_config(NfcEinkScreen* screen, const uint8_t* data, uint8_t data_length);
NfcCommand eink_goodisplay_listener_callback(NfcGenericEvent event, void* context);
NfcCommand eink_goodisplay_poller_callback(NfcGenericEvent event, void* context);
