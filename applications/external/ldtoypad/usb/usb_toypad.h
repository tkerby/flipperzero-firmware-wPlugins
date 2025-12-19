#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <pthread.h>

#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HID_EP_SZ  0x20 // 32 bytes packet size
#define HID_EP_IN  0x81
#define HID_EP_OUT 0x01

#define MAX_TOKENS 7

#define FRAME_TYPE_RESPONSE 0x55
#define FRAME_TYPE_REQUEST  0x56

typedef struct {
    unsigned char index;
    unsigned int id;
    unsigned int pad;
    unsigned char uid[7];
    uint8_t token[180];
    char name[16];
} Token;

typedef struct {
    Token* tokens[MAX_TOKENS];
    uint8_t tea_key[16];
} ToyPadEmu;

enum ConnectedStatus {
    ConnectedStatusDisconnected = 0,
    ConnectedStatusConnected = 1,
    ConnectedStatusReconnecting = 2,
    ConnectedStatusCleanupWanted = 3
};

extern ToyPadEmu* emulator;

extern FuriHalUsbInterface usb_hid_ldtoypad;

char* get_debug_text();

void set_debug_text(char* text);

usbd_device* get_usb_device();

bool ToyPadEmu_remove(int index);

void ToyPadEmu_clear();

Token* createCharacter(int id);

Token* createVehicle(int id, uint32_t upgrades[2]);

int get_connected_status();
void set_connected_status(int status);

void calculate_checksum(uint8_t* buf, int length, int place);

#ifdef __cplusplus
}
#endif
