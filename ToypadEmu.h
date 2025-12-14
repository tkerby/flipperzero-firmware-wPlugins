#pragma once

#ifndef TOYPADEMU_H
#define TOYPADEMU_H

#include <furi.h>

#define MAX_TOKENS 7
#define NUM_BOXES  7 // the number of boxes (7 boxes always)

#ifdef __cplusplus
extern "C" {
#endif

extern bool quick_swap;

typedef struct {
    unsigned char index;
    unsigned int id;
    unsigned int pad;
    unsigned char uid[7];
    uint8_t token[180];
    char name[16];
} Token;

typedef struct BoxInfo {
    const uint8_t x; // X-coordinate
    const uint8_t y; // Y-coordinate
    bool isFilled; // Indicates if the box is filled with a Token (minifig / vehicle)
    int index; // The index of the token in the box
} BoxInfo;

typedef struct {
    Token* tokens[MAX_TOKENS];
    uint8_t tea_key[16];
} ToyPadEmu;

ToyPadEmu* get_ToyPadEmu_inst();

bool ToyPadEmu_remove(int index);

void ToyPadEmu_clear();

void ToyPadEmu_remove_all_tokens();

uint8_t ToyPadEmu_get_token_count();

bool ToyPadEmu_place_token(Token* token, int selectedBox);

void ToyPadEmu_place_tokens(Token* tokens[MAX_TOKENS], BoxInfo boxes[NUM_BOXES]);

void ToyPadEmu_remove_old_token(Token* token);

int ToyPadEmu_get_token_count_by_id(unsigned int id);

static inline bool is_vehicle(const Token* token) {
    return !token->id;
}
static inline bool is_minifig(const Token* token) {
    return token->id != 0;
}

Token* ToyPadEmu_get_token(int index);

Token* ToyPadEmu_find_token_by_index(int index);

Token* createCharacter(int id);

Token* createVehicle(int id, uint32_t upgrades[2]);

#ifdef __cplusplus
}
#endif

#endif // TOYPADEMU_H
