/*
 * Gen 2 link patch lists (engine/link/link.asm: FixDataForLinkTransfer and the
 * .party_patch_loop in Gen2ToGen2LinkComms). Replaces the trade app's
 * dynamically allocated list with fixed arrays.
 */
#include <string.h>
#include <pkb/party_block.h>

#define PATCH_DATA_SIZE 0xFC /* SERIAL_PATCH_DATA_SIZE */
/* Entries are written starting at wPlayerPatchLists + SERIAL_RNS_LENGTH. */
#define PATCH_FIRST_ENTRY 10

bool pkb_patch_list_build(uint8_t* region, size_t region_len, uint8_t patch[PKB_PATCH_BLOCK_LEN]) {
    size_t out = PATCH_FIRST_ENTRY;
    bool ok = true;

    memset(patch, 0, PKB_PATCH_BLOCK_LEN);
    memset(patch, PKB_SERIAL_PREAMBLE_BYTE, PKB_PATCH_PREAMBLE_LEN);

    for(size_t part = 0; part < 2; part++) {
        size_t base = part * PATCH_DATA_SIZE;
        size_t end = part == 0 ? PATCH_DATA_SIZE : region_len;
        if(end > region_len) end = region_len;
        for(size_t i = base; i < end; i++) {
            if(region[i] != PKB_SERIAL_NO_DATA_BYTE) continue;
            region[i] = PKB_SERIAL_PATCH_TERMINATOR; /* SERIAL_PATCH_REPLACEMENT_BYTE */
            if(out < PKB_PATCH_BLOCK_LEN) patch[out] = (uint8_t)(i - base + 1);
            else ok = false;
            out++;
        }
        if(out < PKB_PATCH_BLOCK_LEN) patch[out] = PKB_SERIAL_PATCH_TERMINATOR;
        else ok = false;
        out++;
    }
    return ok;
}

void pkb_patch_list_apply(const uint8_t* patch, size_t patch_len, uint8_t* region, size_t region_len) {
    size_t base = 0;
    int parts_left = 2;
    for(size_t i = 0; i < patch_len && parts_left; i++) {
        uint8_t b = patch[i];
        if(b == 0 || b == PKB_SERIAL_PREAMBLE_BYTE || b == PKB_SERIAL_NO_DATA_BYTE) continue;
        if(b == PKB_SERIAL_PATCH_TERMINATOR) {
            base = PATCH_DATA_SIZE;
            parts_left--;
            continue;
        }
        size_t idx = base + b - 1;
        if(idx < region_len) region[idx] = PKB_SERIAL_NO_DATA_BYTE;
    }
}
