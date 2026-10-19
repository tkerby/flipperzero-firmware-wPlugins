#include "battle_gfx.h"
#include "assets.h"
#include "bank.h"
#include "frame.h"
#include <stdlib.h>
#include <string.h>

bool bpic_load(MonPic* p, uint8_t species, uint8_t kind) {
    return monpic_load_kind(p, species, kind);
}

// ------------------------------------------------------------------------------------------------ platform
// Contract 73: the platforms are BAKED (tools/platbake, files/plat.bin: 16 pictures for the shapes the game uses, thickness 2): the builder that made them on every battle (an ellipse test per pixel, ~0.9 KB of
// plugin code) lives on in tools/platbake/plat_ref.h as the reference. The picture is read into one heap block (levels only, no mask: contract 67). Contract 82: the picture is mirror-symmetric, so only its LEFT HALF is
// stored and held (columns 0 .. rx + 1, the centre column included: 68 - 200 B, half of the heap of a battle's platforms); plat_draw paints it twice, the second time mirrored.
// plat.bin: "NPLT", version 2, n = 16, u16, n x { rx, ry, u16 offset }, then the half pictures (stride (rx + 2 + 3) / 4, 2 ry + 5 rows).
#define PLAT_N 16
bool plat_build(Plat* p, int rx, int ry, int thick) {
    (void)thick; // (the game's one thickness is 2)
    memset(p, 0, sizeof(*p));
    uint8_t d[8 + 4 * PLAT_N];
    if(!read_at("plat.bin", 0, d, sizeof(d))) return false;
    for(unsigned i = 8; i < sizeof(d); i += 4) {
        if(d[i] != rx || d[i + 1] != ry) continue;
        const int w = rx + 2, h = 2 * ry + 5, bpr = (w + 3) / 4;
        uint8_t* blk = malloc((size_t)bpr * h);
        if(!blk) return false;
        if(!read_at("plat.bin", (uint32_t)(d[i + 2] | d[i + 3] << 8), blk, (uint32_t)(bpr * h))) {
            free(blk);
            return false;
        }
        p->blk = blk;
        p->b.pix = blk;
        p->b.w = (uint8_t)w;
        p->b.h = (uint8_t)h;
        p->b.bpr = (uint8_t)bpr;
        p->ox = (int8_t)(rx + 1);
        p->oy = (int8_t)(ry + 1);
        return true;
    }
    return false;
}

void plat_free(Plat* p) {
    free(p->blk);
    memset(p, 0, sizeof(*p));
}

void plat_draw(LvlBuf lvl, Plat* p, int cx, int cy) { // the left half with its centre column on cx, then the same half mirrored from cx to the right edge (level 0 is transparent: the centre column is painted twice, the same)
    if(!p->blk) return;
    const int y = cy - p->oy;
    blit_auto(lvl, &p->b, cx - p->ox, y);
    p->b.flip = 1;
    blit_auto(lvl, &p->b, cx, y);
    p->b.flip = 0;
}
