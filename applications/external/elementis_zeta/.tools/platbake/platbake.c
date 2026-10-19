// platbake: the battle platforms (plat_ref.h, the original builder) -> nucleo/files/plat.bin (contract 73).
//   plat.bin = "NPLT", u8 version 2, u8 n (= PLAT_N), u16 0, n x { u8 rx, u8 ry, u16 offset }, then each platform's 2-bit picture: THE LEFT HALF only, columns 0 .. rx + 1 (the centre column included; row stride (rx + 2 + 3) / 4,
//   2 ry + 5 rows, thickness 2). The picture is mirror-symmetric (the baker checks every pixel), the plugin draws the half twice, the second time mirrored (Blit.flip): half of the heap during a battle (contract 82).
// The plugin used to build them on every battle (plat_build: ~0.9 KB of code); it now reads the one it needs (battle_gfx.c).
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "plat_ref.h"

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "usage: platbake OUT.bin\n");
        return 2;
    }
    uint8_t out[8 + 4 * PLAT_N + 16384];
    memset(out, 0, sizeof(out));
    memcpy(out, "NPLT", 4);
    out[4] = 2;
    out[5] = PLAT_N;
    size_t pos = 8 + 4 * PLAT_N;
    for(int i = 0; i < PLAT_N; i++) {
        int rx, ry, w, h;
        plat_shape(i, &rx, &ry);
        uint8_t* blk = plat_ref_build(rx, ry, 2, &w, &h);
        if(!blk) return 1;
        const int bpr = (w + 3) / 4, hw = rx + 2, hbpr = (hw + 3) / 4; // the full row stride, the half's width and stride
        for(int y = 0; y < h; y++)
            for(int x = 0; x < w; x++) {
                const int a = (blk[y * bpr + (x >> 2)] >> ((x & 3) * 2)) & 3, m = w - 1 - x, c = (blk[y * bpr + (m >> 2)] >> ((m & 3) * 2)) & 3;
                if(a != c) {
                    fprintf(stderr, "platbake: platform rx %d ry %d is not mirror-symmetric at (%d, %d)\n", rx, ry, x, y);
                    return 1;
                }
            }
        uint8_t* half = calloc(1, (size_t)hbpr * h);
        for(int y = 0; y < h; y++)
            for(int x = 0; x < hw; x++) half[y * hbpr + (x >> 2)] |= (uint8_t)(((blk[y * bpr + (x >> 2)] >> ((x & 3) * 2)) & 3) << ((x & 3) * 2));
        free(blk);
        blk = half;
        const size_t n = (size_t)hbpr * h;
        if(pos + n > sizeof(out)) {
            fprintf(stderr, "platbake: out of room\n");
            return 1;
        }
        out[8 + 4 * i] = (uint8_t)rx;
        out[9 + 4 * i] = (uint8_t)ry;
        out[10 + 4 * i] = (uint8_t)(pos & 255);
        out[11 + 4 * i] = (uint8_t)(pos >> 8);
        memcpy(out + pos, blk, n);
        pos += n;
        free(blk);
    }
    FILE* f = fopen(argv[1], "wb");
    if(!f) return 1;
    fwrite(out, 1, pos, f);
    fclose(f);
    printf("platbake: %s  %d platforms, %zu bytes\n", argv[1], PLAT_N, pos);
    return 0;
}
