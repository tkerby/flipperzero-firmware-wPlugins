#pragma once

#include <furi.h>

#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include "../usb/usb_toypad.h"

#include "sub_screens.h"

#define NUM_BOXES 7 // the number of boxes (7 boxes always)

#define TOKEN_DELAY_TIME 150 // delay time for the token to be placed on the pad in ms

typedef struct LDToyPadSceneEmulate LDToyPadSceneEmulate;

LDToyPadSceneEmulate* ldtoypad_scene_emulate_alloc();

void ldtoypad_scene_emulate_free(LDToyPadSceneEmulate* ldtoypad_emulate);

View* ldtoypad_scene_emulate_get_view(LDToyPadSceneEmulate* ldtoypad_scene_emulate);

unsigned char generate_checksum(const unsigned char* command, size_t len);

void selectedBox_to_pad(Token* new_character, int selectedBox);

typedef enum {
    MiniSelectionFavorite,
    MiniSelectionSave,
    MiniSelectionCount // Total number of selections, must always be last
} MiniSelectionType;

struct BoxInfo {
    const uint8_t x; // X-coordinate
    const uint8_t y; // Y-coordinate
    bool isFilled; // Indicates if the box is filled with a Token (minifig / vehicle)
    int index; // The index of the token in the box
};
typedef struct BoxInfo BoxInfo;

typedef struct {
    bool left_pressed;
    bool right_pressed;
    bool up_pressed;
    bool down_pressed;
    bool ok_pressed;
    // bool back_pressed;
    bool back_long_pressed;
    bool connected;
    char* connection_status;

    uint32_t selected_minifigure_index;
    uint32_t selected_vehicle_index;

    usbd_device* usbDevice;

    // setting indexes below
    bool show_debug_text_index;
    bool show_icons_index;
    bool minifig_only_mode;
    bool quick_swap;

    bool show_mini_menu_selected;
    MiniSelectionType mini_option_selected;

    // Inner-screens / pseudo-screens
    bool show_placement_selection_screen;
    SelectionType sub_screen_box_selected;
} LDToyPadSceneEmulateModel;

void minifigures_submenu_callback(void* context, uint32_t index);
void vehicles_submenu_callback(void* context, uint32_t index);
void saved_token_submenu_callback(void* context, uint32_t index);

int get_token_count_of_specific_id(unsigned int id);

extern struct BoxInfo boxInfo[NUM_BOXES]; // Array of box information
