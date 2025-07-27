#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <pthread.h>

#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TOKENS 7

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
    int token_count;
    uint8_t tea_key[16];
} ToyPadEmu;

extern ToyPadEmu* emulator;

extern FuriHalUsbInterface usb_hid_ldtoypad;

char* get_debug_text();

void set_debug_text(char* text);

usbd_device* get_usb_device();

bool ToyPadEmu_remove(int index);

Token* createCharacter(int id);

Token* createVehicle(int id, uint32_t upgrades[2]);

int get_connected_status();
void set_connected_status(int status);

#ifdef __cplusplus
}
#endif
