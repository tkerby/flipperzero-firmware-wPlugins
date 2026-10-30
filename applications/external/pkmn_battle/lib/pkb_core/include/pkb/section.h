#pragma once

/* The Flipper loads each allocated ELF section of a FAP with its own malloc,
 * and needs a free heap block of the section size + 1 KiB. A single .text of
 * 64 KiB no longer fits (largest free block is about 62 KiB), so the biggest
 * functions go into a second code section. The name must not start with
 * ".text", or the SDK's linker script merges it back into .text. */
#if defined(__arm__)
#define PKB_TEXT2 __attribute__((section(".pkbtext")))
#else
#define PKB_TEXT2
#endif
