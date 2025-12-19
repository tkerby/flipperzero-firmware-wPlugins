#include "ToyPadEmu.h"

#include <dolphin/dolphin.h>

#include "usb/frame.h"
#include "bytes.h"
#include "minifigures.h"

#define TOKEN_DELAY_TIME 150 // delay time for the token to be placed on the pad in ms

ToyPadEmu* emulator;

ToyPadEmu* get_ToyPadEmu_inst() {
    return emulator;
}

bool quick_swap = false;

// Define box information (coordinates and dimensions) for each box (7 boxes total)
struct BoxInfo boxInfo[NUM_BOXES] = {
    {18, 26, false, -1}, // Selection 0 (box)
    {50, 20, false, -1}, // Selection 1 (circle)
    {85, 27, false, -1}, // Selection 2 (box)
    {16, 40, false, -1}, // Selection 3 (box)
    {35, 41, false, -1}, // Selection 4 (box)
    {70, 41, false, -1}, // Selection 5 (box)
    {86, 40, false, -1} // Selection 6 (box)
};

static unsigned char generate_checksum(const unsigned char* command, size_t len) {
    unsigned char result = 0;

    // Add bytes, wrapping naturally with unsigned char overflow
    for(size_t i = 0; i < len; ++i) {
        result += command[i];
    }

    return result;
}

// Remove a token
bool ToyPadEmu_remove(int index) {
    if(index < 0 || index >= MAX_TOKENS || emulator->tokens[index] == NULL) {
        return false; // Invalid index or already removed
    }

    // Send removal command (assuming this is already implemented)
    unsigned char buffer[32] = {0};
    buffer[0] = FRAME_TYPE_REQUEST; // Magic number
    buffer[1] = 0x0b; // Size
    buffer[2] = emulator->tokens[index]->pad; // Pad number
    buffer[3] = 0x00;
    buffer[4] = index; // Index of token to remove
    buffer[5] = 0x01; // Tag removed (not placed)
    memcpy(&buffer[6], emulator->tokens[index]->uid, 7); // UID
    buffer[13] = generate_checksum(buffer, 13);

    usbd_ep_write(get_usb_device(), HID_EP_IN, buffer, sizeof(buffer));

    if(emulator->tokens[index] != NULL) {
        // Free the token and clear the slot
        free(emulator->tokens[index]);
    }

    emulator->tokens[index] = NULL;

    return true;
}

void ToyPadEmu_clear() {
    // clear the tokens on the emulator and free the memory
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            free(emulator->tokens[i]);
            emulator->tokens[i] = NULL;
        }
    }
    memset(emulator->tokens, 0, sizeof(emulator->tokens));

    // clear all box info
    for(int i = 0; i < NUM_BOXES; i++) {
        boxInfo[i].isFilled = false;
        boxInfo[i].index = -1; // Reset index
    }
}

Token* ToyPadEmu_get_token(int index) {
    if(index < 0 || index >= MAX_TOKENS) {
        return NULL; // Invalid index
    }
    return emulator->tokens[index];
}

Token* ToyPadEmu_find_token_by_index(int index) {
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL && emulator->tokens[i]->index == index) {
            return emulator->tokens[i];
        }
    }
    return NULL;
}

void ToyPadEmu_remove_old_token(Token* token) {
    // Check if the selected token is already placed on the toypad check by uid if the token is already placed then remove the old one and place the new one
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            if(memcmp(emulator->tokens[i]->uid, token->uid, 7) == 0) {
                // remove the old token
                if(ToyPadEmu_remove(i)) {
                    // get the box of the old token and set it to not filled
                    for(int j = 0; j < NUM_BOXES; j++) {
                        if(boxInfo[j].index == i) {
                            boxInfo[j].isFilled = false;
                            boxInfo[j].index = -1; // Reset index
                            break;
                        }
                    }
                    furi_delay_ms(TOKEN_DELAY_TIME); // wait for the token to be removed
                }
                break;
            }
        }
    }
}

static void selectedBox_to_pad(Token* const token, int selectedBox);

bool ToyPadEmu_place(Token* token, int selectedBox) {
    ToyPadEmu_remove_old_token(token); // Remove old token if it exists by UID

    // Find an empty slot or use the next available index
    int new_index = -1;
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] == NULL) {
            new_index = i;
            break;
        }
    }
    if(new_index == -1) {
        return false; // No empty slot available
    }

    unsigned char buffer[32] = {0};

    selectedBox_to_pad(token, selectedBox);

    boxInfo[selectedBox].isFilled = true;
    token->index = new_index;
    emulator->tokens[new_index] = token;
    boxInfo[selectedBox].index = new_index;

    // Send placement command
    buffer[0] = FRAME_TYPE_REQUEST;
    buffer[1] = 0x0b; // Size always 11
    buffer[2] = token->pad;
    buffer[3] = 0x00;
    buffer[4] = token->index;
    buffer[5] = 0x00;
    memcpy(&buffer[6], token->uid, 7);
    buffer[13] = generate_checksum(buffer, 13);

    usbd_ep_write(get_usb_device(), HID_EP_IN, buffer, sizeof(buffer));

    /* Award some XP to the dolphin after placing a minifigure/vehicle. This needs to
    * happen outside of the ISR context of the USB, so we place it here.
    */
    dolphin_deed(DolphinDeedNfcReadSuccess);

    return true;
}

