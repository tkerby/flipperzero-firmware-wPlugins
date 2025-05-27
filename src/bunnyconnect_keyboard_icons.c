#include "../lib/bunnyconnect_keyboard_icons.h"
#include <gui/icon_i.h>

// Save button icons data
static const uint8_t I_KeySave_24x11_data[] = {0x07, 0xE0, 0x08, 0x10, 0x08, 0x10, 0x07,
                                               0xE0, 0x00, 0x00, 0x0F, 0xF0, 0x08, 0x10,
                                               0x08, 0x50, 0x08, 0x20, 0x08, 0x10, 0x0F,
                                               0xF0, 0xFF, 0xFF, 0x80, 0x01, 0x80, 0x01,
                                               0x80, 0x01, 0x80, 0x01, 0xFF, 0xFF};

static const uint8_t I_KeySaveSelected_24x11_data[] = {0x07, 0xE0, 0x0F, 0xF0, 0x0F, 0xF0, 0x07,
                                                       0xE0, 0x00, 0x00, 0x0F, 0xF0, 0x0F, 0xF0,
                                                       0x0F, 0xF0, 0x0F, 0xE0, 0x0F, 0xF0, 0x0F,
                                                       0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Keyboard switch icons data
static const uint8_t I_KeyKeyboard_10x11_data[] = {0xFF, 0xC0, 0x80, 0x40, 0x80, 0x40, 0x8F, 0xC0,
                                                   0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
                                                   0x80, 0x00, 0x80, 0x00, 0xFF, 0xC0};

static const uint8_t I_KeyKeyboardSelected_10x11_data[] = {0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0,
                                                           0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0,
                                                           0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0,
                                                           0xFF, 0xC0, 0xFF, 0xC0};

// Backspace icons data
static const uint8_t I_KeyBackspace_16x9_data[] = {
    0xFF,
    0xF0,
    0x80,
    0x10,
    0x80,
    0x30,
    0x83,
    0xF0,
    0x86,
    0x30,
    0x8C,
    0x10,
    0x98,
    0x10,
    0xB0,
    0x30,
    0xFF,
    0xF0};

static const uint8_t I_KeyBackspaceSelected_16x9_data[] = {
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0,
    0xFF,
    0xF0};

// Define frames arrays
static const uint8_t* const I_KeySave_24x11_frames[] = {I_KeySave_24x11_data};
static const uint8_t* const I_KeySaveSelected_24x11_frames[] = {I_KeySaveSelected_24x11_data};
static const uint8_t* const I_KeyKeyboard_10x11_frames[] = {I_KeyKeyboard_10x11_data};
static const uint8_t* const I_KeyKeyboardSelected_10x11_frames[] = {
    I_KeyKeyboardSelected_10x11_data};
static const uint8_t* const I_KeyBackspace_16x9_frames[] = {I_KeyBackspace_16x9_data};
static const uint8_t* const I_KeyBackspaceSelected_16x9_frames[] = {
    I_KeyBackspaceSelected_16x9_data};

// Create icon instances
const Icon I_KeySave_24x11 = {
    .width = 24,
    .height = 11,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = I_KeySave_24x11_frames};

const Icon I_KeySaveSelected_24x11 = {
    .width = 24,
    .height = 11,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = I_KeySaveSelected_24x11_frames};

const Icon I_KeyKeyboard_10x11 = {
    .width = 10,
    .height = 11,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = I_KeyKeyboard_10x11_frames};

const Icon I_KeyKeyboardSelected_10x11 = {
    .width = 10,
    .height = 11,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = I_KeyKeyboardSelected_10x11_frames};

const Icon I_KeyBackspace_16x9 = {
    .width = 16,
    .height = 9,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = I_KeyBackspace_16x9_frames};

const Icon I_KeyBackspaceSelected_16x9 = {
    .width = 16,
    .height = 9,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = I_KeyBackspaceSelected_16x9_frames};
