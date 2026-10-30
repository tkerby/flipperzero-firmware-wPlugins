/*
 * A minimal CGB boot ROM written for this harness (SameBoy's own boot ROMs
 * need rgbds to build). It sets the post-boot state a CGB game looks at,
 * enables CGB mode from the header's CGB flag, unmaps itself and falls
 * through to $0100. No logo check or animation.
 */
#include <stdint.h>
#include <string.h>

#include "boot_stub.h"

static const uint8_t code[] = {
    0x31, 0xFE, 0xFF, /* ld sp, $FFFE */
    0xFA, 0x43, 0x01, /* ld a, [$0143]   ; CGB flag */
    0xE0, 0x4C, /*       ldh [rKEY0], a  ; $80/$C0: CGB mode */
    0x3E, 0x91, /*       ld a, $91       ; LCD on, as the real boot ROM */
    0xE0, 0x40, /*       ldh [rLCDC], a  ; leaves it (Init waits for LY) */
    0x3E, 0x11, /*       ld a, $11       ; "running on a CGB" */
    0x01, 0x00, 0x00, /* ld bc, $0000 */
    0x11, 0x56, 0xFF, /* ld de, $FF56 */
    0x21, 0x0D, 0x00, /* ld hl, $000D */
    0xC3, 0xFE, 0x00, /* jp $00FE */
};

void boot_stub_build(uint8_t rom[BOOT_STUB_SIZE]) {
    memset(rom, 0, BOOT_STUB_SIZE);
    memcpy(rom, code, sizeof(code));
    rom[0xFE] = 0xE0; /* ldh [rBANK], a  ; bit 0 set: unmap, PC -> $0100 */
    rom[0xFF] = 0x50;
}