typedef enum {
    PAD_CIRCLE = 1,
    PAD_2 = 2,
    PAD_3 = 3,
} Pad;

static void selectedBox_to_pad(Token* const token, int selectedBox) {
    // Map 7 boxes to 3 pads
    static const Pad box_to_pad[7] = {
        PAD_2, // 0
        PAD_CIRCLE, // 1
        PAD_3, // 2
        PAD_2, // 3
        PAD_2, // 4
        PAD_3, // 5
        PAD_3 // 6
    };

    // Defensive bounds check: negative or out-of-range values trigger crash
    if((unsigned)selectedBox >= COUNT_OF(box_to_pad)) {
        furi_crash("Selected pad is invalid"); // It should never reach this
    }

    // Assign the pad using a simple, fast lookup
    token->pad = box_to_pad[selectedBox];
}

void ToyPadEmu_remove_all_tokens() {
    // Remove all tokens from the toypad by ToyPadEmu_remove with waiting between each removal
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            if(ToyPadEmu_remove(i)) {
                furi_delay_ms(TOKEN_DELAY_TIME); // wait for the token to be removed
            }
        }
    }
    // Clear the box info
    ToyPadEmu_clear();
}

void ToyPadEmu_place_tokens(Token* tokens[MAX_TOKENS], BoxInfo boxes[NUM_BOXES]) {
    if(tokens == NULL || boxes == NULL) {
        return; // Invalid input
    }
    // Clear toypad by removing all tokens
    ToyPadEmu_remove_all_tokens();

    // Place all the tokens on the toypad in the correct boxes from boxinfo and inxexes from tokens
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(tokens[i] != NULL) {
            // Find the box for this token
            for(int j = 0; j < NUM_BOXES; j++) {
                if(boxes[j].index == i) {
                    if(ToyPadEmu_place(tokens[i], j)) {
                        furi_delay_ms(TOKEN_DELAY_TIME);
                    }
                    break;
                }
            }
        }
    }
}

uint8_t ToyPadEmu_get_token_count() {
    // Count the number of tokens currently placed on the toypad
    uint8_t count = 0;
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            count++;
        }
    }
    return count;
}

int ToyPadEmu_get_token_count_by_id(unsigned int id) {
    // Get the number of tokens with the same ID

    if(quick_swap) {
        return 0;
    }

    int count = 0;

    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            if(emulator->tokens[i]->id == id) {
                count++;
            }
        } else {
            break;
        }
    }
    return count;
}

static void create_uid(Token* token, int id) {
    char version_name[7];
    snprintf(version_name, sizeof(version_name), "%s", furi_hal_version_get_name_ptr());

    token->uid[0] = 0x04; // uid always 0x04

    // when token is a vehicle we want a random uid for upgrades etc when creating a new vehicle
    if(is_vehicle(token)) {
        for(int i = 1; i <= 5; i++) {
            token->uid[i] = rand() % 256;
        }
    } else {
        int count = ToyPadEmu_get_token_count_by_id(id);

        // Generate UID for a minfig, that is always the same for your Flipper Zero
        for(int i = 1; i <= 5; i++) {
            // Combine id, version_name, and index for a hash
            token->uid[i] =
                (uint8_t)((id * 31 + count * 17 + version_name[i % sizeof(version_name)]) % 256);
        }
    }

    token->uid[6] = 0x80; // last uid byte always 0x80
}

Token* createCharacter(int id) {
    Token* token = (Token*)malloc(sizeof(Token)); // Allocate memory for the token

    token->id = id; // Set the ID

    create_uid(token, id); // Create the UID

    // convert the name to a string
    snprintf(token->name, sizeof(token->name), "%s", get_minifigure_name(id));

    return token; // Return the created token
}

Token* createVehicle(int id, uint32_t upgrades[2]) {
    Token* token = (Token*)malloc(sizeof(Token));

    // Initialize the token data to zero
    memset(token, 0, sizeof(Token));

    // Dont set an ID for vehicles, it is in the token buffer already, we can use this to see if it is an minfig or vehicle by checking if the ID set or not

    // Generate a random UID and store it
    create_uid(token, id);

    // Write the upgrades and ID to the token data
    writeUInt32LE(&token->token[0x23 * 4], upgrades[0], 0); // Upgrades[0]
    writeUInt16LE(&token->token[0x24 * 4], id, 0); // ID
    writeUInt32LE(&token->token[0x25 * 4], upgrades[1], 0); // Upgrades[1]
    writeUInt16BE(&token->token[0x26 * 4], 1, 0); // Constant value 1 (Big Endian)

    snprintf(token->name, sizeof(token->name), "%s", get_vehicle_name(id));

    return token;
}
